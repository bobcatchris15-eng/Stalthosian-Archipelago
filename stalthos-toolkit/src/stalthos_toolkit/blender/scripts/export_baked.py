"""Export a baked Blender master and verify its per-material glTF bindings.

This script is Blender-hosted, but the GLB binding validator below uses only
plain Python data so it can be exercised by the offline test suite.
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path
from typing import Any


CHANNEL_KEYS = {
    "albedo": ("pbrMetallicRoughness", "baseColorTexture"),
    "roughness": ("pbrMetallicRoughness", "metallicRoughnessTexture"),
    "normal": ("normalTexture",),
}


def validate_material_bindings(gltf: dict[str, Any],
                               expected: dict[str, dict[str, str]]) -> dict[str, Any]:
    """Check each material's PBR channels resolve to its intended baked image."""
    materials = gltf.get("materials") or []
    images = gltf.get("images") or []
    textures = gltf.get("textures") or []
    material_by_name = {str(item.get("name", "")): item for item in materials}
    image_names = [str(item.get("name", "")) for item in images]
    issues: list[str] = []
    checked = 0

    for material_name, channels in expected.items():
        material = material_by_name.get(material_name)
        if material is None:
            issues.append(f"missing exported material {material_name}")
            continue
        for channel, image_name in channels.items():
            route = CHANNEL_KEYS.get(channel)
            if route is None:
                issues.append(f"unsupported baked channel {channel} on {material_name}")
                continue
            value: Any = material
            for key in route:
                value = value.get(key, {}) if isinstance(value, dict) else {}
            if not isinstance(value, dict) or "index" not in value:
                issues.append(f"{material_name} has no exported {channel} texture")
                continue
            texture_index = value["index"]
            if not isinstance(texture_index, int) or not 0 <= texture_index < len(textures):
                issues.append(f"{material_name} has invalid {channel} texture index")
                continue
            image_index = textures[texture_index].get("source")
            if not isinstance(image_index, int) or not 0 <= image_index < len(images):
                issues.append(f"{material_name} {channel} texture has no embedded image")
                continue
            actual = image_names[image_index]
            if actual != image_name:
                issues.append(f"{material_name} {channel} points to {actual!r}, expected {image_name!r}")
                continue
            checked += 1

    material_count = len(materials)
    for mesh in gltf.get("meshes") or []:
        for primitive in mesh.get("primitives") or []:
            if "material" in primitive:
                index = primitive["material"]
                if not isinstance(index, int) or not 0 <= index < material_count:
                    issues.append("primitive references an invalid material index")
                elif "TEXCOORD_0" not in (primitive.get("attributes") or {}):
                    issues.append(f"primitive using material {index} has no TEXCOORD_0 UVs")
    return {"ok": not issues, "materials_checked": len(expected),
            "texture_bindings_checked": checked, "issues": issues}


def read_glb_json(path: str | Path) -> dict[str, Any]:
    """Read the JSON chunk from a GLB 2.0 file."""
    data = Path(path).read_bytes()
    if len(data) < 20:
        raise ValueError("GLB is truncated")
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2 or length != len(data):
        raise ValueError("invalid GLB 2.0 header")
    chunk_length, chunk_type = struct.unpack_from("<II", data, 12)
    if chunk_type != 0x4E4F534A or 20 + chunk_length > len(data):
        raise ValueError("GLB has no valid JSON chunk")
    return json.loads(data[20:20 + chunk_length].decode("utf-8").rstrip(" \t\r\n\0"))


def main() -> None:
    import bpy

    args = sys.argv[sys.argv.index("--") + 1:]
    params = json.loads(Path(args[args.index("--params") + 1]).read_text(encoding="utf-8"))
    bpy.ops.wm.open_mainfile(filepath=params["source_blend"], load_ui=False)
    expected: dict[str, dict[str, str]] = {}
    for material in bpy.data.materials:
        channels = {channel: str(material.get(f"stalthos_baked_{channel}"))
                    for channel in CHANNEL_KEYS
                    if material.get(f"stalthos_baked_{channel}")}
        if channels:
            expected[material.name] = channels
    if not expected:
        raise RuntimeError("baked blend has no image-backed Stalthos materials")
    bpy.context.scene.frame_set(1)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        if obj.type in {"MESH", "ARMATURE", "EMPTY"}:
            obj.select_set(True)
    result = bpy.ops.export_scene.gltf(filepath=params["output_glb"], export_format="GLB",
                                       use_selection=True, export_yup=True,
                                       export_animations=True)
    if "FINISHED" not in result:
        raise RuntimeError("Blender glTF exporter did not finish")
    check = validate_material_bindings(read_glb_json(params["output_glb"]), expected)
    if not check["ok"]:
        raise RuntimeError("baked GLB binding check failed: " + "; ".join(check["issues"]))
    payload = {"ok": True, "output_glb": params["output_glb"],
               "blender_version": bpy.app.version_string,
               "material_bindings": check}
    Path(params.get("result_json", "result.json")).write_text(
        json.dumps(payload, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
