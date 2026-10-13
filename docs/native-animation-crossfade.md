# Native Animation Crossfade Integration

## Overview

This document records the bounded integration of explicit-seconds animation crossfading in `stalthos-runtime/engine/main.cpp`.
The integration consumes the reviewed domain contract `stalthos::domain::AnimationBlend` (`stalthos-runtime/domain/animation_blend.hpp`) to transition smoothly between `Idle` and `Walk` clips on creature armatures, replacing prior instantaneous binary clip switches (`amount = 1.0f / 0.0f`).

## Ownership & Scope Boundaries

Per the task constraints:
- **Owned paths**:
  1. `stalthos-runtime/engine/main.cpp` (playback branch, transition bookkeeping, receipt metrics)
  2. `docs/native-animation-crossfade.md` (this report)
- **Unmodified paths**: No modifications made to `CMakeLists.txt`, `domain/animation_blend.hpp`, `domain/locomotion_clip.hpp`, any asset files, toolkit scripts, or root repository directories. All compilation and run artifacts reside within isolated `var/`.

---

## Domain Contract Consumption

`stalthos::domain::AnimationBlend` manages crossfading state with bounded timing:
- `kCrossfadeDurationSeconds = 0.25f`
- Explicit advance with elapsed delta time (`advance(state, dt, kCrossfadeDurationSeconds)`)
- State evaluation via `resolve(state)`, yielding normalized weights:
  $$\text{idleWeight} + \text{walkWeight} = 1.0$$
- Paused delta time (`dt <= 0.0f`) preserves progress without drift or resets.
- Interrupted reversals immediately pivot target clip and scale remaining progress smoothly without sudden pops or 1-frame snaps to rest pose.

---

## Wicked Engine Animation Component Semantics

Wicked Engine applies animation transforms sequentially in `wiScene.cpp` during scene updates:
$$\mathbf{T}_{\text{final}} = \operatorname{Lerp}(\mathbf{T}_{\text{current}}, \mathbf{T}_{\text{anim}}, \text{amount})$$
$$\mathbf{R}_{\text{final}} = \operatorname{Slerp}(\mathbf{R}_{\text{current}}, \mathbf{R}_{\text{anim}}, \text{amount})$$

Crucial engine characteristics observed and accommodated:
1. **No Automatic Rest Pose Reset**: Wicked does not revert local bone transforms to binding/rest pose prior to sampling animations; it lerps over whatever transform is currently present in the scene entity hierarchy.
2. **Dependency Queue Optimization**: If an `AnimationComponent` is paused and `last_update_time == timer`, Wicked's dependency scanner skips sampling the animation altogether.
3. **Dual Nonzero Weight Restriction**: Both clips may have non-zero amounts **only while blending** (`transitionProgress < 1.0f`).
   - During blend:
     - `idleAnim->amount = blendWeights.idleWeight`
     - `walkAnim->amount = blendWeights.walkWeight`
     - `idleAnim->Play()`
     - `walkAnim->Pause()` (phase driven by distance/stride pacing)
     - `walkAnim->timer = walkAnim->start + creaturePhase`
     - `walkAnim->last_update_time = walkAnim->timer - 1.0f` to guarantee Wicked samples the paused distance-driven walk pose even on frames with near-zero displacement.
   - Once settled (`transitionProgress >= 1.0f`):
     - Active clip `amount = 1.0f`
     - Inactive clip `amount = 0.0f`, `Pause()`, and `last_update_time = timer`. This guarantees zero additive drift and zero evaluation overhead from the inactive clip.
4. **Single-Clip Fallback**: If an armature has only a single clip (e.g., Idle-only or Walk-only), the playback branch gracefully forces full weight to the available clip, avoiding uninitialized component evaluation.

---

## Authoritative Movement & Terrain Support Preservation

