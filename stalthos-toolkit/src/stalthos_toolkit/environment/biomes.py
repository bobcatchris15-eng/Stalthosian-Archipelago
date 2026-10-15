"""Biome classification from the heightfield + hydrology outputs.

The planning docs (`flora.md`, `geography.md`, `ecology-and-hydrology.md`)
are explicit that climate gradients follow **rainfall and topography**, not
arbitrary biome borders, and that flora form a **continuum** with island
divergence rather than theme-park tiles. So this module:

* classifies each cell from real derived fields (elevation above sea level,
  slope, channel proximity, moisture suitability) — no per-island hardcoded
  biome rectangles;
* maps each class to the flora/behaviour brief the docs give it (which forms
  go there and what the sauropod can browse);
* records area fractions and dominant classes as map properties.

No species, taxon or DOI is invented here: the class names are habitat
descriptions borrowed from the planning docs.
"""

from __future__ import annotations

from typing import Any

from pydantic import BaseModel, Field

from .heightfield import Heightfield
from .hydrology import HydrologyResult

# Class ids are stable strings so downstream scatter stages can key on them.
RIVER_CHANNEL = "river_channel"
STILL_WATER = "still_water"
TIDAL_FLAT = "tidal_flat"
MARSH = "marsh"
FLOODPLAIN_WETLAND = "floodplain_wetland"
RIPARIAN = "riparian"
RAIN_FOREST = "rain_forest"
BROADLEAF_WOODLAND = "broadleaf_woodland"
CONIFER_WOODLAND = "conifer_woodland"
SCRUB_TALUS = "scrub_talus"
ROCKY_UPLAND = "rocky_upland"
DUNES_AND_SAND = "dunes_and_sand"
HEATH = "heath"

