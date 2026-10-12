# Smalltrotter walk stance revision

The walk clip now advances the actor root by one bounded stride per 2-second
cycle while the stance target moves backward by the matching root distance.
Swing uses a smoothstep recovery with a raised arc. A deterministic two-bone
solver bends each leg without changing the authored segment lengths. The stride
is capped at 30% of trunk length and 60% of combined leg length to keep the
short-limbed, deep-bodied form away from forced extension.

The hind-pad boxes are rigidly assigned to their corresponding toe bones after
automatic body weighting. This keeps the actual support surface attached to the
foot instead of allowing nearby torso groups to lift it away from a correctly
targeted ankle. Blender then samples all evaluated pad vertices for every frame
and records stance-only ground gaps and pad-centroid motion from touchdown,
including the actor's measured root translation. Height tolerance and
touchdown-excursion tolerance are checked independently per selector.

On the compatible Smalltrotter source built with Blender 5.2.0 LTS (seed 42,
independent juvenile), the measured stride is 0.162 m and the exported root
delta is 0.1620000005 m. All 31 stance samples for each hind pad remain within
the authored 0.020 m height tolerance; the measured pad-bottom range is about
0.00307–0.00977 m. Maximum touchdown-anchored horizontal excursion is
0.01264 m for each hind pad, within the same 0.020 m tolerance. Limb channels
match exactly at the sampled cycle endpoints, and the exported GLB contains a
validated animation. These measurements establish a bounded proxy clip, not a
final animation or runtime root-motion integration.

The user's primary still reference shows a deep rounded torso, broad muscular
limbs and feet, an upright long neck, and a low tapered tail. The gait preserves
the authored limb lengths and supports that substantial quadruped stance; no
stride or anatomy claim is inferred from the still. The current proxy remains
visibly simpler than the reference in torso volume, neck/head silhouette,
bristles, folds, and foot/toe detail. Matching three-quarter and broadside
captures against the reference, visual gait review, and runtime root-motion
extraction remain open. The image supplies art direction only, not gait evidence.

The Blender-free helpers cover finite-input validation, exact local loop closure,
the root-stride/stance cancellation, two-bone segment lengths, and touchdown
excursion with body stride included. The integration check also asserts that
the report's contact, foot-lock, and root-motion gates reflect the measured
values. A direct test against the prebuilt compatible source passed with actual
Blender; a fresh end-to-end test on this worktree's current base source cannot
reach rig validation because that base lacks the pad geometry supplied by the
separately owned creature-pose change.
