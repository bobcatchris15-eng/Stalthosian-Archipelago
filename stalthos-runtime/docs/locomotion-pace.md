# Locomotion Pace Policy

`LocomotionPace` is a pure C++17 policy helper that derives body speed from a
measured clip stride and a proposed target cycle rate. Its initial default is
1.0 cycle per second, with a configurable maximum physical speed of 1.0 m/s.
For the currently measured 0.16128 m Smalltrotter clip stride this gives
0.16128 m/s. These values are provisional and need visual and gameplay review.

Forward and reverse use the same derived magnitude with opposite signs. Idle
returns zero speed and zero cadence. Stride, authored clip duration, target
cycle rate, and maximum speed must all be finite and positive. Invalid metadata
is rejected; overflow while deriving speed is rejected. The helper is
allocation-free and does not mutate caller state.

This policy does not alter `LocomotionClip`: animation phase remains driven by
actual signed displacement and measured stride. Do not slow an animation by
setting an arbitrary timer; that breaks the distance-to-phase relationship.
The authored trot and any new stride remain pending. This helper establishes
neither gait quality nor gameplay acceptance, and it does not provide foot
locking or terrain contact evidence.

## Host integration evidence — 2026-10-12

The imported creature now uses this proposed policy for forward and reverse
movement; idle remains zero. Actual displacement still drives `LocomotionClip`.
With measured stride 0.16128 m, the proposed one-cycle-per-second setting gives
0.16128 m/s. Proxy diagnostics retain 2 m/s. This is intentionally provisional;
a larger authored stride or suitable faster gait is needed for useful traversal.

Release build and 15 native tests passed. Actual Wicked runs:
- `var/reports/runtime/20261012-155808-1a96d620`: forward 1.515966 m, phase
  0.799176 s, detached camera does not stop diagnostic movement.
- `var/reports/runtime/20261012-155855-49c12d1a`: reverse -0.602879 m, phase
  0.523827 s. Expected and actual phase agree within floating-point tolerance.
- `var/reports/runtime/20261012-155920-35b4a7f7`: idle probe, recorded separately.

The images still show unresolved foot contact, angular limbs and provisional
surfacing. These results establish cadence mechanics, not a visually accepted gait.

Longer diagnostic needs routes now take longer at this provisional body speed.
Adjust their fixture starting positions and duration before interpreting a missed
contact as a ledger failure. The previous 2 m/s continuity runs remain historical
validation of resource persistence, rather than current slow-pace traversal evidence.
