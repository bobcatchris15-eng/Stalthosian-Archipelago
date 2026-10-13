# Smalltrotter Neutral Idle

The `Smalltrotter_NeutralIdle` action is a distinct, clearly named neutral standing idle clip authored alongside `Smalltrotter_WalkCycle`. It ensures a bounded resting posture without any actor-root translation.

## Specifications
- **Clip Identifier:** `Smalltrotter_NeutralIdle`
- **Clip Duration:** 2.0 s (49 frames at 24 fps)
- **Support Posture:** Evaluated across all 4 pads (`bone:pes_foot_l`, `bone:pes_foot_r`, `diagnostic:manus_hand_l`, `diagnostic:manus_hand_r`) in the rest support posture.
- **Root Motion:** Evaluated across all frames; `root_translation_zero` is an actual measurement (`root_translation_max_m <= 1e-6 m`).
- **Loop Seamlessness:** Loop verified across all action F-curves via Blender 5.2 slot/channelbag traversal (`max_channel_error <= 1e-6`).
- **Validation Scope:** Validated independently from the walk cycle; does not assert native terrain contact (`terrain_contact_asserted: false`).

## Blender 5.2 Action Slot Architecture
In Blender 5.2 LTS, the Animation 2025 layered action system assigns F-curves into channelbags owned by action slots (`ActionSlot`). Exporting distinct clips via NLA tracks requires explicit action slot assignment:
```python
track_walk = arm.animation_data.nla_tracks.new()
strip_walk = track_walk.strips.new(walk_action.name, int(walk_action.frame_range[0]), walk_action)
if hasattr(walk_action, "slots") and len(walk_action.slots) > 0 and hasattr(strip_walk, "action_slot"):
    strip_walk.action_slot = walk_action.slots[0]

track_idle = arm.animation_data.nla_tracks.new()
strip_idle = track_idle.strips.new(idle_action.name, int(idle_action.frame_range[0]), idle_action)
if hasattr(idle_action, "slots") and len(idle_action.slots) > 0 and hasattr(strip_idle, "action_slot"):
    strip_idle.action_slot = idle_action.slots[0]
```
During evaluation of both clips, NLA tracks are kept empty/unassigned so no unmuted NLA blending can silently pollute frame measurements.

## Host Runtime Integration (Wicked Engine)
- **Clip Selection:** The host evaluates actual displacement (`actualDistance = hypot(x - prevX, z - prevZ)`).
  - When displaced (`actualDistance > 1e-5`): Walk clip is active (`walkAnim.amount = 1.0f`, distance-driven phase playback). Idle clip is muted and paused (`idleAnim.amount = 0.0f`, `idleAnim.Pause()`).
  - When stationary (`actualDistance <= 1e-5`): Idle clip is active (`idleAnim.amount = 1.0f`, continuous playback via `idleAnim.Play()`). Walk clip is muted and paused (`walkAnim.amount = 0.0f`, `walkAnim.Pause()`).
- **Bone Overwrite Prevention:** In Wicked Engine, a paused animation with `amount = 0.0f` does not lerp onto transforms and is skipped during evaluation if its timer does not advance, ensuring non-selected clips never corrupt active bone poses.
- **Root Motion Extraction & Legacy Fallback:** Actor-root translation channels on the walk clip are stripped for displacement driving. If an imported asset contains only a single walk clip (legacy fallback), the host safely operates without an idle clip. Any unrecognized clips are rejected with descriptive errors.
