"""Offline checks for the Smalltrotter's exported image-based PBR palette."""

from __future__ import annotations

import sys
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parent.parent / "src" / "stalthos_toolkit" / "blender" / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import _creature_materials as cm


def test_export_surface_samples_are_deterministic_and_seeded():
    a = cm._surface_sample("hide", 0.23, 0.71, 42)
    assert a == cm._surface_sample("hide", 0.23, 0.71, 42)
    assert a != cm._surface_sample("hide", 0.23, 0.71, 43)


def test_hide_map_carries_continuous_ochre_and_dark_mottle_variation():
    samples = [cm._surface_sample("hide", i / 20, j / 20, 42)[0]
               for i in range(20) for j in range(20)]
    assert len(set(samples)) > 100
    assert min(c[0] for c in samples) < 0.20
    assert max(c[0] for c in samples) > 0.30
    assert samples == [cm._surface_sample("hide", i / 20, j / 20, 42)[0]
                       for i in range(20) for j in range(20)]


def test_smalltrotter_export_palette_separates_skin_and_features():
    dorsal, _ = cm._surface_sample("hide", 0.5, 0.5, 42)
    ventral, _ = cm._surface_sample("ventral", 0.5, 0.5, 42)
    keratin, keratin_rough = cm._surface_sample("keratin", 0.5, 0.5, 42)
    bristle, bristle_rough = cm._surface_sample("bristle", 0.5, 0.5, 42)
    eye, eye_rough = cm._surface_sample("eye", 0.5, 0.5, 42)
    mottle, mottle_rough = cm._surface_sample("mottle", 0.5, 0.5, 42)

    assert dorsal[0] > dorsal[1] > dorsal[2]
    assert ventral[0] > dorsal[0] and ventral[1] > dorsal[1]
    assert max(keratin) < max(dorsal) * 0.5
    assert max(bristle) < max(dorsal) * 0.5
    assert max(eye) < max(bristle) and eye_rough < bristle_rough
    assert max(mottle) < max(dorsal) * 0.6 and mottle_rough > 0.5
    assert 0.2 <= keratin_rough <= 0.7


def test_unknown_export_surface_is_rejected():
    try:
        cm._surface_sample("unknown", 0.0, 0.0, 1)
    except ValueError as exc:
        assert "Unknown Smalltrotter surface" in str(exc)
    else:
        raise AssertionError("unknown surface accepted")
