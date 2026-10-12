import sys
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[1] / "src" / "stalthos_toolkit" / "blender" / "scripts"
sys.path.insert(0, str(SCRIPTS))

import anatomy_pose as pose


def params():
    return {"trunk_length_m": 0.72, "trunk_depth_m": 0.31, "trunk_width_m": 0.22,
            "stance_height_m": 0.95, "trunk_arch": 0.05, "leg_upper_m": 0.27,
            "leg_lower_m": 0.234, "leg_spread_deg": 9.0, "leg_diameter_m": 0.063}


def test_both_support_pairs_reach_declared_pad_plane():
    p = params()
    offsets = pose.support_offsets(p)
    assert abs(pose.expected_pad_bottom_z(p, pose.REAR_SUPPORT_T, offsets) - pose.PAD_GROUND_Z_M) < 1e-9
    assert abs(pose.expected_pad_bottom_z(p, pose.FRONT_SUPPORT_T, offsets) - pose.PAD_GROUND_Z_M) < 1e-9


def test_authored_leg_segment_lengths_are_inputs_and_not_rebalanced():
    p = params()
    before = (p["leg_upper_m"], p["leg_lower_m"])
    pose.support_offsets(p)
    assert (p["leg_upper_m"], p["leg_lower_m"]) == before


def test_offsets_are_smooth_through_the_support_span_and_held_at_ends():
    p = params()
    offsets = pose.support_offsets(p)
    assert offsets["rear_offset_m"] < 0.0
    assert offsets["front_offset_m"] < 0.0
    assert pose.offset_at(p, offsets, 0.0) == offsets["rear_offset_m"]
    assert pose.offset_at(p, offsets, 1.0) == offsets["front_offset_m"]
    midpoint = pose.offset_at(p, offsets, (pose.REAR_SUPPORT_T + pose.FRONT_SUPPORT_T) / 2)
    assert min(offsets.values()) <= midpoint <= max(offsets.values())
