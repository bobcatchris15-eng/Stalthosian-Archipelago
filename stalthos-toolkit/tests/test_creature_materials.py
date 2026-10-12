"""Unit tests for creature procedural material authoring helpers.

These tests run offline without bpy and without Blender or network access.
They verify:
- Safe offline importability without bpy.
- Deterministic seed behavior (same seed -> same parameters).
- Distinct seed behavior (different seed -> different parameters).
- Restrained albedo variation and absence of generic neutral gray.
- Valid roughness, subsurface scattering, and bump parameters.
- Preset coverage: dorsal, underside, and keratin.
"""

from __future__ import annotations

import sys
from pathlib import Path
import pytest

# Ensure scripts directory can be imported
SCRIPTS_DIR = Path(__file__).resolve().parent.parent / "src" / "stalthos_toolkit" / "blender" / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import _creature_materials as cm


def test_offline_import_without_bpy():
    """Verify that pure parameter helpers and presets are importable without bpy."""
    assert hasattr(cm, "get_creature_material_spec")
    assert hasattr(cm, "CREATURE_PRESETS")
    assert "smalltrotter_dorsal" in cm.CREATURE_PRESETS
    assert "smalltrotter_underside" in cm.CREATURE_PRESETS
    assert "smalltrotter_keratin" in cm.CREATURE_PRESETS


def test_deterministic_seed_consistency():
    """Verify that same seed produces identical parameters across calls."""
    spec_a1 = cm.get_creature_material_spec("smalltrotter_dorsal", seed=42)
    spec_a2 = cm.get_creature_material_spec("smalltrotter_dorsal", seed=42)
    assert spec_a1 == spec_a2
    assert spec_a1["base"] == spec_a2["base"]
    assert spec_a1["rough"] == spec_a2["rough"]


def test_seed_variation_is_restrained():
    """Verify variation across seeds is subtle/restrained (< 5%), avoiding wild noise."""
    spec_a = cm.get_creature_material_spec("smalltrotter_dorsal", seed=10)
    spec_b = cm.get_creature_material_spec("smalltrotter_dorsal", seed=999)

    # Different seeds produce different subtle tints
    assert spec_a["base"] != spec_b["base"]

    # Restrained delta: each color channel differs by less than 0.05
    for ca, cb in zip(spec_a["base"][:3], spec_b["base"][:3]):
        assert abs(ca - cb) < 0.05

    # Roughness values remain in plausible physical ranges
    assert 0.35 <= spec_a["rough"][1] < spec_a["rough"][0] <= 0.75
    assert 0.35 <= spec_b["rough"][1] < spec_b["rough"][0] <= 0.75


def test_no_generic_gray_albedo():
    """Verify that presets have distinct chromatic character (earthy/buff/keratin), not neutral gray."""
    for key, spec in cm.CREATURE_PRESETS.items():
        base = spec["base"]
        # In neutral gray, r == g == b. In natural skin/keratin, channels have organic spread.
        r, g, b = base[0], base[1], base[2]
        spread = max(abs(r - g), abs(r - b), abs(g - b))
        assert spread >= 0.02, f"Preset {key} base albedo is too close to generic gray: {base}"


def test_underside_lighter_than_dorsal():
    """Verify biological countershading principle: underside albedo is lighter than dorsal."""
    dorsal = cm.get_creature_material_spec("smalltrotter_dorsal", seed=123)
    underside = cm.get_creature_material_spec("smalltrotter_underside", seed=123)

    dorsal_lum = 0.2126 * dorsal["base"][0] + 0.7152 * dorsal["base"][1] + 0.0722 * dorsal["base"][2]
    under_lum = 0.2126 * underside["base"][0] + 0.7152 * underside["base"][1] + 0.0722 * underside["base"][2]
    assert under_lum > dorsal_lum, "Underside albedo must be lighter than dorsal (countershading)."


def test_smalltrotter_palette_matches_reference_direction():
    """Keep the draft in the reference's ochre, brown-mottled, cream palette."""
    dorsal = cm.get_creature_material_spec("smalltrotter_dorsal", seed=123)
    underside = cm.get_creature_material_spec("smalltrotter_underside", seed=123)

    # Warm ochre has a clear red-over-green-over-blue ordering; brown marks
    # are substantially darker than the midtone in every channel.
    assert dorsal["base"][0] > dorsal["base"][1] > dorsal["base"][2]
    assert max(dorsal["mottling"][:3]) < min(dorsal["base"][:3])
    assert underside["base"][0] > underside["base"][1] > underside["base"][2]
    assert all(a > b for a, b in zip(underside["base"][:3], dorsal["base"][:3]))


def test_keratin_has_lower_roughness_and_subsurface():
    """Verify keratin structure: lower roughness (more specular reflection) and minimal SSS."""
    keratin = cm.get_creature_material_spec("smalltrotter_keratin", seed=456)
    dorsal = cm.get_creature_material_spec("smalltrotter_dorsal", seed=456)

    assert keratin["rough"][0] < dorsal["rough"][0]
    assert keratin["subsurface"] < dorsal["subsurface"]
    assert "wave_scale" in keratin


def test_unknown_preset_raises():
    """Verify ValueError is raised on unrecognized preset name."""
    with pytest.raises(ValueError, match="Unknown creature preset"):
        cm.get_creature_material_spec("non_existent_beast", seed=1)
