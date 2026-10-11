from __future__ import annotations

import hashlib
import subprocess
import struct
from pathlib import Path

import pytest

from stalthos_toolkit.optimize.glb import (
    ARRAY_BUFFER,
    FLOAT,
    SHORT,
    UNSIGNED_BYTE,
    GlbDocument,
    GlbError,
    MeshArrays,
    validate_with_tools,
)
from stalthos_toolkit.optimize import glb as glb_module
from stalthos_toolkit.optimize.lods import (
    PLANT_POLICY,
    _apply_arrays,
    build_lod_chain,
)


def _quantized_doc(component_type: int, normalized: bool) -> tuple[GlbDocument, dict]:
    positions = struct.pack("<9f", 0, 0, 0, 1, 0, 0, 0, 1, 0)
    if component_type == SHORT:
        uv_data = struct.pack("<6h", -32768, 16384, 0, -16384, 32767, 0)
    elif component_type == UNSIGNED_BYTE:
        uv_data = struct.pack("<6B", 0, 255, 64, 128, 255, 0)
    else:  # pragma: no cover - helper is deliberately limited to these fixtures
        raise AssertionError(component_type)
    index_data = struct.pack("<3H", 0, 1, 2)
    binary = positions + uv_data + index_data
    position_view = {"buffer": 0, "byteOffset": 0, "byteLength": len(positions), "target": ARRAY_BUFFER}
    uv_view = {"buffer": 0, "byteOffset": len(positions), "byteLength": len(uv_data), "target": ARRAY_BUFFER}
    index_view = {"buffer": 0, "byteOffset": len(positions) + len(uv_data),
                  "byteLength": len(index_data), "target": 34963}
    gltf = {
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [position_view, uv_view, index_view],
        "accessors": [
            {"bufferView": 0, "componentType": FLOAT, "count": 3, "type": "VEC3"},
            {"bufferView": 1, "componentType": component_type, "count": 3,
             "type": "VEC2", "normalized": normalized},
            {"bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
        "materials": [{"name": "baked-material"}],
        "images": [{"uri": "albedo.png"}],
        "textures": [{"source": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1},
                                      "indices": 2, "material": 0}]}],
    }
    return GlbDocument(gltf, binary), gltf["meshes"][0]["primitives"][0]


@pytest.mark.parametrize("component_type,normalized,expected", [
    (SHORT, True, [(-1.0, 16384 / 32767), (0.0, -16384 / 32767), (1.0, 0.0)]),
    (UNSIGNED_BYTE, True, [(0.0, 1.0), (64 / 255, 128 / 255), (1.0, 0.0)]),
])
def test_quantized_uv_round_trip_keeps_encoding_bounds_and_material_bindings(
    component_type: int, normalized: bool, expected: list[tuple[float, float]],
) -> None:
    source, primitive = _quantized_doc(component_type, normalized)
    arrays = source.primitive_arrays(primitive)
    assert arrays is not None
    derived = source.copy()
    derived.gltf["meshes"][0]["primitives"][0] = _apply_arrays(derived, primitive, arrays)
    derived = GlbDocument.from_bytes(_write_bytes(derived))

    round_tripped_primitive = derived.gltf["meshes"][0]["primitives"][0]
    uv_accessor_index = round_tripped_primitive["attributes"]["TEXCOORD_0"]
    uv_accessor = derived.gltf["accessors"][uv_accessor_index]
    assert uv_accessor["componentType"] == component_type
    assert uv_accessor["normalized"] is normalized
    assert uv_accessor["min"] == ([-32767.0, -16384.0] if component_type == SHORT else [0.0, 0.0])
    assert uv_accessor["max"] == ([32767.0, 16384.0] if component_type == SHORT else [255.0, 255.0])
    actual = derived.accessor_values(uv_accessor_index)
    for observed, target in zip(actual, expected):
        assert observed == pytest.approx(target)
    assert round_tripped_primitive["material"] == 0
    assert derived.gltf["textures"] == source.gltf["textures"]
    assert derived.gltf["images"] == source.gltf["images"]


def _write_bytes(doc: GlbDocument) -> bytes:
    """Serialize through a temporary path to exercise the production GLB writer."""
    import tempfile

    with tempfile.TemporaryDirectory() as directory:
        path = doc.write(Path(directory) / "roundtrip.glb")
        return path.read_bytes()


def test_unsupported_uv_component_fails_with_typed_glb_error() -> None:
    source, primitive = _quantized_doc(SHORT, True)
    arrays = source.primitive_arrays(primitive)
    assert arrays is not None
    arrays.uv_component = 5125  # glTF UNSIGNED_INT is not a supported UV encoding here.
    with pytest.raises(GlbError, match="unsupported UV accessor componentType 5125"):
        _apply_arrays(source.copy(), primitive, arrays)


def test_compact_preserves_and_remaps_embedded_image_payloads() -> None:
    source, _ = _quantized_doc(SHORT, True)
    image_bytes = b"\x89PNG\r\n\x1a\nembedded-image-payload"
    source.append_buffer_view(b"discarded unreferenced bytes")
    image_view = source.append_buffer_view(image_bytes)
    source.gltf["images"] = [{"name": "baked-image", "mimeType": "image/png",
                              "bufferView": image_view}]

    compacted = source.compact()
    assert compacted.gltf["images"][0]["bufferView"] != image_view
    compacted_image, _, _ = compacted._buffer_view_bytes(compacted.gltf["images"][0]["bufferView"])
    assert compacted_image == image_bytes

    reloaded = GlbDocument.from_bytes(_write_bytes(compacted))
    reloaded_image, _, _ = reloaded._buffer_view_bytes(reloaded.gltf["images"][0]["bufferView"])
    assert reloaded_image == image_bytes


