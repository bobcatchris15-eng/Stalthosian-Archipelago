# Conifer Foliage Refinement: Terminal Needle and Scale Sprays

## Overview and Problem Statement

The species definition for Splitcrown (`species/plants/splitcrown.yaml`, `plant.splitcrown.ar1`) specifies conifer foliage anatomy:
- `foliage.organ`: `scale_spray`
- `foliage.overall_shape`: `needled`
- `foliage.length`: `3.5 cm` (`0.035 m`)
- `foliage.width`: `0.9 cm` (`0.009 m`)
- `foliage.aspect_ratio`: `3.9`

Prior generation used mature-derived fallback blade dimensions (`leaf_length_m = 0.378 m` on sapling, `1.26 m` on young adult; `leaf_width_m = 0.126 m` on sapling, `0.42 m` on young adult). This rendered conifer foliage as coarse broad billboards (ribbon blades 42 cm wide and 1.26 m long). In native inspection scenes (`var/reports/runtime/20261013-183102-4b62df0f`), reference trees appeared thin and sparse with isolated flat paddles and an open sky canopy.

Simply shrinking individual blades to 3.5 cm at existing cluster points would reduce foliage to invisible sparse points and shrink the crown spread away from authored targets. Conversely, attempting to brute-force hundreds of thousands of individual needles would explode triangle budgets beyond the target hardware floor (GTX 1080 8 GB, 30 FPS floor).

## Implemented Foliage Refinement

Ownership was strictly bounded to:
- `stalthos-toolkit/src/stalthos_toolkit/commands/generate_plant.py`
- `stalthos-toolkit/src/stalthos_toolkit/blender/scripts/plants.py` (`tree` family only)
- `stalthos-toolkit/tests/test_tree_needle_spray.py` (new offline test suite)
- `stalthos-toolkit/docs/tree-needle-spray.md` (this report)

### 1. Authored Foliage Anatomy Consumption (`generate_plant.py`)
- Reads `definition["anatomy"]["foliage"]` for explicit tree stages.
- Consumes authored scale/needle dimensions (`needle_length_m = 0.035`, `needle_width_m = 0.009`, `foliage_organ = "scale_spray"`, `foliage_shape = "needled"`).
- Preserves `leaf_length_m` and `leaf_width_m` macro stage scaling to preserve radial branching headroom and crown span calculations.
- Completely preserves canonical no-stage execution, non-tree families (`fern`, `horsetail`, `rosette`, `shrub`), and broadleaf trees.

### 2. Bounded Deterministic Terminal Spray Architecture (`blender/scripts/plants.py`)
- For conifer trees with `scale_spray` or `needled` anatomy, replaces single broad billboard blades with `_add_terminal_needle_spray`.
- **Shoot Frame and Envelope**: The terminal spray originates at branch anchors and reaches along the branch departure vector `d` scaled by `leaf_length_m * 0.82` (matching the droop envelope).
- **Branchlet Structure**:
  - Central leader shoot bearing alternating needle pairs and a terminal acute needle.
  - Two lateral sprigs branching at an acute 24° angle at mid-shoot (`t = 0.45`), expanding the spray laterally to provide believable conifer foliage volume.
- **Needle Morphology**:
  - Small needle blades (2 segments = 2 quads, acute tapered apex matching `aspect_ratio ~ 3.9`).
  - Needles angled forward and outward at 22°–32° from the shoot axis.
- **Budget and Determinism**:
  - Bounded finite blade count: 17 needles per spray.
  - Zero unseeded randomness: uses `S.rng(seed, "tree", "leaf", level, node_index)` exclusively.
  - Preserves exact mesh names (`tree_structure`, `tree_foliage`) and material names (`tree_bark`, `tree_foliage`) required for baking and the engine resource pipeline.

## Measured Geometry and Target Comparisons (Blender 5.2.0 LTS, seed=42)

