# Hero View Host Integration

The Native Host (`stalthos-runtime/engine/main.cpp`) integrates `stalthos::domain::HeroViewController` to decouple input routing from camera modes and enforce tether bounds and safety limits.

## Controls
- `W/A/S/D`
  - In **BodySteering** focus (`InputFocus::BodySteering`): steers the animal directly relative to its facing direction.
  - In **FreeCamera** focus (`InputFocus::FreeCamera`): pans the detached camera anchor laterally and radially within tether limits. Autonomous animal movement, diagnostic walks, and need sequences continue uninterrupted.
- `F1` toggles `LocallyDetached` camera mode and `FreeCamera` input focus, initializing the detached camera anchor at the animal's physical position.
- `V` toggles close inspection preset (`CameraFraming::closeInspectionPreset(yaw)` at 3 m) and standard follow framing (`CameraFraming::followPreset(yaw)` at 8 m).
- Mouse Wheel (`wi::input::GetMouseState().delta_wheel`): applies bounded zoom via controller zoom input and updates camera distance.
- Left-Click Drag (`wi::input::GetMouseState().left_button_press`): applies smooth orbit relative to the target without resetting each frame.

## Camera Initialization and Robustness
- **Initial Valid Framing**: Prior to the main loop and before any fallible frame update, a valid camera output is constructed (`CameraFraming::computeFraming`) using the initial hero position, heading, and CLI/preset distance, ensuring `eye` and `target` are never zero vectors.
- **Rollback and Conflict Tracking**: Prior to processing interactive input and updating the controller each frame, a snapshot of controller state is retained. If an update triggers a `TetherTerrainConflictException` (e.g. terrain lift conflicts with the 3D tether bound), the host catches the exception, rolls back the controller to the before-frame state, preserves the last valid `cameraOut`, and increments `tether_terrain_conflicts` in the receipt.
- **Unused Helpers**: Legacy standalone helper functions (such as the legacy `Camera` wrapper) have been removed in favor of direct controller evaluation and Wicked view transform updates.

## Persistence Semantics
- **Legacy Compatibility**: `HostPose::cameraMode` preserves legacy 0/1/2 semantics (0: behind follow, 1: broadside, 2: frontal). It is NOT reinterpreted as `LocallyDetached` mode.
- **Save Scope & Persistence Limitation**: Saves record legacy pose and orientation. Full camera controller runtime state (e.g., detached camera anchor coordinates, orbit elevation/azimuth, input focus) is explicitly documented as not persisted in the legacy host pose format, avoiding state reinterpretation or corruption across versions.

## Verifiable Outputs
Execution records verifiable fields in `runtime-receipt.json`:
- `camera_mode`: "TetheredFollow" or "LocallyDetached"
- `input_focus`: "BodySteering" or "FreeCamera"
- `camera_eye`: coordinates `{x, y, z}` of the active camera eye
- `camera_target`: coordinates `{x, y, z}` of the camera look target
- `camera_orbit_distance`: current camera distance in meters
- `camera_tether_comfort_radius`: soft tether comfort radius
- `camera_tether_hard_radius`: strict physical tether cap
- `tether_terrain_conflicts`: count of typed terrain/tether conflict exceptions handled
- `pending_audio`: current audio integration status ("none")
- `pending_travel_to`: whether an active ground click travel destination is pending
- `pending_raycast`: mouse raycasting status ("none")
- Complete controls and full camera state persistence remain explicitly disclaimed under limits.

Parent review corrected initiallegacy0/1/2 presetapplication, moved fullcamera snapshot beforetoggle/pan, and gatedinteractiveinput on focus. ActualWicked eye is reported separately from requestedcontroller eye. Interactive end-to-end control acceptance remains pending actualhost probes; no audio/raycast/TravelTo implementation.

## Parent runtime evidence — 2026-10-12

Release build and 15 native tests passed. The scripted `-DiagnosticCamera`
probe applies orbit and zoom, detaches and pans, then returns to follow mode.
Run `var/reports/runtime/20261012-155808-1a96d620` completed stage 3 with
745 detached frames, 0.552458 m of animal movement during detachment, and zero
tether conflicts. Actual Wicked eye coordinates match requested framing within
float precision. The detached capture deliberately looks away from the animal.
This verifies renderer/state progression; physical mouse and keyboard acceptance
remains pending. Full camera state persistence, audio, raycast and TravelTo remain
unimplemented.
