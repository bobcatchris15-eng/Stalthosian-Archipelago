"""Blender-free deterministic contracts for the walk-cycle planner."""

import math
import pytest

from stalthos_toolkit.blender.scripts.gait import (
    LATERAL_SEQUENCE_PHASES,
    contact_summary,
    limb_angles,
    loop_error,
    foot_trajectory,
    root_motion_offset,
    solve_two_bone_ik,
    stance_fraction,
    touchdown_excursion,
    wrap_phase,
)
from stalthos_toolkit.commands.rig_creature import _validate_walk_animation
from stalthos_toolkit.core.errors import ValidationFailed
from stalthos_toolkit.optimize.glb import GlbDocument


def test_lateral_sequence_phase_plan_is_stable_and_wraps():
    assert LATERAL_SEQUENCE_PHASES == {
        "rear_L": 0.0, "front_L": 0.25, "rear_R": 0.5, "front_R": 0.75,
    }
    assert wrap_phase(-0.25) == 0.75
    assert wrap_phase(1.0) == 0.0
    assert stance_fraction(0.1, 0.0, 0.62) == 0.1 / 0.62
    assert stance_fraction(0.9, 0.0, 0.62) == -1.0


def test_limb_channels_repeat_exactly_at_cycle_boundary():
    for limb_phase in LATERAL_SEQUENCE_PHASES.values():
        first = limb_angles(0.0, limb_phase, 0.62)
        end = limb_angles(1.0, limb_phase, 0.62)
        assert first == end
        assert loop_error(first[:2], end[:2]) == 0.0
    assert math.isfinite(limb_angles(0.37, 0.5, 0.62)[0])


def test_root_motion_and_limb_pose_have_explicit_exact_cycle_contract():
    stride = 0.8
    assert root_motion_offset(0.0, stride) == 0.0
    assert root_motion_offset(1.0, stride) == stride
    for offset in LATERAL_SEQUENCE_PHASES.values():
        first = foot_trajectory(0.0, offset, 0.62, stride, swing_lift_m=0.12)
        end = foot_trajectory(1.0, offset, 0.62, stride, swing_lift_m=0.12)
        assert first == end
        assert loop_error((first["x_m"], first["z_m"]),
                          (end["x_m"], end["z_m"])) == 0.0


def test_stance_trajectory_pins_world_foot_while_root_advances():
    stride = 0.8
    for phase in (0.50, 0.55, 0.61, 0.98, 1.0):
        foot = foot_trajectory(phase, 0.5, 0.62, stride)
        assert foot["stance"] is True
        assert math.isclose(root_motion_offset(phase, stride) + foot["x_m"],
                            0.4, abs_tol=1e-12)


def test_touchdown_excursion_adds_body_stride_and_anchors_at_touchdown():
    phases = [0.00, 0.05, 0.10, 0.15, 0.50, 0.55, 0.60, 0.98, 1.00]
    # Root-relative pad coordinates include both wrap-side stance fragments.
    local = [(-0.4, 0.0, 0.0), (-0.44, 0.0, 0.0), (-0.48, 0.0, 0.0),
             (-0.4824, 0.0, 0.0), (0.0, 0.0, 0.0), (-0.04, 0.0, 0.0),
             (-0.08, 0.0, 0.0), (-0.384, 0.0, 0.0), (-0.4, 0.0, 0.0)]
    result = touchdown_excursion(local, phases, 0.5, 0.62, root_stride_m=0.8)
    assert result["stance_windows"] == 2
    assert [window["touchdown_phase"] for window in result["windows"]] == [-0.5, 0.5]
    assert result["max_excursion_m"] < 1e-12
    no_root = touchdown_excursion(local[4:7], phases[4:7], 0.5, 0.62)
    with_root = touchdown_excursion(local[4:7], phases[4:7], 0.5, 0.62,
                                    root_stride_m=0.8)
    assert with_root["max_excursion_m"] < no_root["max_excursion_m"]


def test_two_bone_ik_respects_segment_lengths_and_rejects_bad_inputs():
    hip = (0.0, 0.0, 1.0)
    target = (0.4, 0.0, 0.1)
    knee, ankle = solve_two_bone_ik(hip, target, 0.7, 0.6, (1.0, 0.0, 0.0))
    assert math.isclose(math.dist(hip, knee), 0.7, abs_tol=1e-9)
    assert math.isclose(math.dist(knee, ankle), 0.6, abs_tol=1e-9)
    assert math.isclose(math.dist(ankle, target), 0.0, abs_tol=1e-8)
    with pytest.raises(ValueError, match="positive"):
        solve_two_bone_ik(hip, target, 0.0, 0.6)


