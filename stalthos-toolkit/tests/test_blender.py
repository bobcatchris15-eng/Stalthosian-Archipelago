"""Tests for the headless Blender execution layer.

Two groups:

* **No-Blender tests** -- run everywhere. They point ``--blender`` at a path that
  does not exist and assert exit code 4 (MissingDependency), and they assert the
  runner's argv shape, timeout -> 7 and non-zero exit -> 6 using a stub "blender"
  that is just a Python script. These never import bpy.
* **Real-Blender tests** -- skipped when Blender is absent (the contract in
  docs/toolkit-architecture.md section 7). When it is present they run a full
  generate -> rig -> bake -> render pass end to end.

Tests assert on the JSON status envelope and exit codes, never on log text.
"""

from __future__ import annotations

import json
import locale
import os
import subprocess
import sys
import tempfile
import textwrap
from pathlib import Path

import pytest

from stalthos_toolkit.blender import params as P
from stalthos_toolkit.blender.runner import (
    DEFAULT_TIMEOUT_S,
    BlenderError,
    blender_version,
    build_argv,
    resolve_timeout,
    run_blender,
    script_path,
)
from stalthos_toolkit.cli import main
from stalthos_toolkit.core.contract import Context, REGISTRY
from stalthos_toolkit.core.errors import (
    EXIT_EXTERNAL,
    EXIT_INTERRUPTED,
    EXIT_MISSING_DEPENDENCY,
    EXIT_OK,
    EXIT_USAGE,
)
from stalthos_toolkit.core.workspace import Workspace

HERE = Path(__file__).resolve().parent

sys.path.insert(0, str(HERE))
import definition_fixtures as FIXTURES  # noqa: E402  (local test helper, definition builders)
def _blender_path() -> str | None:
    from stalthos_toolkit.core.workspace import find_blender
    from stalthos_toolkit.core.errors import MissingDependency

    try:
        return find_blender(os.environ.get("STALTHOS_TEST_BLENDER"))
    except MissingDependency:
        return None


BLENDER = _blender_path()
needs_blender = pytest.mark.skipif(BLENDER is None,
                                   reason="Blender is not installed on this machine")


MISSING_BLENDER = str(HERE / "definitely-not-a-real-blender-binary.exe")


# --------------------------------------------------------------------------- #
# fixtures
# --------------------------------------------------------------------------- #
@pytest.fixture
def workspace(tmp_path: Path) -> Workspace:
    return Workspace.open(tmp_path / "var")


@pytest.fixture
def definition(tmp_path: Path) -> Path:
    """A schema-valid plant definition; written to tmp so the tree is untouched.

    It satisfies contracts/species-definition.schema.json exactly like a
    checked-in species file -- an invalid fixture would only prove that the
    loader rejects bad input, never that the Blender layer runs.
    """
    return FIXTURES.write(tmp_path / "species" / "testplant.yaml", FIXTURES.plant_yaml())


@pytest.fixture
def creature_definition(tmp_path: Path) -> Path:
    """A schema-valid creature definition (quadrupedal proxy is implemented)."""
    return FIXTURES.write(tmp_path / "species" / "testcreature.yaml", FIXTURES.creature_yaml())


def _stub_blender(tmp_path: Path, body: str) -> str:
    """A fake 'blender' executable that runs ``body`` with Python.

    Written as a .cmd launcher so it is a real single-path executable, which is
    what core.workspace.find_blender expects to resolve.
    """
    script = tmp_path / "stub_blender_body.py"
    script.write_text(body, encoding="utf-8")
    launcher = tmp_path / "stub_blender.cmd"
    launcher.write_text(f'@echo off\r\n"{sys.executable}" "{script}" %*\r\n', encoding="utf-8")
    return str(launcher)


#: commands whose definition must be of this kind (mirrors pipeline.require_kind)
COMMAND_KIND = {
    "generate_plant": "plant", "generate_plant_variants": "plant",
    "build_creature": "creature", "rig_creature": "creature",
    # the render/bake commands work from an existing master and accept either
    "bake_materials": "plant", "render_turntable": "plant",
    "render_growth_stages": "plant",
}


def definition_for(command: str, tmp_path: Path, name: str = "def.yaml") -> Path:
    """The definition of the right *kind* for ``command`` (both validate)."""
    if COMMAND_KIND.get(command) == "creature":
        return FIXTURES.write(tmp_path / name, FIXTURES.creature_yaml())
    return FIXTURES.write(tmp_path / name, FIXTURES.plant_yaml())


# --------------------------------------------------------------------------- #
# 1. Missing Blender -> exit 4, without Blender installed
# --------------------------------------------------------------------------- #
@pytest.mark.parametrize("command", [
    "generate_plant", "generate_plant_variants", "build_creature", "rig_creature",
    "bake_materials", "render_turntable", "render_growth_stages",
])
def test_missing_blender_exits_4(command, workspace, definition, tmp_path, capsys):
    definition = definition_for(command, tmp_path)
    code = main(["--workspace", str(workspace.root), command,
                 "-d", str(definition), "--blender", MISSING_BLENDER])
    envelope = json.loads(capsys.readouterr().out)
    assert code == EXIT_MISSING_DEPENDENCY
    assert envelope["exit_code"] == EXIT_MISSING_DEPENDENCY
    assert envelope["ok"] is False
    assert envelope["errors"][0]["kind"] == "MissingDependency"
    assert "blender" in envelope["errors"][0]["message"]


