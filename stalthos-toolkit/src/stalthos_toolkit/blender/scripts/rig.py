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
import gait as GAIT


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


def author_walk_cycle(arm: bpy.types.Object, contacts: list[dict[str, Any]],
                      shape: dict[str, Any],
                      seed: int, output_glb: str, metadata_path: str,
                      report: S.Reporter) -> tuple[str, dict[str, Any]]:
    """Key a short cyclic limb action and measure its named toe contacts."""
    import json
    import os

    from mathutils import Vector

    contact = next((c for c in contacts if c.get("id") == "walk_cycle"), {})
    targets = contact.get("targets", [])
    tolerance = {t["selector"]: float(t["tolerance_m"]) for t in targets}
    # The proxy's YAML selectors do not match its generated naming. Keep this
    # explicit mapping in the emitted metadata so the proxy limitation is visible.
    target_bones = {t["selector"]: GAIT.CONTACT_BONE_ALIASES[t["selector"]] for t in targets}
    body = bpy.data.objects.get("body_proxy")
    if body is None:
        raise RuntimeError("walk_cycle contact checks require body_proxy foot-pad geometry")
    head_len = float(shape["head_length_m"])
    stance_height = float(shape["stance_height_m"])
    leg_radius = float(shape["leg_diameter_m"]) * 0.5
    pad_indices = {}
    for selector, bone_name in target_bones.items():
        bone = arm.pose.bones.get(bone_name)
        if bone is None:
            continue
        ankle = arm.matrix_world.inverted() @ bone.head
        center = Vector((ankle.x + head_len * 0.25, ankle.y,
                         ankle.z - max(0.02, stance_height * 0.06)))
        # creature.py's MeshBuilder.add_box `size` is a half extent.
        pad_indices[selector] = [v.index for v in body.data.vertices
            if abs(v.co.x - center.x) <= head_len * 0.5 + 1e-5
            and abs(v.co.y - center.y) <= leg_radius * 1.1 + 1e-5
            and abs(v.co.z - center.z) <= leg_radius * 0.5 + 1e-5]
    duration_s, fps = 2.0, 24
    intervals = int(duration_s * fps)
    first_frame, last_frame = 1, intervals + 1
    scene = bpy.context.scene
    scene.render.fps = fps
    scene.frame_start, scene.frame_end = first_frame, last_frame
    scene.frame_set(first_frame)

    action = bpy.data.actions.new("Smalltrotter_WalkCycle")
    arm.animation_data_create()
    arm.animation_data.action = action
    angles = {label: [] for label in GAIT.LATERAL_SEQUENCE_PHASES}
    for label, phase_offset in GAIT.LATERAL_SEQUENCE_PHASES.items():
        upper = arm.pose.bones.get(f"leg_{label}_00")
        lower = arm.pose.bones.get(f"leg_{label}_01")
        if upper is None or lower is None:
            raise RuntimeError(f"walk cycle requires rig bones for {label}")
        upper.rotation_mode = lower.rotation_mode = "XYZ"
        for frame in range(first_frame, last_frame + 1):
            phase = (frame - first_frame) / intervals
            hip, knee, _ = GAIT.limb_angles(phase, phase_offset, 0.62)
            upper.rotation_euler.x = hip
            lower.rotation_euler.x = knee
            upper.keyframe_insert(data_path="rotation_euler", index=0, frame=frame,
                                  group=f"walk_{label}")
            lower.keyframe_insert(data_path="rotation_euler", index=0, frame=frame,
                                  group=f"walk_{label}")
            angles[label].append((hip, knee))
    # Blender 5.2 uses layered actions without the legacy Action.fcurves API.
    # Keyed boundary values match exactly; consumers repeat this 2 s clip.

    loop_errors = {label: GAIT.loop_error(values[0], values[-1])
                   for label, values in angles.items()}
    loop_ok = bool(loop_errors) and max(loop_errors.values()) <= 1e-7

    ground_distances = {selector: [] for selector in target_bones}
    stance_distances = {selector: [] for selector in target_bones}
    stance_positions = {selector: [] for selector in target_bones}
    target_limb = {"bone:pes_foot_l": "rear_L", "bone:pes_foot_r": "rear_R"}
    sample_frames = sorted(set([first_frame, last_frame] +
                               list(range(first_frame, last_frame, max(1, fps // 6)))))
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
            limb_phase = GAIT.LATERAL_SEQUENCE_PHASES[target_limb[selector]]
            in_stance = GAIT.stance_fraction(phase, limb_phase, 0.62) >= 0.0
            if in_stance:
                stance_distances[selector].append(float(min(world_z)))
                stance_positions[selector].append((center_x, center_y))
        body_eval.to_mesh_clear()
    scene.frame_set(first_frame)
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
    slip = {}
    for selector, positions in stance_positions.items():
        if len(positions) < 2:
            slip[selector] = {"samples": len(positions), "max_displacement_m": None}
            continue
        origin = positions[0]
        slip[selector] = {"samples": len(positions),
            "max_displacement_m": max(math.hypot(x - origin[0], y - origin[1])
                                      for x, y in positions)}
    metadata = {
        "clip": "walk_cycle", "source_contact_id": contact.get("id"),
        "seed": int(seed), "duration_s": duration_s, "fps": fps,
        "frame_range": [first_frame, last_frame], "loop": {"passed": loop_ok,
            "max_channel_error": max(loop_errors.values()) if loop_errors else None,
            "per_limb_error": loop_errors},
        "contacts": {**geometry, "nominal_stance_samples": stance_geometry,
            "each_target_all_stance_samples_within_tolerance": each_target_stance_ok,
            "sampled_stance_position_spread": slip,
            "selector_to_bone_aliases": target_bones,
            "foot_pad_vertex_counts": {selector: len(indices) for selector, indices in pad_indices.items()},
            "measurement": "evaluated, armature-deformed hind foot-pad bottom vertices against z=0"},
        "terrain_contact_height": {"label": "in-place terrain contact height check",
            "actor_stride_translation_included": False,
            "horizontal_foot_lock_checked": False,
            "per_target_minimum_gap_m": {name: row["min_abs_ground_distance_m"]
                for name, row in target_rows.items()},
            "sampled_stance_position_spread_m": {
                name: row["max_displacement_m"] for name, row in slip.items()}},
        "source_bounds_and_first_frame_pose": {"body_proxy_world_z_bounds_m": body_z,
            "toe_bone_endpoints_world_m": neutral_toes,
            "grounding_note": "lowering whole proxy to support hind toes would place body_proxy below z=0"},
        "validation": {"passed": loop_ok and each_target_stance_ok,
            "loop_passed": loop_ok, "geometric_contact_passed": each_target_stance_ok},
        "limitations": ["YAML selector names do not exist in the proxy rig; explicit hind-toe aliases were used.",
            "Foot-pad geometry is part of body_proxy, but this in-place cycle has no actor stride translation and does not prove horizontal foot lock.",
            "Sampled stance position spread can span separate stance episodes; it is not a planted-foot slip measurement.",
            "Proxy geometry and automatically weighted rig are not a final creature asset."],
    }
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
    with report.time("export_neutral"):
        glb = export_neutral(params["outputs"]["neutral_pose"], report)
        artifacts.append({"path": glb, "role": "runtime", "kind": "glb"})
    if params.get("gait_contacts"):
        with report.time("author_walk_cycle"):
            animated_glb, gait_metadata = author_walk_cycle(
                arm, params["gait_contacts"], p, params.get("gait_seed", p["seed"]),
                params["outputs"]["walk_cycle"], params["outputs"]["motion_metadata"], report)
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
