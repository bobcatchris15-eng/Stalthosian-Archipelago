from __future__ import annotations

import struct
from pathlib import Path

from stalthos_toolkit.optimize import tools as toolbox
from stalthos_toolkit.optimize.decimate import _cluster_positions, cluster_decimate
from stalthos_toolkit.optimize.glb import (
    ARRAY_BUFFER,
    ELEMENT_ARRAY_BUFFER,
    FLOAT,
    UNSIGNED_INT,
    GlbDocument,
    MeshArrays,
)
from stalthos_toolkit.optimize.lods import PLANT_POLICY, build_lod_chain


def _plane_grid(size: int, *, offset_x: float = 0.0) -> MeshArrays:
    positions = [(offset_x + x / (size - 1), y / (size - 1), 0.0)
                 for y in range(size) for x in range(size)]
    indices: list[int] = []
    for y in range(size - 1):
        for x in range(size - 1):
            a = y * size + x
            indices.extend((a, a + size, a + 1, a + 1, a + size, a + size + 1))
    return MeshArrays(positions=positions, indices=indices)


def _write_multi_primitive_glb(path: Path, meshes: list[MeshArrays]) -> Path:
    doc = GlbDocument({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": []}],
        "materials": [{"name": f"mat-{index}"} for index in range(len(meshes))],
        "buffers": [{"byteLength": 0}],
    })
    for material, mesh in enumerate(meshes):
        position_data = b"".join(struct.pack("<3f", *point) for point in mesh.positions)
        position_view = doc.append_buffer_view(position_data, target=ARRAY_BUFFER)
        position_accessor = doc.add_accessor(component_type=FLOAT, atype="VEC3",
                                             count=len(mesh.positions), buffer_view=position_view)
        index_data = b"".join(struct.pack("<I", index) for index in mesh.indices)
        index_view = doc.append_buffer_view(index_data, target=ELEMENT_ARRAY_BUFFER)
        index_accessor = doc.add_accessor(component_type=UNSIGNED_INT, atype="SCALAR",
                                           count=len(mesh.indices), buffer_view=index_view)
        doc.gltf["meshes"][0]["primitives"].append({
            "attributes": {"POSITION": position_accessor},
            "indices": index_accessor,
            "mode": 4,
            "material": material,
        })
    path.parent.mkdir(parents=True, exist_ok=True)
    return doc.write(path)


def test_small_planar_mesh_stays_nonempty_and_bounded():
    mesh = _plane_grid(2)
    result = cluster_decimate(mesh, 0.4)
    assert result.mesh.triangle_count == 1
    assert result.achieved_ratio == 0.5


def test_planar_grid_avoids_catastrophic_undershoot():
    mesh = _plane_grid(12)
    result = cluster_decimate(mesh, 0.4)
    target = int(mesh.triangle_count * 0.4)
    assert result.mesh.triangle_count >= target // 2
    assert result.mesh.triangle_count < mesh.triangle_count


