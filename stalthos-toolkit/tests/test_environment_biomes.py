import pytest
from stalthos_toolkit.environment.biomes import (
    classify_cell,
    RIVER_CHANNEL,
    STILL_WATER,
    TIDAL_FLAT,
    MARSH,
    FLOODPLAIN_WETLAND,
    RIPARIAN,
    RAIN_FOREST,
    BROADLEAF_WOODLAND,
    CONIFER_WOODLAND,
    SCRUB_TALUS,
    ROCKY_UPLAND,
    DUNES_AND_SAND,
    HEATH,
)

@pytest.mark.parametrize(
    "elevation, slope_deg, moisture, is_channel, is_below_sea, accumulation, expected",
    [
        # --- Water Bodies ---
        # RIVER_CHANNEL: elevation <= sea_level, is_channel=True
        (-1.0, 0.0, 0.0, True, True, 0.0, RIVER_CHANNEL),
        (0.0, 0.0, 0.0, True, False, 0.0, RIVER_CHANNEL),

        # STILL_WATER: elevation <= sea_level, is_below_sea=True or elevation < sea_level - epsilon, is_channel=False
        (-1.0, 0.0, 0.0, False, True, 0.0, STILL_WATER),
        (-0.5, 0.0, 0.0, False, False, 0.0, STILL_WATER),  # < 0 - 1e-9

        # TIDAL_FLAT: elevation <= sea_level, is_below_sea=False, is_channel=False, not strictly less than -1e-9
        (0.0, 0.0, 0.0, False, False, 0.0, TIDAL_FLAT),

        # --- Riparian ---
        # RIPARIAN: relief > 0, is_channel=True
        (1.0, 0.0, 0.0, True, False, 0.0, RIPARIAN),
        # RIPARIAN: relief > 0, accumulation > 1.0
        (5.0, 0.0, 0.0, False, False, 2.0, RIPARIAN),

        # --- Low and Wet (relief <= 6.0, moisture >= 0.6) ---
        # FLOODPLAIN_WETLAND: slope_deg <= 4.0
        (2.0, 2.0, 0.7, False, False, 0.0, FLOODPLAIN_WETLAND),
        # MARSH: slope_deg > 4.0
        (2.0, 5.0, 0.7, False, False, 0.0, MARSH),

        # --- Rain Forest ---
        # RAIN_FOREST: moisture >= 0.78, relief <= 4.0, slope_deg <= 20.0 (but not falling into low and wet if possible, note: currently falls into low and wet trap! We test what SHOULD be RAIN_FOREST)
        (3.0, 10.0, 0.8, False, False, 0.0, RAIN_FOREST),
        (3.0, 3.0, 0.8, False, False, 0.0, RAIN_FOREST),

        # --- Woodlands ---
        # BROADLEAF_WOODLAND: moisture >= 0.5, relief > 6.0 (not low)
        (10.0, 5.0, 0.55, False, False, 0.0, BROADLEAF_WOODLAND),
        # CONIFER_WOODLAND: moisture >= 0.32, relief >= 18.0
        (20.0, 5.0, 0.4, False, False, 0.0, CONIFER_WOODLAND),
        # BROADLEAF_WOODLAND (fallback from conifer if relief < 18.0)
        (10.0, 5.0, 0.4, False, False, 0.0, BROADLEAF_WOODLAND),

        # --- Highlands (relief >= 40.0) ---
        # ROCKY_UPLAND: slope_deg >= 30.0
        (50.0, 35.0, 0.2, False, False, 0.0, ROCKY_UPLAND),
        # HEATH (highland fallback): slope_deg < 30.0
        (50.0, 20.0, 0.2, False, False, 0.0, HEATH),

        # --- Scrub / Talus ---
        # SCRUB_TALUS: slope_deg >= 28.0 (and not highland rocky upland)
        (20.0, 30.0, 0.2, False, False, 0.0, SCRUB_TALUS),

        # --- Lowlands (relief <= 6.0) not caught by wet conditions ---
        # DUNES_AND_SAND: low, moisture < 0.6
        (5.0, 10.0, 0.2, False, False, 0.0, DUNES_AND_SAND),

        # --- Default Fallback ---
        # HEATH: everything else (e.g. relief between 6 and 40, moisture < 0.32, slope < 28)
        (20.0, 10.0, 0.2, False, False, 0.0, HEATH),
    ]
)
def test_classify_cell(
    elevation: float,
    slope_deg: float,
    moisture: float,
    is_channel: bool,
    is_below_sea: bool,
    accumulation: float,
    expected: str,
):
    """Test deterministic cell classification covering all biome types."""
    sea_level = 0.0
    result = classify_cell(
        elevation=elevation,
        slope_deg=slope_deg,
        moisture=moisture,
        is_channel=is_channel,
        is_below_sea=is_below_sea,
        sea_level=sea_level,
        accumulation=accumulation,
    )
    assert result == expected, f"Expected {expected}, got {result} for elevation={elevation}, slope={slope_deg}, moisture={moisture}"
