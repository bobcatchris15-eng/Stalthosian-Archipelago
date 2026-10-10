"""Stage 6/8 optimization tests. These pass WITHOUT Blender, gltfpack or Compressonator.

Fixtures build real GLB files and real manifests on disk: the point of these
stages is that acceptance is decided from recorded, hashed data, not by opening
a renderer.

Where an external tool would be used, the test asserts the *typed degradation*
(exit 4, warning, `measured=False`) rather than the tool's output. Two tests use
a stub executable on PATH to prove the command line we build is well formed --
they verify our wiring, not the tool.
"""

from __future__ import annotations

import json
import os
import struct
from pathlib import Path

import pytest

from stalthos_toolkit.benchmark.measure import (
    GTX_1080_BUDGET,
    budget_context,
    compare_before_after,
    frame_time_measurement,
    measure_glb,
    streaming_cost,
    vram_estimate,
)
from stalthos_toolkit.cli import main
from stalthos_toolkit.core.errors import EXIT_MISSING_DEPENDENCY, EXIT_OK, EXIT_USAGE
from stalthos_toolkit.optimize import tools as toolbox
from stalthos_toolkit.optimize.decimate import cluster_decimate
from stalthos_toolkit.optimize.glb import (
    GlbDocument,
    GlbError,
    check_rig_integrity,
    validate_structure,
)
from stalthos_toolkit.optimize.lods import build_lod_chain, chain_metrics, policy_for
from stalthos_toolkit.optimize.textures import (
    TEXTURE_POLICY,
    compress_textures,
    ktx2_level_count,
    select_format,
    target_resolution,
)

ASSET_ID = "plant.synthetic.lod0"


