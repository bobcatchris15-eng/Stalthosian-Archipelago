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
import json
from typing import Any

import bpy
import mathutils
from mathutils import Quaternion, Vector


def _get_action_fcurves(act: bpy.types.Action) -> list[bpy.types.FCurve]:
    """Return all F-Curves in an action across Blender legacy and layered slot APIs."""
    if hasattr(act, "fcurves") and len(act.fcurves) > 0:
        return list(act.fcurves)
    curves: list[bpy.types.FCurve] = []
    if hasattr(act, "layers"):
        for layer in act.layers:
            for strip in layer.strips:
                if hasattr(strip, "channelbags"):
                    for cb in strip.channelbags:
                        curves.extend(cb.fcurves)
                elif hasattr(strip, "channelbag"):
                    curves.extend(strip.channelbag.fcurves)
    return curves


import os as _os
import sys as _sys

# Blender does not put the --python script's directory on sys.path
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))

import _stalthos as S

S.bootstrap()
import gait as GAIT


RIG_SOURCE_PROPERTY = "stalthos_rig_source_v1"
RIG_SOURCE_SCHEMA = "stalthos-creature-rig-source-v1"
RIG_SHAPE_REQUIRED = (
    "stance_height_m", "trunk_length_m", "trunk_depth_m", "trunk_width_m",
    "trunk_arch", "trunk_segments", "neck_length_m", "neck_segments",
    "tail_length_m", "tail_segments", "tail_rise_deg", "head_length_m",
    "head_depth_m", "leg_spread_deg", "leg_upper_m", "leg_lower_m",
    "leg_diameter_m",
)
RIG_LIMB_NAMES = ("front_left", "front_right", "rear_left", "rear_right")


def _source_record(body: bpy.types.Object) -> dict[str, Any] | None:
    """Load and validate the source's resolved shape/joint data; None is legacy."""
    raw = body.get(RIG_SOURCE_PROPERTY)
    if raw is None:
        return None
    try:
        record = json.loads(str(raw))
    except (TypeError, ValueError) as exc:
        raise RuntimeError(f"invalid {RIG_SOURCE_PROPERTY} JSON: {exc}") from exc
    if not isinstance(record, dict) or record.get("schema") != RIG_SOURCE_SCHEMA:
        raise RuntimeError(f"unsupported or malformed {RIG_SOURCE_PROPERTY} schema")
    shape = record.get("shape")
    graph = record.get("joints")
    if not isinstance(shape, dict) or not isinstance(graph, dict):
        raise RuntimeError(f"{RIG_SOURCE_PROPERTY} must contain shape and joints objects")
    missing = [name for name in RIG_SHAPE_REQUIRED if name not in shape]
    if missing:
        raise RuntimeError(f"{RIG_SOURCE_PROPERTY} shape is missing: {', '.join(missing)}")
    for name in RIG_SHAPE_REQUIRED:
        try:
            value = float(shape[name])
        except (TypeError, ValueError) as exc:
            raise RuntimeError(f"{RIG_SOURCE_PROPERTY} shape {name} is not numeric") from exc
        if not math.isfinite(value):
            raise RuntimeError(f"{RIG_SOURCE_PROPERTY} shape {name} is non-finite")
    for name in ("trunk_length_m", "neck_length_m", "tail_length_m", "head_length_m",
                 "leg_upper_m", "leg_lower_m", "leg_diameter_m", "stance_height_m"):
        if float(shape[name]) <= 0.0:
            raise RuntimeError(f"{RIG_SOURCE_PROPERTY} shape {name} must be positive")

    def valid_chain(name: str, *, minimum: int = 2) -> None:
        rows = graph.get(name)
        if not isinstance(rows, list) or len(rows) < minimum:
            raise RuntimeError(f"{RIG_SOURCE_PROPERTY} joints.{name} must have at least {minimum} points")
        for row in rows:
            if (not isinstance(row, list) or len(row) != 3 or
                    not all(isinstance(value, (int, float)) and math.isfinite(value) for value in row)):
                raise RuntimeError(f"{RIG_SOURCE_PROPERTY} joints.{name} contains an invalid point")

    for name in ("spine", "neck", "head", "tail"):
        valid_chain(name)
    limbs = graph.get("limbs")
    if not isinstance(limbs, dict) or set(limbs) != set(RIG_LIMB_NAMES):
        raise RuntimeError(f"{RIG_SOURCE_PROPERTY} must contain all four named limbs")
    for name, row in limbs.items():
        if not isinstance(row, dict):
            raise RuntimeError(f"{RIG_SOURCE_PROPERTY} joints.limbs.{name} must be an object")
        for key in ("hip", "knee", "ankle", "foot"):
            point = row.get(key)
            if (not isinstance(point, list) or len(point) != 3 or
                    not all(isinstance(value, (int, float)) and math.isfinite(value) for value in point)):
                raise RuntimeError(f"{RIG_SOURCE_PROPERTY} joints.limbs.{name}.{key} is invalid")
        pad_indices = row.get("pad_vertex_indices")
        if (not isinstance(pad_indices, list) or not pad_indices or
                any(not isinstance(index, int) or index < 0 for index in pad_indices) or
                len(set(pad_indices)) != len(pad_indices)):
            raise RuntimeError(f"{RIG_SOURCE_PROPERTY} joints.limbs.{name}.pad_vertex_indices is invalid")
    return record


def _unit(v: mathutils.Vector) -> mathutils.Vector:
    return v.normalized() if v.length > 1e-9 else mathutils.Vector((0.0, 0.0, 1.0))


def _support_offsets(p: dict[str, Any]) -> tuple[float, float]:
    """Local copy of anatomy_pose.support_offsets for trunk/rig alignment."""
    stance = float(p["stance_height_m"])
    trunk_len = float(p["trunk_length_m"])
    depth = float(p["trunk_depth_m"])
    arch = float(p["trunk_arch"])
    spread = math.radians(float(p["leg_spread_deg"]))
    foot_drop = max(0.02, stance * 0.06)
    pad_half = (float(p["leg_diameter_m"]) * 0.5) * 0.5
    upper, lower = float(p["leg_upper_m"]), float(p["leg_lower_m"])
    lower_vertical = 1.0 / math.sqrt(1.0 + math.sin(spread * 0.35) ** 2)

    def offset(t: float) -> float:
        bulge = math.sin(math.pi * t) ** 0.7
        hip_z = stance + depth * (0.45 + 0.55 * bulge) * 0.55
        hip_z += arch * trunk_len * math.sin(math.pi * t)
        raw_pad_bottom = hip_z - upper * math.cos(spread) - lower * lower_vertical
        raw_pad_bottom -= foot_drop + pad_half
        return 0.004 - raw_pad_bottom

    return offset(0.16), offset(0.80)


