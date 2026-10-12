# CameraFraming Domain Helper

The `CameraFraming` domain module provides a provisional, pure, engine-independent C++17
helper for animal-centered third-person follow, orbit, and close inspection framing. It is
not the complete hero-view camera system.

## Motivation and Background

In the initial draft runtime, a hardcoded 8-meter offset (`x, y+4, z-8`) makes the ~1.8-meter
Smalltrotter hero beast appear small and difficult to inspect in detail.

Per `Rauken-Archipelago/docs/hero-view.md`, camera placement and actor movement authority are
strictly decoupled:
- **No camera drives or steers the body**: The camera never modifies or overrides body position, heading, or navigation state.
- **Bounded follow & orbit**: The player can smoothly orbit azimuth and elevation while observing the hero beast.
- **Close inspection preset**: A 3.0m slant-distance view at a three-quarter perspective allows inspecting creature proportions, plumage/scales, and animations clearly.
- **Default follow preset**: An 8.0m slant-distance view preserves the established framing context.

## Guarantees and Constraints

1. **Distance Clamping**: Slant distance between eye and target is strictly clamped to `[2.0, 18.0]` meters (`kMinDistance` to `kMaxDistance`).
2. **Elevation Clamping**: Orbit pitch above the horizontal is bounded to `[-10.0, +75.0]` degrees (`kMinElevationDegrees` to `kMaxElevationDegrees`), preventing gimbal disorientation or underground viewing angles.
3. **Safe Zoom Updates**: `updateZoom` processes finite wheel deltas, rejects overflow in their product, and clamps result distance.
4. **Bounded Angle Wrapping**: Azimuth wraps safely within `[-pi, +pi)` using `std::fmod` without unbounded loops.
5. **Coordinate Overflow Protection**: Positions exceeding `1,000,000` meters are rejected with `std::overflow_error` to prevent precision loss and vector overflow.
6. **Bounded Height Inputs and Terrain Clearance**:
   - Target height is bounded to `[0, 20]` meters and minimum clearance to `[0, 10]` meters as provisional helper limits.
   - An optional `TerrainHeightFn` callback can be provided to query ground height at the eye's `(x, z)` coordinate.
   - Invalid or out-of-bounds sampled terrain heights fail explicitly; the helper does not silently claim clearance when the sample is unknown.
   - If the terrain elevation would place the eye lower than `terrainHeight + minTerrainClearance`, the eye's height is raised.
   - **Target authority is unchanged**: The look target remains centered on the actor (at `bodyPosition.y + targetHeight`).
   - **No magical occlusion claims**: The helper does not claim full raycasted scene visibility or occlusion solving; it provides a reliable floor clearance constraint.
7. **Framerate-Independent Interpolation**: Provides optional bounded exponential interpolation (`interpolateScalar`, `interpolateVec3`, `interpolateAngle`) parameterized by rate and delta-time `dt` (`1 - exp(-rate * dt)`).

The close inspection preset uses the front three-quarter region under this helper's
convention: forward is `+Z` at heading zero and eye offset is `(sin(azimuth), cos(azimuth))`
in the XZ plane. The helper does not implement raycast occlusion, mesh collision, camera
input handling, view transforms, or full hero-view requirements.

## Integration with Engine Host

The engine host wires `CameraFraming` into the input and camera update loop:
- Mouse drag adjusts orbit azimuth and elevation.
- Mouse wheel triggers `CameraFraming::updateZoom`.
- Keybindings (e.g., number keys or toggle) switch between `followPreset()` and `closeInspectionPreset()`.
- The returned `eye` and `target` vectors feed directly into view matrix generation (e.g. `XMMatrixLookAtLH` or Wicked Engine camera transforms).

## Integrated host evidence

Root Release build and CTest 13/13 passed on 2026-10-12. V toggles close inspection; wheel changes bounded distance. Existing three diagnostic direction modes remain. Continuous interactive orbit and the rest of HeroView remain unimplemented. Actual 3 m capture: var/reports/runtime/20261012-064940-5472f335/draft-host.png. No visual acceptance.
