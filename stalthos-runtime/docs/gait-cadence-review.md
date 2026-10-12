# Smalltrotter gait cadence review

Status: source analysis with local parent diagnostic evidence. Fictional juvenile
anatomy and proposed pacing are speculative. This is not gait acceptance.

## Source and measured baseline

- `stalthos-toolkit/src/stalthos_toolkit/blender/scripts/rig.py` authors a 2 s,
  24 FPS clip. Stride is bounded by trunk length * 0.30 and leg segment sum *
  0.32 for the Smalltrotter reference profile, retaining authored bone lengths.
- The locally generated seed42 independent_juvenile metadata measures a
  0.16128 m stride. Implied authored average speed is 0.08064 m/s.
- `stalthos-runtime/engine/main.cpp` requests 2 m/s forward/diagnostic motion,
  -2 m/s reverse diagnostic motion, and -1 m/s manual reverse motion.
- `LocomotionClip::advance` maps signed actual displacement to phase. The host
  removes the imported non-bone actor root translation channel and manually
  writes the paused animation timer, preserving bone animation channels.

At 2 m/s this is 12.40079 cycles/s (24.80159 times the authored clip rate),
regardless of nominal 2 s duration: actual speed / measured stride sets cadence.
Reducing only a wall-clock timer would break this distance mapping.

The parent native moving run `var/reports/runtime/20261012-113703-3ea9fd55`
loads the fresh corrected anatomy GLB: actual phase 0.637122032031039 s versus
expected 0.637122032031041 s over 17.630897520662984 m. Idle run
`20261012-113847-0aa96827` holds phase zero. These establish phase mapping,
not planted feet in the rendered terrain or natural motion.

## Bounded next implementation

1. Finish and locally validate the already dispatched HeroView host integration
   before assigning another host edit. Do not give simultaneous workers main.cpp.
2. Define a species-neutral pace policy in a new bounded locomotion profile/helper,
   with finite positive metadata and explicit movement intent. Derive actual body
   speed from measured stride and a proposed target cycle rate; phase still follows
   actual displacement. A provisional 0.5-1.5 cycles/s corresponds to 0.08064-
   0.24192 m/s for this clip, a tuning experiment rather than a species fact.
3. Apply the same policy to manual and diagnostic movement, with explicit forward,
   reverse and idle semantics. Record requested speed, actual displacement and
   observed cadence in native receipts. Preserve body-relative input and clamps.
4. Review footage for a slow ordinary walk. For useful faster traversal, re-author
   a longer feasible stride or a distinct faster gait, keeping joint lengths and
   contact tolerances. Enlarging metadata without changing the actual motion is
   invalid; repeated local asset generation and measured contacts are required.

Proposed ownership: new runtime locomotion pace helper/test and a separate narrow
host wiring packet after the camera worker is integrated. Authored stride changes
would own toolkit rig/gait scripts and focused tests in another isolated packet.
Do not modify foundation.hpp merely to add a provisional speed constant.

## Required evidence and uncertainty

- Focused tests: metadata rejection, pace scaling, reverse/idle, finite caps,
  clamped movement, and existing LocomotionClip phase wrapping.
- Regenerate actual asset and preserve loop, root-motion, 20 mm ground/contact
  and touchdown gates. Current geometric checks sample hind pads; all four feet
  need coverage before quadruped foot-lock acceptance.
- Capture forward, reverse, idle, turning and slope motion in the Wicked host,
  including actual deformed pad-to-ground positions and visible slip. Receipt
  phase equality alone cannot demonstrate host foot locking or terrain contact.
- Review side-on continuous footage against the supplied visual reference and
  intended ordinary animal movement. Speed reduction is an experiment, not a
  substitute for improving pose, stride, transitions and terrain adaptation.
- The current 8 cm/s authored pace may be too slow for play. Proposed cycle rates
  require testing; no final gameplay speed or biomechanical claim is made here.
- Full draft coverage, reference appearance and target-hardware 30 FPS remain open.