def _aim_bone(pb: bpy.types.PoseBone, desired_arm_direction: mathutils.Vector) -> None:
    """Rotate a pose bone so its head-to-tail axis follows an armature-space vector."""
    desired = _unit(desired_arm_direction)
    if pb.parent:
        parent_pose_rotation = pb.parent.matrix.to_3x3()
        parent_rest_rotation = pb.parent.bone.matrix_local.to_3x3()
        bone_rest_rotation = pb.bone.matrix_local.to_3x3()
        relative_rest = parent_rest_rotation.inverted() @ bone_rest_rotation
        desired_parent = parent_pose_rotation.inverted() @ desired
        rest_parent = relative_rest @ mathutils.Vector((0.0, 1.0, 0.0))
    else:
        relative_rest = pb.bone.matrix_local.to_3x3()
        desired_parent = desired
        rest_parent = relative_rest @ mathutils.Vector((0.0, 1.0, 0.0))
    delta = rest_parent.rotation_difference(desired_parent)
    local_delta = relative_rest.inverted() @ delta.to_matrix().to_3x3() @ relative_rest
    pb.rotation_mode = "QUATERNION"
    pb.rotation_quaternion = local_delta.to_quaternion()


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


def _build_skeleton_from_source(arm: bpy.types.Object, p: dict[str, Any],
                                stats: dict[str, Any], graph: dict[str, Any]) -> dict[str, Any]:
    """Build named bones directly on the source mesh's serialized joint graph."""
    eb = arm.data.edit_bones
    for bone in list(eb):
        eb.remove(bone)

    vectors = lambda name: [mathutils.Vector(point) for point in graph[name]]
    spine_pts, neck_pts = vectors("spine"), vectors("neck")
    head_pts, tail_pts = vectors("head"), vectors("tail")
    spine, spine_tail = _axis_chain(arm.data, "spine", spine_pts)
    for i in range(1, len(spine)):
        spine[i].parent = spine[i - 1]
        spine[i].use_connect = True

    neck_parent = min(spine, key=lambda bone: (bone.head - neck_pts[0]).length)
    neck, neck_tail = _axis_chain(arm.data, "neck", neck_pts,
                                  parent=neck_parent, connect=False)
    head, _ = _axis_chain(arm.data, "head", head_pts,
                          parent=neck_tail, connect=False)
    for chain in (neck, head):
        for index in range(1, len(chain)):
            chain[index].parent = chain[index - 1]
            chain[index].use_connect = True
    tail, _ = _axis_chain(arm.data, "tail", tail_pts, parent=spine[0])
    for i in range(1, len(tail)):
        tail[i].parent = tail[i - 1]
        tail[i].use_connect = True

    limbs = {}
    name_map = (("front_left", "front_L"), ("front_right", "front_R"),
                ("rear_left", "rear_L"), ("rear_right", "rear_R"))
    for source_name, label in name_map:
        row = graph["limbs"][source_name]
        hip, knee, ankle = (mathutils.Vector(row[key]) for key in ("hip", "knee", "ankle"))
        # Attach each limb to the closest generated spine segment. The actual
        # hip/knee/ankle coordinates come from the source graph, not YAML.
        parent_index = min(range(len(spine)),
                           key=lambda index: (spine[index].head - hip).length)
        parent_bone = spine[parent_index]
        bones, lower_bone = _axis_chain(arm.data, f"leg_{label}", [hip, knee, ankle],
                                         parent=parent_bone)
        bones[1].parent = bones[0]
        bones[1].use_connect = True
        toe = arm.data.edit_bones.new(f"toe_{label}")
        toe.head = ankle
        toe.tail = ankle + mathutils.Vector((float(p["head_length_m"]) * 0.6,
                                              0.0, -max(0.01, float(p["stance_height_m"]) * 0.04)))
        toe.parent = lower_bone
        toe.use_connect = True
        limbs[label] = {"bones": 3, "chain": [hip, knee, ankle],
                        "toe": toe.head.copy(), "pad_vertex_indices": row["pad_vertex_indices"]}

    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="OBJECT")
    stats.update({
        "bone_count": len(arm.data.bones), "spine_bones": len(spine),
        "neck_bones": len(neck), "head_bones": len(head), "tail_bones": len(tail),
        "limb_bones": sum(row["bones"] for row in limbs.values()),
        "limb_chains": sorted(limbs),
        "roots": [bone.name for bone in arm.data.bones if bone.parent is None],
        "single_root": len([bone for bone in arm.data.bones if bone.parent is None]) == 1,
        "rig_source_graph_used": True,
    })
    return {"spine": spine_pts, "neck": neck_pts, "head": head_pts,
            "tail": tail_pts, "limbs": limbs, "trunk_segs": len(spine_pts) - 1}


def build_skeleton(arm: bpy.types.Object, p: dict[str, Any], stats: dict[str, Any],
                   joint_graph: dict[str, Any] | None = None) -> dict[str, Any]:
    """Deterministic quadruped skeleton: spine chain, neck, tail, 4 limb chains."""
    if joint_graph is not None:
        return _build_skeleton_from_source(arm, p, stats, joint_graph)
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
        rear_support, front_support = _support_offsets(p)
        blend = min(1.0, max(0.0, (t - 0.16) / (0.80 - 0.16)))
        z += rear_support + (front_support - rear_support) * blend
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
         report: S.Reporter, stats: dict[str, Any], extras: tuple[bpy.types.Object, ...] = ()) -> None:
    """Automatic weights, with an explicit fallback if Blender refuses."""
    warnings = report.warnings
    bpy.ops.object.select_all(action="DESELECT")
    targets = [o for o in (body, detail, *extras) if o and o.data.polygons]
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
    stats["skinned_objects"] = [o.name for o in targets]
    stats["skinned_feature_material_slots"] = {
        obj.name: len(obj.material_slots) for obj in extras if obj and obj.data.polygons}
    for obj in targets:
        vg = obj.vertex_groups
        if not vg:
            warnings.append(f"{obj.name} has no vertex groups after weighting")
        else:
            unassigned = sum(1 for v in obj.data.vertices if not any(v.groups))
            if unassigned:
                warnings.append(f"{obj.name}: {unassigned} vertices carry no bone weight")


def _joint_alignment_metrics(arm: bpy.types.Object, graph: dict[str, Any]) -> dict[str, Any]:
    """Compare every rest-bone endpoint with the serialized source graph."""
    rows = {}

    def chain_error(prefix: str, points: list[list[float]]) -> float:
        errors = []
        for index in range(len(points) - 1):
            bone = arm.data.bones.get(f"{prefix}_{index:02d}")
            if bone is None:
                return math.inf
            errors.append((bone.head_local - mathutils.Vector(points[index])).length)
            errors.append((bone.tail_local - mathutils.Vector(points[index + 1])).length)
        return max(errors, default=0.0)

    for chain_name in ("spine", "neck", "head", "tail"):
        rows[chain_name] = chain_error(chain_name, graph[chain_name])
    limb_names = (("front_left", "front_L"), ("front_right", "front_R"),
                  ("rear_left", "rear_L"), ("rear_right", "rear_R"))
    for source_name, label in limb_names:
        source = graph["limbs"][source_name]
        upper = arm.data.bones.get(f"leg_{label}_00")
        lower = arm.data.bones.get(f"leg_{label}_01")
        toe = arm.data.bones.get(f"toe_{label}")
        if not upper or not lower or not toe:
            rows[f"limbs.{source_name}"] = math.inf
            continue
        targets = ((upper.head_local, source["hip"]),
                   (upper.tail_local, source["knee"]),
                   (lower.tail_local, source["ankle"]),
                   (toe.head_local, source["ankle"]))
        rows[f"limbs.{source_name}"] = max(
            (actual - mathutils.Vector(target)).length for actual, target in targets)
    return {"max_error_m": max(rows.values(), default=0.0), "per_chain_error_m": rows}


