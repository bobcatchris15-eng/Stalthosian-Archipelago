# Scenery ground-contact receipt

- Scope: `stalthos-runtime/engine/environment_scene.hpp` and the assigned calibration docs.
- The contact helper now uses the exact `TransformComponent::RotateRollPitchYaw` quaternion multiply sequence from `wiScene_Components.cpp`, then `XMVector3Rotate` for each local vertex. It computes the minimum vertical translation that leaves each in-bounds vertex at or above `ReferenceLandscape::height` at that vertex's transformed X/Z.
- Compiled the assigned header and probe against the installed Wicked headers using MSVC C++17, `/O2 /MD /DNDEBUG`, and the pinned Release libraries. Engine headers emitted unrelated warnings under `/W4`; this probe used `/W0` for those engine headers. Exit code: 0.
- Mixed-angle rotation comparison against an actual `TransformComponent` world matrix: maximum component error `5.960464478e-08` m.
- Deterministic dressing probe (seed 90210): 163 proposals, 3,876 in-bounds transformed vertices, finite coordinates and gaps, minimum vertex-above-terrain gap `-5.190102037e-08` m (float roundoff), maximum gap `2.268187117` m. Exit code: 0. The maximum is the conservative vertex-fit lift on varied analytic terrain, not a claim that every prop visually floats that high; rendered terrain is triangulated and needs integrated capture review.
- Terrain limitation: contact is exact at sampled vertices against the analytic reference function, not a continuous mesh collision test. Triangle interiors and the rendered triangulated terrain can differ between samples.
- Scale calibration: at a 1.8 m juvenile scale, proposed logs mostly read as 1–3 m branches with occasional larger wood. Pebble dimensions, placement authority, and count budgets remain unchanged.
- Visual status remains unverified; root should review the integrated capture for visible gaps and the foliage material response.
