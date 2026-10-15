"""Test the pure Python deterministic environment heightfield generator."""

import pytest
from stalthos_toolkit.environment.heightfield import Heightfield, isolate_subgrid, synthesize_field, HeightfieldParams

def test_heightfield_slice_bounds():
    hf = Heightfield(width=5, height=5, cell_size=1.0, sea_level=0.0, origin_x=0.0, origin_y=0.0)
    hf.data = [float(i) for i in range(25)]
    sub = isolate_subgrid(hf, 1, 1, 3, 3)
    assert sub.width == 3
    assert sub.height == 3
    assert sub.cell_size == 1.0
    assert sub.origin_x == 1.0
    assert sub.origin_y == 1.0
    assert sub.data == [6.0, 7.0, 8.0, 11.0, 12.0, 13.0, 16.0, 17.0, 18.0]

def test_heightfield_slice_preserves_origin():
    hf = Heightfield(width=10, height=10, cell_size=4.0, sea_level=0.0, origin_x=100.0, origin_y=200.0)
    hf.data = [float(i) for i in range(100)]
    sub = isolate_subgrid(hf, 2, 3, 2, 2)
    assert sub.origin_x == 100.0 + 2 * 4.0
    assert sub.origin_y == 200.0 + 3 * 4.0
    assert sub.width == 2
    assert sub.height == 2

def test_heightfield_synthesize_subgrid_matches_full_slice():
    params = HeightfieldParams(width=16, height=16, cell_size=2.0)
    seed = 12345

    full = synthesize_field(params, seed, 0, 0, 16, 16)

    # Slice the full
    full_slice = isolate_subgrid(full, 4, 4, 8, 8)

    # Generate the subgrid directly
    direct_subgrid = synthesize_field(params, seed, 4, 4, 8, 8)

    assert full_slice.width == direct_subgrid.width
    assert full_slice.height == direct_subgrid.height
    assert full_slice.origin_x == direct_subgrid.origin_x
    assert full_slice.origin_y == direct_subgrid.origin_y

    for y in range(8):
        for x in range(8):
            v1 = full_slice.get(x, y)
            v2 = direct_subgrid.get(x, y)
            assert abs(v1 - v2) < 1e-6
