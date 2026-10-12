# Draft scenery renderer

`engine/environment_scene.hpp` builds startup scenery from the tested deterministic
habitat proposals and shared material palette. The host calls it after Terrain.
Logs use tapered faceted meshes; pebbles and bluff accents use irregular convex
meshes; bank clumps use curved thin blades with a shared double-sided material.
Reversed duplicate foliage triangles were removed to avoid cancelling smooth normals.

Root corrected an incomplete AGY result: its success JSON had an empty response,
an undeclared proposal variable and debug output. Actual root compilation and
runtime capture, rather than that status, establish this implementation evidence.

The pinned Wicked Release host generated 163 props (23 logs, 64 pebbles, 48 bank
clumps, 28 bluff accents), 3809 vertices and 4645 triangles in the fixture.
Root CTest passed11/11 after integration. Evidence is in
`var/reports/runtime/20261012-015245-480f21ed/` and
`var/reports/packet-seven-scenery-final-{build,ctest}.log`.

This remains low-detail provisional scenery. Logs still need terrain-contact and
scale refinement, ground materials need textures, plants need authored foliage,
and there is no prop streaming/LOD/weather deformation. Existing water presentation
remains the older host fixture; the reviewed shore mesh contract is not wired here.
No photographic visual or target frame-rate acceptance is claimed.
