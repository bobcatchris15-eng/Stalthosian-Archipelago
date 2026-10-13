# Bounded Native Woodland Host Rendering

## Summary

This work integrates bounded native woodland stands rendering into the Stalthos native Wicked Engine host, utilizing the reviewed `stalthos::domain::WoodlandStands` placement generator (commit `2d57136`). Woodland stands are rendered using shared mesh and material prototype instances across deterministic positions on dry terrain banks, with base elevations sampled directly from cached rendered terrain triangles.

All hero creature grounding, sole contact diagnostics, four-foot contact flags, animation channels, camera framing, save persistence, and lighting/shadow behavior remain strictly preserved without regression.

## Implementation Details

### Domain Helper & Hard Caps
- **Helper**: `stalthos::domain::WoodlandStands` is instantiated with the domain world seed (`world.worldSeed() = 42`) and default target of 20 trees.
- **Hard Cap**: Strictly enforces `WoodlandStands::kHardCapTrees = 32`.
- **Budgets & Exhaustion**: Candidate attempts (`candidateAttempts = 502`), cluster attempts (`clusterAttempts = 40`), and budget exhaustion (`candidateBudgetExhausted = false`) are truthfully surfaced in runtime receipts.

### CLI Parameters & Host Equivalents
New optional parameters allow providing stage assets to the runtime:
- **PowerShell (`Run.ps1`)**:
  - `-WoodlandAdultTree <path>`: Path to adult tree GLB. Validates path existence before launch.
  - `-WoodlandSaplingTree <path>`: Path to sapling tree GLB. Validates path existence before launch.
- **Host Executable (`Stalthos.exe`)**:
  - `--woodland-adult-tree <path>`
  - `--woodland-sapling-tree <path>`

### Prototype Loading & Resource Sharing
- **Single Import**: Tree assets are imported exactly once via `ImportModel_GLTF` into a prototype scene. `tempScene.Update(0)` resolves renderer object matrices before bounds and part snapshots are taken. Instances use each object world matrix, preserving GLB ancestor rotation and scale. Prototype entities are disabled and have shadow casting disabled.
- **Shared Geometry**: Tree instances create lightweight transform hierarchies referencing the prototype's `meshID` and material entities directly. Vertex and index buffers are never duplicated.
- **Truthful Geometry Accounting**:
  - `woodland_unique_objects`: 2 (foliage and structure meshes).
  - `woodland_unique_vertices`: 8,888.
  - `woodland_unique_triangles`: 5,460.
  - `woodland_drawn_triangles`: 76,440 (14 adult trees &times; 5,460 triangles).
  - Zero repeated imports per frame or across instances.

### Rendered Ground Surface Sampling
- **Elevation Sampling**: Base elevation `y` for each placed tree is sampled directly from the cached `draft reference ground` mesh using `stalthos::domain::RenderedGroundSurface::sampleHeight(x, z)`. Missing rendered ground matrices produce an unavailable-cache issue; identity is never used as a substitute.
- **No Analytic Fallback**: If the rendered ground cache is uninitialized or a coordinate is out of bounds, the tree is explicitly skipped with a typed issue reason (`rendered_ground_unavailable` or `proposal_out_of_rendered_ground_bounds`). Analytical height (`landscape.height(x, z)`) is never quietly substituted.
- **Delta Tracking**: Both sampled rendered ground `y` and analytical `analytical_y` are recorded per tree. Measured differences range from -0.030 m to +0.134 m across dry valley slopes.

### Asset Validation & Authoring Mismatch
- **Reference Prototype**: `var/worktrees/splitcrown-reference/.../plant.splitcrown.ar1/lod0.glb` was inspected.
- **Actual Dimensions** (measured through resolved object matrices):
  - Whole height: 28.06 m.
  - Named trunk/structure geometry height: 26.00 m. Unnamed geometry does not get labeled as trunk.
  - Crown width: 24.81 m.
- **Authored Expectation**: Authored young adult target is 14.0 m height with 6.5 m crown.
- **Diagnostic Diagnosis**: The prototype is used strictly as a provisional adult diagnostic. Actual dimensions and authored mismatch are explicitly reported (`woodland_adult_authored_mismatch = true`, `woodland_adult_stage_correct = false`, `woodland_adult_quality_label = "provisional_unverified"`). No biological stage claim is inferred from a height tolerance. Source minimum Y is aligned to rendered ground; the derived correction is included in the existing authored mismatch description. Jules 8336722969998514867 source fix remains pending.
- **Sapling Gating**: Sapling proposals require an asset with valid sapling dimensions (&le; 10 m, target 4.2 m). A mature GLB is strictly prevented from masquerading as a sapling (`invalid_sapling_asset`). When no valid sapling GLB is provided, all 6 sapling proposals are explicitly skipped (`missing_stage_asset: sapling`).

