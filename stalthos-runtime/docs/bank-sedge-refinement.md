# Riparian Bank Sedge Clump Geometry Refinement

## Context and Visual Diagnosis

Visual review of recent native runtime capture (`E:/Stalthosian Archipelago/var/reports/runtime/20261013-015256-a97a54ec/draft-host.png`) highlighted that riparian bank vegetation clumps (`DressingKind::DryBankClump`, instantiated via `createBankVegetationClumpMesh` in [environment_scene.hpp](../engine/environment_scene.hpp)) presented as tall, stiff, broad triangular upright blades. Rather than reading as natural Cretaceous wetland monocots or riparian sedge tussocks, these props stood like oversized solid spikes or broad swords near the juvenile dinosaur amid the lower, sparser groundcover.

Per `Rauken-Archipelago/docs/flora.md` and the visual guidelines:
- Late Cretaceous riparian ground vegetation includes arching strap-leaf monocots and sedges rather than broad rigid pyramids.
- The scale must remain sensible relative to the juvenile dinosaur ($1.80\,\text{m}$ length, $\sim0.65\,\text{m}$ shoulder/hip height). Bank sedge clumps should arch between $0.14\,\text{m}$ and $0.36\,\text{m}$ in height rather than towering stiffly.
- Morphologically, true sedges exhibit a basal crown/sheath rosette from which narrow, curved, multi-segmented drooping blades radiate with varied azimuths and lengths.

## Refinement Rationale and Geometric Bounds

To address these defects while strictly honoring all system and domain contracts:
1. **Basal Crown Rosette Architecture**:
   - Replaced the wide triangular blade base with a compact basal crown rosette of radius $r_\text{crown} \in [0.015, 0.025] \times s$.
   - Radiates $10\text{--}16$ blades per clump (selected deterministically via `hashUnitF(itemSeed + 50U)`), providing dense tussock fullness without excess polygon cost.

2. **Narrow, Segmented, Drooping Blade Geometry**:
   - Each blade is partitioned into 3 lengthwise segments (4 cross-stations: base sheath $S_0$, lower ascending blade $S_1$, arch crest $S_2$, drooping tip apex $S_3$):
     - $S_0$ (Base): $y_0 = 0.0\,\text{m}$, base width $w_\text{base} \approx 0.008\text{--}0.016\,\text{m}$ (slender strap profile).
     - $S_1$ (Ascending): $y_1 \approx 0.62 \times y_\text{peak}$, width $w_\text{low} = 0.82 \times w_\text{base}$.
     - $S_2$ (Arch crest): $y_2 = y_\text{peak} \approx 0.14\text{--}0.36\,\text{m}$ (calibrated juvenile scale), width $w_\text{mid} = 0.55 \times w_\text{base}$.
     - $S_3$ (Drooping tip): Graceful descending arch returning down toward the soil:
       $$y_\text{tip} = \max\left(0.015\,\text{m},\; y_\text{peak} \times (0.28 - 0.22 \times \text{droopFactor})\right)$$
   - Each blade uses 7 vertices ($2 + 2 + 2 + 1$) and 5 double-sided triangles ($2 + 2 + 1$).

3. **Strict Geometry and Vertex Budget Compliance**:
   - Contract requirement: $\le 256$ triangles and $\le 512$ vertices per clump.
   - For any clump with $B \in [10, 16]$ blades:
     - Vertices per clump: $B \times 7 \le 16 \times 7 = 112$ vertices ($\ll 512$).
     - Triangles per clump: $B \times 5 \le 16 \times 5 = 80$ triangles ($\ll 256$).
   - Finite coordinates guaranteed via `std::clamp(scale, 0.25, 1.4)` and bounded trigonometric projections.

4. **Preservation of Scene Contracts and Material Integrity**:
   - Double-sided material: The shared material `scenery_mat_bank_clump` has `mat->SetDoubleSided(true)`. The mesh emits single-face triangles without reversed duplicate normals, avoiding cancelled smooth normal calculations in Wicked Engine's `MeshComponent::ComputeNormals`.
   - Palette & Vertex Colors: Preserves shared neutral material tint while establishing an organic 4-tier vertex color gradient:
     - Crown root: earthy olive-brown (`#343C20`)
     - Sheath base: deep wetland olive (`#446026`)
     - Arch crest: lush green (`#587E2E`)
     - Sunlit drooping tip: warm golden-green (`#76983A`)
   - Placements, seed mixing, and spatial bounds: Completely unperturbed; `dressing.proposals()` generation, counts (48 bank clumps), coordinates, and water/wetmask exclusions in [habitat_dressing.hpp](../domain/habitat_dressing.hpp) and [environment_scene.hpp](../engine/environment_scene.hpp) are preserved exactly.

## Verification and Test Results

- **Isolated Native Build**: Successfully compiled `Stalthos.exe` and test suites via MSVC 19.51 Release build linked against root `var/vendor/WickedEngine` and `var/vendor/wicked-fresh-release-cache`.
- **Domain Test Suite Execution**:
  - `Stalthos_habitat_dressing.exe`:
    ```
    habitat dressing: proposals=165 (logs=27, pebbles=64, bank_clumps=48, bluff_accents=26) determinism, bounds, exclusions, and budget passed
    ```
  - Full CTest suite: 17/17 tests passed (100% pass rate, 1.67s real time).
- **Visual Claim Notice**:
  - Per AGENTS.md rule 1 ("Honesty over completeness"), visual acceptance cannot and is not claimed prior to a new host capture by the parent workspace orchestrator.

Parent review relocated this document to runtime/docs, rejects nonfinite scale before geometry generation, and corrects the theoretical arch height range to 0.023..0.460m. The shared placement scales determine the narrower actual distribution. Worker tests are evidence reported by the worker; parent combined build and capture are separate gates.
