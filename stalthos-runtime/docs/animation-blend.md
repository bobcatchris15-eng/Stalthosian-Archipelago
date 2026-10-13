# Animation Blend

## Overview
This document proposes a species-neutral, bounded state helper for animation blending (`stalthos::domain::AnimationBlend`). This logic manages crossfade weights between two discrete locomotion animation states (e.g., Idle and Walk), enabling smooth procedural transitions without external framework dependencies.

## Implementation
The `AnimationBlend` core calculates bounded crossfade weights based on an explicit target state, elapsed seconds (`dt`), and a transition duration.

It handles:
- **Zero-duration transitions**: Immediate snap to a new state without divide-by-zero errors.
- **Interrupted transitions**: Seamless state reversion mid-blend, interpreting current transition progress dynamically to prevent visual pop or snap back.
- **Strict Bounding**: Weights strictly enforce a sum of 1.0. All values are clamped between `0.0` and `1.0`.
- **Validation**: Strict finite, non-negative assertions on elapsed deltas and lengths. Invalid states throw descriptive standard exceptions.

## Actual Tests
Offline, standalone validation successfully ensures:
- Initialization defaults to `1.0` progress and `Idle` clip.
- Forward progress accurately computes fractional blends.
- Paused behavior (`dt = 0`) correctly freezes current states.
- Reverse interruption computes inverted progress and resolves exact weights securely.
- Explicit checks confirm zero-duration snaps.
- Invalid input limits (NaN, Inf, negative time, unbounded progress resolution) cleanly throw appropriate argument exceptions.
- Tests use active exception-based checks in optimized Release; NDEBUG does not disable them.

## Pending Validation
- **Wicked Engine Wiring**: Currently an independent domain helper, actual binding to Wicked's animation component and bone hierarchy is pending.
- **Physical Acceptance**: Validating the visual transition aesthetic and ensuring foot sliding/skating remains visually acceptable.
- **Crossfade Durations**: Tuning optimal seconds to allocate for various gait shifts (e.g., stopping vs. starting).
- **Runtime Claim**: Direct asset playback is unsupported within this specific isolation phase.

## Parent review
Original Jules4083995950081524668 candidate frozen518b744 after one pull. Parent corrected advance validation of previous progress and both clip enum values, which previously allowed invalid state to propagate or silently map to Walk. Added zero-time reversal continuity and10,000 interrupted-step normalization checks. Actual isolated optimized MSVC Release test passed. Helper is not wired to the native host and does not establish smoother animation. Zero duration is an explicit snap including zero dt; positive-duration zero dt preserves current weights when target changes.