### Sun Shadows & Materials
- Trees participate fully in scene directional sunlight shadowing (`CAST_SHADOW`) and multi-scale ambient occlusion (`AO_MSAO`).
- Materials, normal maps, roughness, and foliage two-sided properties from the GLB are preserved.

## Verification

### Native Test Suite
All 23 native CTest tests pass (existing parent baseline; this focused correction was rebuilt as the host):
```
100% tests passed, 0 tests failed out of 23
Total Test time (real) = 0.99 sec
```

### Matched Native Captures & Sole Clearances
Comparison between baseline parent capture and woodland host capture using identical hero creature (`creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb`), metadata, `-CreatureSoleOffset 0`, and `-InspectionCamera`:
- **Baseline Capture**: `var/reports/runtime/20261013-133751-3f3f9385` (`draft-host.png`, 2,924,987 bytes)
- **Woodland Capture**: `var/reports/runtime/20261013-134357-e4846183` (`draft-host.png`, 3,025,161 bytes)

#### Idle Sole Clearances & Flags
| Diagnostic Metric | Baseline Run (`133751`) | Woodland Host Run (`134357`) | Parity Status |
| :--- | :--- | :--- | :--- |
| `strict_complete` | `true` | `true` | Preserved |
| `strict_disjoint` | `true` | `true` | Preserved |
| `strict_finite` | `true` | `true` | Preserved |
| `rendered_ground_cached` | `true` | `true` | Preserved |
| Front Left Min Rendered Clearance | -0.8985 mm | -0.8985 mm | Bit-identical |
| Front Right Min Rendered Clearance | -0.9065 mm | -0.9065 mm | Bit-identical |
| Rear Left Min Rendered Clearance | -1.2967 mm | -1.2967 mm | Bit-identical |
| Rear Right Min Rendered Clearance | -1.3005 mm | -1.3005 mm | Bit-identical |
| Ground Support Fallbacks | 0 | 0 | Preserved |

#### Woodland Placed / Skipped Quotas (Seed 42, Target 20)
- `woodland_proposals_total`: 20
- `woodland_proposals_placed`: 14 (all adult stage)
- `woodland_proposals_skipped`: 6 (all sapling stage, `missing_stage_asset: sapling`)
- `woodland_budget_exhausted`: `false`
- `woodland_unique_triangles`: 5,460
- `woodland_drawn_triangles`: 76,440

### Focused transform fixture and host rerun
A synthetic GLB with a 90-degree parent rotation and nonuniform parent scale (2, 3, 1) was imported by the native host. Its local triangle bounds are 1 x 2 x 1 m; the receipt measured transformed crown span 6.00 m and whole height 2.00 m, confirming the parent matrix affects measurements. Receipt: `var/reports/runtime/20261013-152948-3da0599b/runtime-receipt.json`.

The ordinary Splitcrown native host rerun measured 28.0616 m whole, 26.0007 m named structure, and 24.8155 m width, with 14 adult proposals placed and ground cache available. Strict sole flags stayed `true` in the receipt (`strict_complete`, `strict_disjoint`, `strict_finite`); no sole-related host paths were changed. Receipt: `var/reports/runtime/20261013-152951-c556e112/runtime-receipt.json`. These are transform and runtime diagnostics, not visual, biological-stage, contact-acceptance, FPS, or full-draft evidence.

## Limitations
- **Hardware & FPS**: Target hardware is GTX 1080 + Ryzen 1600. External frame-time profiling is not metered (`frame_time_measured: false`). No 30 FPS acceptance or full draft acceptance is claimed (`full_draft_complete: false`, `visual_accepted: false`).
- **Plant LOD Quality**: Plant LOD degraded quality is unverified (`woodland_adult_quality_label: "provisional_unverified"`).
- **Provisional Dimensions**: The provisional adult asset (`lod0.glb`) has 26 m named structure / 28 m whole height exceeding the authored 14 m young adult target. Stage correctness is not inferred from geometry tolerance; upstream source correction remains pending.

## Parent Review
- Owned paths for this focused correction: `stalthos-runtime/docs/woodland-host.md`, `stalthos-runtime/engine/woodland_scene.hpp`.
- No root writes, junction bypasses, push, paid APIs, or planning doc changes.
- Isolated CMake build in worktree `var/build/stalthos-runtime` against readonly root Wicked engine and release cache.
- 23/23 CTests pass cleanly.

## Integrated parent evidence
Parent Release build passed after the final ground diagnostic fix; 23/23 CTests passed. Actual baked prototype capture: var/reports/runtime/20261013-153953-d11dac2c. Fourteen adult instances share mesh/materials and display baked brown trunks. Six saplings remain absent. Datum correction includes instance scale. Correct stage source generation now exists, but its baked exports and native stage comparison remain pending.