def test_gltf_transform_error_table_counts_only_real_errors(monkeypatch: pytest.MonkeyPatch) -> None:
    from stalthos_toolkit.optimize.tools import ToolState

    def discover(name: str):
        if name == "gltf_validator":
            return ToolState(name, False, reason="not installed")
        if name == "gltf_transform":
            return ToolState(name, True, path="gltf-transform")
        return ToolState(name, False, reason="not installed")

    monkeypatch.setattr(glb_module.toolbox, "discover", discover)

    output = {"text": ""}

    def run_tool(*args, **kwargs):
        return subprocess.CompletedProcess(args[0], 0, stdout=output["text"], stderr="")

    monkeypatch.setattr(glb_module.toolbox, "run_tool", run_tool)
    output["text"] = "\n ERROR\n──────\ninfo: No errors found.\n\n WARN\nTANGENT_WARNING"
    clean_result = validate_with_tools("fixture.glb")
    assert clean_result["validated"] is True  # the validator ran; errors carries the result
    assert clean_result["errors"] == 0

    output["text"] = (
        "\n ERROR\n──────\n"
        "│ code │ message │ severity │ pointer │\n"
        "│ BUFFER_VIEW_TARGET_OVERRIDE │ bufferView target changed │ 0 │ /images/0/bufferView │\n"
    )
    error_result = validate_with_tools("fixture.glb")
    assert error_result["validated"] is True
    assert error_result["errors"] == 1
    assert "BUFFER_VIEW_TARGET_OVERRIDE" in error_result["issues"][0]


def test_baked_material_fixture_builds_repeatable_four_level_chain(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    test_root = Path(__file__).resolve().parents[2]
    main_root = next((parent for parent in Path(__file__).resolve().parents if (parent / "var" / "reports" / "baked-material-probe" / "softbough-baked.glb").is_file()), test_root)
    repo = (test_root if (test_root / "var" / "reports" / "baked-material-probe" /
                          "softbough-baked.glb").is_file() else main_root)
    fixture = repo / "var" / "reports" / "baked-material-probe" / "softbough-baked.glb"
    gltfpack_candidates = [base / "var" / "tools" / "gltfpack-1.3" / "gltfpack.exe"
                          for base in (test_root, main_root)]
    gltfpack_candidates.extend(base / "var" / "tools" / "gltfpack.exe"
                               for base in (test_root, main_root))
    gltfpack = next((path for path in gltfpack_candidates if path.is_file()), gltfpack_candidates[0])
    if not fixture.is_file() or not gltfpack.is_file():
        pytest.skip("local baked-material fixture or gltfpack is unavailable")
    monkeypatch.setenv("STEALTHOS_GLTFPACK", str(gltfpack))
    source = GlbDocument.read(fixture)
    source_summary = source.summary()
    source_materials = source.gltf.get("materials")
    source_images = source.gltf.get("images")
    source_textures = source.gltf.get("textures")
    source_image_payloads = _embedded_image_payloads(source)
    runs = []
    for label in ("first", "second"):
        result = build_lod_chain(fixture, tmp_path / label, asset_id="softbough", policy=PLANT_POLICY,
                                 prefer="gltfpack", allow_degraded=True)
        assert len(result.records) == 4
        assert all(a.triangles > b.triangles for a, b in zip(result.records, result.records[1:]))
        hashes = []
        for record in result.records:
            doc = GlbDocument.read(record.path)
            summary = doc.summary()
            assert summary.vertices > 0 and summary.triangles > 0
            assert summary.materials == source_summary.materials
            assert summary.images == source_summary.images
            assert summary.textures == source_summary.textures
            assert [m.get("name") for m in doc.gltf.get("materials", [])] == [
                m.get("name") for m in source_materials
            ]
            assert [i.get("name") for i in doc.gltf.get("images", [])] == [
                i.get("name") for i in source_images
            ]
            assert _material_image_bindings(doc) == _material_image_bindings(source)
            assert _embedded_image_payloads(doc) == source_image_payloads
            for _, _, primitive in doc.iter_primitives():
                assert 0 <= int(primitive.get("material", -1)) < summary.materials
                attributes = primitive.get("attributes") or {}
                if "TEXCOORD_0" in attributes:
                    # Decode every UV after the chain passes through gltfpack and
                    # the toolkit accessor path. Bounds failure raises GlbError.
                    assert len(doc.accessor_values(int(attributes["TEXCOORD_0"]))) > 0
            hashes.append(hashlib.sha256(Path(record.path).read_bytes()).hexdigest())
        runs.append(hashes)
    assert runs[0] == runs[1]


def _material_image_bindings(doc: GlbDocument) -> dict[str, dict[str, str]]:
    images = doc.gltf.get("images", [])
    textures = doc.gltf.get("textures", [])
    result: dict[str, dict[str, str]] = {}
    for material in doc.gltf.get("materials", []):
        pbr = material.get("pbrMetallicRoughness", {})
        slots = {
            "baseColor": pbr.get("baseColorTexture"),
            "metallicRoughness": pbr.get("metallicRoughnessTexture"),
            "normal": material.get("normalTexture"),
        }
        bindings: dict[str, str] = {}
        for slot, info in slots.items():
            if info is None:
                continue
            texture_index = int(info["index"])
            image_index = int(textures[texture_index]["source"])
            bindings[slot] = str(images[image_index].get("name"))
        result[str(material.get("name"))] = bindings
    return result


def _embedded_image_payloads(doc: GlbDocument) -> dict[str, bytes]:
    result = {}
    for image in doc.gltf.get("images", []):
        if "bufferView" in image:
            payload, _, _ = doc._buffer_view_bytes(int(image["bufferView"]))
            result[str(image.get("name"))] = payload
    return result
