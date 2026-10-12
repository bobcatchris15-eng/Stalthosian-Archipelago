# Shore scene draft renderer

`engine/shore_scene.hpp` provides the header helper `populateShoreScene(wi::scene::Scene&, const ReferenceLandscape&, double resolution = 2.0)` to construct runtime water and creekbed geometry from domain contracts.

## Architecture and Geometry Separation

1. **Domain Shore Geometry Consumption**:
   `domain::generateShoreMesh(landscape, resolution)` samples the landscape grid and emits clipped triangle meshes for wet surface and bed within the hard cell limit (65,536 cells) and index/vertex budgets.

2. **Role Splitting Without Unsafe Indices**:
   The domain output packs all vertices and indices into a single `ShoreMesh`. `populateShoreScene` splits geometry by `ShoreMaterial` role (`Surface`, `WetBed`, and `WetBank`):
   - Vertices are indexed locally per role using an O(1) remap table sized to the input vertex count.
   - Indices are remapped locally so no role mesh contains out-of-bounds or cross-role references.
   - Non-finite vertices and out-of-range source indices are strictly rejected.

3. **Triangle Winding Reversal for Wicked Engine Normals**:
   Domain shore geometry uses the standard upward normal orientation `(p1 - p0) × (p2 - p0)`. Because Wicked Engine's `ComputeNormals` calculates face normals with the reversed cross product `(p2 - p0) × (p1 - p0)`, all triangle windings are reversed upon insertion: triangle `(u0, u1, u2)` is stored as `(u0, u2, u1)`. This ensures upward-facing host normals (+Y) for both the water surface and the submerged creek bed.

4. **Shared Materials and Shaders**:
   Material parameters are evaluated deterministically from `domain::EnvironmentPalette`:
   - Water surface (`shore_mat_water_surface`): Configured with `wi::scene::MaterialComponent::SHADERTYPE_WATER`, tinted linear base color, refraction and transmission amounts, and shadows disabled (`SetCastShadow(false)`).
   - Submerged bed (`shore_mat_wet_bed`): Configured as darker, rough, opaque PBR substrate (`SHADERTYPE_PBR`) with shadows enabled (`SetCastShadow(true)`).
   - Moist bank (`shore_mat_wet_bank`): Evaluated from the palette's wet bank role when present.

5. **Legacy Creek Separation**:
   The provisional flat creek fixture in `engine/main.cpp` is not modified here. Root integration will wire `populateShoreScene` into the runtime host and remove the legacy creek strip.

## Compilation and Verification

The helper compiles cleanly in a minimal translation unit against the pinned Wicked Engine headers and compiler configuration (`/std:c++17 /EHsc /utf-8 /arch:AVX` with `_WINDOWS`, `NDEBUG`, `WICKED_CMAKE_BUILD`, `UNICODE`).

## Limitations

- **Provisional Draft**: Mesh detail is governed by discrete grid resolution and 28-step bisection approximations; this is not an analytic hydrodynamic surface.
- **No Final Optics**: Water rendering uses basic planar reflection / refraction flags without authored water caustics, depth-based absorption falloff, or dynamic flow vectors.
- **No Frame-Rate Acceptance**: Standalone helper compilation does not assert 30 FPS acceptance on target GTX 1080 hardware. Final performance and visual acceptance require host integration and capture by root.

Root integration replaces the legacy creek strip. The decorative wet-bed surface is lifted 10 mm above the matching triangulated terrain to avoid coplanar depth flicker; this is an appearance offset, not a change to authoritative landscape/contact heights. Native runtime counts are recorded in the runtime receipt.


Integrated capture found wet-bed/terrain depth interference despite the 10 mm lift. The runtime now renders only the clipped water surface (plus banks when present) and retains the existing terrain creek bed. Domain bed geometry is retained for a future jointly clipped terrain adapter; bed triangle count is truthfully zero in current scene receipts.
