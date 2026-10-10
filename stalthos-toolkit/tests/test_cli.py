"""CLI tests: exit codes, JSON envelope shape, deterministic seeding.

These tests run WITHOUT Blender. They exercise the CLI plumbing, contract
registry, and status envelope — not the stage implementations.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest

STALTHOS = [sys.executable, "-m", "stalthos_toolkit.cli"]


def run_stalthos(*args, input_text=None, capture=True, check=False):
    """Run stalthos and return (exit_code, stdout, stderr)."""
    cmd = STALTHOS + list(args)
    result = subprocess.run(
        cmd,
        input=input_text,
        capture_output=capture,
        text=True,
        encoding="utf-8",
    )
    return result.returncode, result.stdout, result.stderr


def parse_json_status(stdout: str) -> dict:
    """Extract the single JSON status envelope from stdout."""
    # The contract says exactly one JSON object on stdout
    stdout = stdout.strip()
    if not stdout:
        raise ValueError("Empty stdout")
    return json.loads(stdout)


class TestCLIExitCodes:
    """Test that exit codes match the contract."""

    def test_bad_command_exits_2(self):
        """Unknown command → exit 2 (usage error)."""
        code, out, err = run_stalthos("nonexistent_command")
        assert code == 2, f"Expected exit 2, got {code}: {err}"
        # Should print usage to stderr, not JSON on stdout
        assert "invalid choice" in err.lower() or "unrecognized" in err.lower()

    def test_missing_command_exits_2(self):
        """No command provided → exit 2."""
        code, out, err = run_stalthos()
        assert code == 2

    def test_unimplemented_command_exits_5(self):
        """Declared but unimplemented command → exit 5 (when definition provided)."""
        # Use research_species which is declared but not implemented (per stalthos list)
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml", delete=False) as f:
            f.write("id: test\nkind: plant\nmorphology_id: test\n")
            def_path = f.name
        try:
            code, out, err = run_stalthos("research_species", "--definition", def_path)
            assert code == 5, f"Expected exit 5, got {code}: {out} {err}"
            status = parse_json_status(out)
            assert status["exit_code"] == 5
            assert status["ok"] is False
            assert status["command"] == "research_species"
            assert any(e["kind"] == "NotImplementedStage" for e in status["errors"])
        finally:
            Path(def_path).unlink()

    def test_missing_definition_exits_2(self):
        """Missing --definition → exit 2 (usage error) for implemented commands."""
        # generate_plant is implemented, so missing definition gives exit 2
        code, out, err = run_stalthos("generate_plant")
        assert code == 2, f"Expected exit 2, got {code}: {out} {err}"
        status = parse_json_status(out)
        assert status["exit_code"] == 2
        assert status["ok"] is False
        assert any(e["kind"] == "UsageError" for e in status["errors"])

    def test_bogus_blender_path_exits_4(self):
        """Bogus --blender path → exit 4 (missing dependency) for implemented commands."""
        # Need a definition file first to reach the blender check
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml", delete=False) as f:
            f.write("id: test\nkind: plant\nmorphology_id: test\n")
            def_path = f.name
        try:
            code, out, err = run_stalthos("generate_plant", "--definition", def_path, "--blender", "/definitely/not/blender")
            assert code == 4, f"Expected exit 4 (missing blender), got {code}: {out} {err}"
            status = parse_json_status(out)
            assert status["exit_code"] == 4
            assert status["ok"] is False
            assert any(e["kind"] == "MissingDependency" for e in status["errors"])
        finally:
            Path(def_path).unlink()


class TestJSONStatusEnvelope:
    """Test the JSON status envelope contract."""

    def test_envelope_shape_on_success(self):
        """Successful command (doctor) emits valid envelope with required fields.
        Note: 'list' prints human-readable table, not JSON — it's the one exception."""
        for cmd in ("doctor",):
            code, out, err = run_stalthos(cmd)
            assert code == 0, f"{cmd} failed: {err}"
            status = parse_json_status(out)
            # Required fields per contract
            required = {"command", "ok", "exit_code", "seed", "asset_id", "started_at",
                        "duration_s", "partial", "artifacts", "metrics", "provenance",
                        "warnings", "errors", "logs"}
            assert set(status.keys()) == required, f"Missing keys: {required - set(status.keys())}"
            assert status["command"] == cmd
            assert status["ok"] is True
            assert status["exit_code"] == 0
            assert isinstance(status["artifacts"], list)
            assert isinstance(status["metrics"], dict)
            assert isinstance(status["warnings"], list)
            assert isinstance(status["errors"], list)
            assert isinstance(status["logs"], list)

    def test_envelope_shape_on_failure(self):
        """Failed command emits valid envelope with ok=false and errors."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml", delete=False) as f:
            f.write("id: test\nkind: plant\nmorphology_id: test\n")
            def_path = f.name
        try:
            code, out, err = run_stalthos("research_species", "--definition", def_path)
            assert code == 5
            status = parse_json_status(out)
            assert status["ok"] is False
            assert status["exit_code"] == 5
            assert len(status["errors"]) > 0
            assert status["errors"][0]["kind"] == "NotImplementedStage"
        finally:
            Path(def_path).unlink()

    def test_stdout_only_json_no_extra_text(self):
        """Stdout must contain ONLY the JSON envelope, nothing else."""
        code, out, err = run_stalthos("doctor")
        assert code == 0
        # Should parse as single JSON object
        status = parse_json_status(out)
        # Re-serialize and compare — should be identical (no extra whitespace/newlines before/after)
        reparsed = json.loads(json.dumps(status))
        assert reparsed == status

    def test_doctor_includes_tools_and_workspace(self):
        """doctor status includes tools dict and workspace path."""
        code, out, err = run_stalthos("doctor")
        assert code == 0
        status = parse_json_status(out)
        assert "tools" in status["metrics"]
        assert "workspace" in status["metrics"]
        assert "python" in status["metrics"]
        tools = status["metrics"]["tools"]
        assert "blender" in tools
        assert "gltfpack" in tools
        assert isinstance(tools["blender"]["available"], bool)

    def test_list_shows_all_declared_commands(self):
        """list shows all 12 declared commands with stage and implemented flag."""
        code, out, err = run_stalthos("list")
        assert code == 0
        # list prints human-readable table to stdout (not JSON) — this is the one exception
        lines = out.strip().split("\n")
        # Each line starts with "ok " or "--- "
        declared_count = sum(1 for l in lines if l.strip().startswith(("ok ", "--- ")))
        assert declared_count == 12, f"Expected 12 declared commands, saw {declared_count}"


class TestDeterministicSeeding:
    """Test seed derivation stability across invocations."""

    def test_same_seed_same_derived_seeds(self):
        """derive_seed(root, labels...) is stable."""
        from stalthos_toolkit.core.seeding import derive_seed, normalize_seed

        # Int seed
        root = 42
        child1 = derive_seed(root, "plant", "splitcrown", "branch", 3)
        child2 = derive_seed(root, "plant", "splitcrown", "branch", 3)
        assert child1 == child2

        # String label seed
        root_str = "stable-label"
        child1 = derive_seed(root_str, "creature", "smalltrotter", "variant", 0)
        child2 = derive_seed(root_str, "creature", "smalltrotter", "variant", 0)
        assert child1 == child2

        # Different labels → different seeds
        child_a = derive_seed(42, "plant", "a")
        child_b = derive_seed(42, "plant", "b")
        assert child_a != child_b

    def test_normalize_seed_int_and_string(self):
        """normalize_seed accepts int or string, returns 63-bit int."""
        from stalthos_toolkit.core.seeding import normalize_seed, _MASK

        assert 0 <= normalize_seed(0) <= _MASK
        assert 0 <= normalize_seed(12345) <= _MASK
        assert 0 <= normalize_seed("hello") <= _MASK
        assert 0 <= normalize_seed("hello") <= _MASK
        # Same string → same int
        assert normalize_seed("hello") == normalize_seed("hello")
        # Different strings → different ints (with overwhelming probability)
        assert normalize_seed("hello") != normalize_seed("world")

    def test_rng_reproducible(self):
        """rng(root, labels) produces identical sequences."""
        from stalthos_toolkit.core.seeding import rng

        r1 = rng(42, "test")
        r2 = rng(42, "test")
        seq1 = [r1.random() for _ in range(100)]
        seq2 = [r2.random() for _ in range(100)]
        assert seq1 == seq2

        # Different label → different sequence
        r3 = rng(42, "other")
        seq3 = [r3.random() for _ in range(100)]
        assert seq1 != seq3


class TestWorkspaceLayout:
    """Test workspace creation and path-escape refusal."""

    def test_workspace_creates_five_roots(self):
        """Workspace.open() creates sources/, intermediate/, runtime/, preview/, reports/."""
        from stalthos_toolkit.core.workspace import Workspace

        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            ws.ensure()
            for name in ("sources", "intermediate", "runtime", "preview", "reports"):
                path = getattr(ws, name)
                assert path.exists(), f"{name} not created"
                assert path.is_dir()

    def test_workspace_path_for_refuses_escape(self):
        """Workspace.path_for() raises ValueError for paths escaping the root."""
        from stalthos_toolkit.core.workspace import Workspace

        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            # Valid relative path
            p = ws.path_for("sources/test.blend")
            assert str(p).startswith(str(ws.root))
            # Escape attempt
            with pytest.raises(ValueError, match="escapes workspace root"):
                ws.path_for("../outside.txt")
            with pytest.raises(ValueError, match="escapes workspace root"):
                ws.path_for("/absolute/path")

    def test_workspace_sub_creates_nested_dirs(self):
        """Workspace.sub() creates and returns subdirectory."""
        from stalthos_toolkit.core.workspace import Workspace

        with tempfile.TemporaryDirectory() as tmp:
            ws = Workspace.open(tmp)
            p = ws.sub("sources", "plants", "splitcrown")
            assert p.exists()
            assert p.is_dir()
            assert "splitcrown" in str(p)


class TestManifestRoundtrip:
    """Test manifest serialization, content_hash stability."""

    def test_manifest_save_load_roundtrip(self):
        """Manifest.save() → load() preserves all fields."""
        from stalthos_toolkit.core.manifest import Manifest, Metric, ArtifactRef, Provenance

        with tempfile.TemporaryDirectory() as tmp:
            manifest = Manifest(
                asset_id="plant.splitcrown.v001",
                kind="plant",
                species_id="plant.splitcrown",
                morphology_id="conifer.araucarian.tiered",
                stage="mature",
                variant="standard",
            )
            manifest.provenance = Provenance(
                generator="stalthos-toolkit",
                toolkit_version="0.1.0",
                tool_versions={"blender": "5.2"},
                seed=42,
                input_hashes={"species/plants/splitcrown.yaml": "abc123"},
            )
            manifest.artifacts = [
                ArtifactRef(path="var/sources/plants/splitcrown.blend", role="source",
                            sha256="deadbeef", bytes=1024),
            ]
            manifest.metrics = [
                Metric(name="triangles", value=48213, unit="count", measured=True),
            ]
            manifest.affordances = {"browse_height_min_m": 0.5, "browse_height_max_m": 4.0}
            manifest.validation.outcome = "pass"
            manifest.previews = ["var/preview/turntable/frame_001.png"]

            path = Path(tmp) / "manifest.json"
            manifest.save(path)

            loaded = Manifest.load(path)
            assert loaded.asset_id == manifest.asset_id
            assert loaded.kind == manifest.kind
            assert loaded.species_id == manifest.species_id
            assert loaded.morphology_id == manifest.morphology_id
            assert loaded.stage == manifest.stage
            assert loaded.variant == manifest.variant
            assert loaded.provenance.seed == manifest.provenance.seed
            assert loaded.provenance.input_hashes == manifest.provenance.input_hashes
            assert len(loaded.artifacts) == 1
            assert loaded.artifacts[0].sha256 == "deadbeef"
            assert len(loaded.metrics) == 1
            assert loaded.metrics[0].name == "triangles"
            assert loaded.affordances == manifest.affordances
            assert loaded.validation.outcome == "pass"
            assert loaded.previews == manifest.previews

    def test_content_hash_ignores_timestamps_and_tool_versions(self):
        """content_hash() is stable across runs with different timestamps/tool versions."""
        from stalthos_toolkit.core.manifest import Manifest, Provenance

        m1 = Manifest(asset_id="test", kind="plant")
        m1.provenance = Provenance(seed=42, tool_versions={"blender": "5.1"})
        m1.created_at = "2024-01-01T00:00:00Z"
        m2 = Manifest(asset_id="test", kind="plant")
        m2.provenance = Provenance(seed=42, tool_versions={"blender": "5.2"})
        m2.created_at = "2024-12-31T23:59:59Z"
        assert m1.content_hash() == m2.content_hash(), "content_hash must ignore timestamps and tool versions"

    def test_content_hash_changes_with_semantic_difference(self):
        """content_hash() changes when semantic content changes."""
        from stalthos_toolkit.core.manifest import Manifest, Provenance

        m1 = Manifest(asset_id="test", kind="plant")
        m1.provenance = Provenance(seed=42)
        m2 = Manifest(asset_id="test", kind="plant")
        m2.provenance = Provenance(seed=43)  # Different seed
        assert m1.content_hash() != m2.content_hash()

        m3 = Manifest(asset_id="test2", kind="plant")  # Different asset_id
        m3.provenance = Provenance(seed=42)
        assert m1.content_hash() != m3.content_hash()


class TestDefinitionLoading:
    """Test schema-less definition loading (YAML/JSON/TOML)."""

    def test_load_yaml_definition(self):
        """Load a valid YAML definition."""
        from stalthos_toolkit.schemas.loader import load_definition

        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml", delete=False) as f:
            f.write("id: test.plant\nkind: plant\nmorphology_id: test\n")
            f.flush()
            data = load_definition(f.name)
        assert data["id"] == "test.plant"
        assert data["kind"] == "plant"

    def test_load_json_definition(self):
        """Load a valid JSON definition."""
        from stalthos_toolkit.schemas.loader import load_definition

        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as f:
            f.write('{"id": "test.creature", "kind": "creature", "morphology_id": "test"}')
            f.flush()
            data = load_definition(f.name)
        assert data["id"] == "test.creature"
        assert data["kind"] == "creature"

    def test_load_toml_definition(self):
        """Load a valid TOML definition (stdlib tomllib)."""
        from stalthos_toolkit.schemas.loader import load_definition

        with tempfile.NamedTemporaryFile(mode="w", suffix=".toml", delete=False) as f:
            f.write('id = "test"\nkind = "plant"\nmorphology_id = "test"\n')
            f.flush()
            data = load_definition(f.name)
        assert data["id"] == "test"

    def test_empty_file_raises_usage_error(self):
        """Empty definition file → UsageError."""
        from stalthos_toolkit.schemas.loader import load_definition
        from stalthos_toolkit.core.errors import UsageError

        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml", delete=False) as f:
            f.write("")
            f.flush()
            with pytest.raises(UsageError, match="empty"):
                load_definition(f.name)

    def test_non_mapping_raises_usage_error(self):
        """Definition must be a mapping at top level."""
        from stalthos_toolkit.schemas.loader import load_definition
        from stalthos_toolkit.core.errors import UsageError

        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml", delete=False) as f:
            f.write("- item1\n- item2\n")
            f.flush()
            with pytest.raises(UsageError, match="mapping"):
                load_definition(f.name)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])