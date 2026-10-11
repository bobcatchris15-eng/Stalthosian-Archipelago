"""Blender-free deterministic contracts for the walk-cycle planner."""

import math

from stalthos_toolkit.blender.scripts.gait import (
    LATERAL_SEQUENCE_PHASES,
    contact_summary,
    limb_angles,
    loop_error,
    stance_fraction,
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
