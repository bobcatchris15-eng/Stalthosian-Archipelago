# Animation Transition Probe: Idle / Walk / Reverse / Idle

## Context & Objectives
The animation transition probe provides a bounded, same-process diagnostic in the Wicked Engine host runtime to verify transitions between neutral idle and active locomotion.

The probe runs a deterministic five-phase schedule driven by actual engine `dt`:
1. `idle_1` (0.0s – 2.0s): Stationary standing neutral idle
2. `forward_walk` (2.0s – 4.0s): Forward locomotion driven by signed distance
3. `idle_2` (4.0s – 6.0s): Stationary standing neutral idle
4. `reverse_walk` (6.0s – 8.0s): Reverse locomotion driven by negative signed distance
5. `idle_3` (8.0s – 10.0s): Stationary standing neutral idle

Total duration is bounded at 10.0 seconds.

## Tested Asset Chain
- Model GLB: `creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb`
- Animation Metadata: `creature.smalltrotter.st1.independent_juvenile.walk_cycle.json`
- Clips present in asset:
  - `Smalltrotter_WalkCycle`: 2.0s cyclic locomotion clip (49 frames @ 24 fps, stride 0.16128 m)
  - `Smalltrotter_NeutralIdle`: 2.0s cyclic neutral rest clip (49 frames @ 24 fps, root translation 0.0 m)

## Exclusive Clip Selection & Pose Invariance
To prevent conflicting bone transforms from polluting the skeleton, the runtime enforces strict exclusive clip selection:
- When displaced (`actualDistance > 1e-5`):
  - Walk clip: `walkAnim->amount = 1.0f;` timer driven by signed displacement.
  - Idle clip: `idleAnim->amount = 0.0f; idleAnim->last_update_time = idleAnim->timer; idleAnim->Pause();`
- When stationary (`actualDistance <= 1e-5`):
  - Walk clip: `walkAnim->amount = 0.0f; walkAnim->last_update_time = walkAnim->timer; walkAnim->Pause();`
  - Idle clip: `idleAnim->amount = 1.0f;` continuous loop playback via `idleAnim->Play()`.

### Proof of Inactive Clip Invariance in Wicked Engine
In Wicked Engine (`wiScene.cpp` lines 9497–9500 in `Scene::ScanAnimationDependencies`):
```cpp
AnimationComponent& animationA = animations[i];
if (!animationA.IsPlaying() && animationA.last_update_time == animationA.timer)
{
    continue;
}
```
When an animation is paused and its `last_update_time` equals its `timer`, the engine skips it completely during dependency scanning. It is never added to the animation job queue and executes zero channel evaluations. In addition, its `amount` is set to `0.0f`. This state matches the pinned engine skip condition. Runtime telemetry measures paused/zero-amount state; it does not instrument the animation job queue or count bone writes.

## Locomotion Phase Driving & Signed Distance
- During `forward_walk` (Phase 1): `Pace::Intent::Forward` produces positive body velocity. Actual horizontal displacement is positive (`+actualDistance`), advancing `creaturePhase` forward via `LocomotionClip::advance(creaturePhase, distance)`.
- During `reverse_walk` (Phase 3): `Pace::Intent::Reverse` produces negative body velocity. Actual horizontal displacement produces negative signed distance (`-actualDistance`), causing `creaturePhase` to reverse cyclically via `LocomotionClip::advance`.
- Walk animation timer is synchronized to distance: `walkAnim->timer = static_cast<float>(walkAnim->start + creaturePhase);`.

## Neutral Idle Reset & Lifecycle Behavior
- On transition from moving to stationary (entering Phase 2 and Phase 4), `idleAnim->timer` is reset to `idleAnim->start`. This guarantees that coming to a halt resets the creature to its authored neutral resting posture rather than resuming mid-cycle.
- While hero is alive, `idleAnim->Play()` loops the neutral idle.
- If the hero transitions to terminal death (`LifeStage::Dead`), `idleAnim->Pause()` and `walkAnim->Pause()` immediately freeze all animation playback.

## Transition Captures
Meaningful frame captures are recorded at each transition point:
- `transition-0-initial-idle.png`: Start of initial neutral idle (t = 0.0s)
- `transition-1-idle-to-forward.png`: Transition from idle to forward walk (t = 2.0s)
- `transition-2-forward-to-idle.png`: Transition from forward walk to second idle (t = 4.0s)
- `transition-3-idle-to-reverse.png`: Transition from idle to reverse walk (t = 6.0s)
- `transition-4-reverse-to-idle.png`: Transition from reverse walk to final idle (t = 8.0s)
- `transition-5-final-idle.png`: Final state at completion (t = 10.0s)
- `draft-host.png`: Mid-run host evidence render (t = 5.0s)

## Mutual Exclusion of Diagnostic Modes
`Run.ps1` and `main.cpp` enforce strict mutual exclusion among diagnostic switches:
- `--diagnostic-walk`
- `--diagnostic-reverse-walk`
- `--diagnostic-needs`
- `--diagnostic-transition`
Any combination of more than one of these flags is rejected with a fatal error.

## Remaining Limitations & Discontinuity
- **Discontinuity:** Because exclusive binary clip selection (`amount = 1.0f / 0.0f`) is used without an intermediate crossfade or inertialization blend tree, an instantaneous (one-frame) pose snap occurs at transition boundaries between walk stance and neutral rest.
- Smooth blending is not asserted or claimed without measured blend tree verification.
- Tail clearance and native per-foot terrain IK remain unresolved limitations.

Parent review requires two distinct clips and >=10s, measures each phase inactive state after Run, and labels the global state guard without claiming direct job/write instrumentation. Huge finite elapsed values clamp before integer conversion. The returned neutral poses and moving poses are measured separately per phase; pose snaps and terrain contact remain unaccepted.