BIOME_CLASSES: tuple[str, ...] = (
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

# Dominant-state role in the ecosystem; not a food-web verdict.
BIOME_ROLE: dict[str, str] = {
    RIVER_CHANNEL: "surface_water",
    STILL_WATER: "surface_water",
    TIDAL_FLAT: "shore",
    MARSH: "wetland_forage",
    FLOODPLAIN_WETLAND: "wetland_forage",
    RIPARIAN: "riparian_forage",
    RAIN_FOREST: "closed_canopy",
    BROADLEAF_WOODLAND: "woodland_browse",
    CONIFER_WOODLAND: "woodland_structural",
    SCRUB_TALUS: "open_scrub",
    ROCKY_UPLAND: "exposed_rock",
    DUNES_AND_SAND: "dry_lowland_scrub",
    HEATH: "open_upland_scrub",
}

# Briefs straight out of the planning docs' flagship community table and
# "Island divergence" section, keyed by habitat rather than invented species.
BIOME_BRIEF: dict[str, str] = {
    RIVER_CHANNEL: (
        "Open channel: water surface and bed; Waterstar in pools and backwaters, "
        "Ribbonherb and Jointreed at the margins."
    ),
    STILL_WATER: (
        "Pools, backwaters and filled depressions: Waterstar shallows, Ribbonherb "
        "rosettes, damp-hollow fungi."
    ),
    TIDAL_FLAT: (
        "Coastal shore below the water table: Fan-crown, Spindlebrush and "
        "Needlegrass analogue on damp sand and gravel."
    ),
    MARSH: (
        "Wet hollows and seeps: Jointreed thickets, Ribbonherb, Spearleaf; key "
        "sauropod forage (see Seep Bowl in geography.md)."
    ),
    FLOODPLAIN_WETLAND: (
        "Floodplain and terrace bottoms: Riverplume conifer patches, Royal frond, "
        "Spearleaf and Beadflower."
    ),
    RIPARIAN: (
        "River corridors: Softbough riparian shrub (primary daily juvenile browse), "
        "Riverplume, Beadflower, Basketfern in humid hollows."
    ),
    RAIN_FOREST: (
        "Humid closed canopy / mossy gorge: Splitcrown, Mottleleaf, Basketfern, "
        "epiphytes; shade-tolerant and structurally dense."
    ),
    BROADLEAF_WOODLAND: (
        "Main woodland: Mottleleaf with Mottleleaf-family shrubs, Copperleaf on "
        "terraced edges, Forkfern in clearings; shoots and low branches browse."
    ),
    CONIFER_WOODLAND: (
        "Rocky upland conifers: Splitcrown and Tallcrown rosettes; young shoots and "
        "fallen foliage, strong structural habitat."
    ),
    SCRUB_TALUS: (
        "Talus and rocky slopes: Cleftroot in fractures, Forkfern mats, Stonecrown; "
        "occasional sheltered browse and cover."
    ),
    ROCKY_UPLAND: (
        "Exposed rock and caprock: Cleftroot crevices, cliff mosses; mostly structural."
    ),
    DUNES_AND_SAND: (
        "Sandy lowland / pine-barren analogue: Spindlebrush on waxy narrow leaves, "
        "Needlegrass analogue tufts, Knuckleberry."
    ),
    HEATH: (
        "Open wet upland: Copperleaf, Spindlebrush, Spear- and Ribbonherb patches; "
        "seasonal tender browse."
    ),
}

BIOME_BRIEF_STATUS = "Proposed"  # habitat brief, not a settled species list


class BiomeMap(BaseModel):
    """Per-cell biome classification plus area summary."""

    classes: list[str]
    roles: dict[str, str] = Field(default_factory=lambda: dict(BIOME_ROLE))
    briefs: dict[str, str] = Field(default_factory=lambda: dict(BIOME_BRIEF))
    fractions: dict[str, float]
    dominant: list[str]
    cell_size: float
    water_fraction: float = 0.0
    wetland_fraction: float = 0.0
    canopy_potential: float = 0.0

    def cells_of(self, biome: str) -> list[int]:
        return [i for i, c in enumerate(self.classes) if c == biome]


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * max(0.0, min(1.0, t))


def classify_cell(
    *,
    elevation: float,
    slope_deg: float,
    moisture: float,
    is_channel: bool,
    is_below_sea: bool,
    sea_level: float,
    accumulation: float,
) -> str:
    """Classify a single cell. Pure, so it is trivially testable."""
    if elevation <= sea_level:
        if is_channel:
            return RIVER_CHANNEL
        return STILL_WATER if is_below_sea or elevation < sea_level - 1e-9 else TIDAL_FLAT

    relief = max(elevation - sea_level, 0.0)
    if is_channel:
        return RIPARIAN
    if accumulation > 1.0:
        return RIPARIAN

    flat = slope_deg <= 8.0
    low = relief <= 6.0
    wet = moisture >= 0.6

    if moisture >= 0.78 and relief <= 4.0 and slope_deg <= 20.0:
        return RAIN_FOREST
    if low and wet:
        return FLOODPLAIN_WETLAND if slope_deg <= 4.0 else MARSH
    if moisture >= 0.5:
        return BROADLEAF_WOODLAND if not low else FLOODPLAIN_WETLAND
    if moisture >= 0.32:
        return CONIFER_WOODLAND if relief >= 18.0 else BROADLEAF_WOODLAND
    if relief >= 40.0:
        return ROCKY_UPLAND if slope_deg >= 30.0 else HEATH
    if slope_deg >= 28.0:
        return SCRUB_TALUS
    if low:
        return DUNES_AND_SAND
    return HEATH


def classify(
    heightfield: Heightfield,
    hydrology: HydrologyResult,
) -> BiomeMap:
    """Classify every cell and summarise the result."""
    routing = hydrology.routing
    moisture = hydrology.moisture.suitability
    slopes = heightfield.slope_degrees
    n = len(heightfield.elevation)
    sea_level = routing.sea_level

    classes: list[str] = []
    counts: dict[str, int] = {}
    for idx in range(n):
        elevation = heightfield.elevation[idx]
        klass = classify_cell(
            elevation=elevation,
            slope_deg=slopes[idx] if idx < len(slopes) else 0.0,
            moisture=moisture[idx] if idx < len(moisture) else 0.0,
            is_channel=bool(routing.channel_mask[idx]),
            is_below_sea=elevation < sea_level - 1e-9,
            sea_level=sea_level,
            accumulation=routing.accumulation[idx],
        )
        classes.append(klass)
        counts[klass] = counts.get(klass, 0) + 1

    total = float(n) or 1.0
    fractions = {k: counts[k] / total for k in BIOME_CLASSES if k in counts}
    dominant = sorted(fractions.items(), key=lambda kv: (-kv[1], kv[0]))

    water = sum(
        fractions.get(k, 0.0) for k in (RIVER_CHANNEL, STILL_WATER, TIDAL_FLAT)
    )
    wetland = sum(fractions.get(k, 0.0) for k in (MARSH, FLOODPLAIN_WETLAND, RIPARIAN))
    canopy_classes = (RAIN_FOREST, BROADLEAF_WOODLAND, CONIFER_WOODLAND)
    canopy = sum(fractions.get(k, 0.0) for k in canopy_classes)

    return BiomeMap(
        classes=classes,
        fractions=fractions,
        dominant=[k for k, _ in dominant[:3]],
        cell_size=heightfield.cell_size,
        water_fraction=water,
        wetland_fraction=wetland,
        canopy_potential=canopy,
    )


def summarize(biome_map: BiomeMap) -> dict[str, Any]:
    """Biome metrics for the status envelope (all genuinely derived)."""
    return {
        "biomes_present": sorted(biome_map.fractions),
        "dominant_biomes": biome_map.dominant,
        "water_fraction": biome_map.water_fraction,
        "wetland_fraction": biome_map.wetland_fraction,
        "canopy_potential_fraction": biome_map.canopy_potential,
        "class_count": len(biome_map.classes),
    }