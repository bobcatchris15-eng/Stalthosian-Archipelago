# Creature body support follows rendered terrain

The host now samples its cached, actual rendered ground triangles for both the
creature root elevation and the five-point body support calculation. The previous
analytical height function differs from this coarse triangle mesh by as much as
96 mm around the measured footprint. No constant sole offset was added.

A failed rendered support sample retries the whole support calculation and root
on the analytical surface; heights from two surfaces are never mixed into one
support pose. Source, sample counts, fallback counts and last-frame failure reason
are reported. A subsequent valid frame clears its failure reason. Unsupported
slopes retain the existing flat-yaw fallback. DisableTerrainSupport retains flat
yaw, with root elevation still following the visible surface when available.

## Parent verification, 2026-10-13

Release build and all 22 native CTest groups passed. Toolkit source was unchanged;
the last full toolkit result remains 1988 passed, 2 skipped from the earlier wake.
The same bristle-parent-verified asset, inspection camera and zero sole offset
were used for the matched six-second idle comparison:

| Foot | Before rendered minimum | After rendered minimum |
| --- | ---: | ---: |
| Front left | +36.25 mm | -0.90 mm |
| Front right | -40.29 mm | -0.91 mm |
| Rear left | +27.95 mm | -1.30 mm |
| Rear right | -45.61 mm | -1.30 mm |

Parent baseline: var/reports/runtime/20261013-111238-39251b9a.
Parent corrected idle: var/reports/runtime/20261013-113032-b0aa4269.
Parent corrected walk: var/reports/runtime/20261013-113057-3abea8aa.
All four final sole groups were complete, disjoint and finite in both corrected
runs, with 572/572/588/588 selected vertices and zero support fallbacks.
Walk final minima were +2.43/+41.41/+68.99/-0.86 mm respectively. Walk samples
are phase-dependent, so these final frames do not establish whole-cycle contact.

Parent five-phase transition run 20261013-113222-f2511100 retains positive forward
and negative reverse distance, paused zero-weight inactive clips and zero travel
in its three idle phases. The signed-distance phase formula was independently
checked. Disabled-support run 20261013-113302-998e8a92 reports mode disabled.

An attempted saved-pose boundary fixture at x=95.9 was rejected by the host's
existing playable-bounds guard before launch (20261013-113122-9461e340). Its raw
failure is retained. It does not exercise the native support fallback; normal
playable bounds stay well inside the cached mesh. Invalid/boundary surface cases
are covered by the pure surface tests; native cache-failure injection remains
unperformed.

## Limits

This corrects the sampled body's relationship to the displayed terrain at this
reference location. It does not implement per-foot IK, locking, force/friction,
terrain-relative swing trajectories or continuous contact acceptance. Lowest
selected body-mesh sole points are measured; separate nail meshes are excluded.
The final idle samples still penetrate by about 1 mm. Reference appearance,
shadow/contact presentation, other terrain slopes, whole-cycle animation,
physical-input behavior, full draft coverage and 30 FPS acceptance remain open.

Worker source was frozen as 93c5761; parent hardened whole-pose fallback and
last-frame source/reason telemetry before its own build and native runs.
