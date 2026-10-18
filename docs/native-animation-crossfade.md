# Native animation crossfade

The native host uses the reviewed `domain/animation_blend.hpp` state to transition
between idle and walk over 0.25 physical seconds. Signed measured body distance
continues to drive walk phase, including reverse motion. Actual displacement,
rather than blocked movement intent, selects the clip. Death stops blend progress
and both animation clocks.

Wicked's pinned `Scene::ScanAnimationDependencies` visits animation-manager indices
in order and evaluates overlapping tracks sequentially. It blends each channel
onto the existing local transform. Applying two nominal weights that sum to one
therefore retains part of the previous frame. For clips with identical target/path
sets, the host instead applies the first evaluated clip at amount 1, then the
second at its logical weight. The logical idle/walk weights still sum to one;
engine amounts intentionally have different semantics during blending.

`engine/crossfade_transition.hpp` compares full-width entity IDs and paths without
packing away identity bits. Topology is analyzed once during setup and reused;
component-manager order is resolved when applying weights. If channel sets differ,
the existing nominal-weight fallback is retained and the receipt explicitly marks
that full shared tracks were not verified. No exact-mix claim applies to that case.
Paused walk tracks are forced to sample during blending; after settling, inactive
clips have zero amount, are paused, and are excluded by the engine timer guard.

The diagnostic transition records at most 64 samples per phase: logical weights,
applied amounts, actual clip order, local translation/rotation frame deltas and
finite-value flags. Quaternion deltas normalize both inputs to avoid mistaking
floating-point length error for rotation. Interior captures occur at or after
halfway progress when an actual frame samples that interval. A low frame rate
can skip the interval; absence of a capture is recorded rather than invented.

The stale `idle_reset_to_neutral_verified` success claim was removed. The receipt
now reports `idle_crossfade_settled`, which establishes state convergence only.
It does not establish smoothness, foot locking or final gait acceptance.

Root validation and exact runtime paths are in
`.clanker/tasks/continuation-20261017/receipt.json` and
`var/reports/continuation-20261017/`. The initial corrected native build passed
24/24 CTest suites; its actual ten-second transition run verified 225 shared
channels, one setup topology analysis, normalized logical weights, first-clip
amount 1 during blending and settled inactive-clip guards. Signed phase error
was approximately 2.2e-16 seconds in that run. An accelerated death run retained
death truth and frozen blend weights. The no-creature fallback also ran.
The final rebuild additionally corrects quaternion telemetry normalization;
final runtime replays are recorded in the receipt.

This is provisional playback. In-place idle blended with locomotion can still
slide the feet. Per-frame deltas and still captures do not establish final
visual continuity or target-hardware 30 FPS acceptance. No foot IK solver is
introduced here, and the source-derived Smalltrotter has not replaced the
older generated runtime creature.