The crossfade integration strictly preserves existing locomotive and physical contracts:
- **Authoritative Stride Pacing**: Movement stride (`0.16128m`) remains strictly driven by signed root displacement. Forward moves advance phase positively; reverse moves advance phase identically using absolute distance pacing, preserving leg swing timing.
- **Actor-Root Translation Removal**: Actor root translation channels remain stripped from animated clips (`actor_root_translation_channels_removed = 1`) to ensure game-host translation remains authoritative.
- **Grounding and Body Support**: Creature sole offset (`0.0m`) and 4-point body terrain support mode (`terrain_supported`) remain intact.

---

## Empirical Verification: 10s Diagnostic Transition Probe

A 10-second `DiagnosticTransition` probe was executed against the smalltrotter creature GLB:
```powershell
pwsh -File stalthos-runtime/scripts/Run.ps1 -DiagnosticTransition -Duration 10 `
  -Creature "E:/Stalthosian Archipelago/var/bristle-coherence-parent-verified-20261013/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.glb" `
  -CreatureAnimationMetadata "E:/Stalthosian Archipelago/var/bristle-coherence-parent-verified-20261013/runtime/creatures/creature.smalltrotter.st1/creature.smalltrotter.st1.independent_juvenile.walk_cycle.json" `
  -CreatureSoleOffset 0 -InspectionCamera
```

### Measured Receipt Metrics

| Metric | Baseline (Binary Switch) | Crossfade Integration |
| :--- | :--- | :--- |
| **Run ID** | `20261013-184418-47803ebc` | `20261013-185127-1718bca6` |
| `animation_crossfade_duration_s` | N/A (binary) | `0.25` |
| `both_clips_nonzero_weights_only_during_blend` | N/A | `true` |
| `animation_crossfade_blend_frames_total` | 0 | `94` |
| `final_idle_weight` / `final_walk_weight` | `1.0` / `0.0` | `1.0` / `0.0` |
| Total Accumulated Distance | `0.0 m` (net closed loop) | `0.0 m` (net closed loop) |
| Root Ground Delta | `0.02068 m` | `0.02004 m` |
| Support Mode | `terrain_supported` | `terrain_supported` |
| Support Slope Radians | `0.0270` | `0.0272` |
| Actor Root Translation Removed | `1` | `1` |

### Transition Phase Breakdown

| Phase | Intent | Total Frames | Blend Frames | Settled Frames | Start Active Amount | End Idle Weight | End Walk Weight | Inactive Amount Zero | Inactive Paused |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **idle_1** | Idle | 54 | 0 | 54 | 1.00 | 1.00 | 0.00 | `true` | `true` |
| **forward_walk** | Forward | 133 | 11 | 122 | 0.40 | 0.00 | 1.00 | `true` | `true` |
| **idle_2** | Idle | 78 | 23 | 55 | 0.10 | 1.00 | 0.00 | `true` | `true` |
| **reverse_walk** | Reverse | 253 | 30 | 223 | 0.02 | 0.00 | 1.00 | `true` | `true` |
| **idle_3** | Idle | 244 | 30 | 214 | 0.02 | 1.00 | 0.00 | `true` | `true` |

Captured frame sequence:
- `transition-0-initial-idle.png`
- `transition-1-idle-to-forward.png`
- `transition-2-forward-to-idle.png`
- `transition-3-idle-to-reverse.png`
- `transition-4-reverse-to-idle.png`
- `transition-5-final-idle.png`

All captures saved in `var/reports/runtime/20261013-185127-1718bca6/`.

---

## Honest Limitations

1. **Visual Foot-Locking / Skate**: Blending between an in-place Idle and a distance-driven Walk cycle linearly interpolates bone positions over 0.25 seconds. While this eliminates abrupt single-frame pose snapping, some minor foot sliding during the 0.25s blend window is physically inherent without inverse kinematics (IK). No claim of final foot-locking acceptance is made.
2. **Artistic & Visual Acceptance**: Transition captures show continuous pose transitions without mesh tearing or additive drift. However, artistic review remains subject to user evaluation (`visual_accepted: false`).
3. **Performance Floor**: Integration runs comfortably on native host hardware during diagnostic probes, but full 30 FPS acceptance on target GTX 1080 / first-gen Ryzen requires full-scene load profiling.