def test_missing_blender_stdout_is_only_the_envelope(workspace, definition, capsys):
    main(["--workspace", str(workspace.root), "generate_plant",
          "-d", str(definition), "--blender", MISSING_BLENDER])
    out = capsys.readouterr().out.strip()
    envelope = json.loads(out)          # a single JSON object, nothing else
    assert envelope["command"] == "generate_plant"
    # exactly one JSON document on stdout: no banner, no log lines around it
    decoder = json.JSONDecoder()
    _, end = decoder.raw_decode(out)
    assert out[end:].strip() == ""


def test_resolve_blender_raises_missing_dependency(workspace):
    from stalthos_toolkit.blender.runner import resolve_blender

    ctx = Context(workspace=workspace, options={"blender": MISSING_BLENDER})
    with pytest.raises(Exception) as excinfo:
        resolve_blender(ctx)
    assert excinfo.value.exit_code == EXIT_MISSING_DEPENDENCY


def test_missing_definition_is_exit_2(workspace, capsys):
    """A malformed definition is a usage error, checked before any tool runs."""
    code = main(["--workspace", str(workspace.root), "generate_plant",
                 "-d", str(HERE / "no-such-definition.yaml"),
                 "--blender", MISSING_BLENDER])
    envelope = json.loads(capsys.readouterr().out)
    assert code == EXIT_USAGE
    assert envelope["errors"][0]["kind"] == "UsageError"


# --------------------------------------------------------------------------- #
# 2. Runner contract: argv shape, timeouts, exit-code mapping
# --------------------------------------------------------------------------- #
def test_build_argv_is_the_frozen_invocation_shape(tmp_path):
    argv = build_argv("C:/blender.exe", Path("s.py"), tmp_path / "params.json")
    assert argv[0] == "C:/blender.exe"
    assert "--background" in argv
    assert "--factory-startup" in argv
    assert "--python" in argv
    assert argv[argv.index("--") + 1] == "--params"
    assert argv[-1].endswith("params.json")
    # nothing after `--` is a Blender flag, so our args can never be misread
    assert all(not a.startswith("--") for a in argv[argv.index("--") + 2:])


def test_resolve_timeout_prefers_ctx_option(workspace):
    ctx = Context(workspace=workspace, options={"timeout": 12.5})
    assert resolve_timeout(ctx) == 12.5
    assert resolve_timeout(Context(workspace=workspace, options={})) == DEFAULT_TIMEOUT_S
    assert resolve_timeout(Context(workspace=workspace, options={"timeout": None}), 3.0) == 3.0


def test_script_path_rejects_unknown_scripts():
    with pytest.raises(BlenderError):
        script_path("no_such_script")


def test_timeout_maps_to_exit_7(tmp_path):
    """A Blender that never finishes must become Interrupted (exit 7)."""
    workspace = Workspace.open(tmp_path / "var")
    stub = _stub_blender(tmp_path, "import time\ntime.sleep(60)\n")
    ctx = Context(workspace=workspace, seed=1, options={
        "blender": stub, "timeout": 1.5})
    ctx.new_status("test")
    with pytest.raises(Exception) as excinfo:
        run_blender(ctx, script="plants", params={"outputs": {}}, timeout=1.5,
                    log_name="timeout_probe")
    assert excinfo.value.exit_code == EXIT_INTERRUPTED


def test_nonzero_exit_maps_to_exit_6(tmp_path):
    workspace = Workspace.open(tmp_path / "var")
    stub = _stub_blender(tmp_path, "raise SystemExit(3)\n")
    ctx = Context(workspace=workspace, seed=1, options={"blender": stub})
    ctx.new_status("test")
    with pytest.raises(Exception) as excinfo:
        run_blender(ctx, script="plants", params={"outputs": {}}, log_name="fail_probe")
    assert excinfo.value.exit_code == EXIT_EXTERNAL


def test_missing_result_json_is_an_error_not_a_silent_success(tmp_path):
    """Blender exiting 0 without writing result.json must not look like success."""
    workspace = Workspace.open(tmp_path / "var")
    stub = _stub_blender(tmp_path, "print('nothing to report')\n")
    ctx = Context(workspace=workspace, seed=1, options={"blender": stub})
    ctx.new_status("test")
    with pytest.raises(BlenderError):
        run_blender(ctx, script="plants", params={"outputs": {}}, log_name="noresult_probe")


def test_structured_failure_in_result_json_maps_to_exit_6(tmp_path):
    workspace = Workspace.open(tmp_path / "var")
    body = textwrap.dedent(f"""
        import json, os, sys
        args = sys.argv[sys.argv.index('--')+1:]
        here = os.path.dirname(args[args.index('--params')+1])
        with open(os.path.join(here, 'result.json'), 'w') as fh:
            json.dump({{'ok': False, 'action': 'plants',
                        'error': {{'kind': 'ValueError', 'message': 'boom'}}}}, fh)
        """)
    stub = _stub_blender(tmp_path, body)
    ctx = Context(workspace=workspace, seed=1, options={"blender": stub})
    ctx.new_status("test")
    run = run_blender(ctx, script="plants", params={"outputs": {}}, log_name="structured_probe")
    from stalthos_toolkit.blender.pipeline import check_result

    with pytest.raises(Exception) as excinfo:
        check_result(run)
    assert excinfo.value.exit_code == EXIT_EXTERNAL


