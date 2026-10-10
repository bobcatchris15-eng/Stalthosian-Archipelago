"""Deterministic review renders (bpy, headless).

Used by both ``render_turntable`` and ``render_growth_stages``.

Determinism measures (all of them explicit, none time-based):
  * the camera is placed from the asset bounding box at *fixed* azimuths
    (0/90/180/270) with a fixed elevation and a fixed fit distance formula --
    no auto-framing that could depend on render order;
  * lighting presets are fixed rigs: a fixed sun angle, fixed world background
    colour, fixed strength. No HDRI rotation, no sky time-of-day, no animated
    sun rotation, no random jitter;
  * EEVEE: fixed TAA samples, jitter off. Cycles: fixed seed with
    ``use_animated_seed = False`` and deterministic denoising;
  * the scene is pinned to frame 1;
  * colour management uses a fixed view transform with fixed exposure/gamma, so
    two runs of the same asset produce byte-identical PNGs.
"""

from __future__ import annotations

import math
import os
from typing import Any

import bpy
import mathutils

import os as _os
import sys as _sys

# Blender does not put the --python script's directory on sys.path
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))

import _stalthos as S

S.bootstrap()

#: Fixed lighting presets. `sun_azimuth`/`sun_elevation` are literal degrees.
PRESETS: dict[str, dict[str, Any]] = {
    "neutral": {
        "world": (0.055, 0.058, 0.062, 1.0), "world_strength": 1.0,
        "sun_energy": 3.2, "sun_azimuth": 135.0, "sun_elevation": 42.0,
        "sun_angle_deg": 4.0, "sun_color": (1.0, 0.98, 0.95),
        "fill_energy": 0.9, "rim_energy": 0.6,
    },
    "sun": {
        "world": (0.10, 0.16, 0.26, 1.0), "world_strength": 1.1,
        "sun_energy": 5.5, "sun_azimuth": 118.0, "sun_elevation": 26.0,
        "sun_angle_deg": 0.6, "sun_color": (1.0, 0.95, 0.86),
        "fill_energy": 0.3, "rim_energy": 0.2,
    },
    "overcast": {
        "world": (0.32, 0.35, 0.38, 1.0), "world_strength": 2.4,
        "sun_energy": 0.8, "sun_azimuth": 200.0, "sun_elevation": 60.0,
        "sun_angle_deg": 25.0, "sun_color": (0.92, 0.94, 0.98),
        "fill_energy": 0.4, "rim_energy": 0.2,
    },
    "wet": {
        "world": (0.09, 0.11, 0.14, 1.0), "world_strength": 1.0,
        "sun_energy": 2.4, "sun_azimuth": 150.0, "sun_elevation": 20.0,
        "sun_angle_deg": 8.0, "sun_color": (0.85, 0.9, 1.0),
        "fill_energy": 0.5, "rim_energy": 1.1,
    },
}


def setup_world(scene, preset: dict[str, Any]) -> None:
    world = scene.world or bpy.data.worlds.new("review_world")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs[0].default_value = preset["world"]
    bg.inputs[1].default_value = float(preset["world_strength"])


def setup_lights(scene, preset: dict[str, Any], seed: int) -> list[bpy.types.Object]:
    made = []
    for name, energy, offset in (("key_sun", preset["sun_energy"], 0.0),
                                 ("fill_sun", preset["fill_energy"], 70.0),
                                 ("rim_sun", preset["rim_energy"], 200.0)):
        data = bpy.data.lights.new(name, type="SUN")
        data.energy = float(energy)
        data.angle = math.radians(float(preset["sun_angle_deg"]))
        data.color = preset["sun_color"]
        if name == "key_sun":
            data.angle = math.radians(float(preset["sun_angle_deg"]))
        obj = bpy.data.objects.new(name, data)
        scene.collection.objects.link(obj)
        az = math.radians(float(preset["sun_azimuth"]) + offset)
        el = math.radians(float(preset["sun_elevation"]))
        obj.rotation_euler = (math.radians(90.0) - el, 0.0, az)
        made.append(obj)
    return made


def setup_camera(scene, objects, *, azimuth_deg: float, elevation_deg: float,
                 fit: float = 1.35) -> bpy.types.Object:
    """Fixed-framing camera: bounding-box fit with a constant formula."""
    if not objects:
        raise RuntimeError("nothing to frame")
    deps = bpy.context.evaluated_depsgraph_get()
    mins = mathutils.Vector((math.inf,) * 3)
    maxs = mathutils.Vector((-math.inf,) * 3)
    for obj in objects:
        if obj.type != "MESH":
            continue
        for corner in obj.bound_box:
            world = obj.matrix_world @ mathutils.Vector(corner)
            for i in range(3):
                mins[i] = min(mins[i], world[i])
                maxs[i] = max(maxs[i], world[i])
    if mins.x == math.inf:
        raise RuntimeError("no mesh objects to frame")
    centre = (mins + maxs) * 0.5
    extent = max((maxs - mins).length, 1e-3)

    data = bpy.data.cameras.new("review_cam")
    data.lens = float(fit) * 0.0 + 55.0
    cam = bpy.data.objects.new("review_cam", data)
    scene.collection.objects.link(cam)
    az = math.radians(azimuth_deg)
    el = math.radians(elevation_deg)
    distance = extent * 1.45
    direction = mathutils.Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az),
                                  math.sin(el)))
    cam.location = centre + direction * distance
    forward = (centre - cam.location).normalized()
    cam.rotation_euler = forward.to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    return cam


