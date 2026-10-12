# HeroViewController

`HeroViewController` is a pure deterministic C++17 state machine governing the third-person camera and input routing for the hero animal, serving as the interface between user control and `HeroSim` / `BeastCore` intent without assuming authority over animal locomotion.

## Focus and Modes

The controller cleanly decouples input focus from camera modes with explicit, independent transactional setters:

1. **Input Focus** (`InputFocus`):
   - `BodySteering`: Directional commands (e.g. WASD) drive animal physical steering. Travel destination proposals are ignored/cleared.
   - `FreeCamera`: Directional commands pan the camera anchor locally; ground clicks record `pendingDestination` intent.
   - Independent setter: `setInputFocus(InputFocus focus)`.

2. **Camera Mode** (`CameraMode`):
   - `TetheredFollow`: The camera look-target is centered on the animal's physical body position.
   - `LocallyDetached`: The camera looks at an independent anchor point tethered to the animal's current position.
   - Independent setter: `setCameraMode(CameraMode mode)`.

3. **Convenience & Anchor Setters**:
   - `setDetachedAnchor(Vec3 anchor)`: Directly sets the detached anchor position with coordinate magnitude validation (`<= 1e6`).
   - `returnToAnimal()`: Atomically returns to `TetheredFollow` and `BodySteering`, clearing any pending destination.
   - `toggleDetachedMode(Vec3 currentBodyPosition)`: High-level toggle convenience.

## Tether Boundaries & Attenuation

The detached anchor and rendered camera eye are bounded relative to the animal's current body position (independent of facing/heading):

- **Comfort Radius** (`comfortRadius`, default `30.0` m):
  - Soft threshold.
  - **Soft Outward-Only Attenuation**: Radial motion expanding away from the body beyond the comfort radius is attenuated linearly:
    $$\alpha = \frac{\text{hardRadius} - d}{\text{hardRadius} - \text{comfortRadius}} \in [0, 1]$$
    reaching $0$ at the hard radius.
  - **Inward & Lateral Control Retention**: Radial inward motion (towards the animal) and tangential/lateral motion (orbiting the boundary) remain 100% responsive and unattenuated.
- **Hard Radius** (`hardRadius`, default `50.0` m):
  - Strict physical cap.
  - Bounds the **ACTUAL rendered camera eye** in 3D (and planar projection), accounting for spherical orbit distance/elevation offset, target height ($0.8$ m), and terrain clearance lift.
  - Clamping adjusts the detached anchor to guarantee that rendered camera eye distance does not exceed the hard tether boundary.
- **Configurable Bounds**:
  - `setTetherBounds(double comfortRadius, double hardRadius)`: Validates that $0 < \text{comfortRadius} \le \text{hardRadius} \le 10^6$.
  - `getComfortRadius()` and `getHardRadius()`.

## Dynamic Follow & Facing Invariance

- **Moving Body Smooth Follow**:
  - When the animal moves such that the anchor lies outside the comfort radius, the anchor smoothly tracks towards the comfort boundary at rate `kEnvelopeFollowRate = 3.0` s$^{-1}$ via exponential decay:
    $$\text{factor} = 1.0 - \exp(-\text{rate} \cdot \text{cappedDt})$$
  - The exponential formulation guarantees asymptotic convergence with strictly zero overshoot even under arbitrarily large simulation timesteps.
- **Facing Invariance**:
  - The detached anchor is anchored to the animal's current world position, completely independent of body facing/heading. Animal rotation (`bodyHeading`) does not induce detached anchor or camera eye movement.

## Terrain Clearance & Conflict Resolution

- Camera eye height queries the optional `CameraFraming::TerrainHeightFn` callback to ensure minimum terrain clearance (`0.25` m).
- If terrain clearance lifts the camera eye vertically such that the required vertical displacement exceeds the hard tether radius ($|\Delta Y| > \text{hardRadius}$), both geometric constraints cannot simultaneously be satisfied.
- In this conflict scenario, the controller throws `TetherTerrainConflictException` (derived from `std::runtime_error`) and rolls back state transactionally to the pre-update snapshot.

## Numerical Safety & Transactional Guarantees

- **Coord Bounds**: All coordinates (`bodyPosition`, `detachedAnchor`, destinations) are strictly checked against `CameraFraming::kMaxCoordinateMagnitude` ($10^6$ m).
- **Stable Input Normalization**: `applyDetachedPan` uses `std::hypot` to prevent intermediate floating-point overflow on large inputs before normalizing magnitude.
- **Timestep Caps**: Deltas are capped at `kMaxTimeStep = 0.5` s to protect against simulation lag spikes.
- **Arithmetic Overflow**: Orbit and zoom multiplication steps check bounds prior to evaluation.
- **State Validation**: `setState(...)` validates enums, finite floats, coordinate caps, and tether radius consistency prior to mutation.

## What is Implemented vs What is NOT

### Implemented
- Pure deterministic C++17 state machine with zero allocations and no global state.
- Decoupled camera mode and input focus with transactional getters and setters.
- Configurable comfort and hard tether bounds.
- Soft outward-only attenuation with 100% inward and lateral control retention.
- Moving-body smooth envelope follow with large-dt overshoot prevention.
- Hard tether bounding on the actual rendered camera eye (accounting for orbit offset, target height, and terrain lift).
- Heading invariance for detached camera anchor and eye.
- Typed exception (`TetherTerrainConflictException`) and rollback on impossible terrain/tether clearance conflicts.
- Coordinate safety caps ($10^6$), stable hypot normalization, and finite input checks.

### NOT Implemented / Delegated to Host Engine
- **Occlusion Raycasting**: Raycasts against scene geometry (trees, boulders, overhangs) are delegated to the host engine physics/query systems.
- **Pathfinding / Navigation**: Ground-picking produces suggested destination vectors (`activeTravelDestination`), but navigation mesh queries and steering execution belong to `BeastCore` / host.
- **Animal Locomotion Authority**: The controller never mutates body position, velocity, or stance; movement truth remains exclusively with the physics/actor domain.
- **Audio Spatialization**: Listener transform updates and environmental audio occlusion are delegated to the audio subsystem.
Root stepped-terrain regression found a final terrain lift could violate the 3D eye radius after two provisional projections. A final actual-eye check now rejects that case transactionally. The pure controller is foundation only; the native host still uses the earlier inspection/follow controls until explicit integration.
