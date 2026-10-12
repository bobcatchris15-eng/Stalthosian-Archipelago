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


def root_motion_offset(cycle_phase: float, stride_length_m: float) -> float:
    """Forward root-motion distance within one cycle (the end is one stride)."""
    phase = float(cycle_phase)
    stride = float(stride_length_m)
    if not math.isfinite(phase) or not math.isfinite(stride):
        raise ValueError("cycle phase and stride length must be finite")
    if not 0.0 <= phase <= 1.0 or stride < 0.0:
        raise ValueError("cycle phase must be in [0, 1] and stride must be non-negative")
    return stride * phase


def foot_trajectory(cycle_phase: float, limb_phase: float, duty_factor: float,
                    stride_length_m: float, *, swing_lift_m: float = 0.0) -> dict[str, float | bool]:
    """Return root-relative foot target for a planted stance and lifted recovery.

    Root motion advances linearly by one stride over the cycle. During stance,
    ``x_m`` moves backward by the same distance the actor advances, so the
    touchdown point remains fixed in world space. Swing uses smoothstep to
    return the foot to the next touchdown position. At phase 0/1 the local
    pose is identical while root motion differs by exactly one stride.
    """
    phase = float(cycle_phase)
    stride = float(stride_length_m)
    lift = float(swing_lift_m)
    if not all(math.isfinite(v) for v in (phase, stride, lift, float(limb_phase), float(duty_factor))):
        raise ValueError("gait inputs must be finite")
    if not 0.0 <= phase <= 1.0 or stride < 0.0 or lift < 0.0:
        raise ValueError("cycle phase must be in [0, 1], stride and swing lift must be non-negative")
    duty = min(0.95, max(0.05, float(duty_factor)))
    local = wrap_phase(phase - float(limb_phase))
    in_stance = local < duty or (phase == 1.0 and local == 0.0)
    if in_stance:
        return {"x_m": -stride * local, "z_m": 0.0, "stance": True,
                "stance_progress": local / duty}
    swing = min(1.0, max(0.0, (local - duty) / (1.0 - duty)))
    eased = swing * swing * (3.0 - 2.0 * swing)
    return {"x_m": stride * (-duty + eased),
            "z_m": lift * math.sin(math.pi * swing), "stance": False,
            "stance_progress": -1.0}


def solve_two_bone_ik(hip: tuple[float, float, float], target: tuple[float, float, float],
                      upper_length_m: float, lower_length_m: float,
                      pole: tuple[float, float, float] = (0.0, 0.0, 1.0)
                      ) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """Solve a deterministic two-segment IK chain in 3D, returning knee and reachable ankle."""
    h = tuple(float(v) for v in hip)
    t = tuple(float(v) for v in target)
    p = tuple(float(v) for v in pole)
    upper, lower = float(upper_length_m), float(lower_length_m)
    if len(h) != 3 or len(t) != 3 or len(p) != 3:
        raise ValueError("hip, target, and pole must be 3-vectors")
    if not all(math.isfinite(v) for v in (*h, *t, *p, upper, lower)):
        raise ValueError("IK inputs must be finite")
    if upper <= 0.0 or lower <= 0.0:
        raise ValueError("segment lengths must be positive")
    delta = tuple(t[i] - h[i] for i in range(3))
    distance = math.sqrt(sum(v * v for v in delta))
    if distance <= 1e-9:
        raise ValueError("hip and target must be separated")
    direction = tuple(v / distance for v in delta)
    minimum = abs(upper - lower) + 1e-8
    maximum = upper + lower - 1e-8
    reach = min(maximum, max(minimum, distance))
    ankle = tuple(h[i] + direction[i] * reach for i in range(3))
    pole_projection = sum(p[i] * direction[i] for i in range(3))
    bend = tuple(p[i] - pole_projection * direction[i] for i in range(3))
    bend_len = math.sqrt(sum(v * v for v in bend))
    if bend_len <= 1e-9:
        fallback = (1.0, 0.0, 0.0) if abs(direction[0]) < 0.9 else (0.0, 0.0, 1.0)
        projection = sum(fallback[i] * direction[i] for i in range(3))
        bend = tuple(fallback[i] - projection * direction[i] for i in range(3))
        bend_len = math.sqrt(sum(v * v for v in bend))
    bend = tuple(v / bend_len for v in bend)
    along = (upper * upper + reach * reach - lower * lower) / (2.0 * reach)
    height = math.sqrt(max(0.0, upper * upper - along * along))
    knee = tuple(h[i] + direction[i] * along + bend[i] * height for i in range(3))
    return knee, ankle


def touchdown_excursion(samples: list[tuple[float, float, float]],
                        phases: list[float], limb_phase: float,
                        duty_factor: float, *, root_stride_m: float = 0.0
                        ) -> dict[str, float | int | None]:
    """Measure maximum horizontal pad-center travel from touchdown per stance."""
    if len(samples) != len(phases):
        raise ValueError("positions and phases must have equal lengths")
    stride = float(root_stride_m)
    if not math.isfinite(stride) or stride < 0.0:
        raise ValueError("root stride must be finite and non-negative")
    indexed = sorted((float(phase), tuple(float(v) for v in pos))
                     for phase, pos in zip(phases, samples))
    if not indexed:
        return {"samples": 0, "stance_windows": 0, "max_excursion_m": None,
                "touchdown_phase": None}
    if any(len(pos) != 3 or not all(math.isfinite(v) for v in (*pos, phase))
           for phase, pos in indexed):
        raise ValueError("touchdown samples must contain finite phases and 3D positions")
    duty = min(0.95, max(0.05, float(duty_factor)))
    offset = float(limb_phase) % 1.0
    groups = []
    current = []
    last_local = None
    for phase, position in indexed:
        local = wrap_phase(phase - offset)
        if local < duty:
            if last_local is not None and local < last_local:
                groups.append(current)
                current = []
            current.append((phase, local, position))
            last_local = local
        elif current:
            groups.append(current)
            current = []
            last_local = None
    if current:
        groups.append(current)
    if (len(groups) > 1 and groups[0] and groups[-1]
            and groups[0][0][1] > groups[-1][-1][1]):
        groups[0] = groups[-1] + groups[0]
        groups.pop()
    windows = []
    for group in groups:
        touchdown = min(group, key=lambda item: item[1])
        touchdown_phase = touchdown[0] - touchdown[1]
        equivalent_phase = wrap_phase(touchdown_phase)
        # Contact may begin before phase zero for a cyclic limb. Use the
        # measured same-pose sample inside this clip, then place it in the
        # touchdown's root-motion cycle.
        touchdown_sample = min(indexed,
            key=lambda item: abs(wrap_phase(item[0] - equivalent_phase + 0.5) - 0.5))
        origin_x = touchdown_sample[1][0] + stride * touchdown_phase
        origin_y = touchdown_sample[1][1]
        excursion = max(math.hypot(pos[0] + stride * phase - origin_x,
                                   pos[1] - origin_y)
                        for phase, _, pos in group)
        windows.append({"samples": len(group), "touchdown_phase": touchdown_phase,
                        "max_excursion_m": excursion})
    excursions = [window["max_excursion_m"] for window in windows]
    return {"samples": len(indexed), "stance_windows": len(groups),
            "max_excursion_m": max(excursions) if excursions else None,
            "touchdown_phase": windows[0]["touchdown_phase"] if len(windows) == 1 else None,
            "windows": windows}


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
