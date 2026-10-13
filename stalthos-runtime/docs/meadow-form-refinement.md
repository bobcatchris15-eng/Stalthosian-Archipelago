# Meadow Form Refinement

## Context and Goal

In earlier native drafts (e.g. `20261012-233927-5d2900ad`), procedural meadow groundcover tufts presented as sparse, tall, stiff triangular upright swords/pyramids that stood rigidly perpendicular to the soil surface. In Late Cretaceous island ecosystems (per `Rauken-Archipelago/docs/flora.md`), groundcover was not a modern monoculture mown lawn, but a heterogeneous mosaic of arching strap-leaf monocots/sedges, low disturbance fernlets, and creeping herbaceous broadleaf rosettes.

This refinement updates `stalthos-runtime/engine/meadow_scene.hpp` to transform the visual silhouette of groundcover into natural, low-profile, graceful drooping forms with seeded morphological variety and local cluster affinity, while strictly preserving deterministic positioning, exclusions, resource caps, and producer result APIs.

## Morphological Diversity

Rather than a single uniform blade archetype, `appendTuft` now instantiates three distinct Late Cretaceous groundcover morphotypes selected deterministically per clump and weighted by habitat:

1. **Slender Curved Blade Clump (Needlegrass / Strap-Leaf Sedge Analogue)**:
   - 4 to 6 arching, slender blades with reduced vertical height ($0.08\text{--}0.25\,\text{m}$ depending on habitat, down from up to $0.52\,\text{m}$).
   - Narrower base widths ($0.009\text{--}0.015\,\text{m}$) tapering to $48\%$ mid-span and a sharp apex.
   - Genuine curvature: mid-span vertex rises to peak arch ($72\%$ of height), while the blade tip droops downward toward the soil ($y_\text{tip} \approx y_0 + [0.14\text{--}0.32] \times h$).
   - Double-sided geometry: 5 vertices, 3 triangles per blade.

2. **Small Cretaceous Fernlet Rosette (Royal Frond / Disturbance Fern Analogue)**:
   - 4 to 5 low, horizontally spreading pinnate fronds in a radial rosette ($0.10\text{--}0.16\,\text{m}$ span).
   - Broadened mid-pinnae lateral spread ($0.032\text{--}0.048\,\text{m}$) capturing the characteristic feathered silhouette of low polypodiopsids.
   - Deep forest olive and emerald vertex color gradient ($[36, 48, 22]$ stipe root, $[54, 96, 36]$ pinnae mid, $[80, 124, 44]$ frond tip).
   - Double-sided geometry: 5 vertices, 3 triangles per frond.

3. **Low Creeping Herb Rosette (Creeping Broadleaf / Beadflower Analogue)**:
   - 4 to 5 flat, soil-hugging diamond/oval leaves ($0.065\text{--}0.10\,\text{m}$ length, $0.024\text{--}0.036\,\text{m}$ width).
   - Very low vertical profile ($<0.05\,\text{m}$) hugging the terrain surface.
   - Herbaceous spring-olive vertex color tones ($[44, 54, 24]$ base, $[74, 104, 40]$ leaf body, $[98, 126, 48]$ apex).
   - Double-sided geometry: 4 vertices, 2 triangles per leaf.

## Habitat Composition

- **Riparian**: Predominantly arching sedges ($60\%$), accompanied by damp ground fernlets ($28\%$) and occasional low herbs ($12\%$).
- **OpenLowland**: Heterogeneous mosaic of low creeping herbs ($45\%$), slender curved blades ($33\%$), and sunny disturbance fernlets ($22\%$).
- **Woodland**: Dominated by shade-tolerant fernlets ($55\%$) and low creeping herbs ($30\%$), with sparse fine arching blades ($15\%$).

## Determinism and Resource Bounds

All sampling positions, grid lattices, water exclusions, and coordinate bounding logic remain identical to the frozen spatial contract:
- Coordinate bounds: $x, z \in [-55.0, 55.0]\,\text{m}$.
- Strict water exclusion: `ReferenceLandscape::waterContains(x, z)` rejects wet samples without placing vegetation in water.
- Forbidden habitats: `Creek`, `Bluff`, `Outcrop` rejected.
- Hard safety limits:
  - Max tufts: $4096$
  - Max triangles: $80,000$
  - Max scene entities: $8$

## Verified Runtime Receipts

Isolated native build and execution via `Run.ps1` with the verified juvenile Smalltrotter skin asset (`20261013-013921-eafbd522`):
- `meadow_batch_objects`: 3
- `meadow_tufts`: 3,919 (identical to baseline spatial sample count)
- `meadow_triangles`: 49,844 (well below 80,000 limit, reduced from 70,446 due to slender herb/fern efficiency)
- `meadow_vertices`: 86,534 (down from 117,410)
- `meadow_rejected_wet_samples`: 425 (exact deterministic match to baseline)

Visual inspection of `draft-host.png` confirms that the tall rigid triangular pyramids are eliminated, replaced by a believable, low-lying, multi-form Cretaceous groundcover layer.
