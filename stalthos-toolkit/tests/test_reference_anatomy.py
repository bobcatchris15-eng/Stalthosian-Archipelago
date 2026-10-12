"""Offline tests for the reference-driven Smalltrotter blockout helpers."""
from __future__ import annotations

import random
import sys
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).resolve().parents[1] / "src" / "stalthos_toolkit" / "blender" / "scripts"
sys.path.insert(0, str(SCRIPTS))

import reference_anatomy as anatomy


class MeshStub:
    def __init__(self):
        self.verts = []
        self.faces = []

    def add_vertex(self, point):
        self.verts.append(tuple(float(v) for v in point))
        return len(self.verts) - 1

    def add_face(self, face, *, smooth=True):
        self.faces.append(tuple(face))

    def add_tube(self, path, radii, *, sides=8, cap=True, smooth=True):
        self.faces.append((tuple(path), tuple(radii), sides, cap, smooth))


def test_profile_is_limited_to_smalltrotter_and_does_not_mutate_input():
    source = {"total_length_m": 5.6, "trunk_length_m": 1.68, "neck_length_m": 2.016,
              "head_length_m": 0.1792, "head_depth_m": 0.952, "tail_length_m": 1.904,
              "leg_diameter_m": 0.196, "tube_sides": 10}
    before = dict(source)

    result = anatomy.apply_profile(source, anatomy.SMALLTROTTER_ASSET_ID)
    other = anatomy.apply_profile(source, "creature.other.quad")

    assert source == before
    assert other == source
    assert result["_reference_anatomy_profile"] == anatomy.SMALLTROTTER_PROFILE
    assert result["trunk_length_m"] > source["trunk_length_m"]
    assert result["head_length_m"] > source["head_length_m"]
    assert result["head_depth_m"] < source["head_depth_m"]
    assert result["leg_diameter_m"] > source["leg_diameter_m"]
    assert result["tube_sides"] >= 18


def test_ellipsoid_helper_emits_smooth_closed_grid():
    mesh = MeshStub()
    anatomy.add_ellipsoid(mesh, (1, 2, 3), (2, 1, 0.5), segments=12, rings=8)
    assert len(mesh.verts) == 12 * 9
    assert len(mesh.faces) == 12 * 8
    assert all(len(face) == 4 for face in mesh.faces)
    xs = [point[0] for point in mesh.verts]
    assert min(xs) == pytest.approx(-1.0)
    assert max(xs) == pytest.approx(3.0)


def test_ellipsoid_face_winding_points_outward():
    mesh = MeshStub()
    centre = (1.0, -2.0, 3.0)
    anatomy.add_ellipsoid(mesh, centre, (2.0, 1.0, 0.5), segments=16, rings=12)
    for face in mesh.faces:
        p0, p1, p2 = (mesh.verts[i] for i in face[:3])
        u = tuple(p1[i] - p0[i] for i in range(3))
        v = tuple(p2[i] - p0[i] for i in range(3))
        normal = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        midpoint = tuple(sum(mesh.verts[j][i] for j in face) / len(face) for i in range(3))
        outward = tuple(midpoint[i] - centre[i] for i in range(3))
        assert sum(normal[i] * outward[i] for i in range(3)) >= -1e-10


def test_bezier_and_polyline_helpers_are_bounded_and_reproducible():
    controls = ((0, 0, 0), (1, 0, 1), (2, 0, 1), (3, 0, 0))
    assert anatomy.bezier_point(controls, 0) == pytest.approx(controls[0])
    assert anatomy.bezier_point(controls, 1) == pytest.approx(controls[-1])
    assert anatomy.sample_polyline(((0, 0, 0), (2, 0, 0), (4, 2, 0)), 0.5) == pytest.approx((2, 0, 0))
    assert anatomy.sample_polyline(((0, 0, 0), (2, 0, 0)), 4) == pytest.approx((2, 0, 0))


def test_irregular_ridge_is_seeded_and_tapers_toward_tail():
    points = ((0, 0, 0), (1, 0, 0), (2, 0, 0))
    first, second = MeshStub(), MeshStub()
    count_a = anatomy.add_irregular_ridge(first, points, count=9, max_length=0.2,
                                          rng=random.Random(42), length_falloff=True)
    count_b = anatomy.add_irregular_ridge(second, points, count=9, max_length=0.2,
                                          rng=random.Random(42), length_falloff=True)
    assert count_a == count_b == 9
    assert first.faces == second.faces
    lengths = [max(radii) / 0.22 for _, radii, *_ in first.faces]
    assert all(0.015 <= length <= 0.27 for length in lengths)


def test_cubic_path_rejects_wrong_control_count():
    with pytest.raises(ValueError, match="exactly four"):
        anatomy.bezier_point(((0, 0, 0), (1, 1, 1)), 0.5)
