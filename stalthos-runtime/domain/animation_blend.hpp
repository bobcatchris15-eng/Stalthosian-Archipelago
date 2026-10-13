#pragma once

#include "animation_transition.hpp"
#include <cmath>
#include <stdexcept>

namespace stalthos {
namespace domain {

// A deterministic, stateless helper to calculate bounded crossfade weights between
// active and inactive animation clips (e.g. idle and walk).
// Produces weights for a future host integration; it does not change clip playback.
class AnimationBlend {
public:
    using ClipKind = AnimationTransitionSchedule::ClipKind;

    struct Weights {
        double idleWeight;
        double walkWeight;
    };

    struct State {
        ClipKind targetClip = ClipKind::Idle;
        double transitionProgress = 1.0; // 1.0 means fully transitioned to targetClip
    };

    // Calculate the new state and weights given the elapsed time (dt), a requested transition,
    // and the previous state.
    // Transition duration is how long a full 0.0 to 1.0 transition should take.
    // If the target clip changes, the transition progress is inverted or adjusted to
    // allow seamless interrupted transitions.
    static State advance(const State& previous, ClipKind newTarget, double dt, double transitionDuration) {
        (void)resolve(previous);
        if (newTarget != ClipKind::Idle && newTarget != ClipKind::Walk) {
            throw std::invalid_argument("unknown target clip");
        }
        if (!std::isfinite(dt) || dt < 0.0) {
            throw std::invalid_argument("dt must be finite and non-negative");
        }
        if (!std::isfinite(transitionDuration) || transitionDuration < 0.0) {
            throw std::invalid_argument("transitionDuration must be finite and non-negative");
        }

        State next = previous;

        if (next.targetClip != newTarget) {
            // Target changed. The progress towards the *old* target becomes the
            // starting distance from the *new* target.
            // If we were 0.8 towards Idle, we are now 1.0 - 0.8 = 0.2 towards Walk.
            next.transitionProgress = 1.0 - next.transitionProgress;
            next.targetClip = newTarget;
        }

        if (transitionDuration == 0.0) {
            next.transitionProgress = 1.0;
        } else if (dt > 0.0) {
            next.transitionProgress += dt / transitionDuration;
        }

        if (next.transitionProgress > 1.0) {
            next.transitionProgress = 1.0;
        }
        if (next.transitionProgress < 0.0) {
            next.transitionProgress = 0.0;
        }

        return next;
    }

    // Resolves the current state into explicit normalized weights for blending.
    static Weights resolve(const State& state) {
        if (state.targetClip != ClipKind::Idle && state.targetClip != ClipKind::Walk) {
            throw std::invalid_argument("unknown state clip");
        }
        if (!std::isfinite(state.transitionProgress) || state.transitionProgress < 0.0 || state.transitionProgress > 1.0) {
            throw std::invalid_argument("state progress must be bounded in [0, 1]");
        }

        double targetWeight = state.transitionProgress;
        double otherWeight = 1.0 - targetWeight;

        // Ensure exactly sum to 1.0, resolving any minor floating point precision issues.
        // It's already mathematically close, but explicit bounding prevents drift.
        if (targetWeight == 1.0) otherWeight = 0.0;
        if (targetWeight == 0.0) otherWeight = 1.0;

        if (state.targetClip == ClipKind::Idle) {
            return {targetWeight, otherWeight};
        } else {
            return {otherWeight, targetWeight};
        }
    }
};

} // namespace domain
} // namespace stalthos
