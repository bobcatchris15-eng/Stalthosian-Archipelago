# Organic Rocks (`stalthos-runtime/engine/organic_rocks.hpp`)

## Purpose and Scope

`organic_rocks.hpp` implements `stalthos::engine::populateOrganicRocks` (and `PopulateOrganicRocks`), providing deterministic, bounded procedural rock and boulder generation that consumes authored placements from `stalthos::domain::ReferenceLandscape`.

This module replaces the legacy provisional box rocks previously placed in `engine/main.cpp` (`Terrain`) with natural, multi-faceted, irregular flattened/convex rocky volumes.

## Design and Geometry

1. **Subdivided Icosahedron Base**:
   - Begins from an icosahedron base (12 vertices, 20 triangular faces).
   - Subdivides each face into 4 sub-triangles (80 triangles, 42 unique vertices after shared midpoint deduplication).
   - Eliminates poles and zero-area degeneracy while keeping per-rock triangle counts well under the `< 1500` limit (exactly 80 triangles and 42 vertices per rock).
2. **Deterministic Faceting and Asymmetry**:
   - Uses domain hash seeding derived strictly per placement (`domain::hashMix` and placement index) without global randomness or time.
   - Applies oblate height compression (`ry` ~ 0.42–0.58 scale) and horizontal elongation (`rx`, `rz`) reflecting natural sedimentary and caprock bedding planes.
   - Vertices are perturbed with controlled radial jitter for faceted mineral appearance.
3. **Winding and Normals**:
   - Integrates `engine/environment_scene.hpp detail::addOrientedTriangle` to orient every triangle according to Wicked Engine's outward convention: `cross(p2 - p0, p1 - p0)` facing outward.
   - Computes normals with `COMPUTE_NORMALS_SMOOTH_FAST` to preserve organic faceting without black artifacts or inverted faces.
4. **Terrain Ground Fit**:
   - Reuses `detail::groundContactOffset` and `rotatePropVertex` to sample reference terrain elevation across all rock vertices.
   - Rocks are seated firmly on the analytic landscape without floating or sinking.
   - **Limitation**: Contact is exact at sampled vertices against the analytic reference height function; triangle interiors and rendered terrain interpolation may differ slightly between grid vertices.
5. **Shared Earthy PBR Material**:
   - Reuses a single shared material entity per scene (`scenery_mat_organic_rock`), evaluated from `domain::EnvironmentPalette::evaluate(SceneryMaterialRole::Rock, seed)`.
   - Dielectric non-metal (`metalness = 0.0`), weathered mineral roughness (`roughness = 0.70 - 0.88`), muted earthy albedo with subtle weathering undertones.
6. **Safety Bounds**:
   - Hard cap of `< 128` rock objects per scene.
   - ReferenceLandscape seed 1 produces exactly 22 rock objects (17 Rocks, 5 Boulders), 924 vertices, and 1,760 triangles in total.

## Interface

```cpp
namespace stalthos::engine {

struct OrganicRockCounts {
    std::size_t objectCount = 0;
    std::size_t vertexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t rockCount = 0;
    std::size_t boulderCount = 0;
};

OrganicRockCounts populateOrganicRocks(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    std::uint64_t seed = 1
);

}
```

## Root Wiring Instructions

In `stalthos-runtime/engine/main.cpp`:
1. Include `#include "engine/organic_rocks.hpp"`.
2. In `Terrain(wi::scene::Scene& scene)` (or after `populateEnvironmentScene` in `wWinMain`), remove the provisional box rock placement loop:
   ```cpp
   // REMOVE old box loop:
   // int rockIndex=0;
   // for (const auto& placement : landscape.placements()) {
   //     using Kind=stalthos::domain::PlacementKind;
   //     if (placement.kind!=Kind::Rock && placement.kind!=Kind::Boulder) continue;
   //     float scale=static_cast<float>(placement.scale)*(placement.kind==Kind::Boulder?2.f:1.f);
   //     Box(scene, "provisional outcrop rock " + std::to_string(rockIndex++), ...);
   // }
   ```
3. Call `populateOrganicRocks`:
   ```cpp
   const auto rockCounts = stalthos::engine::populateOrganicRocks(scene, landscape, world.worldSeed());
   ```