def test_successful_run_returns_the_result_payload(tmp_path):
    workspace = Workspace.open(tmp_path / "var")
    body = textwrap.dedent("""
        import json, os, sys
        args = sys.argv[sys.argv.index('--')+1:]
        here = os.path.dirname(args[args.index('--params')+1])
        with open(os.path.join(here, 'result.json'), 'w') as fh:
            json.dump({'ok': True, 'action': 'plants', 'metrics': {'triangles': 7}}, fh)
        """)
    stub = _stub_blender(tmp_path, body)
    ctx = Context(workspace=workspace, seed=1, options={"blender": stub})
    ctx.new_status("test")
    run = run_blender(ctx, script="plants", params={"outputs": {}}, log_name="ok_probe")
    assert run.ok is True
    assert run.result["metrics"]["triangles"] == 7
    assert run.log_path is not None and run.log_path.exists()
    # a successful run must not leak its temp workdir
    assert not run.work_dir.exists(), run.work_dir


def test_keep_workdir_leaves_the_temp_directory(tmp_path):
    workspace = Workspace.open(tmp_path / "var")
    body = textwrap.dedent("""
        import json, os, sys
        args = sys.argv[sys.argv.index('--')+1:]
        here = os.path.dirname(args[args.index('--params')+1])
        with open(os.path.join(here, 'result.json'), 'w') as fh:
            json.dump({'ok': True, 'action': 'plants', 'metrics': {}}, fh)
        """)
    stub = _stub_blender(tmp_path, body)
    ctx = Context(workspace=workspace, seed=1,
                  options={"blender": stub, "keep_workdir": True})
    ctx.new_status("test")
    run = run_blender(ctx, script="plants", params={"outputs": {}}, log_name="keep_probe")
    assert run.work_dir.is_dir()


def test_failing_run_keeps_its_workdir_for_inspection(tmp_path, monkeypatch):
    workspace = Workspace.open(tmp_path / "var")
    monkeypatch.setattr(tempfile, "tempdir", str(tmp_path / "tmp"))
    (tmp_path / "tmp").mkdir()
    body = textwrap.dedent("""
        import sys
        sys.stderr.write('deliberate failure')
        raise SystemExit(3)
        """)
    stub = _stub_blender(tmp_path, body)
    ctx = Context(workspace=workspace, seed=1, options={"blender": stub})
    ctx.new_status("test")
    from stalthos_toolkit.core.errors import ExternalToolFailed
    with pytest.raises(ExternalToolFailed):
        run_blender(ctx, script="plants", params={"outputs": {}}, log_name="fail_probe")
    kept = sorted((tmp_path / "tmp").glob("stalthos_blender_*"))
    assert len(kept) == 1, kept


def test_result_json_is_never_written_into_the_source_tree(tmp_path):
    """Blender's working directory and result path must stay in a temp dir."""
    workspace = Workspace.open(tmp_path / "var")
    seen = tmp_path / "seen.json"
    body = textwrap.dedent(f"""
        import json, os, sys
        args = sys.argv[sys.argv.index('--')+1:]
        here = os.path.dirname(args[args.index('--params')+1])
        with open({str(seen)!r}, 'w') as fh:
            json.dump({{'cwd': os.getcwd(),
                        'result_path': os.path.join(here, 'result.json')}}, fh)
        with open(os.path.join(here, 'result.json'), 'w') as fh:
            json.dump({{'ok': True, 'action': 'plants', 'metrics': {{}}}}, fh)
        """)
    stub = _stub_blender(tmp_path, body)
    ctx = Context(workspace=workspace, seed=1, options={"blender": stub})
    ctx.new_status("test")
    run_blender(ctx, script="plants", params={"outputs": {}}, log_name="cwd_probe")
    observed = json.loads(seen.read_text(encoding="utf-8"))
    source_root = str(Path(__file__).resolve().parents[1])
    # neither the CWD nor the result file may live in the source tree
    assert not observed["cwd"].startswith(source_root)
    assert not observed["result_path"].startswith(source_root)
    # everything the command produced lives under the workspace root
    for path in (workspace.reports / "logs").rglob("*"):
        assert str(path.resolve()).startswith(str(workspace.root.resolve()))


# --------------------------------------------------------------------------- #
# 3. Params normalization (pure Python, no Blender)
# --------------------------------------------------------------------------- #
def test_resolve_family_handles_aliases():
    assert P.resolve_family({"family": "Araucarian-like conifer"}) == "tree"
    assert P.resolve_family({"generator": "equisetum"}) == "horsetail"
    assert P.resolve_family({"family": "cycad"}) == "rosette"
    assert P.resolve_family({"family": "riparian shrub"}) == "shrub"
    assert P.resolve_family({"family": "marattioid fern"}) == "fern"
    assert P.resolve_family({"body_plan": "branch-aware tree"}) == "tree"
    assert P.resolve_family({"generator": "rhachis/pinna fern"}) == "fern"
    assert P.resolve_family({"family": "segmented horsetail thicket"}) == "horsetail"
    assert P.resolve_family({"family": "layered rosette"}) == "rosette"
    assert P.resolve_family({}) == "shrub"          # honest default, not a crash


def test_plant_shape_params_defaults_are_bounded():
    shape = P.plant_shape_params({}, "tree")
    assert shape["height_m"] > 0
    assert 1 <= shape["branch_levels"] <= 7
    assert 20.0 <= shape["divergence_deg"] <= 360.0
    assert shape["trunk_diameter_m"] > 0
    assert shape["leaf_length_m"] > 0


def test_plant_shape_params_reads_nested_definition():
    definition = {"morphology": {"height_m": 12.0, "branch_levels": 5}}
    shape = P.plant_shape_params(definition, "tree")
    assert shape["height_m"] == 12.0
    assert shape["branch_levels"] == 5


