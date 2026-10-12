"""Pure neutral support-pose math for the quadruped proxy.

The authored limb segment lengths remain inputs and are never rescaled. The
body axis follows a shallow support curve so all four proxy pads can meet the
same ground plane while keeping the named joint-chain contract intact.
"""
from __future__ import annotations

import math
from typing import Any

REAR_SUPPORT_T = 0.16
FRONT_SUPPORT_T = 0.80
PAD_GROUND_Z_M = 0.004


def _trunk_values(params: dict[str, Any], t: float) -> tuple[float, float, float]:
    trunk_len = float(params["trunk_length_m"])
    stance = float(params["stance_height_m"])
    depth_base = float(params["trunk_depth_m"])
    arch = float(params["trunk_arch"])
    bulge = math.sin(math.pi * t) ** 0.7
    depth = depth_base * (0.45 + 0.55 * bulge)
    z = stance + depth * 0.55 + arch * trunk_len * math.sin(math.pi * t)
    return z, depth, float(params["trunk_width_m"]) * (0.5 + 0.5 * bulge)


def support_offsets(params: dict[str, Any]) -> dict[str, float]:
    """Return body-axis Z offsets that put both support pairs on the pad plane."""
    upper = float(params["leg_upper_m"])
    lower = float(params["leg_lower_m"])
    spread = math.radians(float(params["leg_spread_deg"]))
    stance = float(params["stance_height_m"])
    leg_r = float(params["leg_diameter_m"]) * 0.5
    lower_vertical_ratio = 1.0 / math.sqrt(1.0 + math.sin(spread * 0.35) ** 2)
    foot_drop = max(0.02, stance * 0.06)
    # MeshBuilder.add_box treats each size component as a half-extent.
    pad_half_height = leg_r * 0.5

    def at(t: float) -> float:
        hip_z, _, _ = _trunk_values(params, t)
        raw_sole_z = hip_z - upper * math.cos(spread) - lower * lower_vertical_ratio - foot_drop - pad_half_height
        return PAD_GROUND_Z_M - raw_sole_z

    return {"rear_offset_m": at(REAR_SUPPORT_T), "front_offset_m": at(FRONT_SUPPORT_T)}


def offset_at(params: dict[str, Any], offsets: dict[str, float], t: float) -> float:
    """Interpolate support corrections along the trunk and hold them at ends."""
    rear = float(offsets["rear_offset_m"])
    front = float(offsets["front_offset_m"])
    alpha = (max(REAR_SUPPORT_T, min(FRONT_SUPPORT_T, t)) - REAR_SUPPORT_T) / (FRONT_SUPPORT_T - REAR_SUPPORT_T)
    return rear + (front - rear) * alpha


def expected_pad_bottom_z(params: dict[str, Any], t: float, offsets: dict[str, float]) -> float:
    """Analytic neutral pad bottom for a support pair, before Blender evaluation."""
    upper = float(params["leg_upper_m"])
    lower = float(params["leg_lower_m"])
    spread = math.radians(float(params["leg_spread_deg"]))
    stance = float(params["stance_height_m"])
    leg_r = float(params["leg_diameter_m"]) * 0.5
    hip_z, _, _ = _trunk_values(params, t)
    lower_ratio = 1.0 / math.sqrt(1.0 + math.sin(spread * 0.35) ** 2)
    return (hip_z + offset_at(params, offsets, t) - upper * math.cos(spread)
            - lower * lower_ratio - max(0.02, stance * 0.06) - leg_r * 0.5)
