# Camera State Persistence

This document specifies the persistence contract, binary format, validation rules, and host integration contract for camera state in the Stalthosian Archipelago (STL6 format).

## Format Specification (STL6)

The runtime records interactive camera mode, input focus, detached anchor coordinates, and spherical orbit parameters in `SaveData` via `std::optional<CameraStateSnapshot>`. In save version 6 (magic `STL6`, version `6`), the camera state snapshot is serialized immediately following the `resourceLedger` optional block and immediately before `activeTransients`:

- **1 byte**: presence flag (`0` = omitted / `std::nullopt`, `1` = present). Any value `> 1` is rejected as corrupt (`corrupt camera state presence flag`).
- When present, a fixed-width **50-byte** payload:
  - **1 byte**: `mode` (`uint8_t`, `0` = `TetheredFollow`, `1` = `LocallyDetached`; values `> 1` rejected as `invalid camera mode`).
  - **1 byte**: `focus` (`uint8_t`, `0` = `BodySteering`, `1` = `FreeCamera`; values `> 1` rejected as `invalid input focus`).
  - **24 bytes**: `detachedAnchor` (three 8-byte IEEE 754 doubles: `x`, `y`, `z`). Must be finite and within coordinate safety bound $\le 10^6$ meters; violations rejected as `invalid camera detached anchor`.
  - **8 bytes**: `orbitAzimuth` (8-byte IEEE 754 double, finite radians, bounded to -10 through +75 degrees).
  - **8 bytes**: `orbitElevation` (8-byte IEEE 754 double, finite radians, bounded to -10 through +75 degrees).
  - **8 bytes**: `orbitDistance` (8-byte IEEE 754 double, finite meters, bounded to the actual controller range $2.0 \le \text{orbitDistance} \le 18.0$). Orbit violations rejected as `invalid camera orbit parameters`.

## Validation and Transactional Guarantees

1. **Transactional Writes**: `writeSave` executes full validation (`persistence::detail::validate`) before creating or opening any destination or temporary files. Any malformed input (mode `> 1`, focus `> 1`, non-finite or out-of-bounds anchor, non-finite or negative or excessive orbit distance) throws immediately, guaranteeing disk state is never partially written or corrupted.
2. **Failclean Read Validation**: `readSave` validates camera presence flags and validates the deserialized `CameraStateSnapshot` immediately upon parsing. Corrupted flags, corrupted payloads, out-of-range bounds, truncated payloads, or trailing bytes trigger clean exceptions and prevent partial state exposure.
3. **No Parallel Codec**: Version STL6 is integrated directly into the unified `persistence.hpp` serialization pipeline. Old formats (STL1 through STL5) are handled by the same codec without divergent or duplicate parsing logic.

## Legacy Compatibility and `HostPose::cameraMode` Semantics

- **Strict Separation of Concerns**: Legacy `HostPose::cameraMode` (values `0` = behind follow, `1` = broadside, `2` = frontal) is preserved strictly for legacy third-person framing angles. It is NOT conflated with, replaced by, or reinterpreted as `CameraMode::LocallyDetached`.
- **Old Save Formats (STL1..5)**:
  - Reading any legacy STL1, STL2, STL3, STL4, or STL5 save file succeeds with `data.cameraState` set to `std::nullopt`.
  - Loaded `HostPose::cameraMode` retains its exact legacy `0`/`1`/`2` framing value.
- **Save Migration**: Re-saving any loaded save automatically upgrades the file format to `STL6`. If `cameraState` is `std::nullopt`, the STL6 presence byte is serialized as `0` (1 byte overhead), preserving exact size matching and backwards portability.

## Host Integration Contract (Remaining Parent Host Wiring)

Integration with the Wicked Engine host (`stalthos-runtime/engine/main.cpp`) is owned by the host integration worker. The required integration pattern is:

1. **On Save**:
   Capture current controller state from `stalthos::domain::HeroViewController`:
   ```cpp
   const auto camState = viewController.getState();
   stalthos::persistence::CameraStateSnapshot camSnap;
   camSnap.mode = static_cast<std::uint8_t>(camState.mode);
   camSnap.focus = static_cast<std::uint8_t>(camState.focus);
   camSnap.detachedAnchor = camState.detachedAnchor;
   camSnap.orbitAzimuth = camState.orbit.azimuth;
   camSnap.orbitElevation = camState.orbit.elevation;
   camSnap.orbitDistance = camState.orbit.distance;
   data.cameraState = camSnap;
   ```

2. **On Load**:
   Inspect `loadedSaveData->cameraState`:
   - If present (`loadedSaveData->cameraState.has_value()`):
     ```cpp
     const auto& cam = *loadedSaveData->cameraState;
     auto state = viewController.getState();
     state.mode = static_cast<stalthos::domain::CameraMode>(cam.mode);
     state.focus = static_cast<stalthos::domain::InputFocus>(cam.focus);
     state.detachedAnchor = cam.detachedAnchor;
     state.orbit.azimuth = cam.orbitAzimuth;
     state.orbit.elevation = cam.orbitElevation;
     state.orbit.distance = cam.orbitDistance;
     viewController.setState(state);
     ```
   - If absent (`std::nullopt`, as with STL1..5 saves):
     Fall back to existing CLI preset logic (`--inspection`, `--camera-distance`, or legacy `HostPose::cameraMode` angle).

3. **Autonomy and Diagnostic Decoupling**:
   When restoring `InputFocus::FreeCamera`, host steering must remain decoupled: free-camera navigation operates independent of hero locomotion and autonomous behaviors.

## Parent runtime verification — 2026-10-12

Reviewed5b434e6 and aligned distance/elevation validation with CameraFraming
constants. Snapshot default distance is8m. Parent tests cover rejecting controller
boundary violations;17/17 persistence cases passed. Combined native CTest17/17.
Host now writes/restores the optional snapshot; present snapshots take precedence
in framing, while legacy saves retain their old preset behavior.

Actual save20261012-233954-9187394f ends inLocallyDetached/FreeCamera at2m
orbit distance. Reload20261012-234033-3e343062 restores the same mode/focus/
distance and exactly matching reported Wicked eye coordinates, then re-saves STL6.
The earlier STL5 resource-continuity save also loads without a camera snapshot.
Tether configuration and travel intent remain outside this DTO; mouse/keyboard,
audio/raycast/TravelTo and full visual acceptance remain pending.
