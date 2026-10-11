"""Material/texture baking (bpy, headless).

Plan requirement, handled explicitly:

    "baking of multiscale structure; avoid baking directional lighting into albedo"

Implementation: for every bake target the object's material is temporarily
swapped for a **flat, emission-only** copy of the procedural structure
(``_materials.structure_emission_material``). Emission carries no shading from
any light rig, so the baked albedo image is *pure surface structure*. The scene
is additionally stripped of lights and its world is set to black for the whole
bake, so even an indirect path cannot inject directional light. Lighting is
never part of any baked channel; the albedo pass is structure only.

Passes written: albedo (structure), roughness, normal (geometry + structure
bump), thickness (Solidify thickness attribute, geometric).

Determinism: UVs are unwrapped with a fixed deterministic operator invocation
(smart project with fixed parameters), Cycles is pinned to a non-animated seed,
and every image is saved once with fixed settings.
"""

from __future__ import annotations

import os
from typing import Any

import bpy

import os as _os
import sys as _sys

# Blender does not put the --python script's directory on sys.path
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))

import _stalthos as S

S.bootstrap()

import _materials as MAT  # noqa: E402


PASSES = ("albedo", "roughness", "normal", "thickness")


def _set_object_materials(obj: Any, materials: list[Any], polygon_indices: list[int]) -> None:
    """Replace mesh slots and restore each polygon's authored slot index."""
    obj.data.materials.clear()
    for material in materials:
        obj.data.materials.append(material)
    for polygon, material_index in zip(obj.data.polygons, polygon_indices):
        polygon.material_index = material_index


def _ensure_uv(obj: bpy.types.Object, report: S.Reporter) -> str:
    if obj.data.uv_layers and obj.data.uv_layers[0].name:
        return "existing"
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    try:
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(angle_limit=1.15192, island_margin=0.02,
                                 area_weight=0.0, correct_aspect=True)
        bpy.ops.object.mode_set(mode="OBJECT")
        return "smart_project"
    except RuntimeError as exc:
        report.warn(f"smart_project failed on {obj.name}: {exc}")
        try:
            bpy.ops.object.mode_set(mode="OBJECT")
        except RuntimeError:
            pass
        return "failed"