# --- fixtures ---------------------------------------------------------------
def build_test_glb(path: Path, *, grid: int = 12, skinned: bool = False) -> Path:
    """Write a small but real GLB (grid mesh, optional skin + animation)."""
    positions = []
    for z in range(grid):
        for y in range(grid):
            for x in range(grid):
                positions.append((x / grid, y / grid, ((x * 7 + y * 13 + z * 3) % 5) / 5))
    indices: list[int] = []
    stride = grid * grid
    for z in range(grid - 1):
        for y in range(grid - 1):
            for x in range(grid - 1):
                a = z * stride + y * grid + x
                indices += [a, a + grid, a + 1, a + 1, a + grid, a + grid + 1]
    normals = [(0.0, 0.0, 1.0)] * len(positions)
    uvs = [(p[0], p[1]) for p in positions]

    binary = bytearray()
    accessors: list[dict] = []
    views: list[dict] = []

    def add_view(payload: bytes, target: int | None = None) -> int:
        pad = (4 - len(binary) % 4) % 4
        binary.extend(b"\0" * pad)
        offset = len(binary)
        binary.extend(payload)
        view: dict = {"buffer": 0, "byteOffset": offset, "byteLength": len(payload)}
        if target:
            view["target"] = target
        views.append(view)
        return len(views) - 1

    position_view = add_view(b"".join(struct.pack("<3f", *p) for p in positions), 34962)
    position_accessor = len(accessors)
    accessors.append({"bufferView": position_view, "componentType": 5126,
                      "count": len(positions), "type": "VEC3",
                      "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 1.0]})
    normal_view = add_view(b"".join(struct.pack("<3f", *n) for n in normals), 34962)
    normal_accessor = len(accessors)
    accessors.append({"bufferView": normal_view, "componentType": 5126,
                      "count": len(normals), "type": "VEC3"})
    uv_view = add_view(b"".join(struct.pack("<2f", *u) for u in uvs), 34962)
    uv_accessor = len(accessors)
    accessors.append({"bufferView": uv_view, "componentType": 5126,
                      "count": len(uvs), "type": "VEC2"})
    index_view = add_view(b"".join(struct.pack("<I", i) for i in indices), 34963)
    index_accessor = len(accessors)
    accessors.append({"bufferView": index_view, "componentType": 5125,
                      "count": len(indices), "type": "SCALAR"})

    attributes = {"POSITION": position_accessor, "NORMAL": normal_accessor,
                  "TEXCOORD_0": uv_accessor}
    nodes = [{"name": "root", "children": [1, 2]}]
    if skinned:
        joint_view = add_view(b"".join(struct.pack("<4B", 0, 1, 0, 0) for _ in positions), 34962)
        joint_accessor = len(accessors)
        accessors.append({"bufferView": joint_view, "componentType": 5121,
                          "count": len(positions), "type": "VEC4"})
        weight_view = add_view(b"".join(struct.pack("<4f", 0.75, 0.25, 0.0, 0.0)
                                        for _ in positions), 34962)
        weight_accessor = len(accessors)
        accessors.append({"bufferView": weight_view, "componentType": 5126,
                          "count": len(positions), "type": "VEC4"})
        attributes["JOINTS_0"] = joint_accessor
        attributes["WEIGHTS_0"] = weight_accessor
        nodes.append({"name": "hide", "mesh": 0, "skin": 0})
    else:
        nodes.append({"name": "hide", "mesh": 0})
    nodes.append({"name": "pelvis", "translation": [0.0, 0.0, 0.0]})

    gltf: dict = {
        "asset": {"version": "2.0", "generator": "synthetic-test"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "meshes": [{"name": "grid", "primitives": [
            {"attributes": attributes, "indices": index_accessor, "mode": 4, "material": 0}]}],
        "materials": [{"name": "bark"}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(binary)}],
    }
    if skinned:
        gltf["skins"] = [{"joints": [2], "skeleton": 2}]
        keys = [0.0, 0.5, 1.0]
        key_view = add_view(b"".join(struct.pack("<f", t) for t in keys))
        key_accessor = len(accessors)
        accessors.append({"bufferView": key_view, "componentType": 5126,
                          "count": len(keys), "type": "SCALAR"})
        output_view = add_view(b"".join(struct.pack("<4f", t, 0.0, 0.0, 1.0) for t in keys))
        output_accessor = len(accessors)
        accessors.append({"bufferView": output_view, "componentType": 5126,
                          "count": len(keys), "type": "VEC4"})
        gltf["animations"] = [{
            "name": "gait",
            "samplers": [{"input": key_accessor, "output": output_accessor,
                          "interpolation": "LINEAR"}],
            "channels": [{"sampler": 0, "target": {"node": 2, "path": "translation"}}],
        }]

    path.parent.mkdir(parents=True, exist_ok=True)
    return GlbDocument(gltf, bytes(binary)).write(path)


def _png(width: int = 64, height: int = 64) -> bytes:
    header = bytes((0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A))
    return header + b"\0" * 8 + struct.pack(">II", width, height) + b"\0" * 16


def write_manifest(root: Path, *, asset_id: str = ASSET_ID, kind: str = "plant",
                   with_texture: bool = False, validation: str = "pass",
                   source: Path | None = None) -> Path:
    """A manifest that satisfies `validate/checks.py` (previews, hashes, provenance)."""
    from stalthos_toolkit.core.seeding import sha256_file

    reports = root / "reports" / "manifests"
    reports.mkdir(parents=True, exist_ok=True)
    preview = root / "preview" / "turntable.png"
    preview.parent.mkdir(parents=True, exist_ok=True)
    preview.write_bytes(_png())

    artifacts: list[dict] = []
    if source is not None:
        artifacts.append({
            "path": str(source), "role": "source", "sha256": sha256_file(source),
            "bytes": source.stat().st_size,
            "importer_settings": {"triangles": 100},
        })
    if with_texture:
        texture = root / "intermediate" / f"{asset_id}" / "textures" / "albedo.png"
        texture.parent.mkdir(parents=True, exist_ok=True)
        texture.write_bytes(_png())
        artifacts.append({
            "path": str(texture), "role": "texture", "sha256": sha256_file(texture),
            "bytes": texture.stat().st_size,
            "importer_settings": {"kind": "color", "expected_format": "BC7"},
        })

    manifest = {
        "schema_version": "1.0",
        "asset_id": asset_id,
        "kind": kind,
        "species_id": "plant.synthetic",
        "morphology_id": "generator.test",
        "stage": "mature",
        "variant": None,
        "created_at": "2026-01-01T00:00:00+00:00",
        "provenance": {
            "generator": "stalthos-toolkit",
            "toolkit_version": "0.1.0",
            "tool_versions": {"blender": "5.2", "gltfpack": "not-available"},
            "command": "generate_plant",
            "command_args": {},
            "seed": 42,
            "input_hashes": {"definition:plant.synthetic.yaml": "a" * 64},
            "licenses": [{"id": "definition", "applies_to": "definition",
                          "license": "CC0-1.0"}],
            "references": [{"id": "ref-1", "kind": "photo", "locator": "example.invalid/1",
                            "usage_rights": "unknown"}],
        },
        "artifacts": artifacts,
        "metrics": [{"name": "triangles", "value": 100, "unit": "triangles", "measured": True},
                    {"name": "vertices", "value": 64, "unit": "vertices", "measured": True}],
        "affordances": {},
        "validation": {"outcome": validation, "checks": [], "report_path": None},
        "previews": [str(preview)],
        "notes": "",
    }
    path = reports / f"{asset_id}.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return path


@pytest.fixture()
def workspace(tmp_path: Path) -> Path:
    for name in ("sources", "intermediate", "runtime", "preview", "reports"):
        (tmp_path / name).mkdir(parents=True, exist_ok=True)
    return tmp_path


def run_cli(*args: str) -> tuple[int, dict]:
    """Run the CLI in-process and return (exit_code, status envelope)."""
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = main(list(args))
    payload = json.loads(buffer.getvalue())
    return code, payload


# --- GLB reader/writer ------------------------------------------------------
def test_glb_round_trip_and_counts(tmp_path: Path):
    path = build_test_glb(tmp_path / "source.glb", grid=10)
    doc = GlbDocument.read(path)
    summary = doc.summary()
    assert summary.triangles == 2 * (10 - 1) ** 3
    assert summary.vertices == 1000
    assert doc.unique_material_count() == 1

    again = GlbDocument.read(path)
    assert again.summary().triangles == summary.triangles
    assert check_rig_integrity(again) == []


def test_glb_rejects_garbage(tmp_path: Path):
    junk = tmp_path / "junk.glb"
    junk.write_bytes(b"not a glb at all")
    with pytest.raises(GlbError):
        GlbDocument.read(junk)


def test_skinned_document_reports_intact_rig(tmp_path: Path):
    path = build_test_glb(tmp_path / "creature.glb", grid=8, skinned=True)
    doc = GlbDocument.read(path)
    assert check_rig_integrity(doc) == []
    assert doc.summary().skins == 1
    assert doc.summary().animations == 1
    rig = validate_structure(doc)
    assert rig["validated"] is True and rig["errors"] == 0


def test_rig_integrity_flags_broken_documents(tmp_path: Path):
    path = build_test_glb(tmp_path / "creature.glb", grid=6, skinned=True)
    doc = GlbDocument.read(path)
    doc.gltf["animations"][0]["channels"][0]["target"]["node"] = 99
    issues = check_rig_integrity(doc)
    assert any(issue["kind"] == "animation_target_missing" for issue in issues)


# --- decimation -------------------------------------------------------------
def test_cluster_decimate_is_deterministic_and_reduces(tmp_path: Path):
    doc = GlbDocument.read(build_test_glb(tmp_path / "src.glb", grid=12))
    primitive = next(doc.iter_primitives())[2]
    mesh = doc.primitive_arrays(primitive)
    first = cluster_decimate(mesh, 0.25)
    second = cluster_decimate(mesh, 0.25)
    assert first.mesh.positions == second.mesh.positions
    assert first.mesh.indices == second.mesh.indices
    assert 0 < first.mesh.triangle_count < mesh.triangle_count
    assert first.mesh.triangle_count <= int(mesh.triangle_count * 0.25) + 1


def test_cluster_decimate_keeps_skinning_attributes(tmp_path: Path):
    doc = GlbDocument.read(build_test_glb(tmp_path / "src.glb", grid=8, skinned=True))
    mesh = doc.primitive_arrays(next(doc.iter_primitives())[2])
    result = cluster_decimate(mesh, 0.4)
    assert result.mesh.joints is not None and result.mesh.weights is not None
    assert len(result.mesh.joints) == result.mesh.vertex_count
    assert all(abs(sum(w) - 1.0) < 1e-5 for w in result.mesh.weights)


# --- LOD chain --------------------------------------------------------------
def test_lod_chain_is_monotonic_and_deterministic(tmp_path: Path):
    source = build_test_glb(tmp_path / "src.glb", grid=12)
    first = build_lod_chain(source, tmp_path / "a", asset_id="plant.synthetic",
                            policy=policy_for("plant"), allow_degraded=True)
    second = build_lod_chain(source, tmp_path / "b", asset_id="plant.synthetic",
                             policy=policy_for("plant"), allow_degraded=True)
    assert first.monotonic() is True
    assert [r.sha256 for r in first.records] == [r.sha256 for r in second.records]
    assert len(first.records) == len(policy_for("plant").ratios)
    for record in first.records:
        assert Path(record.path).exists()
        assert record.sha256 and record.bytes > 0
        assert record.triangles > 0
    # LOD levels must get smaller on disk as well as in triangle count
    assert first.records[-1].bytes < first.records[0].bytes


def test_lod_chain_without_optimizer_is_degraded_and_unmeasured(tmp_path: Path):
    source = build_test_glb(tmp_path / "src.glb", grid=10)
    result = build_lod_chain(source, tmp_path / "lod", asset_id="plant.synthetic",
                             policy=policy_for("plant"), allow_degraded=True)
    assert result.simplifier in ("gltfpack", "python")
    if result.simplifier == "python":
        assert result.degraded is True
        assert any("meshoptimizer" in w or "gltfpack" in w for w in result.warnings)
    metrics = {m["name"]: m for m in chain_metrics(result)}
    assert metrics["lod_silhouette_drift.lod1"]["measured"] is False
    assert metrics["lod_triangles.lod1"]["measured"] is True
    assert metrics["impostor_generated"]["measured"] is False
    assert metrics["collision_convex"]["measured"] is False


def test_lod_chain_reports_collision_and_impostor_stub(tmp_path: Path):
    source = build_test_glb(tmp_path / "src.glb", grid=10)
    result = build_lod_chain(source, tmp_path / "lod", asset_id="plant.synthetic",
                             policy=policy_for("plant"), allow_degraded=True)
    collision = result.collision
    assert collision is not None
    assert Path(collision["path"]).exists()
    assert collision["convex"] is False
    assert collision["convex_hull_implemented"] is False
    assert collision["measured"]["convex_hull_volume"] is False

    stub = json.loads(Path(result.impostor["path"]).read_text(encoding="utf-8"))
    assert stub["implemented"] is False
    assert stub["status"] == "stub_not_generated"


def test_lod_chain_preserves_skin_and_animation(tmp_path: Path):
    source = build_test_glb(tmp_path / "creature.glb", grid=8, skinned=True)
    result = build_lod_chain(source, tmp_path / "lod", asset_id="creature.synthetic",
                             policy=policy_for("creature"), allow_degraded=True)
    for record in result.records:
        doc = GlbDocument.read(record.path)
        assert doc.summary().skins == 1
        assert doc.summary().animations == 1
        assert check_rig_integrity(doc) == []


def test_lod_chain_rejects_geometryless_source(tmp_path: Path):
    path = tmp_path / "empty.glb"
    GlbDocument({"asset": {"version": "2.0"}, "nodes": [], "meshes": []}).write(path)
    with pytest.raises(GlbError):
        build_lod_chain(path, tmp_path / "out", asset_id="plant.synthetic", allow_degraded=True)


def test_policy_budgets_are_strictly_decreasing():
    for policy in policy_for("plant"), policy_for("creature"):
        assert policy.ratios[0] == 1.0
        assert all(b < a for a, b in zip(policy.ratios, policy.ratios[1:]))
        assert policy.collision_triangles <= 512


# --- textures ---------------------------------------------------------------
def test_format_policy_per_slot_kind():
    assert select_format("normal").format == "BC5"
    assert select_format("color").format == "BC7"
    assert select_format("albedo").format == "BC7"
    assert select_format("roughness").format == "BC4"
    assert select_format("thickness").format == "BC4"
    assert select_format("mask").format == "BC5"
    assert select_format("something-new").format in ("BC4", "BC5")
    assert select_format("normal").max_resolution == 2048
    assert select_format("roughness").max_resolution == 1024
    assert "normal" in TEXTURE_POLICY and "color" in TEXTURE_POLICY


def test_target_resolution_aligns_to_bc_block():
    assert target_resolution(1000, 500, 2048) == (1000, 500)
    assert target_resolution(4096, 4096, 2048) == (2048, 2048)
    width, height = target_resolution(1023, 777, 1024)
    assert width % 4 == 0 and height % 4 == 0


def test_texture_compression_without_compressonator_exits_4(tmp_path: Path, monkeypatch):
    monkeypatch.setattr(toolbox, "locate", lambda name: None)
    source = tmp_path / "albedo.png"
    source.write_bytes(_png())
    with pytest.raises(Exception) as excinfo:
        compress_textures([(source, "color")], tmp_path / "out")
    assert excinfo.value.exit_code == EXIT_MISSING_DEPENDENCY


def test_texture_compression_degraded_emits_uncompressed_with_warning(tmp_path: Path, monkeypatch):
    monkeypatch.setattr(toolbox, "locate", lambda name: None)
    source = tmp_path / "albedo.png"
    source.write_bytes(_png())
    report = compress_textures([(source, "color")], tmp_path / "out", allow_degraded=True)
    assert report.degraded is True
    assert report.warnings and "UNCOMPRESSED" in report.warnings[0]
    outcome = report.outcomes[0]
    assert outcome.compressed is False
    assert outcome.degraded is True
    assert outcome.measured["compression_ratio"] is False
    assert Path(outcome.path).exists()
    metrics = {m["name"]: m for m in report.metrics()}
    assert metrics["texture_compression_ratio"]["measured"] is False


def test_texture_compression_invokes_compressonator(tmp_path: Path, monkeypatch):
    """Proves the argv we build is well formed, using a stub in place of the tool.

    This verifies OUR command line, not AMD Compressonator's behaviour.
    """
    recorded: list[list[str]] = []

    def fake_run_tool(path, args, timeout=None, cwd=None):
        recorded.append(list(args))
        target = Path(args[args.index("-o") + 1])
        fmt = args[args.index("-f") + 1]
        vk = {"BC4": 131, "BC5": 139, "BC7": 147}[fmt]
        identifier = bytes((0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A))
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(identifier + struct.pack("<IIII", vk, 1, 64, 64)
                           + struct.pack("<IIII", 0, 0, 1, 2) + b"\0" * 16 + b"\0" * 128)
        import subprocess

        return subprocess.CompletedProcess(args=[path, *args], returncode=0, stdout="",
                                           stderr="Compressonator version 4.5.0")

    monkeypatch.setattr(toolbox, "locate", lambda name: "stub" if name == "compressonator" else None)
    monkeypatch.setattr(toolbox, "run_tool", fake_run_tool)

    source = tmp_path / "albedo.png"
    source.write_bytes(_png())
    report = compress_textures([(source, "color")], tmp_path / "out")
    assert report.degraded is False
    assert recorded and recorded[0][0] == "-i"
    assert "-f" in recorded[0] and recorded[0][recorded[0].index("-f") + 1] == "BC7"
    assert "-fd" in recorded[0]                      # mip chain requested
    outcome = report.outcomes[0]
    assert outcome.compressed is True
    assert outcome.format == "BC7"
    assert outcome.measured["codec"] is True
    assert ktx2_level_count(outcome.path) == 2      # mip levels verified from the header
    assert report.bytes_after > 0


# --- generate_lods command --------------------------------------------------
def test_generate_lods_command_writes_chain_and_updates_manifest(workspace: Path):
    source = build_test_glb(workspace / "sources" / "plant.glb", grid=10)
    manifest = write_manifest(workspace, source=source)
    code, envelope = run_cli("generate_lods", "--workspace", str(workspace),
                             "--asset-id", ASSET_ID)
    assert code == EXIT_OK, envelope
    assert envelope["ok"] is True
    assert envelope["partial"] is False
    assert envelope["metrics"]["monotonic"] is True
    assert envelope["metrics"]["lod_count"] == 4
    out_dir = Path(envelope["metrics"]["out_dir"])
    assert out_dir.parts[-3:] == ("intermediate", "plants", ASSET_ID)

    payload = json.loads(manifest.read_text(encoding="utf-8"))
    names = {m["name"] for m in payload["metrics"]}
    assert "lod_triangles.lod0" in names
    silhouette = next(m for m in payload["metrics"] if m["name"] == "lod_silhouette_drift.lod1")
    assert silhouette["measured"] is False
    roles = {a["role"] for a in payload["artifacts"]}
    assert "runtime" in roles and "collision" in roles
    # stage 6 stages the chain in var/intermediate; publish promotes it
    for artifact in payload["artifacts"]:
        if artifact["role"] == "runtime":
            assert "intermediate" in artifact["path"]
    for artifact in payload["artifacts"]:
        assert Path(artifact["path"]).exists()
        if artifact["role"] != "source":
            assert artifact["sha256"]


def test_generate_lods_exit_4_when_compressonator_missing(workspace: Path, monkeypatch):
    monkeypatch.setattr(toolbox, "locate", lambda name: None)
    source = build_test_glb(workspace / "sources" / "plant.glb", grid=6)
    manifest = write_manifest(workspace, source=source, with_texture=True)
    before = manifest.read_text(encoding="utf-8")
    code, envelope = run_cli("generate_lods", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_MISSING_DEPENDENCY, envelope
    assert envelope["ok"] is False
    assert envelope["errors"][0]["detail"]["tool"] == "compressonator"
    assert manifest.read_text(encoding="utf-8") == before  # nothing written


def test_generate_lods_exit_2_without_manifest(workspace: Path):
    code, envelope = run_cli("generate_lods", "--workspace", str(workspace),
                             "--asset-id", "plant.nothing")
    assert code == EXIT_USAGE
    assert envelope["errors"][0]["kind"] == "UsageError"


def test_generate_lods_dry_run_writes_nothing(workspace: Path):
    source = build_test_glb(workspace / "sources" / "plant.glb", grid=6)
    write_manifest(workspace, source=source)
    code, envelope = run_cli("generate_lods", "--workspace", str(workspace),
                             "--asset-id", ASSET_ID, "--dry-run")
    assert code == EXIT_OK
    assert envelope["metrics"]["plan"]["asset_id"] == ASSET_ID
    assert not (workspace / "intermediate" / "plants" / ASSET_ID).exists()


# --- benchmark --------------------------------------------------------------
def test_measure_glb_counts_are_measured(tmp_path: Path):
    path = build_test_glb(tmp_path / "lod0.glb", grid=10)
    result = measure_glb(path)
    assert result["summary"]["triangles"] == 2 * (10 - 1) ** 3
    assert result["file_bytes"] > 0
    assert result["resident_bytes"] > 0
    assert all(m["measured"] for m in result["measurements"])


def test_frame_time_is_reported_as_not_measured(monkeypatch):
    monkeypatch.setattr(toolbox, "locate", lambda name: None)
    frame = frame_time_measurement()
    assert frame["measured"] is False
    assert frame["frame_ms_p99"] is None
    assert "not measured" in frame["reason"].lower() or "NOT measured" in frame["reason"]


def test_budget_context_states_the_hardware_target():
    budget = budget_context()["reference"]
    assert "GTX 1080" in budget["name"]
    assert budget["fps_floor"] == 30
    assert budget["frame_ms_budget"] == 33.33
    assert budget["resolution"] == [1920, 1080]


def test_vram_and_streaming_estimates_are_unmeasured():
    vram = vram_estimate({"lod0": 1000, "lod1": 100}, 500)
    assert vram["value"] == 1600
    assert vram["measured"] is False
    streaming = streaming_cost({"lod0": 100, "lod1": 10})
    assert streaming["resident_if_all_loaded_bytes"] == 110
    assert streaming["measured"] is False
    assert streaming["wall_clock_ms"] is None


def test_compare_before_after_is_measured_except_silhouette():
    before = {"summary": {"triangles": 1000, "vertices": 600, "draw_calls": 2}, "file_bytes": 1000}
    after = {"summary": {"triangles": 500, "vertices": 320, "draw_calls": 2}, "file_bytes": 600}
    deltas = {m.name: m for m in compare_before_after(before, after)}
    assert deltas["delta.triangles"].value == pytest.approx(-0.5)
    assert deltas["delta.file_bytes"].value == pytest.approx(-0.4)
    assert deltas["silhouette_drift_pct"].measured is False


def test_benchmark_asset_reports_unmeasured_frame_time(workspace: Path):
    source = build_test_glb(workspace / "sources" / "plant.glb", grid=10)
    write_manifest(workspace, source=source)
    run_cli("generate_lods", "--workspace", str(workspace), "--asset-id", ASSET_ID)

    code, envelope = run_cli("benchmark_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_OK, envelope
    assert envelope["metrics"]["frame_time_measured"] is False
    assert envelope["metrics"]["vram_is_estimate"] is True
    assert envelope["metrics"]["reference_gpu"] == "NVIDIA GTX 1080 8 GB"
    report_path = Path(envelope["metrics"]["report_path"])
    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["frame_time"]["measured"] is False
    assert report["memory"]["vram_lower_bound"]["measured"] is False
    lods = {Path(entry["path"]).stem for entry in report["lods"]}
    assert {"lod0", "lod1", "lod2", "lod3"} <= lods


def test_benchmark_asset_exit_2_without_geometry(workspace: Path):
    manifest = write_manifest(workspace)
    code, envelope = run_cli("benchmark_asset", "--workspace", str(workspace), "--asset-id", ASSET_ID)
    assert code == EXIT_USAGE
    assert "no glTF/GLB artifact" in envelope["errors"][0]["message"]


def test_tool_discovery_reports_absence(monkeypatch):
    monkeypatch.setattr(toolbox.shutil, "which", lambda name: None)
    state = toolbox.discover("gltfpack")
    assert state.available is False
    assert state.to_dict()["name"] == "gltfpack"
    assert "not found" in state.reason
    with pytest.raises(Exception) as excinfo:
        toolbox.require("compressonator")
    assert excinfo.value.exit_code == EXIT_MISSING_DEPENDENCY
    assert "Compressonator" in (excinfo.value.hint or "")


def test_env_override_finds_tool(tmp_path: Path, monkeypatch):
    fake = tmp_path / "gltfpack"
    fake.write_text("#!/bin/sh\necho gltfpack 1.0\n")
    monkeypatch.setenv("STEALTHOS_GLTFPACK", str(fake))
    state = toolbox.discover("gltfpack")
    assert state.available is True and state.path == str(fake)