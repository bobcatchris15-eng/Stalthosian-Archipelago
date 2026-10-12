"""Reference-led, fictional Smalltrotter blockout helpers.

This module is intentionally importable outside Blender. The profile selection
and dimensions are pure data; geometry helpers only need the MeshBuilder
protocol (``add_vertex`` / ``add_face`` / ``add_tube``), so focused tests can
exercise the profile without importing bpy or mathutils.
"""
from __future__ import annotations

import math
from typing import Any, Sequence


SMALLTROTTER_PROFILE = "smalltrotter_reference_v1"
SMALLTROTTER_ASSET_ID = "creature.smalltrotter.st1"


def profile_for(asset_id: str) -> str | None:
    """Select this profile only for the Smalltrotter asset identity."""
    return SMALLTROTTER_PROFILE if str(asset_id).strip().lower() == SMALLTROTTER_ASSET_ID else None


def apply_profile(params: dict[str, Any], asset_id: str) -> dict[str, Any]:
    """Return a per-asset shape copy; leave every other creature unchanged."""
    out = dict(params)
    profile = profile_for(asset_id)
    if profile is None:
        return out
    total = max(0.4, float(params.get("total_length_m", 5.0)))
    out.update({
        "_reference_anatomy_profile": profile,
        # A longer, less globular torso and deep but not spherical ribcage.
        "trunk_length_m": max(float(params.get("trunk_length_m", 0.0)), total * 0.38),
        "trunk_depth_m": total * 0.27,
        "trunk_width_m": total * 0.225,
        # Keep the upright neck long while reserving a compact, readable head.
        "neck_length_m": total * 0.30,
        "head_length_m": total * 0.095,
        "head_depth_m": total * 0.075,
        "tail_length_m": total * 0.30,
        # Substantial limbs support broad grounded feet without changing bone lengths.
        "leg_diameter_m": total * 0.070,
        "tube_sides": max(int(params.get("tube_sides", 10)), 18),
    })
    return out


def add_ellipsoid(
    builder: Any,
    centre: Sequence[float],
    radii: Sequence[float],
    *,
    axes: Sequence[Sequence[float]] | None = None,
    segments: int = 18,
    rings: int = 12,
) -> None:
    """Add a smooth ellipsoid, optionally oriented by three basis vectors."""
    c = tuple(float(v) for v in centre)
    r = tuple(max(1e-5, float(v)) for v in radii)
    basis = axes or ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
    grid: list[list[int]] = []
    for ri in range(rings + 1):
        phi = math.pi * ri / rings
        row: list[int] = []
        for si in range(segments):
            theta = 2.0 * math.pi * si / segments
            local = (r[0] * math.sin(phi) * math.cos(theta),
                     r[1] * math.sin(phi) * math.sin(theta),
                     r[2] * math.cos(phi))
            point = tuple(c[k] + sum(float(basis[a][k]) * local[a] for a in range(3)) for k in range(3))
            row.append(builder.add_vertex(point))
        grid.append(row)
    for ri in range(rings):
        for si in range(segments):
            sj = (si + 1) % segments
            builder.add_face((grid[ri][si], grid[ri][sj], grid[ri + 1][sj], grid[ri + 1][si]), smooth=True)


def bezier_point(points: Sequence[Sequence[float]], t: float) -> tuple[float, float, float]:
    """Cubic Bezier point for a four-point control path."""
    if len(points) != 4:
        raise ValueError("A cubic path requires exactly four control points")
    u = 1.0 - float(t)
    weights = (u ** 3, 3.0 * u * u * t, 3.0 * u * t * t, t ** 3)
    return tuple(sum(weights[i] * float(points[i][axis]) for i in range(4)) for axis in range(3))


def sample_polyline(points: Sequence[Sequence[float]], t: float) -> tuple[float, float, float]:
    """Sample a polyline uniformly by segment index."""
    if not points:
        raise ValueError("Cannot sample an empty path")
    if len(points) == 1:
        return tuple(float(v) for v in points[0])
    value = min(1.0, max(0.0, float(t))) * (len(points) - 1)
    i = min(len(points) - 2, int(value))
    f = value - i
    return tuple(float(points[i][k]) * (1.0 - f) + float(points[i + 1][k]) * f for k in range(3))


def add_irregular_ridge(
    builder: Any,
    points: Sequence[Sequence[float]],
    *,
    count: int,
    max_length: float,
    rng: Any,
    backward: Sequence[float] = (-0.7, 0.0, 0.4),
    length_falloff: bool = False,
) -> int:
    """Add deterministically irregular tapered bristles along a centerline."""
    if count <= 0 or len(points) < 2:
        return 0
    back = tuple(float(v) for v in backward)
    back_len = math.sqrt(sum(v * v for v in back)) or 1.0
    back = tuple(v / back_len for v in back)
    for i in range(count):
        # Uniform random samples create natural gaps and small clumps along
        # the ridge; evenly stepping every strand reads as a manufactured comb.
        t = rng.random()
        start = sample_polyline(points, t)
        jitter = rng.uniform(-0.10, 0.10)
        direction = (back[0] + jitter, back[1] + rng.uniform(-0.16, 0.16), back[2] + rng.uniform(-0.14, 0.30))
        norm = math.sqrt(sum(v * v for v in direction)) or 1.0
        direction = tuple(v / norm for v in direction)
        taper = max(0.30, 1.0 - 0.68 * t) if length_falloff else 1.0
        length = max(0.015, max_length * taper * rng.uniform(0.55, 1.35))
        bend = rng.uniform(0.08, 0.30)
        middle = tuple(start[k] + direction[k] * length * 0.55 + back[k] * length * bend for k in range(3))
        tip = tuple(start[k] + direction[k] * length + back[k] * length * bend for k in range(3))
        builder.add_tube((start, middle, tip), (length * 0.095, length * 0.045, length * 0.008), sides=4, cap=True, smooth=True)
    return count


def profile_summary(asset_id: str, params: dict[str, Any]) -> dict[str, Any] | None:
    """Small truthful record for the blockout result metadata."""
    profile = profile_for(asset_id)
    if profile is None:
        return None
    return {
        "profile": profile,
        "uncertainty": "speculative",
        "basis": "single user-supplied three-quarter image; proportions are draft choices",
        "trunk_length_m": round(float(params["trunk_length_m"]), 4),
        "neck_length_m": round(float(params["neck_length_m"]), 4),
        "head_length_m": round(float(params["head_length_m"]), 4),
        "tail_length_m": round(float(params["tail_length_m"]), 4),
    }