def _feature_skin_metrics(objects: tuple[bpy.types.Object, ...], arm: bpy.types.Object) -> dict[str, Any]:
    rows = {}
    for obj in objects:
        if obj is None or obj.type != "MESH" or not obj.data.polygons:
            continue
        modifier = next((mod for mod in obj.modifiers
                         if mod.type == "ARMATURE" and mod.object == arm), None)
        bounds = [obj.matrix_world @ mathutils.Vector(corner) for corner in obj.bound_box]
        rows[obj.name] = {
            "armature_modifier_bound": modifier is not None,
            "vertex_group_count": len(obj.vertex_groups),
            "material_slots_preserved": len(obj.material_slots),
            "vertex_count": len(obj.data.vertices),
            "unassigned_vertex_count": sum(1 for vertex in obj.data.vertices if not vertex.groups),
            "world_bounds_m": {
                "min": [min(point[axis] for point in bounds) for axis in range(3)],
                "max": [max(point[axis] for point in bounds) for axis in range(3)],
            },
        }
    return rows


def _bind_bristles_to_nearest_bone_by_component(obj: bpy.types.Object, arm: bpy.types.Object,
                                                prefixes: tuple[str, ...]) -> int:
    """Rigidly bind connected components (bristles) to the nearest eligible bone using centroid."""
    if obj is None or arm is None or obj.type != "MESH" or obj.data is None:
        raise ValueError("Invalid mesh or armature object for bristle component binding")
    if not obj.data.vertices:
        return 0

    import bmesh
    vertices = list(range(len(obj.data.vertices)))
    for group in obj.vertex_groups:
        group.remove(vertices)
    candidates = sorted(
        [bone for bone in arm.data.bones if bone.use_deform and bone.name.startswith(prefixes)],
        key=lambda b: b.name,
    )
    if not candidates:
        raise RuntimeError(f"no rig bones match feature binding prefixes {prefixes!r}")

    for row in obj.matrix_world:
        if not all(math.isfinite(val) for val in row):
            raise ValueError(f"non-finite matrix_world on {obj.name}")
    for row in arm.matrix_world:
        if not all(math.isfinite(val) for val in row):
            raise ValueError(f"non-finite matrix_world on {arm.name}")

    det = arm.matrix_world.determinant()
    if not math.isfinite(det) or abs(det) <= 1e-12:
        raise ValueError(f"armature matrix_world is singular or non-finite (det={det})")
    arm_inverse = arm.matrix_world.inverted()

    groups = {bone.name: obj.vertex_groups.get(bone.name) or
              obj.vertex_groups.new(name=bone.name) for bone in candidates}

    bm = bmesh.new()
    try:
        bm.from_mesh(obj.data)
        bm.verts.index_update()
        bm.verts.ensure_lookup_table()
        bm.edges.ensure_lookup_table()

        visited = set()
        components = []
        for v in bm.verts:
            if v.index not in visited:
                comp = []
                stack = [v]
                visited.add(v.index)
                while stack:
                    curr = stack.pop()
                    comp.append(curr.index)
                    for edge in sorted(curr.link_edges, key=lambda e: e.other_vert(curr).index, reverse=True):
                        other = edge.other_vert(curr)
                        if other.index not in visited:
                            visited.add(other.index)
                            stack.append(other)
                components.append(sorted(comp))
        components.sort(key=lambda c: c[0] if c else -1)

        for comp in components:
            centroid = mathutils.Vector((0.0, 0.0, 0.0))
            for idx in comp:
                co = obj.data.vertices[idx].co
                if not all(math.isfinite(val) for val in co):
                    raise ValueError(f"non-finite vertex co at index {idx} in {obj.name}")
                centroid += co
            centroid /= len(comp)
            if not all(math.isfinite(val) for val in centroid):
                raise ValueError(f"non-finite centroid computed for component in {obj.name}")

            point = arm_inverse @ (obj.matrix_world @ centroid)
            if not all(math.isfinite(val) for val in point):
                raise ValueError(f"non-finite local point computed for component in {obj.name}")

            best = None
            best_key = (float("inf"), "")
            for bone in candidates:
                start, end = bone.head_local, bone.tail_local
                delta = end - start
                length_sq = delta.length_squared
                t = 0.0 if length_sq <= 1e-12 else max(0.0, min(1.0, (point - start).dot(delta) / length_sq))
                distance = (point - (start + delta * t)).length_squared
                if not math.isfinite(distance):
                    raise ValueError(f"non-finite bone distance computed for bone {bone.name}")
                key = (distance, bone.name)
                if key < best_key:
                    best = bone
                    best_key = key

            if best is None:
                raise RuntimeError(f"failed to find nearest bone for bristle component in {obj.name}")

            groups[best.name].add(comp, 1.0, "REPLACE")

        return len(components)
    finally:
        bm.free()


def _bind_feature_to_nearest_bone(obj: bpy.types.Object, arm: bpy.types.Object,
                                  prefixes: tuple[str, ...]) -> None:
    """Rigidly bind an isolated feature mesh to its nearest anatomically relevant bone."""
    vertices = list(range(len(obj.data.vertices)))
    for group in obj.vertex_groups:
        group.remove(vertices)
    candidates = [bone for bone in arm.data.bones if bone.use_deform and
                  bone.name.startswith(prefixes)]
    if not candidates:
        raise RuntimeError(f"no rig bones match feature binding prefixes {prefixes!r}")
    groups = {bone.name: obj.vertex_groups.get(bone.name) or
              obj.vertex_groups.new(name=bone.name) for bone in candidates}
    arm_inverse = arm.matrix_world.inverted()
    for vertex in obj.data.vertices:
        point = arm_inverse @ (obj.matrix_world @ vertex.co)
        best = None
        best_distance = float("inf")
        for bone in candidates:
            start, end = bone.head_local, bone.tail_local
            delta = end - start
            length_sq = delta.length_squared
            t = 0.0 if length_sq <= 1e-12 else max(0.0, min(1.0, (point - start).dot(delta) / length_sq))
            distance = (point - (start + delta * t)).length_squared
            if distance < best_distance:
                best, best_distance = bone, distance
        groups[best.name].add([vertex.index], 1.0, "REPLACE")


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