def _ensure_thickness_attribute(obj: bpy.types.Object, reference: float,
                                report: S.Reporter) -> str:
    """Write a real per-vertex thickness estimate into a mesh attribute.

    Thickness is measured as the local surface curvature radius, approximated
    from the distance to the nearest non-adjacent vertex on the same object --
    a direct, deterministic geometric measurement (no randomness, no time).
    The attribute is kept in the saved master so downstream shading can use it.
    """
    import math as _math

    mesh = obj.data
    existing = mesh.attributes.get("stalthos_thickness")
    if existing is not None:
        return "existing"
    count = len(mesh.vertices)
    coords = [v.co for v in mesh.vertices]
    adj: list[set[int]] = [set() for _ in range(count)]
    for edge in mesh.edges:
        a, b = edge.vertices
        adj[a].add(b)
        adj[b].add(a)
    # bucket vertices on a coarse grid so the nearest-neighbour search is local
    cell = max(reference * 8.0, 1e-4)
    grid: dict[tuple[int, int, int], list[int]] = {}
    for index, co in enumerate(coords):
        key = (int(co.x // cell), int(co.y // cell), int(co.z // cell))
        grid.setdefault(key, []).append(index)

    values = []
    for index, co in enumerate(coords):
        base = (int(co.x // cell), int(co.y // cell), int(co.z // cell))
        best = None
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    for other in grid.get((base[0] + dx, base[1] + dy, base[2] + dz), ()):
                        if other == index or other in adj[index]:
                            continue
                        dist = (coords[other] - co).length
                        if dist > 1e-6 and (best is None or dist < best):
                            best = dist
        # a closed tube's half-thickness is roughly the chord between adjacent
        # rings, i.e. half the nearest-neighbour distance
        values.append(min((best or reference) * 0.5, reference * 4.0))

    attribute = mesh.attributes.new("stalthos_thickness", "FLOAT", "POINT")
    attribute.data.foreach_set("value", values)
    mesh.update()
    report.warn(f"{obj.name}: thickness measured as a nearest-neighbour chord "
                f"(reference {reference:.4f} m); it is a geometric estimate, "
                "not an authored wall thickness")
    return "computed"


def _strip_lighting(scene, report: S.Reporter) -> None:
    for obj in list(bpy.data.objects):
        if obj.type in {"LIGHT", "CAMERA"}:
            bpy.data.objects.remove(obj, do_unlink=True)
    world = scene.world or bpy.data.worlds.new("black")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg is not None:
        bg.inputs[0].default_value = (0.0, 0.0, 0.0, 1.0)
        bg.inputs[1].default_value = 0.0
    report.warn("lighting removed for the bake: albedo is structure-only by construction")


def _new_image(name: str, size: int, *, non_color: bool) -> bpy.types.Image:
    img = bpy.data.images.get(name)
    if img is None:
        img = bpy.data.images.new(name, width=size, height=size, alpha=True, float_buffer=False)
    img.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
    return img


def _activate_bake_target(mat: bpy.types.Material, img: bpy.types.Image) -> None:
    """Cycles needs an *active and selected* image texture node per material."""
    nt = mat.node_tree
    tex = None
    for node in nt.nodes:
        if node.type == "TEX_IMAGE":
            tex = node
            break
    if tex is None:
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.location = (100, -300)
    tex.image = img
    for node in nt.nodes:
        if hasattr(node, "select"):
            node.select = False
    tex.select = True
    nt.nodes.active = tex


def _authored_channel_material(source: Any, image: Any, channel: str) -> Any | None:
    """Copy an authored Principled graph and expose one authored PBR input as emission."""
    if source is None or not getattr(source, "use_nodes", False) or source.node_tree is None:
        return None
    mat = source.copy()
    mat.name = f"bake_{channel}_{source.name}"
    nodes = mat.node_tree.nodes
    output = next((node for node in nodes if node.type == "OUTPUT_MATERIAL"
                   and getattr(node, "is_active_output", True)), None)
    if output is None:
        return None
    surface_link = next((link for link in mat.node_tree.links
                         if link.to_node == output and link.to_socket.name == "Surface"), None)
    principled = (surface_link.from_node if surface_link is not None else None)
    if principled is None or principled.type != "BSDF_PRINCIPLED":
        return None
    socket_name = "Base Color" if channel == "albedo" else "Roughness"
    source_socket = principled.inputs.get(socket_name)
    if source_socket is None:
        return None
    emission = nodes.new("ShaderNodeEmission")
    emission.location = (output.location.x - 220, output.location.y)
    if source_socket.is_linked:
        mat.node_tree.links.new(source_socket.links[0].from_socket, emission.inputs["Color"])
    else:
        value = source_socket.default_value
        if channel == "roughness":
            scalar = float(value)
            emission.inputs["Color"].default_value = (scalar, scalar, scalar, 1.0)
        else:
            emission.inputs["Color"].default_value = value
    for link in list(mat.node_tree.links):
        if link.to_node == output and link.to_socket.name == "Surface":
            mat.node_tree.links.remove(link)
    mat.node_tree.links.new(emission.outputs["Emission"], output.inputs["Surface"])
    mat["stalthos_authored_channel"] = channel
    _activate_bake_target(mat, image)
    return mat
def _save(img: bpy.types.Image, path: str) -> str:
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    img.file_format = "PNG"
    img.filepath_raw = os.path.abspath(path)
    img.save()
    return os.path.abspath(path)


def _image_stats(img: bpy.types.Image) -> dict[str, Any]:
    """min/max of a baked image so a flat/empty pass is reported, not hidden."""
    try:
        import numpy as np

        buf = np.empty(len(img.pixels), dtype="float32")
        img.pixels.foreach_get(buf)
        rgb = buf.reshape(-1, 4)[:, :3]
        return {"min": round(float(rgb.min()), 5), "max": round(float(rgb.max()), 5),
                "mean": round(float(rgb.mean()), 5), "flat": bool(rgb.max() - rgb.min() < 1e-4)}
    except Exception as exc:  # pragma: no cover - numpy is bundled with Blender
        return {"error": str(exc)}


def bake(params: dict[str, Any], report: S.Reporter) -> dict[str, Any]:
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = int(params.get("bake_samples", 16))
    scene.cycles.seed = int(params.get("seed", 0)) & 0x7FFFFFFF
    scene.cycles.use_animated_seed = False       # deterministic sampling
    scene.cycles.use_denoising = False           # denoising would blur structure
    scene.cycles.max_bounces = 2
    scene.cycles.diffuse_bounces = 1
    scene.cycles.glossy_bounces = 0
    scene.cycles.transmission_bounces = 0
    scene.cycles.sample_clamp_indirect = 0.0

    objects = [o for o in bpy.data.objects if o.type == "MESH" and o.data.polygons
               and not o.hide_render]
    if not objects:
        raise RuntimeError("no renderable mesh objects to bake")
    report.warn("bake operating on a black world with no lights: no directional lighting "
                "can enter albedo (plan requirement)")

    size = int(params.get("resolution", 512))
    margin = int(params.get("margin", 6))
    thickness = float(params.get("leaf_thickness_m", 0.004))
    seed = int(params.get("seed", 0))
    preset = str(params.get("structure_preset", "bark"))
    scale_hint = float(params.get("scale_hint", 1.0))

    for obj in objects:
        with report.time(f"uv:{obj.name}"):
            obj["uv_source"] = _ensure_uv(obj, report)
        with report.time(f"thickness_attr:{obj.name}"):
            obj["thickness_source"] = _ensure_thickness_attribute(obj, thickness, report)

    written: list[dict[str, Any]] = []
    per_pass: dict[str, list[str]] = {}
    baked_images: dict[str, dict[str, bpy.types.Image]] = {obj.name: {} for obj in objects}
    originals = {
        obj.name: ([slot.material for slot in obj.material_slots],
                   [polygon.material_index for polygon in obj.data.polygons])
        for obj in objects
    }

    for pass_name in PASSES:
        if pass_name not in params.get("passes", list(PASSES)):
            continue
        images = []
        materials: dict[str, list[Any]] = {}
        normal_copies: list[bpy.types.Material] = []
        for index, obj in enumerate(objects):
            img = _new_image(f"bake_{pass_name}_{index}", size,
                             non_color=pass_name != "albedo")
            images.append(img)
            source_slots, _ = originals[obj.name]
            slots = source_slots or [None]
            slot_materials: list[Any] = []
            for slot_index, source_mat in enumerate(slots):
                if pass_name in {"albedo", "roughness"}:
                    mat = _authored_channel_material(source_mat, img, pass_name)
                    if mat is None:
                        report.warn(f"MATERIAL_CHANNEL_FALLBACK: {obj.name} slot {slot_index} "
                                    f"has no supported authored {pass_name} input; using "
                                    "explicit procedural fallback and marking material incomplete")
                        if pass_name == "albedo":
                            mat = MAT.structure_emission_material(
                                f"bake_albedo_fallback_{obj.name}_{slot_index}", preset,
                                seed=seed, scale_hint=scale_hint)
                        else:
                            mat = MAT.structure_roughness_material(
                                f"bake_rough_fallback_{obj.name}_{slot_index}", preset,
                                seed=seed, scale_hint=scale_hint)
                        obj["stalthos_authored_material_complete"] = False
                        _activate_bake_target(mat, img)
                    else:
                        mat["stalthos_material_fallback"] = False
                elif pass_name == "thickness":
                    mat = MAT.structure_thickness_material(f"bake_thick_{obj.name}_{slot_index}",
                                                           seed=seed, scale_hint=scale_hint)
                    _activate_bake_target(mat, img)
                else:
                    if source_mat is not None:
                        mat = source_mat.copy()
                        mat.name = f"bake_normal_{obj.name}_{slot_index}"
                        normal_copies.append(mat)
                    else:
                        report.warn(f"MATERIAL_CHANNEL_FALLBACK: {obj.name} slot {slot_index} "
                                    "has no authored material for normal baking")
                        mat = MAT.build_procedural_material(
                            f"normal_fallback_{obj.name}_{slot_index}", preset,
                            seed=seed, scale_hint=scale_hint)
                        obj["stalthos_authored_material_complete"] = False
                    _activate_bake_target(mat, img)
                slot_materials.append(mat)
            materials[obj.name] = slot_materials

        for obj in objects:
            _, polygon_indices = originals[obj.name]
            _set_object_materials(obj, materials[obj.name], polygon_indices)
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]

        with report.time(f"bake:{pass_name}"):
            try:
                if pass_name == "albedo":
                    # DIFFUSE of an emission-only shader = the structure colour
                    bpy.ops.object.bake(type="EMIT", margin=margin, use_clear=True)
                elif pass_name == "roughness":
                    bpy.ops.object.bake(type="EMIT", margin=margin, use_clear=True)
                elif pass_name == "thickness":
                    bpy.ops.object.bake(type="EMIT", margin=margin, use_clear=True)
                else:
                    bpy.ops.object.bake(type="NORMAL", margin=margin, use_clear=True)
            except RuntimeError as exc:
                report.warn(f"bake pass {pass_name} failed: {exc}")

        for index, obj in enumerate(objects):
            pass_name_ = pass_name
            path = os.path.join(params["outputs"]["textures"], f"{obj.name}_{pass_name_}.png")
            img = images[index]
            if img is not None:
                written.append({"path": _save(img, path), "role": "intermediate",
                                "kind": "texture", "pass": pass_name, "object": obj.name})
                per_pass.setdefault(pass_name, []).append(path)
                baked_images[obj.name][pass_name] = img
                stats = _image_stats(img)
                if stats.get("flat"):
                    report.warn(f"{obj.name}_{pass_name_}.png is flat (no variation captured); "
                                "the bake target needs a structure input")

        # Restore authored inputs after every pass. Later passes (especially
        # NORMAL) must never see an earlier pass's emission/material setup.
        for obj in objects:
            source_slots, polygon_indices = originals[obj.name]
            _set_object_materials(obj, source_slots, polygon_indices)
        for mat in normal_copies:
            if mat.users == 0:
                bpy.data.materials.remove(mat)

    # Bind baked maps only after every pass has finished, leaving authored
    # materials available as inputs throughout the full bake sequence.
    for obj in objects:
        maps = baked_images[obj.name]
        for pass_name in ("albedo", "roughness", "normal"):
            img = maps.get(pass_name)
            if img is not None:
                img.pack()
        source_slots, polygon_indices = originals[obj.name]
        baked_slots = []
        for slot_index, source_mat in enumerate(source_slots):
            name = (source_mat.name if source_mat is not None else f"{obj.name}_slot{slot_index}")
            baked_slots.append(_baked_pbr_material(name, maps))
        if not source_slots:
            baked_slots.append(_baked_pbr_material(f"{obj.name}_baked", maps))
        _set_object_materials(obj, baked_slots, polygon_indices)

    with report.time("save_blend"):
        blend_path = S.save_blend(params["outputs"]["blend"])
    written.append({"path": blend_path, "role": "source", "kind": "blend"})

    if not per_pass:
        raise RuntimeError("no bake pass produced an image")

    return {
        "metrics": {
            "baked_objects": [o.name for o in objects],
            "passes": sorted(per_pass.keys()),
            "resolution": size,
            "texture_count": len(written),
            "images_per_pass": {k: len(v) for k, v in per_pass.items()},
            "bake_type": "EMIT(structure-only) + NORMAL",
            "thickness_source": {o.name: o.get("thickness_source", "unknown") for o in objects},
            "thickness_method": ("per-vertex nearest-neighbour chord written to the "
                                 "stalthos_thickness mesh attribute (geometric estimate)"),
            "lighting_in_albedo": False,
            "material_export_ready": all(
                all(key in baked_images[obj.name] for key in ("albedo", "roughness", "normal"))
                and obj.get("stalthos_authored_material_complete", True)
                for obj in objects
            ),
            "authored_material_complete": all(
                obj.get("stalthos_authored_material_complete", True) for obj in objects
            ),
            "material_slots_preserved": True,
            "directional_light_baked": False,
            "denoising": False,
            "animated_seed": False,
            "determinism": {"seed": seed, "time_input_used": False},
            "note": "albedo/roughness/thickness come from flat emission shaders with all "
                    "lights removed, so no directional lighting can be baked in",
        },
        "artifacts": written,
        "object_names": [o.name for o in objects],
    }


def _baked_pbr_material(name: str, images: dict[str, bpy.types.Image]) -> bpy.types.Material:
    """Build one exportable glTF PBR material from an object's baked channels."""
    mat = bpy.data.materials.new(f"{name}_baked")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    output.location = (500, 0)
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.location = (200, 0)
    mat.node_tree.links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    def texture(channel: str, color_space: str, y: int) -> bpy.types.ShaderNodeTexImage | None:
        image = images.get(channel)
        if image is None:
            return None
        image.colorspace_settings.name = color_space
        image.pack()
        node = nodes.new("ShaderNodeTexImage")
        node.name = f"StalthosBaked_{channel}"
        node.label = f"Baked {channel}"
        node.image = image
        node.location = (-300, y)
        mat[f"stalthos_baked_{channel}"] = (os.path.splitext(os.path.basename(getattr(image, "filepath", "")))[0]
                                               or image.name)
        return node

    albedo = texture("albedo", "sRGB", 300)
    roughness = texture("roughness", "Non-Color", 0)
    normal = texture("normal", "Non-Color", -300)
    if albedo:
        mat.node_tree.links.new(albedo.outputs["Color"], shader.inputs["Base Color"])
    if roughness:
        mat.node_tree.links.new(roughness.outputs["Color"], shader.inputs["Roughness"])
    if normal:
        normal_node = nodes.new("ShaderNodeNormalMap")
        normal_node.location = (0, -300)
        mat.node_tree.links.new(normal.outputs["Color"], normal_node.inputs["Color"])
        mat.node_tree.links.new(normal_node.outputs["Normal"], shader.inputs["Normal"])
    return mat


def main(params: dict[str, Any], report: S.Reporter) -> dict[str, Any]:
    S.reset_scene()
    source = params.get("source_blend")
    if source:
        bpy.ops.wm.open_mainfile(filepath=source, load_ui=False)
    with report.time("strip_lighting"):
        _strip_lighting(bpy.context.scene, report)
    return bake(params, report)


if __name__ == "__main__":
    S.run_main("bake_materials", main)
