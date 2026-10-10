"""Shared bpy runtime for the Stalthos headless scripts.

This module is imported by every script in ``blender/scripts`` and is executed
*inside* Blender. Two hard rules it enforces:

1. **The result never goes to stdout.** Everything is returned through a
   ``result.json`` written next to the params file that Python handed us.
2. **Determinism.** Every stochastic decision comes from ``random.Random``
   instances seeded by seeds that Python derived (``core.derive_seed``, whose
   algorithm is shared verbatim through ``blender/seeds.py``). Time-based
   behaviour, animated Cycles seeds and randomized render jitter are all
   disabled. The scene is built at frame 1 with a fixed frame-1 state, so no
   ``#frame``-driven node ever sees a different value between runs.
"""

from __future__ import annotations

import json
import math
import os
import random
import sys
import time
from typing import Any, Iterable, Sequence

import bpy
import mathutils

MASK = (1 << 63) - 1

#: engine identifiers, resolved per Blender version
EEVEE_ENGINES = ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE")


# --------------------------------------------------------------------------- #
# params / result handoff
# --------------------------------------------------------------------------- #
def script_dir() -> str:
    return os.path.dirname(os.path.abspath(__file__))


def bootstrap() -> None:
    """Make sibling helper modules importable when run via ``--python``."""
    here = script_dir()
    if here not in sys.path:
        sys.path.insert(0, here)


def parse_params(argv: Sequence[str] | None = None) -> dict[str, Any]:
    """Read ``--params <path>`` from the post-``--`` argument list."""
    args = list(sys.argv if argv is None else argv)
    if "--" in args:
        args = args[args.index("--") + 1:]
    params_path = None
    i = 0
    while i < len(args):
        if args[i] == "--params" and i + 1 < len(args):
            params_path = args[i + 1]
            break
        if args[i].startswith("--params="):
            params_path = args[i].split("=", 1)[1]
            break
        i += 1
    if params_path is None:
        params_path = os.environ.get("STALTHOS_PARAMS", "")
    if not params_path or not os.path.isfile(params_path):
        raise SystemExit("stalthos: --params <file.json> is required")
    with open(params_path, "r", encoding="utf-8") as fh:
        params = json.load(fh)
    params["_params_dir"] = os.path.dirname(os.path.abspath(params_path))
    return params


