# Tree Stage Crown Spread and Morphology Correction

## Overview and Problem Statement

During inspection of stage-specific exported plant geometry for the flagship conifer asset (`plant.splitcrown.ar1`), significant crown spread and vertical foliage overshoots were identified:
- **Young Adult**: Authored crown spread is **6.5 m** at height **14.0 m**. Prior exported geometry had a correct trunk height of 14.0007 m, but actual crown spread reached **14.1018 m** (>216% of authored target).
- **Sapling**: Authored crown spread is **2.4 m** at height **4.2 m**. Prior exported geometry had a correct trunk height of 4.2122 m, but actual crown spread reached **7.5764 m** and total height reached **6.5478 m** (foliage fan vertical overshoot of +2.34 m).

## Root Cause Investigation

1. **Hardcoded Crown Radius**:
   In `blender/scripts/plants.py`, `build_tree()` computed:
   ```python
   crown_span = height * 0.42  # desired crown radius
   ```
   This completely bypassed any authored `crown_spread` or `crown_spread_m` specified on the species definition or growth stage.

2. **Mature-Derived Foliage Dimensions**:
   The definition search for foliage length looked for `leaf_length` rather than `anatomy.foliage.length`, falling back to `mature_height * 0.09 = 2.34 m` and `mature_height * 0.03 = 0.78 m`. Because `leaf_scale` in growth stage params defaulted to 1.0, explicit stages (including the 4.2 m sapling) retained 2.34 m mature foliage fan blades.
   A 2.34 m blade pointing upward from the top whorl at 4.2 m produced an overall plant height of `4.2 + 2.34 = 6.548 m`. Extending laterally from outer branches, these large blades added >4.0 m of unscaled diameter.

## Implemented Corrections

Ownership was strictly bounded to:
- `stalthos-toolkit/src/stalthos_toolkit/commands/generate_plant.py`
- `stalthos-toolkit/src/stalthos_toolkit/blender/scripts/plants.py` (tree family only)
- `stalthos-toolkit/tests/test_tree_stage_crown.py` (new offline test suite)
- `stalthos-toolkit/docs/tree-stage-crown.md` (this report)

### 1. Stage Payload Normalization (`commands/generate_plant.py`)
- Reads authored `crown_spread_m` (or `unit_m(crown_spread)`) from the declared growth stage entry for `family == "tree"`.
- Sets `scaled_shape["crown_spread_m"]` directly as absolute metres, avoiding double scaling.
- If explicit stage leaf dimensions/scales are absent, scales the mature-derived fallback foliage dimensions (`leaf_length_m`, `leaf_width_m`) by `stage_block["height_scale"]`.
- Leaves no-stage payload and all other morphology families (`fern`, `horsetail`, `rosette`, `shrub`) untouched.

### 2. Radial Foliage Accounting (`blender/scripts/plants.py`)
- In `build_tree()`:
  ```python
  if "crown_spread_m" in p:
      target_radius = float(p["crown_spread_m"]) * 0.5
      leaf_reach = float(p.get("leaf_length_m", height * 0.09)) * 0.78
      crown_span = max(0.05, (target_radius - leaf_reach) / 0.92)
  else:
      crown_span = height * 0.42  # desired crown radius
  ```
- Structure radial extent accounts for the lateral projection of foliage fan blades so that the composite boundary (structure + foliage) converges cleanly to the authored target.
- Preserves all stochastic seeds, branch angles, whorl counts, and leader fork silhouettes.
- No global GLB or runtime transformation masks applied.

## Geometry Measurements (Fresh Blender 5.2.0 Runs, seed=42)

| Stage | Authored Target | Prior Measured | Corrected Measured | Structure Dimensions (X, Y, Z) | Foliage Dimensions (X, Y, Z) | Delta from Target |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Sapling** | Spread: 2.4 m<br>Height: 4.2 m | Spread: 7.576 m<br>Height: 6.548 m | **Spread: 2.5167 m**<br>**Height: 4.5723 m** | 1.943m x 1.365m x **4.212m** | 2.517m x 1.876m x 2.967m | Spread: +4.8%<br>Trunk Z: +0.3% |
| **Young Adult** | Spread: 6.5 m<br>Height: 14.0 m | Spread: 14.102 m<br>Height: 16.240 m | **Spread: 6.3723 m**<br>**Height: 15.1763 m** | 4.423m x 3.396m x **14.001m** | 6.372m x 5.704m x 10.260m | Spread: -2.0%<br>Trunk Z: +0.01% |
| **Mature** | Spread: 12.0 m<br>Height: 26.0 m | N/A | **Spread: 11.8820 m**<br>**Height: 28.1208 m** | 8.383m x 4.577m x **26.001m** | 11.882m x 8.345m x 18.898m | Spread: -1.0%<br>Trunk Z: +0.00% |
| **No Stage** | N/A (Canonical) | Spread: 26.080 m<br>Height: 28.078 m | **Spread: 26.0798 m**<br>**Height: 28.0775 m** | 21.773m x 18.150m x 26.000m | 26.080m x 21.283m x 16.580m | Identical |

*Note on Total Height*: Total height is measured from ground to highest foliage tip. Upper whorl branches at `0.97 * height` naturally project ascending leaf blades that rise slightly above the trunk leader.

## Anatomy Limitations and Provisional Status

- **Foliage Architecture**: Foliage consists of coarse procedural blade fans (`add_blade`) rather than individual conifer needles or true scale-sprays.
- **Botanical Honesty**: This geometry is an architectural placeholder/proxy representing canopy volume and browse profile for the fictional Splitcrown taxon. It does not represent high-density needle anatomy or genuine conifer phyllotaxy.
- **Status**: **Provisional**. The seed42 measured crown spread is within ±5% of the authored target, and named structure height matches the authored height; total foliage height and biological stage suitability remain provisional.

## Parent verification and limits
Parent excluded no-stage/other-family changes and corrected per-axis explicit leaf handling plus final-height override ratio. Twenty focused tests passed; full offline toolkit2008passed2skipped in518.66s. Frozen hashes matched. Fresh parent source generation, bake_materials and existing export_baked validator succeeded for sapling and young_adult in var/splitcrown-crown-parent-{stage}-20261013. Parent native182646-fe553c16 measures crown2.516655m/6.372292m, namedstructure4.212208m/14.000714m, whole4.572319m/15.176377m. This seed42 fixture is dimensional evidence only; it does not guarantee arbitrary seeds or correct needle anatomy. Named structure matches authored height, while foliage raises whole height above that target. Whole-height/art acceptance is not established. Embedded basecolor/normal binding/hash audit: var/reports/packet-twentytwo-crown-parent-audit.json. Source originalcandidate28bebde retained.
