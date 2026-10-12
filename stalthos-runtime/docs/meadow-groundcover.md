# Meadow Groundcover Scene Helper

`engine/meadow_scene.hpp` provides the native Wicked Engine helper `populateMeadowScene(wi::scene::Scene&, const ReferenceLandscape&, uint64_t seed)` to populate dense, habitat-specific, low-lying groundcover across the central meadow and riparian corridor.

## Design and Habitat Differentiation

1. **Deterministic Spatial Distribution**:
   - Covers the central coordinate expanse $x, z \in [-55.0, 55.0]$ meters.
   - Evaluates a 2.5 m anchor cell lattice with seed-derived jittered tuft placement.
   - High density clustering in `Habitat::Riparian` (richer, taller sedges $[0.30, 0.55\,\text{m}]$).
   - Medium continuous coverage in `Habitat::OpenLowland` ($[0.20, 0.42\,\text{m}]$).
   - Sparse understory tufts in `Habitat::Woodland` ($[0.15, 0.30\,\text{m}]$).
   - **Exclusions**: Strictly skips `Habitat::Creek`, `Habitat::Bluff`, `Habitat::Outcrop`, and any sample where `ReferenceLandscape::waterContains(x, z)` is true. No vegetation is placed in water.

2. **Irregular Curved Foliage Blades**:
   - Each tuft comprises 5 to 7 tapering blades radiating outward with non-uniform azimuth angles and varied tilt/arch.
   - Each blade consists of 5 vertices and 3 double-sided triangles (quad base + tapered triangle tip).
   - Authoritative terrain height grounding: blade root vertices are anchored directly to `ReferenceLandscape::height(x, z)`.

3. **Vertex Color Gradients and Muted Earthy Palette**:
   - Muted, earthy olive coloration without artificial neon hues:
     - Root: Dark loamy olive-brown ($[48\text{--}54, 60\text{--}64, 28\text{--}32]$).
     - Mid: Rich muted olive / moss green ($[65\text{--}78, 88\text{--}112, 36\text{--}44]$).
     - Tip: Sunlit golden olive / dry straw ($[88\text{--}110, 112\text{--}142, 46\text{--}56]$).
   - Subtle deterministic seed jitter per tuft ensures visual variation across the terrain.

4. **Batching and Hard Resource Limits**:
   - Rather than instantiating hundreds of scene entities or individual blade draw calls, groundcover is consolidated into 3 batched scene entities by habitat (`Riparian`, `OpenLowland`, `Woodland`).
   - Hard bounds:
     - Tufts: $\le 4096$ (measured: 979 tufts at world seed 42).
     - Triangles: $\le 80000$ (measured: 17,619 triangles at world seed 42).
     - Batch Scene Entities: $\le 8$ (measured: 3 entities).
     - Materials: Double-sided (`SetDoubleSided(true)`), rough nonmetallic (`SetMetalness(0.0f)`), vertex colors enabled (`SetUseVertexColors(true)`).

5. **Static Determinism**:
   - Seed-derived pseudo-randomness via `domain::hashMix` and `domain::hashUnit`.
   - No runtime per-frame regeneration; no global random state or unseeded time dependencies.
   - Normals and winding: Single-layer double-sided triangles with outward DirectX left-handed winding conventions to prevent normal cancellation.

## Truthful Counts

`populateMeadowScene` returns a truthful struct `MeadowSceneCounts`:
- `batchObjectCount`: Number of Wicked scene entities added (typically 3).
- `tuftCount`: Total individual tufts placed (e.g. 979).
- `vertexCount`: Total vertices generated (e.g. 29,365).
- `triangleCount`: Total triangles generated (e.g. 17,619).
- `rejectedWetSamples`: Number of candidate sample points rejected due to water collision (e.g. 107).

## Root Wiring Instructions

In `stalthos-runtime/engine/main.cpp`:
1. Include the header:
   ```cpp
   #include "meadow_scene.hpp"
   ```
2. Following environment and rock population (e.g. after `populateOrganicRocks`), invoke:
   ```cpp
   const auto meadowCounts = stalthos::engine::populateMeadowScene(scene, landscape, 42);
   ```
3. Report metrics in `runtime-receipt.json`:
   ```json
   "meadow_batch_objects": meadowCounts.batchObjectCount,
   "meadow_tufts": meadowCounts.tuftCount,
   "meadow_vertices": meadowCounts.vertexCount,
   "meadow_triangles": meadowCounts.triangleCount,
   "meadow_rejected_wet_samples": meadowCounts.rejectedWetSamples
   ```
Root render review increased candidates per occupied cell fourfold, retaining existing 4096-tuft/80000-triangle caps and wet rejection. Original worker figures above describe the lower-density baseline; final native receipt is authoritative. Coverage remains provisional and far from the reference environment.
Final actual Wicked receipt20261012-105850-e0f08db3 confirms3919tufts/70446triangles in3batchobjects and425wet candidate rejections. Root combined Release build and14/14CTest passed. This is renderer feasibility, not natural habitat or performance acceptance.
