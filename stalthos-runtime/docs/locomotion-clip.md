# Locomotion Clip Helpers

The `LocomotionClip` domain module provides a safe, plain C++17 bounded math helper
for mapping the host's actual actor root displacement into the playback phase of an
animated cyclic walk or run clip.

## Motivation and Usage

In *The Stalthosian Archipelago*, the engine host controls the physical and logical position
of animals in the world directly. To avoid foot sliding, the animation playback must advance
in sync with the actual physical distance traveled.

`LocomotionClip` uses actual clip metadata—such as the Smalltrotter's authored walk
duration of 2.0s and exactly measured stride of 0.162m (the length the root travels in one full cycle)—
to advance the phase synchronously.

## Wicked Engine Adapter and In-Place Removal

When the future Wicked adapter plays the animation:
- It tracks the current signed displacement distance.
- It calculates the target playback phase based on the real displacement.
- It **removes** the source animation's root translation (to avoid duplicate physical translation
since the host has already moved the actor's world transform).
- It retains the limb movement and rotational channels exactly as authored.

**Explicit Note:** This module provides synchronous phase scaling and root cancellation only.
It explicitly **does not** claim to be a runtime "foot locking" or IK solver, and it is not a
"fake import" replacement for the real animation rig. It merely translates the root's independent
gameplay movement into matching playback time according to extracted rig measurements.

## Guarantees

- **Bounded Arithmetic**: Uses strict finite validation (`std::isfinite`).
- **No Unbounded Loops**: Solves multi-cycle forward and backward movement efficiently using math formulas instead of internal `while` loops, preventing infinite timeouts for arbitrarily large deltas.
- **Consistent Displacement**: Correctly calculates the amount of root displacement required to negate the animation's intrinsic root motion, exactly mirroring the fraction of the stride traveled.
- **Safety**: Frozen or dead actors with zero displacement will cause zero phase change and zero cycles. Overflows, NaNs, zero strides, and invalid durations are cleanly rejected via exceptions without modifying external state.