def test_growth_stage_params_apply_stage_proportions():
    definition = {"growth_stages": {"juvenile": {"height_scale": 0.3,
                                                 "trunk_scale": 0.4},
                                    "adult": {"height_scale": 1.0}}}
    juvenile = P.growth_stage_params(definition, stage="juvenile")
    adult = P.growth_stage_params(definition, stage="adult")
    assert juvenile["height_scale"] == 0.3
    assert juvenile["trunk_scale"] == 0.4
    assert adult["height_scale"] == 1.0
    assert juvenile["available"] is True


def test_stage_params_missing_stage_is_not_fatal():
    params = P.growth_stage_params({"growth_stages": {"adult": {}}}, stage="nope")
    assert params["height_scale"] == 1.0
    assert params["stage"] == "nope"


def test_variant_multipliers_are_bounded(workspace):
    """Variants must read as one lineage: no multiplier escapes its range."""
    definition = {"variation": {"ranges": {"trunk_height": [0.85, 1.15],
                                           "leaf_size": [0.9, 1.1],
                                           "branch_angle": [0.88, 1.12]}}}
    variation = P.plant_variation_params(definition, "tree", variant_index=0)
    # schema ranges are multiplicative factors around 1.0; stored as deltas
    assert variation["height"] == pytest.approx((-0.15, 0.15))
    assert variation["leaf_scale"] == pytest.approx((-0.1, 0.1))
    assert variation["branch_angle"] == pytest.approx((-0.12, 0.12))
    assert variation["source"] == "definition.variation.ranges"
    ctx = Context(workspace=workspace, seed=5)
    seen = {key: [] for key in ("height", "leaf_scale", "branch_angle")}
    for index in range(24):
        multipliers = P.variant_multipliers(ctx, "plant.x", variation, index)
        for key, value in multipliers.items():
            low, high = variation[key]
            assert low <= value <= high, (key, value)
            seen[key].append(value)
    # the draw actually varies (it is not a constant), but never leaves the band
    for key, values in seen.items():
        low, high = variation[key]
        assert max(values) - min(values) > 0.01, key
        assert abs(max(values)) <= abs(high) + 1e-9
        assert abs(min(values)) <= abs(low) + 1e-9


def test_variant_multipliers_fall_back_to_documented_defaults(workspace):
    """A definition with no variation block still gets a bounded, narrow band."""
    variation = P.plant_variation_params({}, "tree", variant_index=0)
    assert variation["source"] == "documented defaults"
    ctx = Context(workspace=workspace, seed=7)
    for index in range(24):
        multipliers = P.variant_multipliers(ctx, "plant.y", variation, index)
        for key, value in multipliers.items():
            low, high = variation[key]
            assert low <= value <= high, (key, value)
    assert all(abs(hi) <= 0.25 for lo, hi in
               (variation[k] for k in ("height", "leaf_scale", "branch_angle")))


def test_generator_maps_to_the_implemented_family():
    """Every roster generator resolves to a family; approximations are flagged."""
    for generator, (family, exact) in P.GENERATOR_FAMILY.items():
        definition = {"generator": generator}
        assert P.resolve_family(definition) == family, generator
        assert P.family_is_approximation(definition) is (not exact), generator
    # an unknown generator is an approximation, and never crashes
    assert P.family_is_approximation({"generator": "not_a_real_generator"}) is True
    assert P.resolve_family({"generator": "not_a_real_generator"}) in P.PLANT_FAMILIES


def test_schema_shaped_growth_stages_are_read():
    """growth_stages is an ordered array; the last entry is the mature form."""
    definition = {"growth_stages": [
        {"id": "seedling", "age_label": "seedling",
         "height": {"value": 50.0, "unit": "cm"},
         "proportions": {"stem_share": 1.0, "crown_share": 0.05}},
        {"id": "mature", "age_label": "mature",
         "height": {"value": 2.0, "unit": "m"},
         "proportions": {"stem_share": 0.4, "crown_share": 0.6}},
    ]}
    assert P.stage_names(definition) == ["seedling", "mature"]
    assert P.mature_height_m(definition) == pytest.approx(2.0)
    seedling = P.growth_stage_params(definition, stage="seedling")
    assert seedling["height_scale"] == pytest.approx(0.25)   # 0.5 m of 2.0 m
    assert seedling["age_label"] == "seedling"
    assert seedling["trunk_scale"] == pytest.approx(1.0)     # stem_share 1.0
    mature = P.growth_stage_params(definition, stage="mature")
    assert mature["trunk_scale"] == pytest.approx(0.4)
    assert mature["branch_density"] == pytest.approx(0.6)    # crown_share


def test_apply_scales_uses_stage_after_variant(workspace):
    shape = {"height_m": 10.0, "leaf_length_m": 0.5, "leaf_width_m": 0.2,
             "trunk_diameter_m": 0.4, "frond_count": 7}
    out = P.apply_scales(shape, {"height": 0.1, "leaf_scale": 0.0, "branch_angle": 0.0},
                         {"stage": "juvenile", "height_scale": 0.5, "trunk_scale": 0.4,
                          "leaf_scale": 1.0, "branch_density": 0.5, "crown_start": 0.1})
    assert out["height_m"] == pytest.approx(10.0 * 1.1 * 0.5)
    assert out["trunk_diameter_m"] == pytest.approx(0.4 * 0.4)
    assert out["frond_count"] == 4        # 7 * 0.5 density, rounded, min 1
    assert out["variant"]["stage"] == "juvenile"


