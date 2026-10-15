import pytest
from stalthos_toolkit.environment.biomes import BiomeMap, summarize, RIVER_CHANNEL, MARSH, RAIN_FOREST, STILL_WATER, BROADLEAF_WOODLAND, CONIFER_WOODLAND

def test_summarize_empty_biome_map():
    biome_map = BiomeMap(
        classes=[],
        fractions={},
        dominant=[],
        cell_size=10.0,
        water_fraction=0.0,
        wetland_fraction=0.0,
        canopy_potential=0.0
    )

    summary = summarize(biome_map)

    assert summary["biomes_present"] == []
    assert summary["dominant_biomes"] == []
    assert summary["water_fraction"] == 0.0
    assert summary["wetland_fraction"] == 0.0
    assert summary["canopy_potential_fraction"] == 0.0
    assert summary["class_count"] == 0


def test_summarize_standard_biome_map():
    biome_map = BiomeMap(
        classes=[RIVER_CHANNEL, MARSH, MARSH, RAIN_FOREST],
        fractions={RIVER_CHANNEL: 0.25, MARSH: 0.5, RAIN_FOREST: 0.25},
        dominant=[MARSH, RIVER_CHANNEL, RAIN_FOREST],
        cell_size=10.0,
        water_fraction=0.25,
        wetland_fraction=0.5,
        canopy_potential=0.25
    )

    summary = summarize(biome_map)

    assert summary["biomes_present"] == sorted([RIVER_CHANNEL, MARSH, RAIN_FOREST])
    assert summary["dominant_biomes"] == [MARSH, RIVER_CHANNEL, RAIN_FOREST]
    assert summary["water_fraction"] == 0.25
    assert summary["wetland_fraction"] == 0.5
    assert summary["canopy_potential_fraction"] == 0.25
    assert summary["class_count"] == 4


def test_summarize_complex_biome_map():
    biome_map = BiomeMap(
        classes=[STILL_WATER, BROADLEAF_WOODLAND, CONIFER_WOODLAND, CONIFER_WOODLAND, CONIFER_WOODLAND],
        fractions={STILL_WATER: 0.2, BROADLEAF_WOODLAND: 0.2, CONIFER_WOODLAND: 0.6},
        dominant=[CONIFER_WOODLAND, BROADLEAF_WOODLAND, STILL_WATER],
        cell_size=1.5,
        water_fraction=0.2,
        wetland_fraction=0.0,
        canopy_potential=0.8
    )

    summary = summarize(biome_map)

    assert summary["biomes_present"] == sorted([STILL_WATER, BROADLEAF_WOODLAND, CONIFER_WOODLAND])
    assert summary["dominant_biomes"] == [CONIFER_WOODLAND, BROADLEAF_WOODLAND, STILL_WATER]
    assert summary["water_fraction"] == 0.2
    assert summary["wetland_fraction"] == 0.0
    assert summary["canopy_potential_fraction"] == 0.8
    assert summary["class_count"] == 5