def test_uv_seam_vertices_do_not_share_a_cluster():
    mesh = MeshArrays(
        positions=[(0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                   (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)],
        indices=[0, 2, 3, 1, 3, 2],
        uvs=[(0.0, 0.0), (1.0, 0.0), (0.0, 0.0), (0.0, 1.0)],
    )
    cells = _cluster_positions(mesh.positions, mesh.indices, 2, uvs=mesh.uvs)
    seam_cells = [key for key, members in cells.items() if 0 in members or 1 in members]
    assert len(seam_cells) == 2


def test_distinct_bone_influence_sets_do_not_share_a_cluster():
    mesh = MeshArrays(
        positions=[(0.0, 0.0, 0.0), (0.1, 0.0, 0.0), (1.0, 0.0, 0.0),
                   (0.0, 1.0, 0.0), (1.0, 1.0, 0.0)],
        indices=[0, 2, 3, 1, 3, 4],
        joints=[(0, 0, 0, 0), (1, 0, 0, 0), (0, 0, 0, 0),
                (0, 0, 0, 0), (1, 0, 0, 0)],
        weights=[(1.0, 0.0, 0.0, 0.0)] * 5,
    )
    cells = _cluster_positions(mesh.positions, mesh.indices, 2,
                               joints=mesh.joints, weights=mesh.weights)
    first_cell = next(key for key, members in cells.items() if 0 in members)
    second_cell = next(key for key, members in cells.items() if 1 in members)
    assert first_cell != second_cell


def test_multi_primitive_four_level_chain_is_monotonic_and_byte_deterministic(
        tmp_path: Path, monkeypatch):
    monkeypatch.setattr(toolbox, "locate", lambda name: None)
    source = _write_multi_primitive_glb(
        tmp_path / "source.glb", [_plane_grid(20), _plane_grid(18, offset_x=3.0)])
    first = build_lod_chain(source, tmp_path / "first", asset_id="plant.regression",
                            policy=PLANT_POLICY, prefer="python", allow_degraded=True)
    second = build_lod_chain(source, tmp_path / "second", asset_id="plant.regression",
                             policy=PLANT_POLICY, prefer="python", allow_degraded=True)

    assert len(first.records) == 4
    assert all(count > 0 for count in first.triangles)
    assert first.monotonic()
    assert all(record.degraded for record in first.records)
    assert all(record.measured["silhouette_drift"] is False for record in first.records)
    for left, right in zip(first.records, second.records):
        assert Path(left.path).read_bytes() == Path(right.path).read_bytes()
        doc = GlbDocument.read(left.path)
        assert len(doc.gltf["meshes"][0]["primitives"]) == 2
        assert [primitive["material"] for primitive in doc.gltf["meshes"][0]["primitives"]] == [0, 1]



def test_global_trim_shares_budget_across_primitives(tmp_path):
    from stalthos_toolkit.optimize.lods import _trim_document
    source = _write_multi_primitive_glb(tmp_path / "source.glb",
                                       [_plane_grid(12), _plane_grid(12, offset_x=3)])
    doc, _ = _trim_document(GlbDocument.read(source), 100)
    assert 0 < doc.summary().triangles <= 100
    assert all(doc.primitive_arrays(p).triangle_count > 0
               for p in doc.gltf["meshes"][0]["primitives"])


def test_compaction_preserves_vertex_index_correspondence():
    from stalthos_toolkit.optimize.decimate import _compact
    mesh = MeshArrays(positions=[(0.,0.,0.), (1.,0.,0.), (0.,1.,0.)], indices=[2,0,1])
    out = _compact(mesh, mesh.indices)
    assert [out.positions[i] for i in out.indices] == [mesh.positions[i] for i in mesh.indices]


def test_source_selection_ignores_prior_collision_and_prefers_master(tmp_path):
    from stalthos_toolkit.optimize.pipeline_state import find_source_asset
    for name in ("collision.glb", "lod0.glb", "master.blend"):
        (tmp_path / name).write_bytes(b"fixture")
    manifest = {"artifacts": [
        {"path": "collision.glb", "role": "collision"},
        {"path": "lod0.glb", "role": "runtime", "importer_settings": {"generated_by": "generate_lods"}},
        {"path": "master.blend", "role": "source"}]}
    assert find_source_asset(manifest, tmp_path)["path"] == "master.blend"


def test_registered_bake_is_preferred_over_original_geometry(tmp_path):
    from stalthos_toolkit.optimize.pipeline_state import find_source_asset
    for name in ("original.glb", "baked.blend"):
        (tmp_path / name).write_bytes(b"fixture")
    entries = [{"path":"original.glb","role":"source"},
               {"path":"baked.blend","role":"source", "importer_settings":{"generated_by":"bake_materials"}}]
    assert find_source_asset({"artifacts":entries}, tmp_path)["path"] == "baked.blend"