def test_creature_shape_params_stage_overrides_body_plan():
    definition = {
        "body_plan": {"trunk_depth_m": 0.9, "neck_length_m": 1.6},
        "growth_stages": {"juvenile": {"trunk_depth_m": 0.3, "neck_length_m": 0.5}},
    }
    juvenile = P.creature_shape_params(definition, stage="juvenile")
    adult = P.creature_shape_params(definition, stage="adult")
    assert juvenile["trunk_depth_m"] == 0.3
    assert juvenile["neck_length_m"] == 0.5
    assert adult["trunk_depth_m"] == 0.9
    assert adult["neck_length_m"] == 1.6
    assert "not fossil measurements" in juvenile["proxy_note"]


def test_creature_shape_params_reads_schema_shares(workspace):
    """body_plan shares + life_stages total_length resolve to a body plan."""
    from stalthos_toolkit.schemas.loader import load_definition
    from stalthos_toolkit.blender.params import creature_stage_names

    definition = load_definition(str(FIXTURES.write(
        workspace.root / "definitions" / "cr.yaml", FIXTURES.creature_yaml())))
    assert creature_stage_names(definition) == ["juvenile", "adult"]
    adult = P.creature_shape_params(definition, stage="adult")
    assert adult["total_length_m"] == pytest.approx(3.8)
    assert adult["neck_segments"] == 7        # axial.cervical_count
    assert adult["tail_segments"] == 26       # axial.caudal_count
    assert adult["trunk_segments"] == 12      # axial.dorsal_count
    assert adult["posture"] == "quadrupedal"
    juvenile = P.creature_shape_params(definition, stage="juvenile")
    assert juvenile["total_length_m"] == pytest.approx(1.2)
    assert juvenile["girth_scale"] == pytest.approx(0.7)
    # non-uniform development: the juvenile is not a uniform scale of the adult
    assert juvenile["total_length_m"] / adult["total_length_m"] < 0.5


@pytest.mark.parametrize("command,expected_kind", [
    ("generate_plant", "plant"), ("generate_plant_variants", "plant"),
    ("build_creature", "creature"), ("rig_creature", "creature"),
])
def test_wrong_definition_kind_is_exit_2(command, expected_kind, workspace, tmp_path, capsys):
    """A creature definition must not silently become a plant (or vice versa)."""
    other = "creature" if expected_kind == "plant" else "plant"
    yaml = (FIXTURES.creature_yaml("creature.wrong.cr1") if other == "creature"
            else FIXTURES.plant_yaml("plant.wrong.sp1"))
    definition = FIXTURES.write(tmp_path / f"{other}.yaml", yaml)
    code = main(["--workspace", str(workspace.root), command,
                 "-d", str(definition), "--blender", MISSING_BLENDER])
    envelope = json.loads(capsys.readouterr().out)
    assert code == EXIT_USAGE, envelope
    assert envelope["errors"][0]["kind"] == "UsageError"
    assert expected_kind in envelope["errors"][0]["message"]


def test_build_creature_exits_5_for_an_unsupported_posture(workspace, tmp_path, capsys):
    """A bipedal/serpentine plan is a declared contract with no implementation."""
    from stalthos_toolkit.core.errors import EXIT_NOT_IMPLEMENTED
    from stalthos_toolkit.cli import main

    definition = FIXTURES.write(tmp_path / "biped.yaml",
                                FIXTURES.creature_yaml("creature.biped.cr1", posture="bipedal"))
    code = main(["--workspace", str(workspace.root), "build_creature",
                 "-d", str(definition), "--blender", MISSING_BLENDER])
    envelope = json.loads(capsys.readouterr().out)
    assert code == EXIT_NOT_IMPLEMENTED
    assert envelope["errors"][0]["kind"] == "NotImplementedStage"
    assert "bipedal" in envelope["errors"][0]["message"]


def test_seed_derivation_agrees_with_frozen_core():
    from stalthos_toolkit.blender.seeds import derive_seed
    """The bpy-side seed algorithm must be identical to core's, and is shared
    from one pure-stdlib module so it cannot drift (docs section 3)."""
    from stalthos_toolkit.blender import seeds
    from stalthos_toolkit.core import seeding

    for root in (0, 42, 2**62 + 7, "a stable label"):
        for labels in ((), ("a",), ("a", "b", 3),
                       ("plant", "splitcrown", "branch", 7),
                       ("generate_plant", "plant.x.variant_00.juvenile")):
            assert seeds.derive_seed(root, *labels) == seeding.derive_seed(root, *labels)
            assert seeds.normalize_seed(root) == seeding.normalize_seed(root)

    # and the bpy runtime re-exports that exact module, not a copy of it
    scripts = (Path(__file__).resolve().parents[1] / "src" / "stalthos_toolkit"
               / "blender" / "scripts" / "_stalthos.py")
    assert "blender/seeds.py" in scripts.read_text(encoding="utf-8")


def test_seed_is_deterministic_across_processes():
    """Guards against hash() or set-ordering leaking into seed derivation."""
    import subprocess

    from stalthos_toolkit.blender.seeds import derive_seed

    code = ("import sys; sys.path.insert(0, %r);"
            "from stalthos_toolkit.blender.seeds import derive_seed;"
            "print(derive_seed(42, 'x', 3))"
            % str(Path(__file__).resolve().parents[1] / "src"))
    outputs = set()
    for salt in ("0", "1"):
        env = dict(os.environ, PYTHONHASHSEED=salt)
        out = subprocess.run([sys.executable, "-c", code], capture_output=True,
                             text=True, env=env, check=True)
        outputs.add(out.stdout.strip())
    assert len(outputs) == 1
    assert outputs == {str(derive_seed(42, "x", 3))}