def _ground(scene, preset: dict[str, Any], extent_hint: float) -> bpy.types.Object | None:
    """A simple contact-shadow catcher so the asset reads as standing on ground."""
    if not bool(preset.get("ground", True)):
        return None
    mesh = bpy.data.meshes.new("ground")
    size = extent_hint
    verts = [(-size, -size, 0.0), (size, -size, 0.0), (size, size, 0.0), (-size, size, 0.0)]
    mesh.from_pydata(verts, [], [(0, 1, 2, 3)])
    mesh.update()
    obj = bpy.data.objects.new("ground", mesh)
    scene.collection.objects.link(obj)
    mat = bpy.data.materials.new("ground_neutral")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = (0.09, 0.095, 0.085, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.85
    obj.data.materials.append(mat)
    return obj


def render_one(scene, path: str, *, engine_id: str) -> str:
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    scene.render.filepath = os.path.abspath(path)
    bpy.ops.render.render(write_still=True)
    return os.path.abspath(path)


def render_sheet(params: dict[str, Any], report: S.Reporter) -> dict[str, Any]:
    scene = bpy.context.scene
    settings = dict(params.get("render", {}))
    presets = params.get("presets") or [settings.get("preset", "neutral")]
    angles = list(params.get("angles") or settings.get("turntable_angles") or [0, 90, 180, 270])
    elevation = float(params.get("elevation_deg", 12.0))
    resolution = int(params.get("resolution", settings.get("resolution", 512)))
    samples = int(params.get("samples", settings.get("samples", 32)))
    seed = int(params.get("seed", 0))
    out_dir = params["outputs"]["images"]
    stem = params.get("stem", "review")

    engine_id = S.configure_scene(scene, engine=params.get("engine", "EEVEE"),
                                  seed=seed, resolution=resolution, samples=samples)
    scene.view_settings.view_transform = str(settings.get("view_transform", "Standard"))
    scene.view_settings.exposure = float(settings.get("film_exposure", 0.0))
    scene.view_settings.gamma = 1.0
    scene.view_settings.look = "None"

    subjects = [o for o in bpy.data.objects if o.type == "MESH" and not o.hide_render]
    if not subjects:
        raise RuntimeError("no renderable objects in the scene")

    mins = mathutils.Vector((math.inf,) * 3)
    maxs = mathutils.Vector((-math.inf,) * 3)
    for obj in subjects:
        for corner in obj.bound_box:
            world = obj.matrix_world @ mathutils.Vector(corner)
            for i in range(3):
                mins[i] = min(mins[i], world[i])
                maxs[i] = max(maxs[i], world[i])
    extent = max((maxs - mins).length, 1e-3)

    written: list[dict[str, Any]] = []
    for preset_name in presets:
        spec = PRESETS.get(preset_name, PRESETS["neutral"])
        with report.time(f"setup:{preset_name}"):
            for obj in list(bpy.data.objects):
                if obj.type in {"LIGHT", "CAMERA", "EMPTY"}:
                    bpy.data.objects.remove(obj, do_unlink=True)
            setup_world(scene, spec)
            setup_lights(scene, spec, seed)
            ground = _ground(scene, {**spec, "ground": bool(params.get("ground", True))},
                             extent * 3.0)
            if ground is not None:
                # sit the ground plane at the asset's lowest point so contact reads
                ground.location = ((mins.x + maxs.x) * 0.5, (mins.y + maxs.y) * 0.5,
                                   mins.z - extent * 0.005)
        for angle in angles:
            with report.time(f"render:{preset_name}:{angle}"):
                cam = setup_camera(scene, subjects, azimuth_deg=float(angle),
                                   elevation_deg=elevation)
                name = f"{stem}_{preset_name}_{int(angle):03d}.png"
                path = render_one(scene, os.path.join(out_dir, name), engine_id=engine_id)
                written.append({"path": path, "role": "preview", "kind": "render",
                                "preset": preset_name, "angle": angle})
                bpy.data.objects.remove(cam, do_unlink=True)
    return written


def main(params: dict[str, Any], report: S.Reporter) -> dict[str, Any]:
    S.reset_scene()
    source = params.get("source_blend")
    if source:
        bpy.ops.wm.open_mainfile(filepath=source, load_ui=False)
    scene = bpy.context.scene
    scene.frame_set(1)
    with report.time("render_sheet"):
        written = render_sheet(params, report)
    metrics = {
        "engine": scene.render.engine,
        "renders": len(written),
        "resolution": int(params.get("resolution", params.get("render", {}).get("resolution", 512))),
        "samples": int(params.get("samples", params.get("render", {}).get("samples", 32))),
        "angles": list(params.get("angles", [0, 90, 180, 270])),
        "presets": list(params.get("presets", ["neutral"])),
        "view_transform": scene.view_settings.view_transform,
        "determinism": {
            "animated_seed": False,
            "time_input_used": False,
            "camera": "fixed azimuth/elevation + bounding-box fit",
            "world": "fixed background colour, no HDRI/time-of-day",
        },
    }
    return {"metrics": metrics, "artifacts": written}


if __name__ == "__main__":
    S.run_main("render", main)