def _author_neutral_idle(arm: 'bpy.types.Object', body: 'bpy.types.Object', scene: 'bpy.types.Scene', fps: int, seed: int, target_bones: dict, diagnostic_limbs: dict, pad_indices: dict, metadata: dict) -> 'bpy.types.Action':
    import bpy
    import math
    from mathutils import Quaternion, Vector

    # --- Author Neutral Idle Action ---
    # Distinct clearly named neutral standing idle action/clip with all four feet
    # in the neutral/rest support posture, no actor-root translation, and a validated
    # seamless idle loop.
    idle_action = bpy.data.actions.new("Smalltrotter_NeutralIdle")

    # Switch active action to idle. No NLA tracks exist yet so zero NLA blending during authoring/evaluation.
    arm.animation_data.action = idle_action

    idle_first_frame, idle_last_frame = 1, int(fps * 2.0) + 1  # 2 second idle

    import random
    rng = random.Random(int(seed))
    sway_yaw = rng.uniform(-0.004, 0.004)
    sway_pitch = rng.uniform(-0.003, 0.003)

    # Reset all bones and root to neutral pose for the idle
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = Quaternion((1.0, 0.0, 0.0, 0.0))
        pb.location = Vector((0.0, 0.0, 0.0))

    arm.location = Vector((0.0, 0.0, 0.0))
    bpy.context.view_layer.update()

    # Keyframe a seamless 2 second loop. Keep the torso and support chain neutral;
    # only neck and head rotations receive small, bounded motion.

    intervals_idle = idle_last_frame - idle_first_frame
    for frame in range(idle_first_frame, idle_last_frame + 1):
        phase = (frame - idle_first_frame) / intervals_idle

        # 1 cycle of breathing over 2 seconds
        breath_cycle = math.sin(phase * 2.0 * math.pi)

        # 1 cycle of sway over 2 seconds
        # using cosine for sway to have it slightly offset from breathing
        sway_cycle = -math.cos(phase * 2.0 * math.pi) + 1.0 # 0 to 2

        for pb in arm.pose.bones:
            if pb.name.startswith("neck_") or pb.name.startswith("head_"):
                # Pose is assigned from neutral each frame, so motion stays bounded.
                pitch = breath_cycle * 0.003 if pb.name.startswith("neck_") else 0.0

                # tiny head gaze sway
                yaw = 0.0
                if pb.name.startswith("head_"):
                    yaw = (sway_cycle - 1.0) * sway_yaw
                    pitch = breath_cycle * 0.002 + (sway_cycle - 1.0) * sway_pitch

                # euler to quaternion
                from mathutils import Euler
                eu = Euler((pitch, yaw, 0.0), 'XYZ')
                pb.rotation_quaternion = eu.to_quaternion()
                pb.keyframe_insert(data_path="rotation_quaternion", frame=frame, group=pb.name)

                # Insert location for completeness (remains 0)
                pb.keyframe_insert(data_path="location", frame=frame, group=pb.name)
            else:
                if frame == idle_first_frame or frame == idle_last_frame:
                    pb.keyframe_insert(data_path="rotation_quaternion", frame=frame, group=pb.name)
                    pb.keyframe_insert(data_path="location", frame=frame, group=pb.name)

        if frame == idle_first_frame or frame == idle_last_frame:
            arm.keyframe_insert(data_path="location", frame=frame, group="root_motion")

    # Actually evaluate all 4 pads and actor root through frames without native terrain acceptance
    idle_root_translations = []
    idle_pad_z = {selector: [] for selector in target_bones}
    idle_pad_xy = {selector: [] for selector in target_bones}
    idle_front_z = {selector: [] for selector in diagnostic_limbs}
    idle_front_xy = {selector: [] for selector in diagnostic_limbs}

    for frame in range(idle_first_frame, idle_last_frame + 1):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        idle_root_translations.append(float(arm.location.length))
        body_eval = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
        evaluated_mesh = body_eval.to_mesh()
        for selector in target_bones:
            indices = pad_indices.get(selector, [])
            if indices and max(indices) < len(evaluated_mesh.vertices):
                wz = [(body_eval.matrix_world @ evaluated_mesh.vertices[i].co).z for i in indices]
                idle_pad_z[selector].append(float(min(wz)))
                wxy = [(body_eval.matrix_world @ evaluated_mesh.vertices[i].co)[:2] for i in indices]
                idle_pad_xy[selector].append((sum(p[0] for p in wxy) / len(wxy), sum(p[1] for p in wxy) / len(wxy)))
        for selector in diagnostic_limbs:
            indices = diagnostic_limbs.get(selector, [])
            if indices and max(indices) < len(evaluated_mesh.vertices):
                wz = [(body_eval.matrix_world @ evaluated_mesh.vertices[i].co).z for i in indices]
                idle_front_z[selector].append(float(min(wz)))
                wxy = [(body_eval.matrix_world @ evaluated_mesh.vertices[i].co)[:2] for i in indices]
                idle_front_xy[selector].append((sum(p[0] for p in wxy) / len(wxy), sum(p[1] for p in wxy) / len(wxy)))
        body_eval.to_mesh_clear()

    scene.frame_set(idle_first_frame)
    idle_max_root_translation_m = max(idle_root_translations) if idle_root_translations else 0.0
    idle_root_translation_zero = bool(idle_max_root_translation_m <= 1e-6)

    idle_hind_excursions = {
        s: (max(math.hypot(p[0] - idle_pad_xy[s][0][0], p[1] - idle_pad_xy[s][0][1]) for p in idle_pad_xy[s]) if idle_pad_xy[s] else 0.0)
        for s in target_bones
    }
    idle_front_excursions = {
        s: (max(math.hypot(p[0] - idle_front_xy[s][0][0], p[1] - idle_front_xy[s][0][1]) for p in idle_front_xy[s]) if idle_front_xy[s] else 0.0)
        for s in diagnostic_limbs
    }

    # Validate seamless loop for idle via action F-curves
    idle_loop_ok = True
    idle_loop_errors = {}
    for fcurve in _get_action_fcurves(idle_action):
        start_val = fcurve.evaluate(idle_first_frame)
        end_val = fcurve.evaluate(idle_last_frame)
        err = abs(start_val - end_val)
        if err > 1e-6:
            idle_loop_ok = False
            idle_loop_errors[fcurve.data_path] = max(idle_loop_errors.get(fcurve.data_path, 0.0), err)

    metadata["idle"] = {
        "clip": "Smalltrotter_NeutralIdle",
        "duration_s": 2.0,
        "fps": fps,
        "frame_range": [idle_first_frame, idle_last_frame],
        "validation": {
            "loop_passed": idle_loop_ok,
            "root_translation_zero": idle_root_translation_zero,
            "root_translation_max_m": idle_max_root_translation_m,
            "terrain_contact_asserted": False,
            "note": "idle validation kept separate from walk cycle, does not assert native terrain contact"
        },
        "measured_support": {
            "four_pads_evaluated": list(target_bones.keys()) + list(diagnostic_limbs.keys()),
            "hind_pad_excursions_m": idle_hind_excursions,
            "front_pad_excursions_m": idle_front_excursions,
            "hind_pad_elevation_min_m": {s: min(idle_pad_z[s]) if idle_pad_z[s] else None for s in target_bones},
            "front_pad_elevation_min_m": {s: min(idle_front_z[s]) if idle_front_z[s] else None for s in diagnostic_limbs},
        }
    }
    if idle_loop_errors:
        metadata["idle"]["loop_errors"] = idle_loop_errors

    return idle_action