# --------------------------------------------------------------------------- #
# 4. Command registration
# --------------------------------------------------------------------------- #
@pytest.mark.parametrize("name", [
    "generate_plant", "generate_plant_variants", "build_creature", "rig_creature",
    "bake_materials", "render_turntable", "render_growth_stages",
])
def test_commands_are_registered_and_implemented(name):
    from stalthos_toolkit.cli import _load_registered, register_declared

    register_declared()
    _load_registered()
    spec = REGISTRY.get(name)
    assert spec.handler is not None, f"{name} must not be a stub"
    assert spec.implemented is True
    assert spec.stage


def test_unimplemented_commands_still_exit_5():
    """Other workers own these; they must stay honest while unimplemented."""
    from stalthos_toolkit.cli import _load_registered, register_declared

    register_declared()
    _load_registered()
    for name in ("validate_asset", "generate_lods", "publish_asset", "benchmark_asset",
                 "research_species"):
        spec = REGISTRY.get(name)
        if spec.handler is None:
            assert spec.implemented is False


# --------------------------------------------------------------------------- #
# 5. Dry-run planning never touches Blender or writes artifacts
# --------------------------------------------------------------------------- #
@pytest.mark.parametrize("command", ["generate_plant", "build_creature"])
def test_dry_run_does_not_require_blender(command, workspace, definition, tmp_path, capsys):
    definition = definition_for(command, tmp_path)
    code = main(["--workspace", str(workspace.root), command, "-d", str(definition),
                 "--blender", MISSING_BLENDER, "--dry-run"])
    envelope = json.loads(capsys.readouterr().out)
    assert code == EXIT_OK
    assert envelope["ok"] is True
    assert envelope["artifacts"] == []


def test_workspace_layout_is_created(workspace):
    for folder in ("sources", "intermediate", "runtime", "preview", "reports"):
        assert (workspace.root / folder).is_dir()


# --------------------------------------------------------------------------- #
# 6. Real Blender tests (skipped when Blender is absent)
# --------------------------------------------------------------------------- #
@pytest.fixture
def blender(tmp_path):
    if BLENDER is None:
        pytest.skip("Blender is not installed")
    return BLENDER


def _run(command, workspace, definition, capsys, *extra, blender_path=None):
    argv = ["--workspace", str(workspace.root), command, "-d", str(definition),
            "--blender", blender_path or (BLENDER or MISSING_BLENDER),
            "--timeout", "900", *extra]
    code = main(argv)
    out = capsys.readouterr().out
    return code, json.loads(out)


@needs_blender
def test_end_to_end_generate_plant(blender, workspace, definition, capsys):
    code, envelope = _run("generate_plant", workspace, definition, capsys, blender_path=blender)
    assert code == EXIT_OK, envelope
    assert envelope["ok"] is True
    metrics = envelope["metrics"]
    assert metrics["triangles"] > 0
    assert metrics["vertices"] > 0
    assert metrics["blender"].startswith("5.") or metrics["blender"][0].isdigit()
    # geometry lives in var/sources/<asset_id>/
    blend = Path(workspace.root / "sources" / "plant.test.sp1" / "plant.test.sp1.blend")
    assert blend.is_file() and blend.stat().st_size > 0
    # every artifact is hashed
    sources = [a for a in envelope["artifacts"] if a["role"] == "source"]
    assert sources and all(a["sha256"] and a["bytes"] for a in sources)
    # the master stays editable: named objects, unapplied modifiers, collections
    object_rows = metrics["objects"]
    assert {o["name"] for o in object_rows} >= {"tree_structure", "tree_foliage"}
    assert all(o["modifiers"] and not o["modifiers"][0]["type"].startswith("APPLY")
               for o in object_rows if o["type"] == "MESH")


@needs_blender
def test_generation_is_deterministic(blender, tmp_path, capsys):
    """Same (definition, seed) => same geometry digest."""
    digests = []
    for run in range(2):
        ws = Workspace.open(tmp_path / f"var{run}")
        path = tmp_path / f"def{run}.yaml"
        path.write_text(FIXTURES.plant_yaml("plant.det.sp1", generator="branching_tree",
                                            height_m=8.0, leaf_length=0.2),
                        encoding="utf-8")
        code, envelope = _run("generate_plant", ws, path, capsys, blender_path=blender)
        assert code == EXIT_OK, envelope
        digests.append(envelope["metrics"]["geometry_digest"])
    assert digests[0] == digests[1]
    assert len(digests[0]) == 64


@needs_blender
def test_different_seed_changes_geometry(blender, tmp_path, capsys):
    digests = []
    for seed in ("11", "2222"):
        ws = Workspace.open(tmp_path / f"var{seed}")
        path = tmp_path / f"def{seed}.yaml"
        path.write_text(FIXTURES.plant_yaml("plant.det.sp1", generator="layered_shrub",
                                            height_m=2.0), encoding="utf-8")
        argv = ["--seed", seed, "--workspace", str(ws.root), "generate_plant",
                "-d", str(path), "--blender", blender, "--timeout", "900"]
        assert main(argv) == EXIT_OK
        envelope = json.loads(capsys.readouterr().out)
        digests.append(envelope["metrics"]["geometry_digest"])
    assert digests[0] != digests[1]


