"""Procedural plant generation (bpy, headless).

Families: ``tree`` (branch-aware, recursive, phyllotaxy), ``fern`` (true
rachis + pinnae), ``horsetail`` (segmented internodes + node whorls),
``rosette`` (layered radial leaves), ``shrub`` (multi-stem woody, browse
targets).

The source master stays **editable**: geometry is built as plain meshes with
named objects, no destructive-only operators, subdivision as *unapplied
modifiers* so a human can keep refining, and collections per organ group.

Determinism: every stochastic decision uses ``random.Random(derive_seed(seed,
*labels))``. There is no global ``random``, no ``numpy.random``, no ``hash()``
and no time input anywhere in this file.
"""

from __future__ import annotations

import math
from typing import Any

import bpy
import mathutils

import os as _os
import sys as _sys

# Blender does not put the --python script's directory on sys.path
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))

import _stalthos as S

S.bootstrap()

import _materials as MAT  # noqa: E402


# --------------------------------------------------------------------------- #
# helpers
# --------------------------------------------------------------------------- #
def _unit(vec: mathutils.Vector) -> mathutils.Vector:
    return vec.normalized() if vec.length > 1e-9 else mathutils.Vector((0.0, 0.0, 1.0))


def _taper(radii: list[float], base: float, tip: float, taper: float, samples: int) -> list[float]:
    out = []
    for i in range(samples):
        t = i / max(samples - 1, 1)
        out.append(base * ((1.0 - t) ** taper) + tip * t)
    return out


def _new_collection(name: str) -> bpy.types.Collection:
    coll = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(coll)
    return coll


