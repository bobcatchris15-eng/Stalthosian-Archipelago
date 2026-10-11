"""Pure, deterministic phase and contact helpers for the proxy walk cycle.

The authored definition supplies ground-contact targets.  The current proxy
does not contain the requested ``pes_foot_l/r`` names; callers provide the
explicit toe-bone aliases used by this rig.
"""

from __future__ import annotations

import math
from typing import Iterable


LATERAL_SEQUENCE_PHASES = {
    "rear_L": 0.00,
    "front_L": 0.25,
    "rear_R": 0.50,
    "front_R": 0.75,
}

CONTACT_BONE_ALIASES = {"bone:pes_foot_l": "toe_rear_L", "bone:pes_foot_r": "toe_rear_R"}


def wrap_phase(value: float) -> float:
    """Map any finite cycle coordinate to [0, 1)."""
    return float(value) % 1.0


def stance_fraction(cycle_phase: float, limb_phase: float, duty_factor: float) -> float:
    """Return stance progress in [0, 1], or -1 while the foot is in swing."""
    duty = min(0.95, max(0.05, float(duty_factor)))
    local = wrap_phase(cycle_phase - limb_phase)
    return local / duty if local < duty else -1.0


def limb_angles(cycle_phase: float, limb_phase: float, duty_factor: float,
                *, hip_amplitude: float = 0.18,
                knee_amplitude: float = 0.28) -> tuple[float, float, bool]:
    """Small, repeatable sagittal proxy angles and the nominal stance flag."""
    local = wrap_phase(cycle_phase - limb_phase)
    stance = local < min(0.95, max(0.05, duty_factor))
    # Push through stance, then recover quickly during swing.
    wave = math.sin(2.0 * math.pi * local)
    hip = hip_amplitude * wave
    knee = knee_amplitude * max(0.0, math.sin(math.pi * local))
    if stance:
        knee *= 0.25
    return hip, knee, stance


def loop_error(start: Iterable[float], end: Iterable[float]) -> float:
    """Euclidean difference between first/last keyed channel values."""
    a, b = tuple(start), tuple(end)
    if len(a) != len(b):
        return math.inf
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def contact_summary(samples: dict[str, list[float]], tolerance_m: float | dict[str, float]) -> dict:
    """Summarize measured ground-distance samples for each authored target."""
    rows = {}
    for name, distances in sorted(samples.items()):
        tolerance = float(tolerance_m[name] if isinstance(tolerance_m, dict) else tolerance_m)
        signed = [float(d) for d in distances if math.isfinite(float(d))]
        finite = [abs(d) for d in signed]
        invalid = len(distances) - len(finite)
        within = [d for d in signed if 0.0 <= d <= tolerance]
        rows[name] = {
            "samples": len(finite),
            "invalid_samples": invalid,
            "within_tolerance": len(within),
            "fraction_within_tolerance": len(within) / len(finite) if finite else 0.0,
            "min_abs_ground_distance_m": min(finite) if finite else None,
            "max_abs_ground_distance_m": max(finite) if finite else None,
            "min_signed_ground_distance_m": min(signed) if signed else None,
            "max_signed_ground_distance_m": max(signed) if signed else None,
            "tolerance_m": tolerance,
        }
    return {"targets": rows, "all_targets_sampled": bool(rows) and all(
        v["samples"] > 0 and v["invalid_samples"] == 0 for v in rows.values())}
