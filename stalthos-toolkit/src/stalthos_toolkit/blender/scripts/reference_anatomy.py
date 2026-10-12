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
        "head_length_m": total * 0.12,
        "head_depth_m": total * 0.085,
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
            # The parameterization's theta/phi order is inward-facing; reverse
            # it so Blender's polygon normals point away from the ellipsoid.
            builder.add_face((grid[ri][si], grid[ri + 1][si], grid[ri + 1][sj], grid[ri][sj]), smooth=True)


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


def add_tapered_limb(
    builder: Any,
    points: Sequence[Sequence[float]],
    depth_radii: Sequence[float],
    width_radii: Sequence[float],
    *,
    sides: int = 16,
    samples_per_segment: int = 4,
) -> list[list[int]]:
    """Sweep one gently curved, tapered elliptical limb surface.

    Control points are authored rig landmarks. Interpolation only affects the
    visible surface; callers retain their original joint graph and pad indices.
    The first ring is intended to sit inside the shoulder/haunch mass, hiding
    its cap and giving the appendage a continuous visual root.
    """
    if len(points) < 2 or len(points) != len(depth_radii) or len(points) != len(width_radii):
        raise ValueError("limb points and both radius arrays must have the same length >= 2")
    if sides < 6 or samples_per_segment < 1:
        raise ValueError("limb sweep needs at least 6 sides and 1 sample per segment")

    controls = [tuple(float(v) for v in point) for point in points]
    rd = [max(1e-5, float(radius)) for radius in depth_radii]
    rw = [max(1e-5, float(radius)) for radius in width_radii]

    def catmull_scalar(values: Sequence[float], i: int, t: float) -> float:
        p0, p1 = values[max(0, i - 1)], values[i]
        p2, p3 = values[i + 1], values[min(len(values) - 1, i + 2)]
        t2, t3 = t * t, t * t * t
        val = 0.5 * ((2 * p1) + (-p0 + p2) * t
                    + (2*p0 - 5*p1 + 4*p2 - p3) * t2
                    + (-p0 + 3*p1 - 3*p2 + p3) * t3)
        if all(v >= -1e-9 for v in values):
            return max(1e-5, val)
        return val

    def catmull_radius(values: Sequence[float], i: int, t: float) -> float:
        return max(1e-5, catmull_scalar(values, i, t))

    sampled: list[tuple[tuple[float, float, float], float, float]] = []
    for i in range(len(controls) - 1):
        a, b = controls[i], controls[i + 1]
        for j in range(samples_per_segment):
            t = j / samples_per_segment
            point = tuple(catmull_scalar([p[k] for p in controls], i, t) for k in range(3))
            sampled.append((point, catmull_radius(rd, i, t), catmull_radius(rw, i, t)))
    sampled.append((controls[-1], rd[-1], rw[-1]))

    rings: list[list[int]] = []
    prior_u: tuple[float, float, float] | None = None
    for i, (centre, depth, width) in enumerate(sampled):
        before = sampled[max(0, i - 1)][0]
        after = sampled[min(len(sampled) - 1, i + 1)][0]
        tangent = tuple(after[k] - before[k] for k in range(3))
        mag = math.sqrt(sum(v*v for v in tangent))
        tangent = tuple(v / mag for v in tangent) if mag > 1e-9 else (0.0, 0.0, -1.0)
        if prior_u is None:
            ref = (1.0, 0.0, 0.0) if abs(tangent[0]) < 0.92 else (0.0, 1.0, 0.0)
            dot = sum(ref[k] * tangent[k] for k in range(3))
            u = tuple(ref[k] - dot * tangent[k] for k in range(3))
        else:
            dot = sum(prior_u[k] * tangent[k] for k in range(3))
            u = tuple(prior_u[k] - dot * tangent[k] for k in range(3))
        norm = math.sqrt(sum(v*v for v in u))
        u = tuple(v / norm for v in u) if norm > 1e-9 else (1.0, 0.0, 0.0)
        v = (tangent[1]*u[2] - tangent[2]*u[1],
             tangent[2]*u[0] - tangent[0]*u[2],
             tangent[0]*u[1] - tangent[1]*u[0])
        prior_u = u
        ring = []
        for s in range(sides):
            angle = 2.0 * math.pi * s / sides
            co = tuple(centre[k] + u[k] * depth * math.cos(angle)
                       + v[k] * width * math.sin(angle) for k in range(3))
            ring.append(builder.add_vertex(co))
        rings.append(ring)

    for i in range(len(rings) - 1):
        a, b = rings[i], rings[i + 1]
        for s in range(sides):
            sn = (s + 1) % sides
            builder.add_face((a[s], a[sn], b[sn], b[s]), smooth=True)
    builder.add_face(tuple(reversed(rings[0])), smooth=True)
    builder.add_face(tuple(rings[-1]), smooth=True)
    return rings


def add_girdle_transition(
    builder: Any,
    centre: Sequence[float],
    radii: Sequence[float],
    *,
    axes: Sequence[Sequence[float]] | None = None,
    segments: int = 16,
    rings: int = 10,
) -> None:
    """Add an oriented shoulder or pelvic girdle mass connecting flank to limb root."""
    add_ellipsoid(builder, centre, radii, axes=axes, segments=segments, rings=rings)



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
        direction = (back[0] + jitter, back[1] + rng.uniform(-0.12, 0.12), back[2] + rng.uniform(-0.08, 0.16))
        norm = math.sqrt(sum(v * v for v in direction)) or 1.0
        direction = tuple(v / norm for v in direction)
        taper = max(0.30, 1.0 - 0.68 * t) if length_falloff else 1.0
        length = max(0.015, max_length * taper * rng.uniform(0.55, 1.35))
        bend = rng.uniform(0.02, 0.12)
        middle = tuple(start[k] + direction[k] * length * 0.55 + back[k] * length * bend for k in range(3))
        tip = tuple(start[k] + direction[k] * length + back[k] * length * bend for k in range(3))
        # A soft, hair-like base reads as a mane; avoid the needle-thin armor
        # profile while retaining a fine tapered tip.
        builder.add_tube((start, middle, tip), (length * 0.22, length * 0.12, length * 0.035), sides=5, cap=True, smooth=True)
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
