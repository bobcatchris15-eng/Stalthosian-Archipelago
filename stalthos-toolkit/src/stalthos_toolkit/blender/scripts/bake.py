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

    for pass_name in PASSES:
        if pass_name not in params.get("passes", list(PASSES)):
            continue
        images = []
        materials = {}
        normal_copies: list[bpy.types.Material] = []
        for index, obj in enumerate(objects):
            img = _new_image(f"bake_{pass_name}_{index}", size,
                             non_color=pass_name != "albedo")
            if pass_name == "albedo":
                mat = MAT.structure_emission_material(f"bake_albedo_{obj.name}", preset,
                                                       seed=seed, scale_hint=scale_hint)
            elif pass_name == "roughness":
                mat = MAT.structure_roughness_material(f"bake_rough_{obj.name}", preset,
                                                       seed=seed, scale_hint=scale_hint)
            elif pass_name == "thickness":
                mat = MAT.structure_thickness_material(f"bake_thick_{obj.name}",
                                                        seed=seed, scale_hint=scale_hint)
            else:
                # normal pass: a per-object *copy* of the authored material.
                # Sharing one material between objects would make the second
                # object's active image node overwrite the first's, so the
                # first object would bake into an empty image.
                source_mat = obj.data.materials[0] if obj.data.materials else None
                if source_mat is not None:
                    mat = source_mat.copy()
                    mat.name = f"bake_normal_{obj.name}"
                    normal_copies.append(mat)
                else:
                    mat = MAT.build_procedural_material(f"normal_{obj.name}", preset,
                                                        seed=seed, scale_hint=scale_hint)
            materials[obj.name] = mat
            _activate_bake_target(mat, img)
            images.append(img)

        originals = {}
        for obj in objects:
            originals[obj.name] = [s.material for s in obj.material_slots]
            obj.data.materials.clear()
            obj.data.materials.append(materials[obj.name])

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

        for obj in objects:
            pass_name_ = pass_name
            path = os.path.join(params["outputs"]["textures"], f"{obj.name}_{pass_name_}.png")
            img = next((i for i, o in zip(images, objects) if o.name == obj.name), None)
            if img is not None:
                written.append({"path": _save(img, path), "role": "intermediate",
                                "kind": "texture", "pass": pass_name, "object": obj.name})
                per_pass.setdefault(pass_name, []).append(path)
                stats = _image_stats(img)
                if stats.get("flat"):
                    report.warn(f"{obj.name}_{pass_name_}.png is flat (no variation captured); "
                                "the bake target needs a structure input")

        # restore the authored material (masters stay editable)
        for obj in objects:
            obj.data.materials.clear()
            for mat in originals[obj.name]:
                if mat is not None:
                    obj.data.materials.append(mat)
        for mat in normal_copies:
            if mat.users == 0:
                bpy.data.materials.remove(mat)

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