def author_walk_cycle(arm: bpy.types.Object, contacts: list[dict[str, Any]],
                      shape: dict[str, Any],
                      seed: int, output_glb: str, metadata_path: str,
                      report: S.Reporter,
                      source_joints: dict[str, Any] | None = None) -> tuple[str, dict[str, Any]]:
    """Author a root-motion walk cycle with two-bone targets and measured pads."""
    import json
    import os

    from mathutils import Quaternion, Vector

    contact = next((c for c in contacts if c.get("id") == "walk_cycle"), {})
    targets = contact.get("targets", [])
    if not targets:
        raise RuntimeError("walk_cycle contact definition has no targets")
    target_bones = {}
    tolerance = {}
    for target in targets:
        selector = target.get("selector")
        if selector not in GAIT.CONTACT_BONE_ALIASES:
            raise RuntimeError(f"unsupported walk_cycle contact selector: {selector!r}")
        if selector in target_bones:
            raise RuntimeError(f"duplicate walk_cycle contact selector: {selector}")
        target_bones[selector] = GAIT.CONTACT_BONE_ALIASES[selector]
        tolerance[selector] = float(target["tolerance_m"])
    body = bpy.data.objects.get("body_proxy")
    if body is None:
        raise RuntimeError("walk_cycle contact checks require body_proxy foot-pad geometry")
    head_len = float(shape["head_length_m"])
    stance_height = float(shape["stance_height_m"])
    leg_radius = float(shape["leg_diameter_m"]) * 0.5
    foot_drop = max(0.02, stance_height * 0.06)
    pad_indices = {}
    source_pad_names = {"bone:pes_foot_l": "rear_left", "bone:pes_foot_r": "rear_right"}
    front_pad_names = {"diagnostic:manus_hand_l": "front_left", "diagnostic:manus_hand_r": "front_right"}
    front_bone_aliases = {"diagnostic:manus_hand_l": "toe_front_L", "diagnostic:manus_hand_r": "toe_front_R"}
    diagnostic_limbs = {}
    diagnostic_coverage = {}

    if source_joints is not None:
        for selector, limb_key in front_pad_names.items():
            limbs = source_joints.get("limbs") if isinstance(source_joints, dict) else None
            row = limbs.get(limb_key) if isinstance(limbs, dict) else None
            indices = row.get("pad_vertex_indices") if isinstance(row, dict) else None
            issue = None
            if not isinstance(indices, list):
                issue = "missing serialized front pad indices"
            elif not indices:
                issue = "serialized front pad indices are empty"
            elif any(not isinstance(index, int) or isinstance(index, bool) or
                     index < 0 or index >= len(body.data.vertices) for index in indices):
                issue = "serialized front pad indices are invalid for body_proxy"
            if issue is None:
                diagnostic_limbs[selector] = list(indices)
            diagnostic_coverage[selector] = {
                "available": issue is None,
                "vertex_count": len(indices) if issue is None else 0,
                "reason_unavailable": issue,
            }

    for selector, bone_name in target_bones.items():
        bone = arm.pose.bones.get(bone_name)
        if bone is None:
            raise RuntimeError(f"walk_cycle target bone is absent: {bone_name}")
        if source_joints is not None:
            pad_indices[selector] = list(
                source_joints["limbs"][source_pad_names[selector]]["pad_vertex_indices"])
            if max(pad_indices[selector]) >= len(body.data.vertices):
                raise RuntimeError(f"serialized source pad indices exceed body_proxy vertices for {selector}")
        else:
            # Legacy source masters predate serialized pad/joint data. Keep
            # their original geometric selector as a compatibility fallback.
            ankle_world = arm.matrix_world @ bone.head
            center_world = ankle_world + arm.matrix_world.to_3x3() @ Vector(
                (head_len * 0.25, 0.0, -foot_drop))
            center = body.matrix_world.inverted() @ center_world
            pad_indices[selector] = [v.index for v in body.data.vertices
                if abs(v.co.x - center.x) <= head_len * 0.5 + 1e-5
                and abs(v.co.y - center.y) <= leg_radius * 1.1 + 1e-5
                and abs(v.co.z - center.z) <= leg_radius * 0.5 + 1e-5]
        if not pad_indices[selector]:
            raise RuntimeError(f"no body_proxy foot-pad vertices found for {selector}")
    # Foot-pad vertices are a separate, box-shaped proxy feature. Automatic
    # weights can blend them into the adjacent trunk and upper-leg groups,
    # making the evaluated pad lag far behind a correctly placed ankle. Keep
    # each hind pad rigidly attached to its toe bone so contact measurements
    # and the visible support surface follow the same articulated foot.
    pad_weight_groups = {}
    for selector, bone_name in target_bones.items():
        group_name = bone_name
        group = body.vertex_groups.get(group_name)
        if group is None:
            group = body.vertex_groups.new(name=group_name)
        indices = pad_indices[selector]
        for existing in body.vertex_groups:
            existing.remove(indices)
        group.add(indices, 1.0, "REPLACE")
        pad_weight_groups[selector] = group_name
    duration_s, fps = 2.0, 24
    intervals = int(duration_s * fps)
    first_frame, last_frame = 1, intervals + 1
    # Keep the translating step conservative for this deep-bodied, short-limbed
    # form. The IK solver retains the authored segment lengths; stride is
    # reduced before it can force a leg to its extension limit.
    trunk_stride_cap = float(shape["trunk_length_m"]) * 0.30
    leg_stride_ratio = 0.60
    if shape.get("_reference_anatomy_profile") == "smalltrotter_reference_v1":
        # The reference form has authored broad feet and a deep trunk. Its
        # measured touchdown excursion needs a shorter root advance while
        # retaining the same planted-pad geometry and contact tolerance.
        leg_stride_ratio = 0.32
    leg_stride_cap = (float(shape["leg_upper_m"]) + float(shape["leg_lower_m"])) * leg_stride_ratio
    stride_length = max(0.05, min(trunk_stride_cap, leg_stride_cap))
    swing_lift = max(0.035, stance_height * 0.12)
    duty_factor = 0.62
    selector_limb = {selector: bone_name.removeprefix("toe_")
                      for selector, bone_name in target_bones.items()}
    limb_selector = {limb: selector for selector, limb in selector_limb.items()}
    contact_correction_max = {selector: 0.0 for selector in target_bones}
    contact_correction_history = {selector: [] for selector in target_bones}
    scene = bpy.context.scene
    scene.render.fps = fps
    scene.frame_start, scene.frame_end = first_frame, last_frame
    scene.frame_set(first_frame)

    action = bpy.data.actions.new("Smalltrotter_WalkCycle")
    arm.animation_data_create()
    arm.animation_data.action = action
    rest = {}
    for label in GAIT.LATERAL_SEQUENCE_PHASES:
        upper = arm.pose.bones.get(f"leg_{label}_00")
        lower = arm.pose.bones.get(f"leg_{label}_01")
        toe = arm.pose.bones.get(f"toe_{label}")
        if upper is None or lower is None or toe is None:
            raise RuntimeError(f"walk cycle requires rig bones for {label}")
        rest[label] = {"upper": upper, "lower": lower,
                       "toe": toe, "hip": upper.head.copy(), "ankle": lower.tail.copy(),
                       "toe_direction": toe.tail.copy() - toe.head.copy()}
    pose_channels = {label: [] for label in GAIT.LATERAL_SEQUENCE_PHASES}
    ik_samples = {label: [] for label in GAIT.LATERAL_SEQUENCE_PHASES}
    root_channels = []
    for frame in range(first_frame, last_frame + 1):
        phase = (frame - first_frame) / intervals
        arm.location.x = GAIT.root_motion_offset(phase, stride_length)
        root_channels.append(float(arm.location.x))
        for label, limb_phase in GAIT.LATERAL_SEQUENCE_PHASES.items():
            upper, lower = rest[label]["upper"], rest[label]["lower"]
            if phase == 1.0:
                # Enforce an exact local-pose loop while the root-motion track
                # carries the one-stride displacement at the boundary.
                start_pose = pose_channels[label][0]
                toe = rest[label]["toe"]
                upper.rotation_mode = lower.rotation_mode = toe.rotation_mode = "QUATERNION"
                upper.rotation_quaternion = Quaternion(start_pose[:4])
                lower.rotation_quaternion = Quaternion(start_pose[4:8])
                toe.rotation_quaternion = Quaternion(start_pose[8:12])
                for bone in (upper, lower, toe):
                    bone.keyframe_insert(data_path="rotation_quaternion", frame=frame,
                                         group=f"walk_{label}")
                pose_channels[label].append(start_pose)
                bpy.context.view_layer.update()
                continue
            target = GAIT.foot_trajectory(phase, limb_phase, duty_factor,
                                          stride_length, swing_lift_m=swing_lift)
            ankle = rest[label]["ankle"] + Vector((target["x_m"], 0.0, target["z_m"]))
            def apply_target(target_ankle):
                knee, reachable_ankle = GAIT.solve_two_bone_ik(
                    tuple(rest[label]["hip"]), tuple(target_ankle),
                    float(shape["leg_upper_m"]), float(shape["leg_lower_m"]),
                    (1.0, 0.0, 0.0))
                knee_vec = Vector(knee)
                _aim_bone(upper, knee_vec - rest[label]["hip"])
                bpy.context.view_layer.update()
                _aim_bone(lower, Vector(reachable_ankle) - knee_vec)
                bpy.context.view_layer.update()
                _aim_bone(rest[label]["toe"], rest[label]["toe_direction"])
                bpy.context.view_layer.update()

            apply_target(ankle)
            selector = limb_selector.get(label)
            if (selector is not None and
                    GAIT.stance_fraction(phase, limb_phase, duty_factor) >= 0.0):
                # Skinning blends the pad across nearby bones. Close the loop
                # against its actual evaluated bottom instead of trusting the
                # ideal two-bone endpoint as a proxy for surface contact.
                residuals = []
                for _ in range(8):
                    body_eval = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
                    evaluated_mesh = body_eval.to_mesh()
                    indices = pad_indices[selector]
                    bottom = min((body_eval.matrix_world @ evaluated_mesh.vertices[i].co).z
                                 for i in indices)
                    body_eval.to_mesh_clear()
                    correction = bottom - 0.004
                    residuals.append(correction)
                    contact_correction_max[selector] = max(
                        contact_correction_max[selector], abs(correction))
                    if abs(correction) < 0.001:
                        break
                    ankle.z -= correction
                    apply_target(ankle)
                contact_correction_history[selector].append(
                    {"phase": phase, "pad_bottom_correction_sequence_m": residuals})
            if GAIT.stance_fraction(phase, limb_phase, duty_factor) >= 0.0:
                _, reachable_ankle = GAIT.solve_two_bone_ik(
                    tuple(rest[label]["hip"]), tuple(ankle),
                    float(shape["leg_upper_m"]), float(shape["leg_lower_m"]),
                    (1.0, 0.0, 0.0))
                actual_ankle = lower.tail.copy()
                desired_ankle = Vector(reachable_ankle)
                ik_samples[label].append({
                    "phase": phase,
                    "desired_ankle_armature_m": list(desired_ankle),
                    "actual_ankle_armature_m": list(actual_ankle),
                    "ankle_error_m": (actual_ankle - desired_ankle).length})
            toe = rest[label]["toe"]
            for bone in (upper, lower, toe):
                bone.keyframe_insert(data_path="rotation_quaternion", frame=frame,
                                     group=f"walk_{label}")
            pose_channels[label].append(tuple(upper.rotation_quaternion) +
                                        tuple(lower.rotation_quaternion) +
                                        tuple(toe.rotation_quaternion))
        arm.keyframe_insert(data_path="location", index=0, frame=frame,
                            group="root_motion")
    loop_errors = {label: GAIT.loop_error(values[0], values[-1])
                   for label, values in pose_channels.items()}
    loop_ok = bool(loop_errors) and max(loop_errors.values()) <= 1e-7
    root_motion_delta = root_channels[-1] - root_channels[0]
    root_motion_ok = abs(root_motion_delta - stride_length) <= 1e-7

    ground_distances = {selector: [] for selector in target_bones}
    stance_distances = {selector: [] for selector in target_bones}
    stance_positions = {selector: [] for selector in target_bones}
    stance_phases = {selector: [] for selector in target_bones}
    stance_sample_details = {selector: [] for selector in target_bones}

    diag_ground_distances = {selector: [] for selector in diagnostic_limbs}
    diag_stance_distances = {selector: [] for selector in diagnostic_limbs}
    diag_stance_positions = {selector: [] for selector in diagnostic_limbs}
    diag_stance_phases = {selector: [] for selector in diagnostic_limbs}
    diag_stance_sample_details = {selector: [] for selector in diagnostic_limbs}

    sample_frames = list(range(first_frame, last_frame + 1))
    for frame in sample_frames:
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        body_eval = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
        evaluated_mesh = body_eval.to_mesh()
        for selector, bone_name in target_bones.items():
            indices = pad_indices.get(selector, [])
            if not indices or max(indices) >= len(evaluated_mesh.vertices):
                continue
            world_z = [(body_eval.matrix_world @ evaluated_mesh.vertices[i].co).z
                       for i in indices]
            ground_distances[selector].append(float(min(world_z)))
            world_xy = [(body_eval.matrix_world @ evaluated_mesh.vertices[i].co)[:2]
                        for i in indices]
            center_x = sum(p[0] for p in world_xy) / len(world_xy)
            center_y = sum(p[1] for p in world_xy) / len(world_xy)
            phase = (frame - first_frame) / intervals
            # Store the actor-local pad center. touchdown_excursion adds the
            # root-motion path and wraps touchdown samples across cycle edges.
            stance_positions[selector].append(
                (center_x - GAIT.root_motion_offset(phase, stride_length), center_y, 0.0))
            stance_phases[selector].append(phase)
            limb_phase = GAIT.LATERAL_SEQUENCE_PHASES[selector_limb[selector]]
            in_stance = GAIT.stance_fraction(phase, limb_phase, duty_factor) >= 0.0
            if in_stance:
                stance_distances[selector].append(float(min(world_z)))
                stance_sample_details[selector].append({
                    "phase": phase, "pad_bottom_z_m": float(min(world_z)),
                    "pad_center_world_xy_m": [center_x, center_y]})

        for selector, indices in list(diagnostic_limbs.items()):
            if not diagnostic_coverage[selector]["available"]:
                continue
            if not indices or max(indices) >= len(evaluated_mesh.vertices):
                diagnostic_coverage[selector].update({
                    "available": False,
                    "reason_unavailable": "front pad indices exceed evaluated mesh coverage",
                })
                continue
            world_z = [(body_eval.matrix_world @ evaluated_mesh.vertices[i].co).z
                       for i in indices]
            diag_ground_distances[selector].append(float(min(world_z)))
            world_xy = [(body_eval.matrix_world @ evaluated_mesh.vertices[i].co)[:2]
                        for i in indices]
            center_x = sum(p[0] for p in world_xy) / len(world_xy)
            center_y = sum(p[1] for p in world_xy) / len(world_xy)
            phase = (frame - first_frame) / intervals
            diag_stance_positions[selector].append(
                (center_x - GAIT.root_motion_offset(phase, stride_length), center_y, 0.0))
            diag_stance_phases[selector].append(phase)
            bone_name = front_bone_aliases[selector]
            limb_key = bone_name.removeprefix("toe_")
            limb_phase = GAIT.LATERAL_SEQUENCE_PHASES[limb_key]
            in_stance = GAIT.stance_fraction(phase, limb_phase, duty_factor) >= 0.0
            if in_stance:
                diag_stance_distances[selector].append(float(min(world_z)))
                diag_stance_sample_details[selector].append({
                    "phase": phase, "pad_bottom_z_m": float(min(world_z)),
                    "pad_center_world_xy_m": [center_x, center_y]})
        body_eval.to_mesh_clear()
    scene.frame_set(first_frame)
    for selector, coverage in diagnostic_coverage.items():
        if not coverage["available"]:
            diagnostic_limbs.pop(selector, None)
            diag_ground_distances.pop(selector, None)
            diag_stance_distances.pop(selector, None)
            diag_stance_positions.pop(selector, None)
            diag_stance_phases.pop(selector, None)
            diag_stance_sample_details.pop(selector, None)
    body_z = None
    if body is not None:
        body_z = {
            "min_m": min((body.matrix_world @ Vector(c)).z for c in body.bound_box),
            "max_m": max((body.matrix_world @ Vector(c)).z for c in body.bound_box),
        }
    neutral_toes = {}
    for name in ("toe_rear_L", "toe_rear_R", "toe_front_L", "toe_front_R"):
        bone = arm.pose.bones.get(name)
        if bone is not None:
            neutral_toes[name] = {
                "head_world_m": list(arm.matrix_world @ bone.head),
                "tail_world_m": list(arm.matrix_world @ bone.tail),
            }
    geometry = GAIT.contact_summary(ground_distances, tolerance)
    target_rows = geometry["targets"]
    stance_geometry = GAIT.contact_summary(stance_distances, tolerance)
    stance_rows = stance_geometry["targets"]
    each_target_stance_ok = bool(stance_rows) and all(
        row["samples"] > 0 and row["invalid_samples"] == 0 and
        row["within_tolerance"] == row["samples"]
        for row in stance_rows.values())
    slip = {selector: GAIT.touchdown_excursion(
                positions, stance_phases[selector],
                GAIT.LATERAL_SEQUENCE_PHASES[selector_limb[selector]], duty_factor,
                root_stride_m=stride_length)
            for selector, positions in stance_positions.items()}
    touchdown_lock_ok = bool(slip) and all(
        slip[selector]["max_excursion_m"] is not None and
        slip[selector]["max_excursion_m"] <= tolerance[selector]
        for selector in slip)

    diag_geometry = GAIT.contact_summary(diag_ground_distances, 0.0)
    diag_stance_geometry = GAIT.contact_summary(diag_stance_distances, 0.0)
    diag_slip = {selector: GAIT.touchdown_excursion(
                positions, diag_stance_phases[selector],
                GAIT.LATERAL_SEQUENCE_PHASES[front_bone_aliases[selector].removeprefix("toe_")], duty_factor,
                root_stride_m=stride_length)
            for selector, positions in diag_stance_positions.items()}

    diagnostic_unaccepted_front_limbs = {
        "metric_scope": "kinematic diagnostic only; does not affect authored hind-contact validation",
        "schedule_source": "inferred from the existing kinematic gait phase sequence; not an authored front contact schedule",
        "selector_to_bone_aliases": front_bone_aliases,
        "coverage": diagnostic_coverage,
        "foot_pad_vertex_counts": {selector: len(indices) for selector, indices in diagnostic_limbs.items()},
        "ground_geometry": diag_geometry,
        "nominal_stance_samples": diag_stance_geometry,
        "touchdown_anchored_stance_excursion": diag_slip,
        "stance_samples": diag_stance_sample_details,
        "note": "front metrics use an inferred kinematic schedule and do not participate in validation or authored contact checks because no front target schedule is defined"
    } if source_joints is not None else {}

    metadata = {
        "clip": "walk_cycle", "source_contact_id": contact.get("id"),
        "seed": int(seed), "duration_s": duration_s, "fps": fps,
        "frame_range": [first_frame, last_frame], "loop": {"passed": loop_ok,
            "max_channel_error": max(loop_errors.values()) if loop_errors else None,
            "per_limb_error": loop_errors,
            "stance_ik_samples": ik_samples,
            "root_motion_delta_m": root_motion_delta,
            "root_motion_matches_stride": root_motion_ok,
            "root_motion_is_extracted_per_cycle": True},
        "contacts": {**geometry, "nominal_stance_samples": stance_geometry,
            "each_target_all_stance_samples_within_tolerance": each_target_stance_ok,
            "touchdown_anchored_stance_excursion": slip,
            "each_target_touchdown_excursion_within_tolerance": touchdown_lock_ok,
            "stance_samples": stance_sample_details,
            "selector_to_bone_aliases": target_bones,
            "foot_pad_vertex_counts": {selector: len(indices) for selector, indices in pad_indices.items()},
            "foot_pad_selector_source": "serialized generated joint graph" if source_joints is not None else "legacy geometric fallback",
            "foot_pad_weight_groups": pad_weight_groups,
            "measurement": "every-frame evaluated armature-deformed hind foot-pad vertices against z=0; horizontal pad-centroid positions include the animated actor root transform",
            "diagnostic_unaccepted_front_limbs": diagnostic_unaccepted_front_limbs},
        "terrain_contact_height": {"label": "root-motion stance contact and touchdown excursion",
            "actor_stride_translation_included": True,
            "horizontal_foot_lock_checked": True,
            "stride_length_m": stride_length,
            "max_ankle_correction_to_measured_pad_m": contact_correction_max,
            "ankle_correction_history": contact_correction_history,
            "per_target_minimum_gap_m": {name: row["min_abs_ground_distance_m"]
                for name, row in target_rows.items()},
            "touchdown_anchored_max_excursion_m": {
                name: row["max_excursion_m"] for name, row in slip.items()}},
        "source_bounds_and_first_frame_pose": {"body_proxy_world_z_bounds_m": body_z,
            "toe_bone_endpoints_world_m": neutral_toes,
            "grounding_note": "contact is measured from actual deformed hind-pad vertices; body height and authored leg lengths are preserved"},
        "validation": {"passed": loop_ok and root_motion_ok and each_target_stance_ok and touchdown_lock_ok,
            "loop_passed": loop_ok, "root_motion_passed": root_motion_ok,
            "geometric_contact_passed": each_target_stance_ok,
            "touchdown_foot_lock_passed": touchdown_lock_ok},
        "limitations": ["YAML selector names do not exist in the proxy rig; explicit hind-toe aliases were used.",
            "Root-motion extraction by the eventual runtime has not been verified.",
            "Touchdown excursion is a sampled pad-centroid measure and retains a nonzero error within the authored 0.02 m tolerance.",
            "Proxy geometry and its automatically weighted body outside the pads are not a final creature asset."],
    }

    # --- Author Neutral Idle Action ---
    idle_action = _author_neutral_idle(
        arm=arm,
        body=body,
        scene=scene,
        fps=fps,
        seed=int(seed),
        target_bones=target_bones,
        diagnostic_limbs=diagnostic_limbs,
        pad_indices=pad_indices,
        metadata=metadata
    )

    # Push both walk and idle actions to NLA tracks for export, assigning Blender 5.2 action slots
    track_walk = arm.animation_data.nla_tracks.new()
    track_walk.name = "WalkTrack"
    strip_walk = track_walk.strips.new(action.name, int(action.frame_range[0]), action)
    if hasattr(action, "slots") and len(action.slots) > 0 and hasattr(strip_walk, "action_slot"):
        strip_walk.action_slot = action.slots[0]

    track_idle = arm.animation_data.nla_tracks.new()
    track_idle.name = "IdleTrack"
    strip_idle = track_idle.strips.new(idle_action.name, int(idle_action.frame_range[0]), idle_action)
    if hasattr(idle_action, "slots") and len(idle_action.slots) > 0 and hasattr(strip_idle, "action_slot"):
        strip_idle.action_slot = idle_action.slots[0]

    # Unlink active action so exporter uses NLA tracks, which guarantees both distinct clips are included
    arm.animation_data.action = None

    os.makedirs(os.path.dirname(os.path.abspath(output_glb)), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=os.path.abspath(output_glb), export_format="GLB",
                              export_animations=True, export_apply=True,
                              use_selection=False, export_yup=True)
    os.makedirs(os.path.dirname(os.path.abspath(metadata_path)), exist_ok=True)
    with open(metadata_path, "w", encoding="utf-8") as stream:
        json.dump(metadata, stream, indent=2)
    if not loop_ok:
        report.warn("walk_cycle loop channel validation failed")
    if not each_target_stance_ok:
        report.warn("walk_cycle geometric ground contact failed tolerance; see motion metadata")
    return os.path.abspath(output_glb), metadata


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
    source_record = _source_record(body)
    source_joints = source_record["joints"] if source_record is not None else None
    if source_record is not None:
        # The resolved profile is authoritative for this source asset. Command
        # parameters can be older/base values, especially for reference-only
        # shape profiles and hand-curved joint paths.
        p.update(source_record["shape"])
        p["_reference_anatomy_profile"] = source_record.get("profile")
        for limb_name, row in source_joints["limbs"].items():
            if any(index >= len(body.data.vertices) for index in row["pad_vertex_indices"]):
                raise RuntimeError(f"serialized source pad indices exceed body_proxy vertices: {limb_name}")
        stats["rig_source"] = {
            "kind": "serialized_generated_shape_and_joint_graph",
            "profile": source_record.get("profile", "unknown"),
            "schema": RIG_SOURCE_SCHEMA,
        }
    else:
        stats["rig_source"] = {
            "kind": "legacy_command_parameters",
            "note": "source blend predates serialized resolved shape and joint graph; using the original parameter-based rig fallback",
        }

    with report.time("build_armature"):
        arm_data = bpy.data.armatures.new("creature_rig")
        arm = bpy.data.objects.new("creature_rig", arm_data)
        bpy.context.scene.collection.objects.link(arm)
        bpy.context.view_layer.objects.active = arm
        arm.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")
        build_skeleton(arm, p, stats, source_joints)
        bpy.ops.object.mode_set(mode="OBJECT")

    with report.time("skin"):
        feature_objects = tuple(bpy.data.objects.get(name) for name in (
            "smalltrotter_eye_and_nostril_blockout",
            "smalltrotter_bristle_ridge_blockout"))
        skin(body, detail, arm, report, stats, extras=feature_objects)
        bristle_components = 0
        if source_joints is not None:
            for obj, prefixes in zip(feature_objects, (("head_",), ("spine_", "neck_", "tail_"))):
                if obj is not None and obj.type == "MESH" and obj.data.polygons:
                    if obj.name == "smalltrotter_bristle_ridge_blockout":
                        bristle_components = _bind_bristles_to_nearest_bone_by_component(obj, arm, prefixes)
                    else:
                        _bind_feature_to_nearest_bone(obj, arm, prefixes)
        stats["skinned_feature_geometry"] = _feature_skin_metrics(feature_objects, arm)
        if source_joints is not None and "smalltrotter_bristle_ridge_blockout" in stats["skinned_feature_geometry"]:
            stats["skinned_feature_geometry"]["smalltrotter_bristle_ridge_blockout"]["connected_components_bound"] = bristle_components

        stats["skinned_feature_geometry_passed"] = all(
            row["armature_modifier_bound"] and row["vertex_group_count"] > 0 and
            row["material_slots_preserved"] > 0 and row["unassigned_vertex_count"] == 0
            for row in stats["skinned_feature_geometry"].values()) if source_joints is not None else True
    if source_joints is not None:
        stats["source_joint_alignment"] = _joint_alignment_metrics(arm, source_joints)

    with report.time("neutral_pose"):
        stats["pose"] = neutral_pose(arm, report)

    artifacts = []
    with report.time("export_neutral"):
        glb = export_neutral(params["outputs"]["neutral_pose"], report)
        artifacts.append({"path": glb, "role": "runtime", "kind": "glb"})
    if params.get("gait_contacts"):
        with report.time("author_walk_cycle"):
            animated_glb, gait_metadata = author_walk_cycle(
                arm, params["gait_contacts"], p, params.get("gait_seed", p["seed"]),
                params["outputs"]["walk_cycle"], params["outputs"]["motion_metadata"], report,
                source_joints=source_joints)
            artifacts.append({"path": animated_glb, "role": "runtime", "kind": "animated_glb"})
            artifacts.append({"path": _os.path.abspath(params["outputs"]["motion_metadata"]),
                              "role": "report", "kind": "motion_metadata"})
            stats["walk_cycle"] = gait_metadata
    with report.time("save"):
        blend_path = S.save_blend(params["outputs"]["blend"])
        artifacts.append({"path": blend_path, "role": "source", "kind": "blend"})

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