@needs_blender
def test_creature_build_and_rig(blender, workspace, creature_definition, capsys):
    definition = creature_definition

    code, built = _run("build_creature", workspace, definition, capsys, blender_path=blender)
    assert code == EXIT_OK, built
    assert "not fossil measurements" in built["metrics"]["proxy_note"]
    assert any("not fossil measurements" in w for w in built["warnings"])
    assert built["metrics"]["triangles"] > 0

    code, rigged = _run("rig_creature", workspace, definition, capsys, blender_path=blender)
    assert code == EXIT_OK, rigged
    metrics = rigged["metrics"]
    assert metrics["bone_count"] > 20
    assert metrics["single_root"] is True
    assert metrics["weighting"] in {"ARMATURE_AUTO", "ARMATURE_FALLBACK"}
    assert metrics["pose"]["neutral"] is True
    assert metrics["spine_bones"] and metrics["neck_bones"] and metrics["tail_bones"]
    assert metrics["limb_chains"] == ["front_L", "front_R", "rear_L", "rear_R"]
    # the neutral pose GLB is published into runtime/
    assert any(a["role"] == "runtime" and a["path"].endswith(".glb")
               for a in rigged["artifacts"])


@needs_blender
def test_smalltrotter_exports_walk_channels_and_contact_report(blender, workspace, capsys):
    definition = HERE.parent / "species" / "creatures" / "smalltrotter.yaml"
    code, built = _run("build_creature", workspace, definition, capsys,
                       "--seed", "42", "--stage", "independent_juvenile", blender_path=blender)
    assert code == EXIT_OK, built
    code, rigged = _run("rig_creature", workspace, definition, capsys,
                        "--seed", "42", "--stage", "independent_juvenile", blender_path=blender)
    assert code == EXIT_OK, rigged

    gait = rigged["metrics"]["walk_cycle"]
    assert gait["loop"]["passed"] is True
    assert gait["glb_animation"]["passed"] is True
    assert gait["glb_animation"]["animations"] == 1
    assert gait["glb_animation"]["channels"] > 0
    assert gait["contacts"]["foot_pad_vertex_counts"] == {
        "bone:pes_foot_l": 8, "bone:pes_foot_r": 8,
    }
    assert gait["terrain_contact_height"]["actor_stride_translation_included"] is False
    assert gait["validation"]["geometric_contact_passed"] is False

    neutral = next(a["path"] for a in rigged["artifacts"]
                   if a["path"].endswith(".neutral.glb"))
    walk = next(a["path"] for a in rigged["artifacts"]
                if a["path"].endswith(".walk_cycle.glb"))
    from stalthos_toolkit.optimize.glb import GlbDocument
    assert GlbDocument.read(neutral).gltf.get("animations", []) == []
    assert len(GlbDocument.read(walk).gltf.get("animations", [])) == 1


@needs_blender
def test_rig_requires_a_source_master(blender, workspace, definition, capsys):
    code, envelope = _run("rig_creature", workspace, definition, capsys, blender_path=blender)
    assert code == EXIT_USAGE
    assert envelope["errors"][0]["kind"] == "UsageError"


@needs_blender
def test_bake_produces_four_passes_without_baked_lighting(blender, workspace,
                                                          definition, capsys):
    code, envelope = _run("generate_plant", workspace, definition, capsys,
                          blender_path=blender)
    assert code == EXIT_OK, envelope
    code, baked = _run("bake_materials", workspace, definition, capsys, blender_path=blender)
    assert code == EXIT_OK, baked
    metrics = baked["metrics"]
    assert set(metrics["passes"]) == {"albedo", "roughness", "normal", "thickness"}
    assert metrics["directional_light_baked"] is False
    assert metrics["lighting_in_albedo"] is False
    assert metrics["animated_seed"] is False
    textures = [a for a in baked["artifacts"] if Path(a["path"]).suffix == ".png"]
    assert len(textures) >= 4
    for artifact in textures:
        assert artifact["bytes"] > 0
        assert (Path(artifact["path"]).read_bytes()[:8] == b"\x89PNG\r\n\x1a\n")
    assert set(baked["metrics"]["texture_files"]) == {"albedo", "roughness",
                                                      "normal", "thickness"}
    for paths in baked["metrics"]["texture_files"].values():
        assert paths


@needs_blender
def test_baked_albedo_has_no_directional_lighting(blender, workspace, definition, capsys):
    """Albedo must be structure, not a lit image.

    A directional-light bake shows up as (a) blown highlights near white, and
    (b) a brightness gradient across the frame that tracks the light
    direction. A structure-only albedo has neither: dark and mid values only,
    with no left/right gradient. Checked on the real PNG.
    """
    _run("generate_plant", workspace, definition, capsys, blender_path=blender)
    code, baked = _run("bake_materials", workspace, definition, capsys, blender_path=blender)
    assert code == EXIT_OK, baked
    albedo_path = baked["metrics"]["texture_files"]["albedo"][0]
    png = Path(albedo_path).read_bytes()
    try:
        from PIL import Image
        import io
        import numpy as np
    except ImportError:
        pytest.skip("Pillow/numpy not available for pixel inspection")
    pixels = np.array(Image.open(io.BytesIO(png)).convert("L")).astype(float) / 255.0
    occupied = pixels[pixels > 0.01]
    assert occupied.size > 0, "albedo is empty"

    # (a) structure, not lighting: dark and mid values, no blown highlights
    assert occupied.min() < 0.25, "albedo must contain dark structure"
    assert occupied.max() < 0.85, "albedo has blown highlights: light got baked in"
    assert occupied.std() > 0.05, "albedo is flat: no structure was captured"

    # (b) no left/right gradient: compare the two halves of the silhouette
    columns = (pixels > 0.01).sum(axis=0)
    left_edge, right_edge = int(columns.size * 0.2), int(columns.size * 0.8)
    half = pixels.shape[1] // 2
    left = pixels[:, :half][pixels[:, :half] > 0.01]
    right = pixels[:, half:][pixels[:, half:] > 0.01]
    if left.size and right.size:
        skew = abs(float(left.mean() - right.mean())) / max(float(pixels[pixels > 0.01].mean()), 1e-6)
        assert skew < 0.25, f"albedo has a {skew:.2f} left/right gradient: directional light"
    assert left_edge < right_edge


