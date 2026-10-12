"""Creature anatomical **proxy** builder (bpy, headless).

IMPORTANT SCOPE NOTE (repeated in every result payload and manifest):

    This is a pipeline-verification proxy built from proportion parameters. It is
    a rigged-able quadruped body plan -- segmented spine, neck, tail, four limbs,
    skull -- driven entirely by the definition's proportions. It is **not**
    paleontological proof, not a skeletal reconstruction, and no claim about
    soft-tissue accuracy is implied. It exists to prove that generation ->
    rig -> bake -> render -> LOD -> validate chains work end to end.

Determinism: all variation comes from ``S.rng(seed, label, ...)``.
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
import anatomy_pose as POSE  # noqa: E402


def _unit(v: mathutils.Vector) -> mathutils.Vector:
    return v.normalized() if v.length > 1e-9 else mathutils.Vector((0.0, 0.0, 1.0))


def build_body(p: dict[str, Any], body: S.MeshBuilder, detail: S.MeshBuilder,
               report: S.Reporter, stats: dict[str, Any]) -> dict[str, Any]:
    seed = p["seed"]
    sides = p["tube_sides"]
    joints: dict[str, list[mathutils.Vector]] = {}

    stance = p["stance_height_m"]
    support_offsets = POSE.support_offsets(p)
    trunk_len = p["trunk_length_m"]
    neck_len = p["neck_length_m"]
    tail_len = p["tail_length_m"]
    head_len = p["head_length_m"]
    tread = math.radians(p["tail_rise_deg"])

    # ---- pelvis / trunk axis -------------------------------------------
    trunk_segs = p["trunk_segments"]
    # +X = forward, +Z = up, +Y = left
    front_x = trunk_len * 0.5
    rear_x = -trunk_len * 0.5
    arch = p["trunk_arch"]

    def trunk_point(t: float) -> mathutils.Vector:
        x = rear_x + trunk_len * t
        bulge = math.sin(math.pi * t) ** 0.7
        depth = p["trunk_depth_m"] * (0.45 + 0.55 * bulge)
        width = p["trunk_width_m"] * (0.5 + 0.5 * bulge)
        z = (stance + depth * 0.55 + arch * trunk_len * math.sin(math.pi * t)
             + POSE.offset_at(p, support_offsets, t))
        return mathutils.Vector((x, 0.0, z)), depth, width

    path, radii_d, radii_w, pts = [], [], [], []
    for i in range(trunk_segs + 1):
        t = i / trunk_segs
        pt, depth, width = trunk_point(t)
        path.append(pt)
        radii_d.append(depth * 0.5)
        radii_w.append(width * 0.5)
        pts.append((pt, depth, width))

    # The trunk is an elliptical sweep: sweep a tube then scale per-ring by
    # building the ring ourselves for an elliptical cross-section.
    rings = _elliptical_tube(body, path, radii_w, radii_d, sides=sides)
    joints["spine"] = [p0 for p0 in path]

    # ---- neck chain -----------------------------------------------------
    neck_segs = p["neck_segments"]
    base = path[-1]
    rise = math.radians(p["neck_rise_deg"])
    neck_dir = mathutils.Vector((math.cos(rise), 0.0, math.sin(rise)))
    neck_path, neck_r = [], []
    nq = S.rng(seed, "creature", "neck")
    for i in range(neck_segs + 1):
        t = i / neck_segs
        sag = nq.uniform(-0.012, 0.012) * neck_len * math.sin(math.pi * t)
        neck_path.append(base + neck_dir * (neck_len * t) + mathutils.Vector((0.0, sag, 0.0)))
        neck_r.append(max(radii_d[-1] * 0.62 * (1.0 - 0.45 * t), neck_len * 0.018))
    _elliptical_tube(body, neck_path, [r * 0.85 for r in neck_r], neck_r, sides=sides)
    joints["neck"] = neck_path

    # ---- skull ----------------------------------------------------------
    head_dir = _unit(neck_path[-1] - neck_path[-2])
    hl, hd, hw = head_len, p["head_depth_m"], head_len * 0.55
    skull_path = [
        neck_path[-1] - head_dir * head_len * 0.1,
        neck_path[-1] + head_dir * head_len * 0.35,
        neck_path[-1] + head_dir * head_len * 0.72,
        neck_path[-1] + head_dir * head_len * 0.95,
    ]
    head_r = [hw * 0.92, hw, hw * 0.82, hd * 0.75]
    _elliptical_tube(body, skull_path, head_r, [r * 1.05 for r in head_r], sides=sides)
    # lower jaw
    jaw = [
        neck_path[-1] - head_dir * head_len * 0.05 + mathutils.Vector((0, 0, -hd * 0.55)),
        neck_path[-1] + head_dir * head_len * 0.6 + mathutils.Vector((0, 0, -hd * 0.62)),
        neck_path[-1] + head_dir * head_len * 0.88 + mathutils.Vector((0, 0, -hd * 0.40)),
    ]
    _elliptical_tube(body, jaw, [hd * 0.3, hd * 0.34, hd * 0.18], [hd * 0.24] * 3, sides=max(5, sides // 2))
    joints["head"] = skull_path
    stats["skull_length_m"] = round(head_len, 5)

    # eye + orbit bosses (small, deterministic, symmetric)
    for sign in (-1.0, 1.0):
        eye = neck_path[-1] + head_dir * head_len * 0.42 + mathutils.Vector((0.0, sign * hw * 0.78, hd * 0.18))
        body.add_uv_sphere(eye, max(hd * 0.17, head_len * 0.05), segments=8, rings=5)

    # ---- tail chain -----------------------------------------------------
    tail_segs = p["tail_segments"]
    tdir = _unit(path[0] - path[1])
    tail_path, tail_r = [], []
    trng = S.rng(seed, "creature", "tail")
    # the tail base follows the corrected rear support anchor; apply authored
    # positive tail rise so the tip keeps clearance over the ground plane
    base_z = path[0].z
    tail_lift = min(tail_len * tread, max(0.0, base_z * 0.55))
    tail_vertex_start = len(body.verts)
    for i in range(tail_segs + 1):
        t = i / tail_segs
        droop = tdir * (tail_len * t)
        lift = mathutils.Vector((0.0, 0.0, tail_lift * t * t + base_z * 0.05 * t))
        wobble = mathutils.Vector((0.0, trng.uniform(-0.01, 0.01) * tail_len * math.sin(math.pi * t), 0.0))
        tail_path.append(path[0] + droop + lift + wobble)
        tail_r.append(max(radii_d[0] * 0.55 * (1.0 - 0.9 * t), tail_len * 0.012))
    _elliptical_tube(body, tail_path, tail_r, [r * 1.05 for r in tail_r], sides=max(5, sides - 2))
    tail_clearance = min(v[2] for v in body.verts[tail_vertex_start:])
    joints["tail"] = tail_path

    # ---- four limbs -----------------------------------------------------
    upper = p["leg_upper_m"]
    lower = p["leg_lower_m"]
    spread = math.radians(p["leg_spread_deg"])
    leg_r = p["leg_diameter_m"] * 0.5
    limb_chains: dict[str, dict[str, list[mathutils.Vector]]] = {}
    pad_bottoms: dict[str, float] = {}
    # fore at t=0.82 of the trunk, hind at t=0.16 (quadruped plan)
    for label, t, is_fore in (("front_left", 0.80, True), ("front_right", 0.80, False),
                              ("rear_left", 0.16, False), ("rear_right", 0.16, False)):
        anchor, depth, width = trunk_point(t)
        sign = 1.0 if label.endswith("left") else -1.0
        side = mathutils.Vector((0.0, sign * (width * 0.5), 0.0))
        hip = anchor + side * 0.85
        # columnar limb: slight outward splay, knee bends backward
        upper_dir = mathutils.Vector((0.0, sign * math.sin(spread), -math.cos(spread)))
        knee = hip + upper_dir * upper
        knee_bend = mathutils.Vector((0.0, sign * math.sin(spread * 0.35), -1.0))
        ankle = knee + _unit(knee_bend) * lower
        foot = ankle + mathutils.Vector((0.0, 0.0, -max(0.02, stance * 0.06)))
        limb_path = [hip, hip + upper_dir * upper * 0.5, knee, knee + (ankle - knee) * 0.5, ankle]
        limb_r = [leg_r * 1.25, leg_r * 1.12, leg_r * 0.95, leg_r * 0.7, leg_r * 0.6]
        _elliptical_tube(body, limb_path, limb_r, [r * 0.92 for r in limb_r], sides=max(5, sides - 2))
        # foot pad
        pad_start = len(body.verts)
        body.add_box(foot + mathutils.Vector((head_len * 0.25, 0.0, 0.0)),
                     (head_len * 0.5, leg_r * 1.1, leg_r * 0.5))
        pad_bottoms[label] = min(v[2] for v in body.verts[pad_start:])
        limb_chains[label] = {"chain": limb_path, "foot": foot, "knee": knee, "hip": hip}
    joints["limbs"] = limb_chains

    # ---- integument detail (fictional placement, proxy-grade) -----------
    surf = p.get("surface", {})
    bristles = 0
    osteoderms = 0
    if surf.get("osteoderm_density", 0.0) > 0.0:
        ornge = S.rng(seed, "creature", "osteoderm")
        rows = int(surf.get("osteoderm_rows", 6))
        count = int(rows * 8 * surf["osteoderm_density"])
        for i in range(count):
            t = ornge.uniform(0.15, 0.9)
            _, depth, width = trunk_point(t)
            a = ornge.uniform(-0.55, 0.55)
            ang = ornge.uniform(0.0, math.pi)
            pos = mathutils.Vector((rear_x + trunk_len * t,
                                    math.sin(ang) * width * 0.5,
                                    stance + depth * 0.55 + arch * trunk_len * math.sin(math.pi * t)
                                    + POSE.offset_at(p, support_offsets, t)
                                    + math.cos(ang) * depth * 0.52))
            rr = max(depth * 0.05, 0.01) * ornge.uniform(0.6, 1.2)
            detail.add_uv_sphere(pos, rr, segments=6, rings=4)
            osteoderms += 1
    if surf.get("bristle_density", 0.0) > 0.0:
        brng = S.rng(seed, "creature", "bristle")
        count = int(60 * surf["bristle_density"])
        blen = surf.get("bristle_length_m", 0.02)
        for i in range(count):
            t = brng.uniform(0.35, 0.98)
            _, depth, width = trunk_point(t)
            a = brng.uniform(0.0, math.pi)
            pos = mathutils.Vector((rear_x + trunk_len * t,
                                    math.sin(a) * width * 0.52,
                                    stance + depth * 0.55 + arch * trunk_len * math.sin(math.pi * t)
                                    + POSE.offset_at(p, support_offsets, t)
                                    + math.cos(a) * depth * 0.5))
            d = mathutils.Vector((0.0, math.sin(a), math.cos(a) * 0.9 + 0.4)).normalized()
            detail.add_tube([pos, pos + d * blen], [blen * 0.16, blen * 0.02], sides=3)
            bristles += 1

    stats.update({
        "trunk_segments": trunk_segs,
        "neck_segments": neck_segs,
        "tail_segments": tail_segs,
        "limbs": 4,
        "stance_height_m": round(stance, 5),
        "support_pose_offsets_m": {k: round(v, 6) for k, v in support_offsets.items()},
        "neutral_pad_bottom_m": round(POSE.PAD_GROUND_Z_M, 6),
        "source_pad_bottoms_m": {k: round(v, 6) for k, v in pad_bottoms.items()},
        "tail_mesh_min_z_m": round(tail_clearance, 6),
        "leg_segment_lengths_m": {"upper": round(upper, 6), "lower": round(lower, 6)},
        "osteoderms": osteoderms,
        "bristles": bristles,
        "proxy_disclaimer": ("anatomical proxy for pipeline verification; "
                             "proportions are parameters, not fossil measurements"),
    })
    return joints


def _elliptical_tube(builder: S.MeshBuilder, path, radii_side, radii_up, *, sides: int = 10) -> list[list[int]]:
    """Sweep an elliptical profile along ``path`` with parallel transport."""
    pts = [mathutils.Vector(p) for p in path]
    n = len(pts)
    rings: list[list[int]] = []
    up_ref = mathutils.Vector((0.0, 0.0, 1.0))
    for i in range(n):
        if i == 0:
            tangent = _unit(pts[1] - pts[0])
        elif i == n - 1:
            tangent = _unit(pts[-1] - pts[-2])
        else:
            tangent = _unit(pts[i + 1] - pts[i - 1])
        side = tangent.cross(up_ref)
        if side.length < 1e-6:
            side = tangent.cross(mathutils.Vector((1.0, 0.0, 0.0)))
        side = _unit(side)
        up = _unit(side.cross(tangent))
        rs = max(float(radii_side[i]), 1e-5)
        ru = max(float(radii_up[i]), 1e-5)
        ring = []
        for s in range(sides):
            ang = 2 * math.pi * s / sides
            pos = pts[i] + side * (math.cos(ang) * rs) + up * (math.sin(ang) * ru)
            ring.append(builder.add_vertex(pos))
        rings.append(ring)
    for i in range(n - 1):
        a, b = rings[i], rings[i + 1]
        for s in range(sides):
            s2 = (s + 1) % sides
            builder.add_face((a[s], a[s2], b[s2], b[s]), smooth=True)
    builder.add_face(tuple(reversed(rings[0])), smooth=False)
    builder.add_face(tuple(rings[-1]), smooth=False)
    return rings


def main(params: dict[str, Any], report: S.Reporter) -> dict[str, Any]:
    p = dict(params["creature"])
    p["seed"] = params["seed"]
    p["surface"] = params.get("surface", {})

    S.reset_scene()
    stats: dict[str, Any] = {}
    body = S.MeshBuilder()
    detail = S.MeshBuilder()

    with report.time("build_geometry"):
        joints = build_body(p, body, detail, report, stats)

    with report.time("to_objects"):
        coll = bpy.data.collections.new("creature")
        bpy.context.scene.collection.children.link(coll)
        body_obj = body.to_object("body_proxy", collection=coll)
        detail_obj = detail.to_object("integument_proxy", collection=coll)
        if not detail_obj.data.polygons:
            detail_obj.hide_render = True
            report.warn("no integument detail requested for this stage")

    with report.time("materials"):
        skin = MAT.build_procedural_material("creature_skin", "skin", seed=p["seed"],
                                             subsurface=0.12)
        MAT.assign(body_obj, skin)
        if detail_obj.data.polygons:
            MAT.assign(detail_obj, skin)

    with report.time("save_blend"):
        blend_path = S.save_blend(params["outputs"]["blend"])

    objects = [body_obj, detail_obj]
    metrics = S.scene_metrics(objects)
    metrics.update(stats)
    metrics["rig_target"] = "spine/neck/tail/limb chains at defined joint positions"
    metrics["joints_recorded"] = sorted(joints.keys())
    metrics["dimensions_m"] = {axis: round(float(getattr(body_obj.dimensions, axis)), 4)
                               for axis in "xyz"}
    metrics["determinism"] = {"seed": p["seed"], "time_input_used": False}

    return {
        "metrics": metrics,
        "artifacts": [{"path": blend_path, "role": "source", "kind": "blend"}],
        "object_names": [o.name for o in objects],
    }


if __name__ == "__main__":
    S.run_main("build_creature", main)
