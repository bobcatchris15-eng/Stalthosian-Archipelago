# Meadow Cluster Readability

## Context and Baseline Inspection

Inspection of the baseline native runtime capture (`var/reports/runtime/20261013-180922-068247d3/draft-host.png`) at the authored 8 m hero camera distance revealed a critical readability defect: procedural meadow groundcover appeared as isolated, tiny, dark specks scattered thinly across a bare, uniform plain.

### Root Causes Identified
1. **Lack of Clustering**: Sampling iterated a regular grid of $2.5\,\text{m}$ cells across $x, z \in [-55.0, 55.0]$, but generated candidate positions with independent uniform jitter across the entire $[-1.1\,\text{m}, +1.1\,\text{m}]$ span of each cell. This distributed blades 1 to 2 meters apart as solitary individuals rather than botanical colonies or clumps.
2. **Sub-Pixel Blade Thinning**: Morphotype 0 blade widths ($0.009\,\text{m}$ base, $0.0045\,\text{m}$ mid-span) and heights ($0.11\text{--}0.16\,\text{m}$) projected to sub-pixel or 1-pixel widths from 8 meters away under standard FOV. Without alpha-to-coverage or thick geometry, rasterization frequently missed pixel centers, causing blades to aliase away or appear as disconnected dark pixels.
3. **Premature Budget Exhaustion**: Uniform candidate inflation across the entire $110 \times 110\,\text{m}$ area exhausted the 4,096 tuft budget prematurely along negative $z$ coordinates, preventing balanced coverage across the visible view corridor.

## Refinement Architecture

This refinement modifies only `stalthos-runtime/engine/meadow_scene.hpp` to implement seeded clustered coverage and readable varied silhouettes at 8 m camera distance, without brute-force budget expansion or uniform grass carpets.

### 1. Seeded Clustered Coverage with Open Gaps
- **Cluster Anchor Latticing**: The $44 \times 44$ cell lattice ($2.5\,\text{m}$ step) acts as a potential cluster anchor field rather than individual blade placement.
- **Biased Creek-Bank Corridor Density**:
  - **Riparian Banks** ($4.6 \le dx < 11.0\,\text{m}$ from creek centerline): High moisture zone; cluster probability set to $75\%$, generating lush groups of $3\text{--}5$ tufts along both creek shores.
  - **OpenLowland Near-Bank Terrace** ($11.0 \le dx < 22.0\,\text{m}$, including hero Smalltrotter position at $x = 12, z = 0$): Playable terrace; cluster probability set to $56\%$, generating prominent meadow clumps of $3\text{--}5$ tufts.
  - **Mid-Lowland** ($22.0 \le dx < 36.0\,\text{m}$): Cluster probability steps down to $28\%$ ($2\text{--}3$ tufts per clump).
  - **Distant Lowland** ($dx \ge 36.0\,\text{m}$): Cluster probability drops to $14\%$ ($2$ tufts), maintaining expansive natural open gaps of bare soil and sand.
  - **Woodland Understory** (`shoulder > 0.35`): Cluster probability set to $35\%$ ($2\text{--}3$ tufts per clump) forming dappled understory patches under the trees.
- **Tight Botanical Clumping**:
  - Tuft 0 (core tuft) anchors the clump center with minor jitter ($\le 0.12\,\text{m}$).
  - Tufts $1 \dots N-1$ (companion tufts) cluster tightly around the anchor within radius $r \in [0.14, 0.40]\,\text{m}$.
  - The resulting clump interweaves blades, fronds, and leaves into a cohesive tussock, while leaving $1.5\text{--}3.5\,\text{m}$ of open ground between clusters.

### 2. Silhouette Readability at 8 m Camera Framing
- **Morphotype 0: Slender Curved Blade Clump (Strap Sedge / Meadow Grass)**:
  - Base height: $0.28\text{--}0.46\,\text{m}$ in Riparian, $0.20\text{--}0.34\,\text{m}$ in OpenLowland (knee-to-calf height relative to juvenile Smalltrotter).
  - Base width: $0.024\text{--}0.038\,\text{m}$, tapering to $0.012\text{--}0.020\,\text{m}$ at mid-span, ensuring solid multi-pixel rasterization coverage at 8 m.
  - Graceful arching: mid-span rises to $72\%$ of height, while tip droops toward soil ($y_\text{tip} \approx y_0 + [0.14\text{--}0.32] \times h$).