def result_path(params: dict[str, Any], name: str = "result.json") -> str:
    """Result always lands next to params.json, never in the source tree."""
    out = params.get("result_path")
    if out:
        parent = os.path.dirname(os.path.abspath(out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        return out
    return os.path.join(params.get("_params_dir", os.getcwd()), name)


def write_result(params: dict[str, Any], result: dict[str, Any]) -> None:
    """Write the result JSON atomically. Never touches stdout."""
    path = result_path(params)
    tmp = path + ".partial"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2, default=str, sort_keys=False)
    os.replace(tmp, path)


class Reporter:
    """Collects timings and warnings for the result payload."""

    def __init__(self) -> None:
        self.timings: dict[str, float] = {}
        self.warnings: list[str] = []
        self.started = time.time()

    def time(self, key: str):
        reporter = self

        class _Ctx:
            def __enter__(self):
                self.t0 = time.time()
                return self

            def __exit__(self, *exc):
                reporter.timings[key] = round(time.time() - self.t0, 4)
                return False

        return _Ctx()

    def warn(self, message: str) -> None:
        if message not in self.warnings:
            self.warnings.append(message)


# --------------------------------------------------------------------------- #
# deterministic seeding
# --------------------------------------------------------------------------- #
# The algorithm lives in one pure-stdlib module (``blender/seeds.py``) that this
# directory re-exports; nothing here re-implements it, so the Python side and the
# bpy side cannot drift apart. A test asserts they agree.
def _load_seeds():
    here = script_dir()
    package = os.path.dirname(here)                     # .../stalthos_toolkit/blender
    for candidate in (here, os.path.dirname(package)):  # scripts dir, then src
        if candidate not in sys.path:
            sys.path.insert(0, candidate)
    try:
        from stalthos_toolkit.blender import seeds as module  # normal import
    except ImportError:  # pragma: no cover - Blender without the package on sys.path
        import importlib.util

        spec = importlib.util.spec_from_file_location(
            "stalthos_blender_seeds", os.path.join(package, "seeds.py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    return module


_SEEDS = _load_seeds()

normalize_seed = _SEEDS.normalize_seed
derive_seed = _SEEDS.derive_seed
rng = _SEEDS.rng
MASK = _SEEDS.MASK


# --------------------------------------------------------------------------- #
# scene hygiene
# --------------------------------------------------------------------------- #
def reset_scene() -> None:
    """Factory-empty scene with autosave/crash files kept out of the tree."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = 1
    scene.frame_current = 1
    scene.frame_set(1)
    # nothing in this pipeline is animated; a single fixed frame is the contract
    scene.render.fps = 24
    if hasattr(scene.render, "use_motion_blur"):
        scene.render.use_motion_blur = False
    if hasattr(scene, "eevee") and hasattr(scene.eevee, "use_motion_blur"):
        scene.eevee.use_motion_blur = False
    # Blender 5.x renamed some of these; guard each so 4.3+ and 5.x both work.
    prefs_edit = bpy.context.preferences.edit
    for attr, value in (("use_keyframe_insert_auto", False), ("use_auto_keying", False),
                        ("use_auto_keyframe_insert_needed", False)):
        if hasattr(prefs_edit, attr):
            try:
                setattr(prefs_edit, attr, value)
            except (AttributeError, TypeError):
                pass
    paths = bpy.context.preferences.filepaths
    if hasattr(paths, "use_auto_save_temporary_files"):
        paths.use_auto_save_temporary_files = False
    paths.save_version = 0
    if hasattr(paths, "temporary_directory"):
        try:
            paths.temporary_directory = scratch_dir()
        except (AttributeError, TypeError):
            pass


def scratch_dir() -> str:
    """Blender's scratch directory: the per-run temp dir, never the source tree."""
    import tempfile

    return os.environ.get("STALTHOS_TMP") or tempfile.gettempdir()


def pick_engine(requested: str) -> str:
    """Resolve an engine id across Blender 4.x/5.x naming."""
    available = {i.identifier for i in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items}
    wanted = (requested or "EEVEE").upper()
    if wanted in {"EEVEE", "BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"}:
        for candidate in EEVEE_ENGINES:
            if candidate in available:
                return candidate
        return "BLENDER_WORKBENCH" if "BLENDER_WORKBENCH" in available else "CYCLES"
    if wanted == "CYCLES":
        return "CYCLES" if "CYCLES" in available else "BLENDER_EEVEE"
    return wanted if wanted in available else "CYCLES"


def configure_scene(
    scene,
    *,
    engine: str = "EEVEE",
    seed: int = 0,
    resolution: int = 512,
    samples: int = 32,
    transparent: bool = False,
) -> str:
    """Configure a render with zero stochastic/time-based variance.

    Determinism measures applied here (documented in docs/toolkit-pipeline.md):
      * engine is resolved to an explicit id, never a per-version guess;
      * Cycles: ``use_animated_seed = False`` and ``seed`` pinned to the seed the
        Python side derived, so sampling noise is identical run to run;
      * denoising is deterministic (OpenImageDenoise, no temporal component);
      * EEVEE: fixed TAA sample count, no jitter-seed randomness;
      * the world is an explicit fixed background colour (no HDRI rotation, no
        time-of-day driver) and the sun is a fixed-angle object with no
        animated rotation;
      * the scene is pinned to frame 1 so ``#frame`` driven nodes are constant;
      * motion blur is off.
    """
    engine_id = pick_engine(engine)
    scene.render.engine = engine_id
    scene.render.resolution_x = int(resolution)
    scene.render.resolution_y = int(resolution)
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = bool(transparent)
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA" if transparent else "RGB"
    scene.render.image_settings.color_depth = "8"
    scene.render.use_file_extension = True
    scene.frame_set(1)

    if engine_id == "CYCLES":
        cy = scene.cycles
        cy.device = "CPU"
        cy.samples = int(samples)
        cy.preview_samples = 1
        cy.use_adaptive_sampling = True
        cy.adaptive_threshold = 0.01
        cy.use_denoising = True
        cy.denoiser = "OPENIMAGEDENOISE"
        cy.denoising_use_gpu = False
        cy.seed = int(seed) & 0x7FFFFFFF
        cy.use_animated_seed = False          # <- no per-frame seed churn
        cy.max_bounces = 6
        cy.diffuse_bounces = 3
        cy.glossy_bounces = 2
        cy.transmission_bounces = 2
        cy.transparent_max_bounces = 4
        cy.caustics_reflective = False
        cy.caustics_refractive = False
        cy.blur_glossy = 1.0
        cy.sample_clamp_indirect = 3.0
        cy.use_light_tree = True
    else:
        ee = scene.eevee
        if hasattr(ee, "taa_render_samples"):
            ee.taa_render_samples = int(samples)
        for attr, value in (("use_gtao", True), ("use_bloom", False),
                            ("use_motion_blur", False),
                            ("use_volumetric_lights", False), ("use_ssr", False),
                            ("use_shadows", True),
                            ("use_shadow_high_bitdepth", True)):
            if hasattr(ee, attr):
                try:
                    setattr(ee, attr, value)
                except (AttributeError, TypeError, ValueError) as exc:
                    # Blender 5.x dropped/renamed some EEVEE switches; a missing
                    # switch must not fail the render, but it is worth reporting
                    print(f"stalthos: eevee setting {attr} unavailable: {exc}",
                          file=sys.stderr)
        for attr, value in (("shadow_ray_count", 2), ("shadow_step_count", 6),
                            ("shadow_resolution_scale", 1.0)):
            if hasattr(ee, attr):
                try:
                    setattr(ee, attr, value)
                except (AttributeError, TypeError, ValueError):
                    pass
    return engine_id


# --------------------------------------------------------------------------- #
# deterministic geometry construction (no operators, no global random)
# --------------------------------------------------------------------------- #
class MeshBuilder:
    """Accumulates verts/faces so a whole plant is one deterministic mesh."""

    def __init__(self) -> None:
        self.verts: list[tuple[float, float, float]] = []
        self.faces: list[tuple[int, ...]] = []
        self.smooth: list[bool] = []

    # -- low level ---------------------------------------------------------
    def add_vertex(self, co: Sequence[float]) -> int:
        self.verts.append((float(co[0]), float(co[1]), float(co[2])))
        return len(self.verts) - 1

    def add_face(self, indices: Sequence[int], *, smooth: bool = True) -> None:
        if len(indices) >= 3:
            self.faces.append(tuple(indices))
            self.smooth.append(smooth)

    # -- primitives --------------------------------------------------------
    def add_tube(
        self,
        path: Sequence[Sequence[float]],
        radii: Sequence[float],
        *,
        sides: int = 8,
        cap: bool = True,
        smooth: bool = True,
    ) -> list[int]:
        """Sweep a circular profile along ``path`` with parallel-transport frames."""
        rings: list[list[int]] = []
        points = [mathutils.Vector(p) for p in path]
        n = len(points)
        if n < 2:
            return []
        # initial frame
        tangent = (points[1] - points[0])
        if tangent.length < 1e-9:
            tangent = mathutils.Vector((0.0, 0.0, 1.0))
        tangent.normalize()
        ref = mathutils.Vector((0.0, 0.0, 1.0))
        if abs(tangent.dot(ref)) > 0.95:
            ref = mathutils.Vector((1.0, 0.0, 0.0))
        normal = (ref - tangent * ref.dot(tangent)).normalized()
        for i in range(n):
            if i > 0:
                tangent = (points[i] - points[i - 1])
                if tangent.length < 1e-9:
                    tangent = mathutils.Vector((0.0, 0.0, 1.0))
                tangent.normalize()
                # parallel transport: keep the frame from twisting unpredictably
                normal = normal - tangent * normal.dot(tangent)
                if normal.length < 1e-9:
                    normal = mathutils.Vector((0.0, 0.0, 1.0))
                normal.normalize()
            binormal = tangent.cross(normal)
            radius = max(float(radii[i]), 1e-5)
            ring: list[int] = []
            for s in range(sides):
                angle = 2.0 * math.pi * s / sides
                offset = normal * (math.cos(angle) * radius) + binormal * (math.sin(angle) * radius)
                ring.append(self.add_vertex(points[i] + offset))
            rings.append(ring)
        for i in range(n - 1):
            a, b = rings[i], rings[i + 1]
            for s in range(sides):
                s2 = (s + 1) % sides
                self.add_face((a[s], a[s2], b[s2], b[s]), smooth=smooth)
        if cap:
            self.add_face(tuple(reversed(rings[0])), smooth=False)
            self.add_face(tuple(rings[-1]), smooth=False)
        return rings

    def add_blade(
        self,
        origin: Sequence[float],
        direction: Sequence[float],
        *,
        length: float,
        width: float,
        segments: int = 5,
        curve: float = 0.35,
        droop: float = 0.25,
        width_power: float = 0.75,
        tip_twist: float = 0.0,
        thickness: float = 0.0,
        smooth: bool = False,
    ) -> list[int]:
        """A tapered leaf/frond/pinna ribbon: base -> tip, width ~ sin(pi*t)^p."""
        base = mathutils.Vector(origin)
        fwd = mathutils.Vector(direction)
        if fwd.length < 1e-9:
            fwd = mathutils.Vector((0.0, 0.0, 1.0))
        fwd.normalize()
        ref = mathutils.Vector((0.0, 0.0, 1.0))
        if abs(fwd.dot(ref)) > 0.95:
            ref = mathutils.Vector((1.0, 0.0, 0.0))
        side = fwd.cross(ref)
        if side.length < 1e-9:
            side = mathutils.Vector((1.0, 0.0, 0.0))
        side.normalize()
        up = side.cross(fwd).normalized()

        rows: list[list[int]] = []
        centre: list[mathutils.Vector] = []
        for i in range(segments + 1):
            t = i / segments
            # droop: gravity bend grows with t^2 so the base stays attached
            bend = droop * curve * length * t * t
            pos = base + fwd * (length * t) - mathutils.Vector((0.0, 0.0, bend))
            twist = tip_twist * t * t
            local_side = (side * math.cos(twist) + up * math.sin(twist)).normalized()
            half = 0.5 * width * (math.sin(math.pi * min(max(t, 0.0), 1.0)) ** width_power)
            centre.append(pos)
            rows.append([
                self.add_vertex(pos - local_side * half),
                self.add_vertex(pos + local_side * half),
            ])
        for i in range(segments):
            a, b = rows[i], rows[i + 1]
            self.add_face((a[0], a[1], b[1], b[0]), smooth=smooth)
        if thickness > 0.0:
            back = [[self.add_vertex(mathutils.Vector(self.verts[idx]) - up * thickness) for idx in row]
                    for row in rows]
            for i in range(segments):
                a, b = rows[i], rows[i + 1]
                c, d = back[i], back[i + 1]
                self.add_face((a[0], b[0], d[0], c[0]), smooth=smooth)
                self.add_face((b[1], a[1], c[1], d[1]), smooth=smooth)
            for row, brow in ((rows[0], back[0]), (rows[-1], back[-1])):
                self.add_face((row[0], row[1], brow[1], brow[0]), smooth=False)
        return rows

    def add_uv_sphere(self, centre: Sequence[float], radius: float, *, segments: int = 10, rings: int = 6) -> None:
        c = mathutils.Vector(centre)
        grid: list[list[int]] = []
        for r in range(rings + 1):
            phi = math.pi * r / rings
            row = []
            for s in range(segments):
                theta = 2.0 * math.pi * s / segments
                pos = c + mathutils.Vector((
                    radius * math.sin(phi) * math.cos(theta),
                    radius * math.sin(phi) * math.sin(theta),
                    radius * math.cos(phi),
                ))
                row.append(self.add_vertex(pos))
            grid.append(row)
        for r in range(rings):
            for s in range(segments):
                s2 = (s + 1) % segments
                self.add_face((grid[r][s], grid[r][s2], grid[r + 1][s2], grid[r + 1][s]), smooth=True)

    def add_box(self, centre: Sequence[float], size: Sequence[float]) -> None:
        cx, cy, cz = centre
        sx, sy, sz = (float(v) for v in size)
        corners = [
            (cx - sx, cy - sy, cz - sz), (cx + sx, cy - sy, cz - sz),
            (cx + sx, cy + sy, cz - sz), (cx - sx, cy + sy, cz - sz),
            (cx - sx, cy - sy, cz + sz), (cx + sx, cy - sy, cz + sz),
            (cx + sx, cy + sy, cz + sz), (cx - sx, cy + sy, cz + sz),
        ]
        idx = [self.add_vertex(c) for c in corners]
        for quad in ((0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1),
                     (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)):
            self.add_face([idx[i] for i in quad], smooth=False)

    # -- output ------------------------------------------------------------
    def to_object(self, name: str, *, collection=None, smooth_all: bool = False) -> bpy.types.Object:
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.verts, [], self.faces)
        mesh.validate(verbose=False)
        if smooth_all or any(self.smooth):
            for poly in mesh.polygons:
                poly.use_smooth = smooth_all or self.smooth[poly.index] if poly.index < len(self.smooth) else False
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        (collection or bpy.context.scene.collection).objects.link(obj)
        return obj


def unit_vector(v) -> mathutils.Vector:
    """Normalized vector with a safe fallback (shared by every script)."""
    vec = mathutils.Vector(v)
    return vec.normalized() if vec.length > 1e-9 else mathutils.Vector((0.0, 0.0, 1.0))


def basis_from_direction(direction: Sequence[float], up: Sequence[float] = (0.0, 0.0, 1.0)) -> mathutils.Matrix:
    """A stable orthonormal basis whose +Y follows ``direction``."""
    fwd = mathutils.Vector(direction)
    if fwd.length < 1e-9:
        fwd = mathutils.Vector((0.0, 0.0, 1.0))
    fwd.normalize()
    upv = mathutils.Vector(up)
    if abs(fwd.dot(upv)) > 0.999:
        upv = mathutils.Vector((1.0, 0.0, 0.0))
    x = upv.cross(fwd)
    if x.length < 1e-9:
        x = mathutils.Vector((1.0, 0.0, 0.0))
    x.normalize()
    z = fwd.cross(x).normalized()
    return mathutils.Matrix((x, fwd, z)).transposed()


# --------------------------------------------------------------------------- #
# measurement
# --------------------------------------------------------------------------- #
def triangle_count(obj: bpy.types.Object) -> int:
    mesh = obj.data
    try:
        return sum(max(len(p.vertices) - 2, 0) for p in mesh.polygons)
    except (AttributeError, TypeError):
        return len(mesh.loop_triangles) or len(mesh.polygons)


def object_metrics(obj: bpy.types.Object) -> dict[str, Any]:
    mesh = obj.data
    mats = [slot.material.name for slot in obj.material_slots if slot.material]
    modifiers = [{"name": m.name, "type": m.type, "show_viewport": bool(m.show_viewport),
                  "show_render": bool(m.show_render)} for m in obj.modifiers]
    return {
        "name": obj.name,
        "type": obj.type,
        "vertices": len(mesh.vertices) if obj.type == "MESH" else 0,
        "triangles": triangle_count(obj) if obj.type == "MESH" else 0,
        "polygons": len(mesh.polygons) if obj.type == "MESH" else 0,
        "dimensions": {axis: round(float(value), 6) for axis, value in zip("xyz", obj.dimensions)},
        "materials": mats,
        "modifiers": modifiers,
        "collections": [c.name for c in obj.users_collection],
        "geometry_digest": geometry_digest(obj),
    }


def scene_metrics(objects: Iterable[bpy.types.Object]) -> dict[str, Any]:
    rows = [object_metrics(o) for o in objects]
    materials: set[str] = set()
    for obj in objects:
        for slot in obj.material_slots:
            if slot.material:
                materials.add(slot.material.name)
    return {
        "objects": rows,
        "object_count": len(rows),
        "triangles": sum(r["triangles"] for r in rows),
        "vertices": sum(r["vertices"] for r in rows),
        "polygons": sum(r["polygons"] for r in rows),
        "materials": sorted(materials),
        "geometry_digest": scene_digest(list(objects)),
    }


def bounds(objects: Iterable[bpy.types.Object]) -> dict[str, list[float]]:
    """World-space axis-aligned bounds over mesh vertices (dims are post-modifier
    only for evaluated objects, so read the raw data for honest reporting)."""
    mins = [math.inf] * 3
    maxs = [-math.inf] * 3
    for obj in objects:
        if obj.type != "MESH":
            continue
        matrix = obj.matrix_world
        for vertex in obj.data.vertices:
            world = matrix @ vertex.co
            for i in range(3):
                mins[i] = min(mins[i], world[i])
                maxs[i] = max(maxs[i], world[i])
    if mins[0] == math.inf:
        return {"min": [0.0, 0.0, 0.0], "max": [0.0, 0.0, 0.0]}
    return {"min": mins, "max": maxs}


def max_height(objects: Iterable[bpy.types.Object]) -> float:
    box = bounds(objects)
    return round(float(box["max"][2]), 4)


def max_spread(objects: Iterable[bpy.types.Object]) -> float:
    box = bounds(objects)
    return round(max(box["max"][0] - box["min"][0], box["max"][1] - box["min"][1]), 4)


def geometry_digest(obj: bpy.types.Object) -> str:
    """sha256 over the canonical mesh data.

    A .blend file embeds raw in-memory datablock pointers, so two byte-identical
    scenes still differ by ~100 bytes of addresses. This digest is the actual
    reproducibility proof for the geometry itself.
    """
    import hashlib
    import struct

    if obj.type != "MESH":
        return ""
    h = hashlib.sha256()
    h.update(obj.name.encode("utf-8"))
    mesh = obj.data
    for vertex in mesh.vertices:
        h.update(struct.pack("<3f", vertex.co.x, vertex.co.y, vertex.co.z))
    for poly in mesh.polygons:
        h.update(struct.pack("<I", len(poly.vertices)))
        for index in poly.vertices:
            h.update(struct.pack("<I", index))
    for slot in obj.material_slots:
        h.update((slot.material.name if slot.material else "-").encode("utf-8"))
    return h.hexdigest()


def scene_digest(objects: Iterable[bpy.types.Object]) -> str:
    """One digest over a whole scene's mesh data, independent of .blend pointers."""
    import hashlib

    h = hashlib.sha256()
    for obj in sorted(objects, key=lambda o: o.name):
        h.update(geometry_digest(obj).encode("utf-8"))
    return h.hexdigest()


def blender_version() -> str:
    return bpy.app.version_string


def result_envelope(
    params: dict[str, Any],
    *,
    action: str,
    reporter: Reporter,
    metrics: dict[str, Any],
    artifacts: list[dict[str, Any]] | None = None,
    extra: dict[str, Any] | None = None,
) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "ok": True,
        "action": action,
        "command": params.get("command", ""),
        "asset_id": params.get("asset_id", ""),
        "seed": params.get("seed"),
        "blender_version": blender_version(),
        "blender_version_tuple": list(bpy.app.version),
        "engine": params.get("engine", ""),
        "resolution": params.get("resolution"),
        "timings": dict(reporter.timings),
        "total_s": round(time.time() - reporter.started, 4),
        "metrics": metrics,
        "artifacts": artifacts or [],
        "warnings": list(reporter.warnings),
    }
    if extra:
        payload.update(extra)
    return payload


def failure_envelope(params: dict[str, Any], *, action: str, error: BaseException,
                     reporter: Reporter | None = None) -> dict[str, Any]:
    return {
        "ok": False,
        "action": action,
        "command": params.get("command", ""),
        "asset_id": params.get("asset_id", ""),
        "seed": params.get("seed"),
        "blender_version": blender_version(),
        "error": {"kind": type(error).__name__, "message": str(error)},
        "timings": dict(reporter.timings) if reporter else {},
        "warnings": list(reporter.warnings) if reporter else [],
        "metrics": {},
        "artifacts": [],
    }


def save_blend(path: str) -> str:
    """Save the source master. The path is absolute and pre-created by Python."""
    parent = os.path.dirname(os.path.abspath(path))
    if parent:
        os.makedirs(parent, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(path), compress=False, copy=False)
    return os.path.abspath(path)


def run_main(action: str, handler) -> None:
    """Standard script wrapper: always write a result JSON, never print one."""
    params: dict[str, Any] = {}
    reporter = Reporter()
    try:
        bootstrap()
        params = parse_params()
        payload = handler(params, reporter) or {}
        envelope = result_envelope(params, action=action, reporter=reporter, metrics=payload.pop("metrics", {}),
                                   artifacts=payload.pop("artifacts", []), extra=payload or None)
    except BaseException as exc:  # noqa: BLE001 - result JSON is the contract
        import traceback

        try:
            envelope = failure_envelope(params, action=action, error=exc, reporter=reporter)
            envelope["error"]["traceback"] = traceback.format_exc()[-4000:]
        except Exception:
            raise
    # The result JSON is the contract, including for failures. The process exit
    # code stays 0 so Python can read the structured failure; the Python side
    # turns `ok: false` into ExternalToolFailed (exit 6) with full detail.
    write_result(params or {}, envelope)