# --------------------------------------------------------------------------- #
# tree: recursive branching with phyllotaxy
# --------------------------------------------------------------------------- #
def build_tree(p: dict[str, Any], root: S.MeshBuilder, leaves: S.MeshBuilder,
               report: S.Reporter, stats: dict[str, Any]) -> None:
    seed = p["seed"]
    height = p["height_m"]
    trunk_r = p["trunk_diameter_m"] * 0.5
    sides = p["tube_sides"]
    divergence = math.radians(p["divergence_deg"])
    branch_angle = math.radians(p["branch_angle_deg"])
    node_start, node_end = p["node_start"], p["node_end"]
    nodes_per_level = p["nodes_per_level"]
    children = p["children_per_node"]

    # --- trunk with root flare -------------------------------------------
    seg = max(8, p["branch_levels"] * 6)
    path, radii = [], []
    lean_rng = S.rng(seed, "tree", "trunk")
    lean = mathutils.Vector((lean_rng.uniform(-0.05, 0.05), lean_rng.uniform(-0.05, 0.05), 1.0))
    for i in range(seg + 1):
        t = i / seg
        path.append(mathutils.Vector((lean.x * height * 0.06 * t * t,
                                      lean.y * height * 0.06 * t * t,
                                      height * t)))
        base = trunk_r * ((1.0 - t * 0.62) ** 1.0)
        flare = 1.0 + (p["root_flare"] - 1.0) * max(0.0, 1.0 - t / 0.12)
        radii.append(max(base * flare, trunk_r * 0.06))
    root.add_tube(path, radii, sides=sides)

    # --- recursive branching ---------------------------------------------
    branch_count = 0
    leaf_count = 0

    def grow(origin: mathutils.Vector, direction: mathutils.Vector, length: float,
             radius: float, level: int, node_index: int) -> None:
        nonlocal branch_count, leaf_count
        if level <= 0 or length < height * 0.01:
            return
        max_z = height * 1.12          # hard height envelope for the whole crown
        # a branch must never climb above the requested plant height
        headroom = max_z - origin.z
        length = min(length, max(headroom * 0.95, height * 0.02))
        r = S.rng(seed, "tree", "branch", level, node_index)
        # A conifer branch leaves the stem outward and arcs gently. Gravity is
        # applied as a *constant* small pull per step, so a branch bends a little
        # at the tip instead of diving or standing bolt upright.
        steps = max(3, 6 - level)
        droop_per_step = -0.030 * level
        bpath, bradii = [], []
        cur = origin.copy()
        step_dir = S.unit_vector(direction)
        for i in range(steps + 1):
            t = i / steps
            step_dir = S.unit_vector(step_dir + mathutils.Vector(
                (r.uniform(-0.05, 0.05), r.uniform(-0.05, 0.05), droop_per_step)))
            cur = cur + step_dir * (length / steps)
            bpath.append(cur.copy())
            # the taper exponent is softened so a branch does not vanish into a
            # hairline before it reaches its foliage
            bradii.append(max(radius * ((1.0 - 0.72 * t) ** p["taper"])
                              + radius * 0.08, radius * 0.12))
        root.add_tube(bpath, bradii, sides=max(5, sides - level))
        branch_count += 1

        tip = bpath[-1]
        tip_dir = S.unit_vector(bpath[-1] - bpath[-2])

        # leaves clustered near the tip (and along thin outer branches)
        cluster = int(p["leaf_cluster"] * (1.0 + 0.35 * (p["branch_levels"] - level)))
        if cluster:
            lr = S.rng(seed, "tree", "leaf", level, node_index)
            for i in range(cluster):
                t = lr.uniform(0.45, 1.0)
                anchor = bpath[min(len(bpath) - 1, int(t * len(bpath)))]
                az = lr.uniform(0.0, 2 * math.pi)
                el = lr.uniform(-0.15, 1.0)
                d = mathutils.Vector((math.cos(az) * math.cos(el * math.pi / 2),
                                     math.sin(az) * math.cos(el * math.pi / 2),
                                     math.sin(el * math.pi / 2)))
                scale = p["leaf_length_m"] * lr.uniform(0.75, 1.15)
                leaves.add_blade(anchor, d, length=scale,
                                 width=p["leaf_width_m"] * lr.uniform(0.8, 1.2),
                                 segments=p["leaf_segments"], curve=0.3,
                                 droop=0.35 * level, tip_twist=math.radians(p["twist_deg"]))
                leaf_count += 1

        if level > 1:
            kids = max(1, children if level <= p["branch_levels"] - 1 else 1)
            for k in range(kids):
                # phyllotaxis: golden-angle offset per child index, level offset
                az = divergence * (node_index + k + level)
                lateral = mathutils.Vector((math.cos(az), math.sin(az), 0.0))
                child_dir = S.unit_vector(tip_dir * math.cos(branch_angle * 0.9)
                                          + lateral * math.sin(branch_angle * 0.9))
                grow(tip, child_dir, length * (0.62 + 0.06 * level), bradii[-1] * 0.78,
                     level - 1, node_index * kids + k)

    span = max(node_end - node_start, 0.05)
    if "crown_spread_m" in p:
        target_radius = float(p["crown_spread_m"]) * 0.5
        leaf_reach = float(p.get("leaf_length_m", height * 0.09)) * 0.78
        crown_span = max(0.05, (target_radius - leaf_reach) / 0.92)
    else:
        crown_span = height * 0.42            # desired crown radius
    for i in range(nodes_per_level):
        t = node_start + span * (i / max(nodes_per_level - 1, 1))
        idx = min(int(t * seg), seg)
        anchor = path[idx]
        base_dir = S.unit_vector(path[min(idx + 1, seg)] - path[max(idx - 1, 0)])
        az = divergence * i
        lateral = mathutils.Vector((math.cos(az), math.sin(az), 0.0))
        # a branch leaves the trunk at the whorl angle, purely laterally
        d = S.unit_vector(base_dir * math.cos(branch_angle)
                          + lateral * math.sin(branch_angle))
        # primary branch length: bounded by the remaining crown radius budget and
        # by the trunk still above this node, so height stays the dominant axis
        headroom = max(height - anchor.z, height * 0.05)
        node_len = min(headroom * 0.72, crown_span * (0.6 + 0.4 * (1.0 - t)) * 0.5)
        grow(anchor, d, node_len, radii[idx] * 0.72, p["branch_levels"], i)

    stats["branch_count"] = branch_count
    stats["trunk_segments"] = seg + 1
    stats["leaf_blades"] = leaf_count
    stats["branch_levels"] = p["branch_levels"]
    stats["crown_radius_m"] = round(crown_span, 4)