- **Morphotype 1: Small Cretaceous Fernlet Rosette (Royal Frond / Disturbance Fern)**:
  - Frond length: $0.24\text{--}0.38\,\text{m}$, pinna spread: $0.10\text{--}0.16\,\text{m}$.
  - Stipe width: $\ge 0.016\,\text{m}$. Radiating fan layout produces distinct feathered diamond silhouettes.
- **Morphotype 2: Low Creeping Herb Rosette (Broadleaf Groundcover / Beadflower Analogue)**:
  - Leaf length: $0.14\text{--}0.22\,\text{m}$, leaf width: $0.065\text{--}0.10\,\text{m}$.
  - Radiating flat rosette ($0.28\text{--}0.44\,\text{m}$ total spread) providing stark geometric contrast against arching upright grass blades.
- **Vertex Color Palette**:
  - Enhanced living chlorophyll tones with dark grounded roots ($[42, 58, 26]$ Riparian, $[48, 62, 28]$ Lowland), vibrant foliage mids ($[74, 124, 44]$ Riparian, $[84, 120, 42]$ Lowland), and sunlit tips ($[108, 160, 52]$ Riparian, $[122, 154, 50]$ Lowland).

### 3. Exclusions and Invariants
- **Strict Water Exclusion**: `ReferenceLandscape::waterContains(tuftX, tuftZ)` evaluated for every tuft sample. Submerged samples are rejected and increment `counts.rejectedWetSamples`.
- **Habitat Exclusions**: `Creek`, `Bluff`, and `Outcrop` strictly rejected.
- **Resource Limits**:
  - Max tufts: $4,096$
  - Max triangles: $80,000$
  - Max batch entities: $8$
- **Entity Batching**: Retained exact 3 batch objects (`meadow_groundcover_riparian`, `meadow_groundcover_open_lowland`, `meadow_groundcover_woodland`).
- **Determinism**: Fully derived via `seed ^ domain::hashMix(...)`.

## Verified Runtime Receipts

All runs built isolated against cached Wicked Engine inputs and executed via `Run.ps1` with the verified juvenile Smalltrotter model, walk cycle metadata, and baked Splitcrown trees at matched 8 m inspection camera framing (`camera_distance_m: 8.0`, `inspection_camera: true`).

| Metric | Baseline (`180922-068247d3`) | Intermediate (`182034-2d1c05bb`) | Final Refined (`182257-04538d20`) | Hard Limit |
|---|---|---|---|---|
| `meadow_batch_objects` | 3 | 3 | 3 | $\le 8$ |
| `meadow_tufts` | 3,919 | 1,961 | 2,443 | $\le 4,096$ |
| `meadow_triangles` | 49,844 | 26,704 | 33,285 | $\le 80,000$ |
| `meadow_vertices` | 86,534 | 46,380 | 57,755 | N/A |
| `meadow_rejected_wet_samples` | 425 | 206 | 292 | N/A |

### Visual Verification
Inspection of the matched capture (`var/reports/runtime/20261013-182257-04538d20/draft-host.png`) confirms:
- Isolated tiny specks are eliminated.
- Groundcover forms dense, believable clusters near the hero and along the creek shoreline.
- Arching grass blades and broadleaf herb rosettes exhibit clearly readable silhouettes with natural contact shading against the terrain.
- Open gaps between clusters preserve the natural savannoid / riparian landscape feel without artificial turf carpets.

## Limitations and Boundaries
- No claims of final photorealism, full-draft completion, or 30 FPS acceptance are made.
- Geometry remains low-poly double-sided blade approximations ($3\text{--}5$ triangles per element) bounded for the intended GTX 1080 target, with target-hardware performance still unmeasured.
- Skipped checks: dynamic wind physics simulation, runtime terrain modification, and cross-platform shader validation.

## Parent integrated comparison
Frozen worker775a114 only changed meadow_scene.hpp and its doc (worker doc at repository-level docs; parent stores it here). Parent Release build and actual183102-4b62df0f capture use corrected stage GLBs, existing creature ground offset0 and the same8m inspection framing as182646-fe553c16. Counts:3batches,2443tufts,33285triangles,57755vertices,292wet samples rejected. All twenty trees/eight imported ferns still load; strict sole complete/finite diagnostics remain true. Parent viewed both images: clustered foliage is more readable, but leaves remain angular and the landscape remains unfinished. No target-hardware performance or photographic appearance acceptance. Source retains the exact pinned private creek-center formula with authoritative habitat/wet exclusions. An initial parent attempt to call that private method failed compilation; raw failure is retained, then the documented formula was restored and final build passed. No domain API was widened.