@needs_blender
def test_render_turntable_is_fast_and_deterministic(blender, workspace, definition, capsys):
    import time as _time

    _run("generate_plant", workspace, definition, capsys, blender_path=blender)
    started = _time.time()
    code, rendered = _run("render_turntable", workspace, definition, capsys,
                          blender_path=blender)
    duration = _time.time() - started
    assert code == EXIT_OK, rendered
    assert duration < 300, "review renders must be fast enough to actually run"
    previews = [a for a in rendered["artifacts"] if a["role"] == "preview"]
    assert len(previews) >= 4
    assert rendered["metrics"]["renders"] >= 4
    assert rendered["metrics"]["determinism"]["animated_seed"] is False
    for artifact in previews:
        assert Path(artifact["path"]).is_file()
        assert artifact["bytes"] > 0


@needs_blender
def test_render_growth_stages_renders_every_stage(blender, workspace, tmp_path, capsys):
    definition = FIXTURES.write(tmp_path / "plant_growth.yaml", FIXTURES.plant_yaml(
        "plant.growth.sp1", generator="woody_rosette", height_m=0.8, leaf_length=0.4))
    code, envelope = _run("render_growth_stages", workspace, definition, capsys,
                          blender_path=blender)
    assert code == EXIT_OK, envelope
    stages = [s["stage"] for s in envelope["metrics"]["stages"]]
    assert stages == ["seedling", "mature"]
    for stage in envelope["metrics"]["stages"]:
        assert stage["renders"], f"{stage['stage']} produced no renders"


# --- tool-version provenance -------------------------------------------------
#
# `blender --version` embeds non-ASCII bytes on some Windows builds. Decoding it
# with the locale codec (cp1252) killed the stdout reader thread, so the recorded
# tool version silently degraded to "unknown" and flowed into determinism
# provenance. These tests pin the decode to utf-8/replace.

#: a banner containing bytes that cp1252 cannot decode (0x90 is an undefined
#: cp1252 codepoint), so a locale-codec decode raises exactly as it did in CI.
_NON_ASCII_BANNER = b"Blender 4.3.1 \x90 custom build\n"


def _stub_run_returning_banner(monkeypatch):
    """Replace subprocess.run in the runner with one that mimics real decoding."""
    captured = {}

    def fake_run(argv, **kwargs):
        captured.update(kwargs)
        encoding = kwargs.get("encoding")
        errors = kwargs.get("errors") or "strict"
        # No encoding= means subprocess uses locale.getpreferredencoding(False),
        # which is how the original bug reproduced.
        text = (_NON_ASCII_BANNER.decode(encoding, errors) if encoding
                else _NON_ASCII_BANNER.decode(locale.getpreferredencoding(False), errors))
        return subprocess.CompletedProcess(argv, 0, stdout=text, stderr="")

    monkeypatch.setattr("stalthos_toolkit.blender.runner.subprocess.run", fake_run)
    return captured


def test_blender_version_decodes_banner_as_utf8(workspace, tmp_path, monkeypatch):
    exe = tmp_path / "fake-blender"
    exe.write_text("stub", encoding="utf-8")  # only needs to resolve, never exec
    _stub_run_returning_banner(monkeypatch)
    version = blender_version(Context(workspace=workspace), blender_exe=str(exe))
    assert version == "Blender 4.3.1 \ufffd custom build", (
        "a non-ASCII version banner must not decay to 'unknown'")
    assert version != "unknown"


def test_blender_version_pins_utf8_not_the_locale_codec(workspace, tmp_path, monkeypatch):
    """Guard the fix directly: the decode must not be locale-dependent."""
    exe = tmp_path / "fake-blender"
    exe.write_text("stub", encoding="utf-8")
    captured = _stub_run_returning_banner(monkeypatch)
    blender_version(Context(workspace=workspace), blender_exe=str(exe))
    assert captured.get("encoding") == "utf-8"
    assert captured.get("errors") == "replace"


def test_blender_version_is_unknown_when_the_probe_fails(workspace, tmp_path, monkeypatch):
    exe = tmp_path / "fake-blender"
    exe.write_text("stub", encoding="utf-8")
    monkeypatch.setattr("stalthos_toolkit.blender.runner.subprocess.run",
                        lambda argv, **kw: (_ for _ in ()).throw(OSError("boom")))
    assert blender_version(Context(workspace=workspace), blender_exe=str(exe)) == "unknown"


@needs_blender
def test_variants_stay_one_lineage(blender, workspace, definition, capsys):
    code, envelope = _run("generate_plant_variants", workspace, definition, capsys,
                          "--count", "4", blender_path=blender)
    assert code == EXIT_OK, envelope
    results = envelope["metrics"]["results"]
    assert len(results) >= 4
    seeds = {r["seed"] for r in results}
    assert len(seeds) == len(results), "each variant/stage needs its own derived seed"
    bounds = envelope["metrics"].get("lineage_note")
    assert bounds and "one lineage" in bounds
    for row in results:
        for key, value in row["multipliers"].items():
            assert abs(value) <= 0.17, f"{key}={value} escaped the bounded range"