# --------------------------------------------------------------------------- #
# fern: true rachis with pinnae
# --------------------------------------------------------------------------- #
def build_fern(p: dict[str, Any], rachis: S.MeshBuilder, blades: S.MeshBuilder,
               report: S.Reporter, stats: dict[str, Any]) -> None:
    seed = p["seed"]
    height = p["height_m"]
    sides = p["tube_sides"]
    pinnae = p["pinnae_per_side"]
    fronds = p["frond_count"]
    span = p["pinna_span"]
    rachis_curve = p["rachis_curve"]
    narrow = p["narrowness"]

    crown_height = float(p.get("crown_trunk_height_m", 0.0))
    if crown_height > 0.0:
        # Profile-only rhizome/trunk: a stout, short support with all frond
        # rachises beginning around one elevated crown, never woody branches.
        trunk_r = p["trunk_diameter_m"] * 0.5
        trunk_path, trunk_radii = [], []
        for i in range(9):
            t = i / 8
            trunk_path.append(mathutils.Vector((0.0, 0.0, crown_height * t)))
            trunk_radii.append(trunk_r * (1.0 - 0.22 * t))
        rachis.add_tube(trunk_path, trunk_radii, sides=sides)
        stats["crown_trunk_height_m"] = round(crown_height, 4)

    total_pinnae = 0
    for f in range(fronds):
        r = S.rng(seed, "fern", "frond", f)
        az = 2 * math.pi * f / fronds + r.uniform(-0.12, 0.12)
        lean = math.radians(r.uniform(28.0, 58.0))
        nominal_length = p.get("frond_length_m", height)
        length = nominal_length * r.uniform(0.82, 1.08)
        inner_frond = crown_height > 0.0 and f % 3 == 0
        if crown_height > 0.0:
            length *= 0.86 if inner_frond else 1.02
        crozier = p["crozier"] * (1.0 if f % 3 else 0.55)   # juvenile fiddleheads
        steps = 16
        path, radii = [], []
        dirv = mathutils.Vector((math.cos(az) * math.sin(lean), math.sin(az) * math.sin(lean), math.cos(lean)))
        cur = mathutils.Vector((math.cos(az) * crown_height * 0.04,
                                math.sin(az) * crown_height * 0.04,
                                crown_height - p["trunk_diameter_m"] * 0.22 if crown_height > 0.0 else 0.0))
        if crown_height > 0.0:
            path.append(cur.copy())  # rachis begins embedded just below the support cap
            radii.append(length * 0.012)
        for i in range(1 if crown_height > 0.0 else 0, steps + 1):
            t = i / steps
            # arch: rise then curve outward/down
            if crown_height > 0.0:
                # Alternate upright inner fronds with broad outer arcs to form
                # an open vase crown rather than a single flat umbrella.
                pitch = (math.radians(18.0 + 48.0 * t) if inner_frond else
                         math.radians(28.0 + 95.0 * t))
            else:
                pitch = lean * (1.0 - 1.35 * rachis_curve * t * t)
            horiz = mathutils.Vector((math.cos(az), math.sin(az), 0.0))
            dirv = _unit(horiz * math.sin(pitch) + mathutils.Vector((0.0, 0.0, math.cos(pitch))))
            step = length / steps
            if crozier > 0.05 and t < 0.18:
                # coiled crozier tip
                curl = math.radians(320.0) * (1.0 - t / 0.18)
                dirv = _unit(dirv + horiz * math.cos(curl) * crozier + mathutils.Vector((0, 0, -math.sin(curl) * crozier)))
            cur = cur + dirv * step
            path.append(cur.copy())
            base = (length * 0.012) * (1.0 - 0.92 * t)
            radii.append(max(base, length * 0.0012))
        rachis.add_tube(path, radii, sides=max(4, sides // 2))

        # pinnae: paired along the rachis, longest at mid-frond (pinna_taper)
        for i in range(pinnae):
            t = span * (0.08 + 0.92 * (i / max(pinnae - 1, 1)))
            idx = min(int(t * steps), steps)
            anchor = path[idx]
            tdir = _unit(path[min(idx + 1, steps)] - path[max(idx - 1, 0)])
            shape = math.sin(math.pi * min(t / span, 1.0)) ** 0.65
            plength = (length * p["pinna_length_ratio"] * shape
                       if crown_height > 0.0 else
                       p["leaf_length_m"] * shape * p["pinna_taper"] * 2.6)
            for sign in (-1.0, 1.0):
                lateral = _unit(tdir.cross(mathutils.Vector((0.0, 0.0, 1.0))) * sign) if abs(tdir.z) < 0.98 \
                    else _unit(tdir.cross(mathutils.Vector((1.0, 0.0, 0.0))) * sign)
                d = _unit(tdir * 0.45 + lateral * 0.9 + mathutils.Vector((0.0, 0.0, -0.06)))
                blades.add_blade(anchor, d, length=plength,
                                 width=plength * narrow,
                                 segments=max(3, p["leaf_segments"] - 1), curve=0.35,
                                 droop=0.22, width_power=0.6,
                                 thickness=p["leaf_width_m"] * 0.02)
                total_pinnae += 1
        # terminal pinna continues the rachis
        blades.add_blade(path[-1], _unit(path[-1] - path[-2]), length=p["leaf_length_m"] * 0.8,
                         width=p["leaf_length_m"] * narrow * 0.6, segments=3, droop=0.4)
    stats["frond_count"] = fronds
    stats["pinnae"] = total_pinnae
    stats["pinnae_per_side"] = pinnae


# --------------------------------------------------------------------------- #
# horsetail: segmented stem with node whorls
# --------------------------------------------------------------------------- #
def build_horsetail(p: dict[str, Any], stems: S.MeshBuilder, whorls: S.MeshBuilder,
                    report: S.Reporter, stats: dict[str, Any]) -> None:
    seed = p["seed"]
    height = p["height_m"]
    sides = p["tube_sides"]
    segments = p["segments"]
    stem_count = p["stem_count"]
    sterile = p["sterile_top"]

    for s in range(stem_count):
        r = S.rng(seed, "horsetail", "stem", s)
        az = r.uniform(0.0, 2 * math.pi)
        rad = r.uniform(0.0, 0.35) * max(height * 0.12, 1e-3)
        base = mathutils.Vector((math.cos(az) * rad, math.sin(az) * rad, 0.0))
        length = height * r.uniform(0.6, 1.05)
        lean = math.radians(p["lean_deg"]) * r.uniform(0.4, 1.0)
        dir_az = r.uniform(0.0, 2 * math.pi)
        dirv = mathutils.Vector((math.cos(dir_az) * math.sin(lean), math.sin(dir_az) * math.sin(lean),
                                 math.cos(lean)))
        wander = r.uniform(-0.04, 0.04)
        seg_len = length / segments
        r_base = max(p["trunk_diameter_m"] * 0.5, height * 0.006)
        pos = base.copy()
        for i in range(segments):
            t = i / segments
            taper = 1.0 - 0.45 * t
            a, b = pos.copy(), pos + dirv * seg_len
            stems.add_tube([a, b], [r_base * taper, r_base * taper * 0.94], sides=sides)
            # node: swollen sheath ring
            ring = r_base * taper * p["sheath_ratio"]
            nodes_pts = []
            sides_r = max(5, sides)
            up = _unit(dirv)
            side = _unit(up.cross(mathutils.Vector((1.0, 0.0, 0.0))))
            other = _unit(up.cross(side))
            for k in range(sides_r + 1):
                ang = 2 * math.pi * k / sides_r
                pts = b + up * (seg_len * 0.02) + side * (math.cos(ang) * ring) + other * (math.sin(ang) * ring)
                nodes_pts.append(pts)
            stems.add_tube(nodes_pts, [ring * 0.35] * len(nodes_pts), sides=max(3, sides_r // 2))
            # whorl of thin branchlets at the node (Equisetum signature)
            if p["whorl_branches"] and i < segments - max(1, int(segments * sterile)):
                wr = S.rng(seed, "horsetail", "whorl", s, i)
                blen = r_base * p["whorl_length_ratio"] * 26.0 * (1.0 - 0.4 * t)
                for k in range(p["whorl_branches"]):
                    a2 = 2 * math.pi * k / p["whorl_branches"] + wr.uniform(-0.1, 0.1)
                    lat = mathutils.Vector((math.cos(a2), math.sin(a2), 0.0))
                    lat = _unit(lat - up * lat.dot(up) + up * 0.35)
                    whorls.add_tube([b, b + lat * blen], [r_base * 0.10, r_base * 0.03], sides=3)
            pos = b
            dirv = _unit(dirv + mathutils.Vector((wander, wander * 0.5, 0.0)))
    stats["stem_count"] = stem_count
    stats["segments_per_stem"] = segments


# --------------------------------------------------------------------------- #
# rosette
# --------------------------------------------------------------------------- #
def build_rosette(p: dict[str, Any], base: S.MeshBuilder, leaves: S.MeshBuilder,
                  report: S.Reporter, stats: dict[str, Any]) -> None:
    seed = p["seed"]
    count = p["leaf_count"]
    layers = p["leaf_layers"]
    spread = math.radians(p["spread_deg"])
    inner = math.radians(p["inner_spread_deg"])
    thickness = p["leaf_thickness_m"]

    for i in range(count):
        r = S.rng(seed, "rosette", "leaf", i)
        layer = i % layers
        t_layer = layer / max(layers - 1, 1)
        az = 2 * math.pi * (i / layers) + layer * 0.7 + r.uniform(-0.15, 0.15)
        ang = inner + (spread - inner) * t_layer + r.uniform(-0.08, 0.08)
        # leaf_length_m is an absolute blade length; do not re-scale by height
        length = p["leaf_length_m"] * r.uniform(1.0 - p["length_jitter"], 1.0 + p["length_jitter"])
        petiole = length * p["petiole_ratio"]
        d = mathutils.Vector((math.cos(az) * math.sin(ang), math.sin(az) * math.sin(ang), math.cos(ang)))
        origin = mathutils.Vector((0.0, 0.0, p["height_m"] * 0.05 * layer))
        stalk = [origin, origin + d * petiole]
        base.add_tube(stalk, [p["trunk_diameter_m"] * 0.4, p["trunk_diameter_m"] * 0.28], sides=6)
        leaves.add_blade(origin + d * petiole, d, length=max(length - petiole, length * 0.2),
                         width=p["leaf_width_m"] * r.uniform(0.85, 1.2),
                         segments=p["leaf_segments"], curve=0.4 + 0.3 * (1 - t_layer),
                         droop=p["stiffness"], width_power=0.45,
                         thickness=thickness)
        stats["leaf_count"] = stats.get("leaf_count", 0) + 1
    stats["leaf_layers"] = layers


# --------------------------------------------------------------------------- #
# broadleaf shrub (key browse target)
# --------------------------------------------------------------------------- #
def build_shrub(p: dict[str, Any], wood: S.MeshBuilder, leaves: S.MeshBuilder,
                report: S.Reporter, stats: dict[str, Any]) -> None:
    seed = p["seed"]
    height = p["height_m"] * p["height_ratio"]
    divergence = math.radians(p["divergence_deg"])
    ang = math.radians(p["branch_angle_deg"])
    sides = p["tube_sides"]

    leaf_count = 0
    max_z = height * 1.02          # hard height envelope: proportions stay honest

    def grow(origin, direction, length, radius, level, index):
        nonlocal leaf_count
        r = S.rng(seed, "shrub", "branch", level, index)
        # hard height envelope: never climb above the requested plant height
        headroom = max(max_z - origin.z, 0.0)
        length = min(length, headroom * 0.95) if headroom < length else length
        if length < height * 0.01:
            return
        steps = 5
        arch = p["arching"] / max(level, 1)
        path, radii = [], []
        cur = origin.copy()
        d = _unit(direction)
        for i in range(steps + 1):
            t = i / steps
            # arch downward, but restore toward the parent direction so the
            # droop stays bounded instead of turning the stem into a dive
            droop = -arch * 0.22
            d = _unit(d + mathutils.Vector((r.uniform(-0.08, 0.08), r.uniform(-0.08, 0.08), droop)))
            d = _unit(d.lerp(S.unit_vector(direction), 0.18))
            cur = cur + d * (length / steps)
            path.append(cur.copy())
            radii.append(max(radius * ((1.0 - t) ** p["taper"]), radius * 0.05))
        wood.add_tube(path, radii, sides=max(4, sides - level))
        tip = path[-1]
        tip_dir = _unit(path[-1] - path[-2])
        if p["leaf_per_node"] and (level <= p["branch_levels"]):
            for i in range(p["leaf_per_node"]):
                lr = S.rng(seed, "shrub", "leaf", level, index, i)
                anchor = path[min(len(path) - 1, int(lr.uniform(0.45, 1.0) * len(path)))]
                a2 = divergence * (index + i) + lr.uniform(-0.3, 0.3)
                lat = mathutils.Vector((math.cos(a2), math.sin(a2), 0.0))
                ld = _unit(tip_dir * 0.5 + lat * 0.8 + mathutils.Vector((0.0, 0.0, lr.uniform(-0.2, 0.4))))
                blade = p["leaf_length_m"] * lr.uniform(0.8, 1.2)
                if anchor.z + abs(ld.z) * blade > max_z:
                    blade *= 0.5
                leaves.add_blade(anchor, ld, length=blade,
                                 width=p["leaf_width_m"] * lr.uniform(0.85, 1.15),
                                 segments=p["leaf_segments"], curve=0.3, droop=0.3,
                                 tip_twist=math.radians(p["twist_deg"]),
                                 thickness=max(p["trunk_diameter_m"] * 0.06, 1e-4))
                leaf_count += 1
        if level > 1:
            for k in range(2):
                a3 = divergence * (index * 2 + k + level)
                lat = mathutils.Vector((math.cos(a3), math.sin(a3), 0.0))
                cd = S.unit_vector(tip_dir * math.cos(ang) + lat * math.sin(ang))
                grow(tip, cd, length * 0.58, radii[-1] * 0.66, level - 1, index * 2 + k)

    # a shrub's primary stem spans the requested height; the recursive levels
    # extend outward from it, never above it
    stem_len = height * 0.78
    for s in range(p["stem_count"]):
        r = S.rng(seed, "shrub", "stem", s)
        a = 2 * math.pi * s / p["stem_count"] + r.uniform(-0.25, 0.25)
        d = mathutils.Vector((math.cos(a) * math.sin(ang * 0.7), math.sin(a) * math.sin(ang * 0.7), math.cos(ang * 0.7)))
        grow(mathutils.Vector((r.uniform(-0.02, 0.02), r.uniform(-0.02, 0.02), 0.0)),
             d, stem_len * r.uniform(0.78, 1.0),
             max(p["trunk_diameter_m"] * 0.5, p["twig_radius_m"] * 2.5),
             p["branch_levels"], s)
    stats["stem_count"] = p["stem_count"]
    stats["branch_levels"] = p["branch_levels"]
    stats["leaf_blades"] = leaf_count
    stats["browse_target"] = True


BUILDERS = {"tree": build_tree, "fern": build_fern, "horsetail": build_horsetail,
            "rosette": build_rosette, "shrub": build_shrub}


# --------------------------------------------------------------------------- #
# entry point
# --------------------------------------------------------------------------- #
def generate(params: dict[str, Any], report: S.Reporter) -> dict[str, Any]:
    p = dict(params["plant"])
    family = params["family"]
    p["seed"] = params["seed"]
    p.setdefault("tube_sides", 8)
    p.setdefault("leaf_segments", 5)
    p.setdefault("trunk_diameter_m", p["height_m"] * 0.03)
    p.setdefault("leaf_length_m", p["height_m"] * 0.09)
    p.setdefault("leaf_width_m", p["height_m"] * 0.03)
    p.setdefault("twist_deg", 6.0)

    S.reset_scene()
    stats: dict[str, Any] = {"family": family}

    structural = S.MeshBuilder()
    foliage = S.MeshBuilder()

    with report.time("build_geometry"):
        BUILDERS[family](p, structural, foliage, report, stats)

    with report.time("to_objects"):
        coll_struct = _new_collection("structure")
        coll_foliage = _new_collection("foliage")
        struct_obj = structural.to_object(f"{family}_structure", collection=coll_struct)
        foliage_obj = foliage.to_object(f"{family}_foliage", collection=coll_foliage)
        if not foliage_obj.data.polygons:
            foliage_obj.hide_render = True
            report.warn("foliage mesh is empty for this family/stage combination")

    with report.time("materials"):
        # unapplied subdivision keeps the master editable (non-destructive)
        for obj, sides in ((struct_obj, 1), (foliage_obj, 1)):
            if sides and obj.data.polygons:
                mod = obj.modifiers.new("refine", "SUBSURF")
                mod.subdivision_type = "SIMPLE"
                mod.levels = 1
                mod.render_levels = 1
        struct_mat = MAT.build_procedural_material(f"{family}_bark", "bark", seed=p["seed"])
        foliage_preset = "frond" if family in {"fern", "horsetail", "rosette"} else "leaf"
        foliage_mat = MAT.build_procedural_material(
            f"{family}_foliage", foliage_preset, seed=p["seed"] + 17,
            transmission=0.18 if family in {"fern", "rosette"} else 0.06)
        MAT.assign(struct_obj, struct_mat)
        if foliage_obj.data.polygons:
            MAT.assign(foliage_obj, foliage_mat)

    with report.time("save_blend"):
        blend_path = S.save_blend(params["outputs"]["blend"])

    objects = [o for o in (struct_obj, foliage_obj)]
    metrics = S.scene_metrics(objects)
    metrics.update(stats)
    metrics["height_m"] = S.max_height(objects)
    metrics["spread_m"] = S.max_spread(objects)
    metrics["materials_count"] = len(metrics["materials"])
    metrics["determinism"] = {
        "seed": p["seed"],
        "seed_source": "python core.derive_seed (mirrored in bpy)",
        "global_random_used": False,
        "time_input_used": False,
    }

    return {
        "metrics": metrics,
        "artifacts": [{"path": blend_path, "role": "source", "kind": "blend"}],
        "object_names": [o.name for o in objects],
    }


def main(params: dict[str, Any], report: S.Reporter) -> dict[str, Any]:
    return generate(params, report)


if __name__ == "__main__":
    S.run_main("generate_plant", main)
