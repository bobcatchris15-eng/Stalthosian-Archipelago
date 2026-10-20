# Nearby contextual destination draft

In the connected world, right-click visible clear dry ground within12m of the
actual body. The native window mouse message feeds an engine camera raycast;
the hit must match resident terrain height. The route samples rendered ground
at <=0.5m steps, rejects grades above0.6 (plus0.02m sample tolerance), water
in the original watershed, and opaque scene obstruction at approximate body
height. Unsupported/nonresident routes reject or abandon with a readable cue.
The bounded body-height obstruction ray skips the first1.3m to avoid self hits;
this is coarse clearance, not a general navigation mesh or collision controller.

HeroSim's existing single contextual goal owns the destination. Ordinary turn
and forward integrator execute it; arrival tolerance0.5m ends it. Need-driven
browsing/drinking/rest yield while active; ordinary autonomy returns afterward.
WASD/body interaction cancel immediately. Camera-only movement never creates
body knowledge, activation, food/water contact or a remote destination. Mouse
ray projection selects a visible candidate; body bounds authorize it separately.
Destination marker/cue are provisional screen text; marker has no depth occlusion.
Direct keyboard input continues to follow the camera's existing body/free focus.

Run.ps1 -DestinationProbe uses a labelled injected WM_RBUTTONDOWN event at a
projected supported4m candidate through the actual native handler and raycast.
It adds synthetic thirst0.8 after acceptance to prove explicit goal priority.
-DestinationOverrideProbe adds ordinary reverse intent during physical seconds
2.5-3.5. -DestinationBoundaryProbe requests a camera anchor40m ahead (existing tether
clamps the native target to30m) and injects a native
right-click toward that visible remote ground; it must reject against body range
while leaving the body stationary. These are automated native input-path checks;
no human physical mouse operation or broad DPI/device acceptance is claimed.

No save fields or terrain/creature changes. Straight clear routes only; no general
pathfinding, water crossing, arbitrary climbing, camera occlusion or final UI art.
Existing animal pace, foot contact, rain and connected-save limitations remain.

The longer12s arrival/handoff trial recorded one support fallback late on the
subsequent ordinary drink approach. That inherited bank-support oddity is logged;
a bounded8s arrival/handoff run isolates destination support without reopening
accepted terrain/drink work. See the receipt for verified results.