def test_gait_helpers_reject_nonfinite_and_out_of_range_inputs():
    with pytest.raises(ValueError, match="finite"):
        root_motion_offset(float("nan"), 0.1)
    with pytest.raises(ValueError, match="cycle phase"):
        foot_trajectory(1.01, 0.0, 0.62, 0.1)
    with pytest.raises(ValueError, match="finite"):
        solve_two_bone_ik((0.0, 0.0, 0.0), (1.0, 0.0, float("inf")), 1.0, 1.0)
    with pytest.raises(ValueError, match="equal lengths"):
        touchdown_excursion([(0.0, 0.0, 0.0)], [], 0.0, 0.62)


def test_contact_summary_uses_authored_tolerance_without_claiming_missing_data():
    summary = contact_summary({"foot": [0.01, 0.04]}, 0.02)
    assert summary["all_targets_sampled"] is True
    row = summary["targets"]["foot"]
    assert row["within_tolerance"] == 1
    assert row["fraction_within_tolerance"] == 0.5
    assert contact_summary({"missing": []}, 0.02)["all_targets_sampled"] is False
    invalid = contact_summary({"bad": [float("nan"), 0.01]}, 0.02)
    assert invalid["targets"]["bad"]["invalid_samples"] == 1
    assert invalid["all_targets_sampled"] is False
    penetrated = contact_summary({"below": [-0.001]}, 0.02)
    assert penetrated["targets"]["below"]["within_tolerance"] == 0


def test_missing_glb_animation_channels_fail_with_typed_validation(tmp_path):
    path = tmp_path / "no-animation.glb"
    GlbDocument({"asset": {"version": "2.0"}}).write(path)
    try:
        _validate_walk_animation(path)
    except ValidationFailed as exc:
        assert "missing valid animation channels" in exc.message
    else:
        raise AssertionError("a GLB without animation channels must fail validation")


def test_contact_summary_preserves_each_selectors_tolerance():
    result = contact_summary({"left": [0.015], "right": [0.015]}, {"left": 0.01, "right": 0.02})
    assert result["targets"]["left"]["within_tolerance"] == 0
    assert result["targets"]["right"]["within_tolerance"] == 1


def test_front_kinematic_diagnostic_metrics_keep_empty_coverage_explicit():
    # Front phase offsets are an existing kinematic schedule, not authored
    # contact targets. Empty source coverage must stay visibly unsampled.
    assert LATERAL_SEQUENCE_PHASES["front_L"] == 0.25
    assert LATERAL_SEQUENCE_PHASES["front_R"] == 0.75
    missing = contact_summary({"diagnostic:manus_hand_l": []}, 0.0)
    row = missing["targets"]["diagnostic:manus_hand_l"]
    assert row["samples"] == 0
    assert row["min_abs_ground_distance_m"] is None
    assert missing["all_targets_sampled"] is False

    # A planted-window drift metric is diagnostic data. Its value does not
    # turn the selector into an authored contact target or validation gate.
    phases = [0.25, 0.30, 0.35]
    positions = [(0.0, 0.0, 0.0), (-0.04, 0.0, 0.0), (-0.08, 0.0, 0.0)]
    drift = touchdown_excursion(positions, phases, 0.25, 0.62,
                                root_stride_m=0.8)
    assert drift["samples"] == 3
    assert drift["max_excursion_m"] < 1e-12


def test_malformed_animated_glb_is_rejected_despite_present_channels(tmp_path):
    import pytest
    # A valid generated channel fixture with just its target corrupted exercises
    # the structural gate, rather than only checking an absent animation list.
    source = GlbDocument({"asset": {"version": "2.0"}, "nodes": [{}],
        "animations": [{"channels": [{"sampler": 0, "target": {"node": 999, "path": "rotation"}}],
                         "samplers": [{"input": 0, "output": 1}]}],
        "accessors": [{"count": 2}, {"count": 2}]})
    path = tmp_path / "bad-target.glb"
    source.write(path)
    with pytest.raises(ValidationFailed):
        _validate_walk_animation(path)
