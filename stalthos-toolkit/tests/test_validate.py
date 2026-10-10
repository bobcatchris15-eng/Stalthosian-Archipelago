"""Acceptance-check and validate_asset tests. These must pass WITHOUT Blender.

The fixtures here are synthetic manifests over real files on disk: that is the
whole point of stage 8 -- acceptance is decided from recorded data, not by
opening Blender.
"""

from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path

import pytest

from stalthos_toolkit.cli import main
from stalthos_toolkit.core.errors import EXIT_OK, EXIT_USAGE, EXIT_VALIDATION
from stalthos_toolkit.validate import (
    OUTCOME_FAIL,
    OUTCOME_PASS,
    OUTCOME_SKIP,
    OUTCOME_WARN,
    run_acceptance,
    semantic_hash,
    sniff_texture,
)

REPO_ROOT = Path(__file__).resolve().parents[1]

BC5_UNORM = 139
BC7_SRGB = 147
BC1_UNORM = 131


def _ktx2(vk_format: int, width: int = 64, height: int = 64) -> bytes:
    """Minimal KTX2 header: identifier, vkFormat, typeSize, pixelWidth/Height."""
    identifier = bytes((0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A))
    return identifier + struct.pack("<IIII", vk_format, 1, width, height) + b"\0" * 16


def _png(width: int = 32, height: int = 32) -> bytes:
    header = bytes((0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A))
    return header + b"\0" * 8 + struct.pack(">II", width, height) + b"\0" * 16


def build_asset(root: Path, *, asset_id: str = "plant.softbough.sh1.lod0") -> tuple[Path, dict]:
    """Write a small synthetic asset tree and return (manifest_path, manifest)."""
    (root / "runtime").mkdir(parents=True, exist_ok=True)
    (root / "preview").mkdir(parents=True, exist_ok=True)
    (root / "reports" / "manifests").mkdir(parents=True, exist_ok=True)

    payloads = {
        "runtime/lod0.glb": b"GLB-lod0" * 64,
        "runtime/lod1.glb": b"GLB-lod1" * 32,
        "runtime/lod2.glb": b"GLB-lod2" * 12,
        "runtime/albedo.ktx2": _ktx2(BC7_SRGB, 1024, 1024),
        "runtime/normal.ktx2": _ktx2(BC5_UNORM, 1024, 1024),
        "preview/turntable.png": _png(),
    }
    hashes = {}
    for relative, data in payloads.items():
        path = root / relative
        path.write_bytes(data)
        hashes[relative] = hashlib.sha256(data).hexdigest()

    def artifact(relative: str, role: str, **importer_settings) -> dict:
        entry = {
            "path": str((root / relative).as_posix()),
            "role": role,
            "sha256": hashes[relative],
            "bytes": len(payloads[relative]),
        }
        if importer_settings:
            entry["importer_settings"] = importer_settings
        return entry

    manifest = {
        "schema_version": "1.0",
        "asset_id": asset_id,
        "kind": "plant",
        "species_id": "plant.softbough.sh1",
        "morphology_id": "arching_shrub",
        "stage": "mature",
        "variant": "waterside_lush",
        "created_at": "2026-10-10T12:00:00+00:00",
        "provenance": {
            "generator": "stalthos-toolkit",
            "toolkit_version": "0.1.0",
            "tool_versions": {"blender": "5.2", "gltfpack": "0.20"},
            "command": "generate_lods",
            "command_args": {"variant": "waterside_lush"},
            "seed": 42,
            "input_hashes": {"definition:softbough.yaml": hashlib.sha256(b"definition").hexdigest()},
            "parent_asset_id": None,
            "licenses": [{"id": "geometry-cc0", "applies_to": "generated_geometry",
                          "license": "CC0-1.0", "holder": "project"}],
            "references": [{"id": "flora-doc", "kind": "planning_doc",
                            "locator": "Rauken-Archipelago/docs/flora.md",
                            "usage_rights": "internal_reference", "redistributable": False,
                            "confidence": "high", "supports": ["habitat"]}],
        },
        "artifacts": [
            artifact("runtime/lod0.glb", "runtime", lod=0, triangles=48213),
            artifact("runtime/lod1.glb", "runtime", lod=1, triangles=12000),
            artifact("runtime/lod2.glb", "runtime", lod=2, triangles=3000),
            artifact("runtime/albedo.ktx2", "texture", kind="color", expected_format="BC7"),
            artifact("runtime/normal.ktx2", "texture", kind="normal", expected_format="BC5"),
        ],
        "metrics": [
            {"name": "triangles", "value": 48213, "unit": "tri", "measured": True},
            {"name": "vertices", "value": 26110, "unit": "verts", "measured": True},
            {"name": "degenerate_face_ratio", "value": 0.0, "measured": True},
            {"name": "inverted_normal_ratio", "value": 0.0, "measured": True},
            {"name": "uv_overlap_ratio", "value": 0.01, "measured": True},
            {"name": "texture_bytes", "value": 4194304, "unit": "B", "measured": True},
        ],
        "affordances": {"structural_role": "understory"},
        "validation": {"outcome": "not_run", "checks": [], "report_path": None},
        "previews": [(root / "preview/turntable.png").as_posix()],
        "notes": "",
    }
    manifest_path = root / "reports" / "manifests" / f"{asset_id}.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return manifest_path, manifest


