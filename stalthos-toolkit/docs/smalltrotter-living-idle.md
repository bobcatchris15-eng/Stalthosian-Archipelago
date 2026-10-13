# Smalltrotter Living Idle

`Smalltrotter_NeutralIdle` is a deterministic, two-second idle clip. It adds a small amount of neck and head motion while leaving the spine, legs, toes, and actor root at their neutral transforms. The loop is authored with the existing Blender 5.2 action and NLA export path.

## Behavior

- At 24 FPS the clip keeps its existing inclusive frame range, 1–49 (48 frame intervals, or two seconds), with matching start and end poses.
- Only `neck_` and `head_` bones receive keyed rotation changes. Neck pitch is at most 0.003 radians. Head pitch and yaw are bounded by the deterministic seed-derived sway and breathing amplitudes (each sway amplitude is at most 0.004 radians; breathing is at most 0.002 radians).
- Spine, leg, toe, and root transforms remain neutral in the idle action. This preserves their authored support posture; it does not claim foot-lock or terrain-contact acceptance.
- The seed-derived amplitudes are generated with a local RNG seeded from the asset seed. Pose values are assigned from neutral for every frame, so motion does not accumulate over time.
- Walk and idle remain separate named actions on NLA tracks. Blender 5.2 action slots are assigned to their strips when available, and the active action is unlinked for export as before.

## Evidence boundary

The rig build samples pad positions and reports the resulting excursions in metadata. Those measurements describe the generated proxy mesh and are not a claim of exact zero pad movement, terrain contact, or final creature quality. A Blender rig build and inspection are required to establish the measured values for a particular asset.