| Stage | Target Crown Spread | Measured Crown Spread | Delta Spread | Target Height | Measured Structure Height | Measured Whole Height | Foliage Quads | Foliage Tris | Needles |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Sapling** | 2.4 m | **2.3851 m** | -0.62% | 4.2 m | **4.2122 m** (+0.29%) | 4.5650 m | 10,608 | 21,216 | 5,304 |
| **Young Adult** | 6.5 m | **6.2483 m** | -3.87% | 14.0 m | **14.0007 m** (+0.01%) | 14.9596 m | 7,480 | 14,960 | 3,740 |

*Note on Envelopes*:
- Structure heights match authored targets within <0.3%.
- Crown spread matches authored targets within <4% across both stages.
- Whole height (to highest needle tip) is tightly controlled (14.9596 m on young adult, reduced from 15.176 m).

## Hashed Artifact Registry

### Sapling (`plant.splitcrown.ar1.sapling`)
- **Source Master**: `var/splitcrown-spray-sapling/sources/plant.splitcrown.ar1/plant.splitcrown.ar1.sapling.blend`
  - Bytes: 1,882,361 | SHA256: `6abc6247ae4ac8e4dc44f336650dd8d9534493e62d192376d716269f6e53c488`
- **Intermediate Baked Master**: `var/splitcrown-spray-sapling/intermediate/bakes/plant.splitcrown.ar1.sapling.baked.blend`
  - Bytes: 3,133,381 | SHA256: `8913c8a8c4f6aba4524334068c1b9fbf0c60bc00c1e8665f298ebcc332f5140b`
- **Exported Baked GLB**: `var/splitcrown-spray-sapling/exports/plant.splitcrown.ar1.sapling.baked.glb`
  - Bytes: 2,405,012 | SHA256: `6b5e8bb523c84e827c01333c9ef375fbd5f96d3cdc6301c70466bbeca2fd21bb`
  - Materials checked: 2 (`tree_foliage_baked`, `tree_bark_baked`), Texture bindings: 6 (Albedo, Roughness, Normal)
- **Turntable Review Sheet**: `var/splitcrown-spray-sapling/preview/plant.splitcrown.ar1/turntable/` (angles 0, 90, 180, 270)

### Young Adult (`plant.splitcrown.ar1.young_adult`)
- **Source Master**: `var/splitcrown-spray-young_adult/sources/plant.splitcrown.ar1/plant.splitcrown.ar1.young_adult.blend`
  - Bytes: 1,543,457 | SHA256: `262f8bbb3d7f44c9bd8bb997198ab042f8b78dfaf76d9a76f66707f17aaccbb4`
- **Intermediate Baked Master**: `var/splitcrown-spray-young_adult/intermediate/bakes/plant.splitcrown.ar1.young_adult.baked.blend`
  - Bytes: 2,234,260 | SHA256: `d05220d4ff8c78e4721314dc969cc291bbb4c44cb3238c609f24a07945af44a9`
- **Exported Baked GLB**: `var/splitcrown-spray-young_adult/exports/plant.splitcrown.ar1.young_adult.baked.glb`
  - Bytes: 1,542,232 | SHA256: `2ef814cc3d41c815757cdf586bd0faeff630b4683c3364deb1737bcda2e80c93`
  - Materials checked: 2 (`tree_foliage_baked`, `tree_bark_baked`), Texture bindings: 6 (Albedo, Roughness, Normal)
- **Turntable Review Sheet**: `var/splitcrown-spray-young_adult/preview/plant.splitcrown.ar1/turntable/` (angles 0, 90, 180, 270)

## Art and Anatomy Limitations and Provisional Status

- **Foliage Density Approximation**: The terminal needle sprays represent an architectural compromise between botanical scale needles (3.5 cm) and game-ready real-time rendering budgets (14k–21k triangles). Individual needle blades are modeled with 2 quads each to form acute needles rather than millions of microscopic sub-millimetre leaves.
- **Status**: **Provisional**. Visual fidelity, browse contact targets, LOD collapse behavior at distance, and habitat placement remain provisional pending multi-asset scene composition and camera review.
