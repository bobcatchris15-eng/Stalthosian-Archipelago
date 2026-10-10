"""Rig creation and neutral-pose export (bpy, headless).

Builds a simple, readable quadruped armature from the same proportion
parameters the proxy body used (so joint positions line up exactly), applies
automatic weights, and exports a validated neutral pose.

Determinism: bone layout is a pure function of the parameters; the only
stochastic element (automatic weight assignment) is seeded by Blender's own
deterministic algorithm and the rig's own weights are reported, never
randomised here.
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


def _unit(v: mathutils.Vector) -> mathutils.Vector:
    return v.normalized() if v.length > 1e-9 else mathutils.Vector((0.0, 0.0, 1.0))


def _axis_chain(arm_data, name: str, points, *, parent=None, connect: bool = False,
                roll: float = 0.0):
    """Create a bone chain from a polyline of head positions.

    Returns ``(bones, last_bone_or_None)``.
    """
    bones = []
    prev = None
    for i in range(len(points) - 1):
        head = mathutils.Vector(points[i])
        tail = mathutils.Vector(points[i + 1])
        if (tail - head).length < 1e-6:
            tail = head + mathutils.Vector((1e-4, 0.0, 0.0))
        bone = arm_data.edit_bones.new(f"{name}_{i:02d}")
        bone.head = head
        bone.tail = tail
        bone.roll = roll
        if parent is not None:
            bone.parent = parent
            bone.use_connect = bool(connect)
        bones.append(bone)
        prev = bone
    return bones, prev


def build_skeleton(arm: bpy.types.Object, p: dict[str, Any], stats: dict[str, Any]) -> dict[str, Any]:
    """Deterministic quadruped skeleton: spine chain, neck, tail, 4 limb chains."""
    eb = arm.data.edit_bones
    for bone in list(eb):
        eb.remove(bone)

    stance = p["stance_height_m"]
    trunk_len = p["trunk_length_m"]
    neck_len = p["neck_length_m"]
    tail_len = p["tail_length_m"]
    head_len = p["head_length_m"]
    arch = p["trunk_arch"]
    rear_x, front_x = -trunk_len * 0.5, trunk_len * 0.5

    def trunk_point(t):
        x = rear_x + trunk_len * t
        bulge = math.sin(math.pi * t) ** 0.7
        depth = p["trunk_depth_m"] * (0.45 + 0.55 * bulge)
        z = stance + depth * 0.55 + arch * trunk_len * math.sin(math.pi * t)
        return mathutils.Vector((x, 0.0, z)), depth

    trunk_segs = p["trunk_segments"]
    spine_pts = [trunk_point(i / trunk_segs)[0] for i in range(trunk_segs + 1)]
    spine, spine_tail = _axis_chain(arm.data, "spine", spine_pts)
    # chain the spine bones to each other so the rig has a single root
    for i in range(1, len(spine)):
        spine[i].parent = spine[i - 1]
        spine[i].use_connect = True

    neck_rise = math.radians(p["neck_rise_deg"])
    neck_dir = mathutils.Vector((math.cos(neck_rise), 0.0, math.sin(neck_rise)))
    base = spine_pts[-1]
    neck_segs = p["neck_segments"]
    neck_pts = [base + neck_dir * (neck_len * (i / neck_segs)) for i in range(neck_segs + 1)]
    neck, neck_tail = _axis_chain(arm.data, "neck", neck_pts, parent=spine_tail, connect=True)

    head_dir = _unit(neck_pts[-1] - neck_pts[-2])
    head_pts = [neck_pts[-1],
                neck_pts[-1] + head_dir * head_len * 0.5,
                neck_pts[-1] + head_dir * head_len]
    head, head_tail = _axis_chain(arm.data, "head", head_pts, parent=neck_tail, connect=True)

    tail_segs = p["tail_segments"]
    tdir = _unit(spine_pts[0] - spine_pts[1])
    tread = math.radians(p["tail_rise_deg"])
    # keep a fraction of the tail clear of the ground, matching creature.py
    tail_droop = min(tail_len * tread, spine_pts[0].z * 0.55)
    tail_pts = [spine_pts[0]]
    for i in range(1, tail_segs + 1):
        t = i / tail_segs
        tail_pts.append(spine_pts[0] + tdir * (tail_len * t)
                        + mathutils.Vector((0.0, 0.0, -tail_droop * t * t)))
    tail, tail_tail = _axis_chain(arm.data, "tail", tail_pts, parent=spine[0])
    for i in range(1, len(tail)):
        tail[i].parent = tail[i - 1]
        tail[i].use_connect = True

    spread = math.radians(p["leg_spread_deg"])
    upper, lower = p["leg_upper_m"], p["leg_lower_m"]
    limbs = {}
    for label, t, sign in (("front_L", 0.80, 1.0), ("front_R", 0.80, -1.0),
                           ("rear_L", 0.16, 1.0), ("rear_R", 0.16, -1.0)):
        anchor, depth = trunk_point(t)
        width = p["trunk_width_m"] * (0.5 + 0.5 * math.sin(math.pi * t) ** 0.7)
        hip = anchor + mathutils.Vector((0.0, sign * width * 0.425, 0.0))
        parent_bone = spine[min(int(t * trunk_segs), trunk_segs - 1)]
        upper_dir = mathutils.Vector((0.0, sign * math.sin(spread), -math.cos(spread)))
        knee = hip + upper_dir * upper
        ankle = knee + _unit(mathutils.Vector((0.0, sign * math.sin(spread * 0.35), -1.0))) * lower
        pts = [hip, knee, ankle]
        bones, tail_b = _axis_chain(arm.data, f"leg_{label}", pts, parent=parent_bone)
        for i in range(1, len(bones)):
            bones[i].parent = bones[i - 1]
            bones[i].use_connect = True
        # toe
        toe = arm.data.edit_bones.new(f"toe_{label}")
        toe.head = ankle
        toe.tail = ankle + mathutils.Vector((head_len * 0.6, 0.0, -max(0.01, stance * 0.04)))
        toe.parent = tail_b
        toe.use_connect = True
        limbs[label] = {"bones": len(bones) + 1, "chain": pts, "toe": toe.head.copy()}

    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="OBJECT")

    stats.update({
        "bone_count": len(arm.data.bones),
        "spine_bones": len(spine),
        "neck_bones": len(neck),
        "head_bones": len(head),
        "tail_bones": len(tail),
        "limb_bones": sum(v["bones"] for v in limbs.values()),
        "limb_chains": sorted(limbs.keys()),
        "roots": [b.name for b in arm.data.bones if b.parent is None],
        "single_root": len([b for b in arm.data.bones if b.parent is None]) == 1,
    })
    return {"spine": spine_pts, "neck": neck_pts, "head": head_pts, "tail": tail_pts,
            "limbs": limbs, "trunk_segs": trunk_segs}


def skin(body: bpy.types.Object, detail: bpy.types.Object, arm: bpy.types.Object,
         report: S.Reporter, stats: dict[str, Any]) -> None:
    """Automatic weights, with an explicit fallback if Blender refuses."""
    warnings = report.warnings
    bpy.ops.object.select_all(action="DESELECT")
    targets = [o for o in (body, detail) if o and o.data.polygons]
    for obj in targets + [arm]:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = arm
    try:
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")
        stats["weighting"] = "ARMATURE_AUTO"
    except RuntimeError as exc:
        warnings.append(f"automatic weights failed ({exc}); falling back to envelope parent")
        for obj in targets:
            mod = obj.modifiers.new("armature", "ARMATURE")
            mod.object = arm
            obj.parent = arm
        stats["weighting"] = "ARMATURE_FALLBACK"
    stats["deform_bones"] = len([b for b in arm.data.bones if b.use_deform])
    stats["vertices_weighted"] = sum(len(o.data.vertices) for o in targets)
    for obj in targets:
        vg = obj.vertex_groups
        if not vg:
            warnings.append(f"{obj.name} has no vertex groups after weighting")
        else:
            unassigned = sum(1 for v in obj.data.vertices if not any(v.groups))
            if unassigned:
                warnings.append(f"{obj.name}: {unassigned} vertices carry no bone weight")


def neutral_pose(arm: bpy.types.Object, report: S.Reporter) -> dict[str, Any]:
    """Validate the neutral pose: no bone carries a non-identity pose transform."""
    arm.data.pose_position = "POSE"
    moved = []
    for pb in arm.pose.bones:
        if (abs(pb.rotation_euler.x) + abs(pb.rotation_euler.y) + abs(pb.rotation_euler.z)
                + abs(pb.location.x) + abs(pb.location.y) + abs(pb.location.z)
                + abs(pb.scale.x - 1.0) + abs(pb.scale.y - 1.0) + abs(pb.scale.z - 1.0)) > 1e-6:
            moved.append(pb.name)
    if moved:
        report.warn(f"neutral pose validation: {len(moved)} bone(s) carry a pose transform")
    bpy.context.view_layer.update()
    return {"neutral": not moved, "offset_bones": sorted(moved)}


def export_neutral(path: str, report: S.Reporter) -> str:
    import os

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=os.path.abspath(path), export_format="GLB",
                              export_animations=False, export_apply=True,
                              use_selection=False, export_yup=True)
    return os.path.abspath(path)


def main(params: dict[str, Any], report: S.Reporter) -> dict[str, Any]:
    p = dict(params["creature"])
    p["seed"] = params["seed"]
    S.reset_scene()
    stats: dict[str, Any] = {}

    with report.time("open_source"):
        source = params.get("source_blend")
        if source:
            bpy.ops.wm.open_mainfile(filepath=source, load_ui=False)

    body = bpy.data.objects.get("body_proxy")
    detail = bpy.data.objects.get("integument_proxy")
    if body is None:
        raise RuntimeError("body_proxy not found: open a creature source .blend first")

    with report.time("build_armature"):
        arm_data = bpy.data.armatures.new("creature_rig")
        arm = bpy.data.objects.new("creature_rig", arm_data)
        bpy.context.scene.collection.objects.link(arm)
        bpy.context.view_layer.objects.active = arm
        arm.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")
        build_skeleton(arm, p, stats)
        bpy.ops.object.mode_set(mode="OBJECT")

    with report.time("skin"):
        skin(body, detail, arm, report, stats)

    with report.time("neutral_pose"):
        stats["pose"] = neutral_pose(arm, report)

    artifacts = []
    with report.time("save"):
        blend_path = S.save_blend(params["outputs"]["blend"])
        artifacts.append({"path": blend_path, "role": "source", "kind": "blend"})
    with report.time("export_neutral"):
        glb = export_neutral(params["outputs"]["neutral_pose"], report)
        artifacts.append({"path": glb, "role": "runtime", "kind": "glb"})

    objects = [o for o in bpy.data.objects if o.type in {"MESH", "ARMATURE"}]
    metrics = S.scene_metrics(objects)
    metrics.update(stats)
    metrics["determinism"] = {"seed": p["seed"], "time_input_used": False}

    return {
        "metrics": metrics,
        "artifacts": artifacts,
        "object_names": [o.name for o in objects],
    }


if __name__ == "__main__":
    S.run_main("rig_creature", main)