def _check(report, check_id):
    return next((c for c in report.checks if c.id == check_id), None)


# --- the happy path ---------------------------------------------------------
def test_full_acceptance_pass(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_PASS, [c.to_dict() for c in report.checks if c.outcome == OUTCOME_FAIL]
    assert _check(report, "manifest.schema_completeness").outcome == OUTCOME_PASS
    assert _check(report, "artifacts.hash_match").outcome == OUTCOME_PASS
    assert _check(report, "geometry.sanity").outcome == OUTCOME_PASS
    assert _check(report, "lods.chain").outcome == OUTCOME_PASS
    assert _check(report, "textures.format").outcome == OUTCOME_PASS
    assert _check(report, "provenance.seed").outcome == OUTCOME_PASS
    assert _check(report, "previews.present").outcome == OUTCOME_PASS


def test_acceptance_never_requires_blender(tmp_path, monkeypatch):
    """A missing Blender must not fail stage 8 checks."""
    import stalthos_toolkit.core.workspace as workspace_module

    def explode(*args, **kwargs):
        raise AssertionError("validate must not look for Blender")

    monkeypatch.setattr(workspace_module, "find_blender", explode)
    _, manifest = build_asset(tmp_path / "var")
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_PASS


# --- each acceptance group fails honestly ------------------------------------
def test_missing_artifact_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    (tmp_path / "var" / "runtime" / "lod1.glb").unlink()
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_FAIL
    assert _check(report, "artifacts.present").outcome == OUTCOME_FAIL


def test_hash_mismatch_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    (tmp_path / "var" / "runtime" / "lod0.glb").write_bytes(b"tampered")
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_FAIL
    assert _check(report, "artifacts.hash_match").outcome == OUTCOME_FAIL


def test_artifact_without_hash_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["artifacts"][0].pop("sha256")
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert _check(report, "artifacts.hashed").outcome == OUTCOME_FAIL


def test_non_monotonic_lod_chain_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["artifacts"][1]["importer_settings"]["triangles"] = 60000
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_FAIL
    check = _check(report, "lods.chain")
    assert check.outcome == OUTCOME_FAIL and check.data["non_monotonic"]


def test_missing_lod_chain_is_warned_not_passed(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    for artifact in manifest["artifacts"]:
        settings = artifact.get("importer_settings")
        if settings:
            settings.pop("lod", None)
            settings.pop("triangles", None)
    manifest["metrics"] = [m for m in manifest["metrics"] if not m["name"].startswith("lod_triangles")]
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert _check(report, "lods.chain").outcome == OUTCOME_WARN
    assert report.outcome == OUTCOME_PASS, "a warning is not a failure unless --strict"


def test_geometry_metrics_absent_is_skip(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["metrics"] = []
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert _check(report, "geometry.sanity").outcome == OUTCOME_SKIP


def test_estimated_geometry_metric_does_not_gate(tmp_path):
    """measured=false marks an estimate; estimates are reported, never enforced."""
    _, manifest = build_asset(tmp_path / "var")
    for metric in manifest["metrics"]:
        if metric["name"] == "triangles":
            metric["measured"] = False
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert _check(report, "geometry.sanity").outcome == OUTCOME_PASS
    assert _check(report, "geometry.measured").outcome == OUTCOME_WARN


def test_out_of_range_measured_metric_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["metrics"].append({"name": "inverted_normal_ratio", "value": 0.02, "measured": True})
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_FAIL
    assert _check(report, "geometry.sanity").data["violations"]


def test_wrong_texture_codec_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    (tmp_path / "var" / "runtime" / "normal.ktx2").write_bytes(_ktx2(BC7_SRGB))
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_FAIL
    assert _check(report, "textures.format").outcome == OUTCOME_FAIL


def test_uncompressed_normal_map_warns(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    (tmp_path / "var" / "runtime" / "normal.ktx2").write_bytes(_png())
    manifest["artifacts"][4]["sha256"] = hashlib.sha256(_png()).hexdigest()
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert _check(report, "textures.compression").outcome == OUTCOME_WARN
    assert report.outcome == OUTCOME_PASS


def test_texture_without_slot_classification_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["artifacts"][3].pop("importer_settings")
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert _check(report, "textures.inventory").outcome == OUTCOME_FAIL


def test_provenance_gaps_fail(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["provenance"]["tool_versions"] = {}
    manifest["provenance"]["licenses"] = []
    manifest["provenance"]["input_hashes"] = {}
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    for check_id in ("provenance.tool_versions", "provenance.licenses", "provenance.input_hashes"):
        assert _check(report, check_id).outcome == OUTCOME_FAIL


def test_unknown_reference_rights_warns(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["provenance"]["references"][0]["usage_rights"] = "unknown"
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert _check(report, "provenance.reference_rights").outcome == OUTCOME_WARN


def test_missing_preview_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["previews"] = []
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert _check(report, "previews.present").outcome == OUTCOME_FAIL


def test_incomplete_manifest_schema_fails(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    del manifest["provenance"]["seed"]
    del manifest["artifacts"]
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_FAIL
    assert _check(report, "manifest.schema_completeness").data["error_count"] >= 2


# --- determinism ------------------------------------------------------------
def test_semantic_hash_ignores_volatile_fields(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    first = semantic_hash(manifest)
    manifest["created_at"] = "2030-01-01T00:00:00+00:00"
    manifest["provenance"]["tool_versions"] = {"blender": "9.9"}
    manifest["validation"] = {"outcome": "pass", "checks": [{"id": "x", "outcome": "pass"}]}
    assert semantic_hash(manifest) == first
    manifest["metrics"][0]["value"] = 48214
    assert semantic_hash(manifest) != first


def test_second_run_confirms_determinism(tmp_path):
    manifest_path, manifest = build_asset(tmp_path / "var")
    assert run_acceptance(manifest, workspace_root=tmp_path / "var").outcome == OUTCOME_PASS
    manifest["validation"] = {
        "outcome": "pass",
        "checks": [{"id": "determinism.semantic_hash", "outcome": "pass",
                    "severity": "info", "detail": "baseline",
                    "data": {"semantic_hash": semantic_hash(manifest)}}],
        "report_path": None,
    }
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    check = _check(report, "determinism.semantic_hash")
    assert check.outcome == OUTCOME_PASS and check.data["baseline"] == "previous_run"


def test_changed_content_between_runs_fails_determinism(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    manifest["validation"] = {
        "outcome": "pass",
        "checks": [{"id": "determinism.semantic_hash", "outcome": "pass", "severity": "info",
                    "data": {"semantic_hash": "0" * 64}}],
        "report_path": None,
    }
    report = run_acceptance(manifest, workspace_root=tmp_path / "var")
    assert report.outcome == OUTCOME_FAIL
    assert _check(report, "determinism.semantic_hash").outcome == OUTCOME_FAIL


def test_round_trip_check_passes(tmp_path):
    _, manifest = build_asset(tmp_path / "var")
    assert _check(run_acceptance(manifest, workspace_root=tmp_path / "var"),
                  "determinism.round_trip").outcome == OUTCOME_PASS


# --- texture sniffing -------------------------------------------------------
def test_sniff_texture_identifies_containers(tmp_path):
    ktx = tmp_path / "a.ktx2"
    ktx.write_bytes(_ktx2(BC5_UNORM, 128, 256))
    info = sniff_texture(ktx)
    assert (info.container, info.codec, info.width, info.height) == ("ktx2", "BC5_UNORM", 128, 256)

    png = tmp_path / "a.png"
    png.write_bytes(_png(16, 8))
    info = sniff_texture(png)
    assert (info.container, info.codec) == ("png", None)

    missing = sniff_texture(tmp_path / "nope.ktx2")
    assert missing.readable is False


# --- the command ------------------------------------------------------------
def test_validate_asset_command_passes_without_blender(tmp_path, capsys):
    workspace = tmp_path / "var"
    build_asset(workspace)
    exit_code = main(["-w", str(workspace), "-s", "42", "validate_asset",
                      "--asset-id", "plant.softbough.sh1.lod0"])
    assert exit_code == EXIT_OK
    envelope = json.loads(capsys.readouterr().out)
    assert envelope["ok"] is True
    assert envelope["metrics"]["blender_required"] is False

    report_path = workspace / "reports" / "validation" / "plant.softbough.sh1.lod0.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["outcome"] == "pass"
    assert report["tool"]["blender_required"] is False
    assert any(a["role"] == "report" for a in envelope["artifacts"])

    manifest = json.loads((workspace / "reports" / "manifests" / "plant.softbough.sh1.lod0.json")
                          .read_text(encoding="utf-8"))
    assert manifest["validation"]["outcome"] == "pass"
    assert manifest["validation"]["report_path"] == str(report_path)


def test_validate_asset_command_exits_3_on_failure(tmp_path, capsys):
    workspace = tmp_path / "var"
    build_asset(workspace)
    manifest_path = workspace / "reports" / "manifests" / "plant.softbough.sh1.lod0.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["previews"] = []
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    exit_code = main(["-w", str(workspace), "validate_asset", "--asset-id", "plant.softbough.sh1.lod0"])
    assert exit_code == EXIT_VALIDATION
    envelope = json.loads(capsys.readouterr().out)
    assert envelope["ok"] is False
    assert envelope["errors"][0]["kind"] == "ValidationFailed"

    updated = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert updated["validation"]["outcome"] == "fail"


def test_validate_asset_missing_manifest_is_usage_error(tmp_path, capsys):
    exit_code = main(["-w", str(tmp_path / "var"), "validate_asset", "--asset-id", "nope"])
    assert exit_code == EXIT_USAGE
    envelope = json.loads(capsys.readouterr().out)
    assert envelope["errors"][0]["kind"] == "UsageError"


def test_validate_asset_dry_run_writes_nothing(tmp_path, capsys):
    workspace = tmp_path / "var"
    build_asset(workspace)
    exit_code = main(["-w", str(workspace), "validate_asset", "--dry-run",
                      "--asset-id", "plant.softbough.sh1.lod0"])
    assert exit_code == EXIT_OK
    assert not (workspace / "reports" / "validation").exists()
    json.loads(capsys.readouterr().out)


def test_validate_asset_strict_turns_warnings_into_failure(tmp_path, capsys):
    workspace = tmp_path / "var"
    build_asset(workspace)
    manifest_path = workspace / "reports" / "manifests" / "plant.softbough.sh1.lod0.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for artifact in manifest["artifacts"]:
        artifact.pop("importer_settings", None)
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    exit_code = main(["-w", str(workspace), "validate_asset", "--strict",
                      "--asset-id", "plant.softbough.sh1.lod0"])
    assert exit_code == EXIT_VALIDATION
    json.loads(capsys.readouterr().out)


@pytest.mark.parametrize("command_name", ["generate_plant", "bake_materials", "render_turntable"])
def test_unimplemented_commands_still_exit_5(tmp_path, command_name, capsys):
    """Honest reporting: unimplemented stages must not fake success.

    Stage 6/8/9 (generate_lods, benchmark_asset, publish_asset) are implemented
    now, so the sample here is a stage that is genuinely still a stub.
    """
    exit_code = main(["-w", str(tmp_path / "var"), command_name, "--asset-id", "plant.softbough.sh1.lod0"])
    assert exit_code == 5
    envelope = json.loads(capsys.readouterr().out)
    assert envelope["errors"][0]["kind"] == "NotImplementedStage"