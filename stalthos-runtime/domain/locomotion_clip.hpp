#pragma once
#include <cmath>
#include <stdexcept>

namespace stalthos {
namespace domain {

// Plain C++17 bounded locomotion phase and root displacement helper.
// Drives cyclic phase advance from signed actual body distance and a fixed stride.
class LocomotionClip {
    double duration_;
    double stride_;

public:
    struct AdvanceResult {
        double phase; // Current animation time [0, duration)
        double rootDisplacement; // Distance the root should be translated in the animation's local space (to be removed from host motion)
        long long cycles; // Number of complete cycles crossed this step
    };

    LocomotionClip(double duration, double stride) : duration_(duration), stride_(stride) {
        if (!std::isfinite(duration_) || duration_ <= 0.0) {
            throw std::invalid_argument("duration must be a finite positive value");
        }
        if (!std::isfinite(stride_) || stride_ <= 0.0) {
            throw std::invalid_argument("stride must be a finite positive value");
        }
    }

    // Advances the phase given a signed physical distance moved, and an initial phase.
    // Negative distance indicates moving backward.
    // Calculates the required in-place root removal displacement.
    AdvanceResult advance(double currentPhase, double distanceMoved) const {
        if (!std::isfinite(currentPhase) || currentPhase < 0.0 || currentPhase >= duration_) {
            throw std::invalid_argument("currentPhase must be finite and within [0, duration)");
        }
        if (!std::isfinite(distanceMoved)) {
            throw std::invalid_argument("distanceMoved must be finite");
        }

        if (distanceMoved == 0.0) {
            return {currentPhase, 0.0, 0};
        }

        // Fraction of the clip's full stride moved
        double strideFraction = distanceMoved / stride_;

        // Time to add
        double timeDelta = strideFraction * duration_;

        double newTime = currentPhase + timeDelta;

        // Safe bounded modulo for cycle wrapping
        double cyclesReal = std::floor(newTime / duration_);

        // We use std::fmod, but handle negative wrapping cleanly
        double newPhase = std::fmod(newTime, duration_);
        if (newPhase < 0.0) {
            newPhase += duration_;
        }

        // In case of precision loss pushing it exactly to duration
        if (newPhase >= duration_) {
            newPhase = 0.0;
        }

        // Ensure cycles count fits in long long without overflow
        if (!std::isfinite(cyclesReal) || cyclesReal >= 9223372036854775808.0 || cyclesReal < -9223372036854775808.0) {
            throw std::overflow_error("cycle count overflow");
        }

        long long cycles = static_cast<long long>(cyclesReal);

        return {newPhase, distanceMoved, cycles};
    }
};

} // namespace domain
} // namespace stalthos
