#pragma once

#include <cmath>
#include <stdexcept>

namespace stalthos {
namespace domain {

// A proposed, species-neutral mapping from measured clip stride to body speed.
// LocomotionClip remains responsible for mapping actual displacement to phase.
class LocomotionPace {
public:
    enum class Intent { Forward, Reverse, Idle };

    struct Limits {
        double targetCyclesPerSecond = 1.0;
        double maxPhysicalSpeed = 1.0;
    };

    struct PaceResult {
        double signedSpeed;
        double cycleRate;
    };

    // Validate all inputs and derived arithmetic before returning a value. This
    // stateless helper cannot partially mutate caller state on invalid metadata.
    static PaceResult calculate(double measuredStride, double authoredDuration,
                                Intent intent) {
        return calculate(measuredStride, authoredDuration, intent, Limits{});
    }

    static PaceResult calculate(double measuredStride, double authoredDuration,
                                Intent intent, const Limits& limits) {
        requirePositiveFinite(measuredStride, "stride must be finite and positive");
        requirePositiveFinite(authoredDuration, "duration must be finite and positive");
        requirePositiveFinite(limits.targetCyclesPerSecond,
                              "target cycle rate must be finite and positive");
        requirePositiveFinite(limits.maxPhysicalSpeed,
                              "maximum speed must be finite and positive");

        double speed = measuredStride * limits.targetCyclesPerSecond;
        if (!std::isfinite(speed)) {
            throw std::overflow_error("derived physical speed overflow");
        }
        if (speed > limits.maxPhysicalSpeed) speed = limits.maxPhysicalSpeed;

        const double actualCycleRate = speed / measuredStride;
        if (!std::isfinite(actualCycleRate)) {
            throw std::overflow_error("derived actual cycle rate overflow");
        }

        switch (intent) {
        case Intent::Forward: return {speed, actualCycleRate};
        case Intent::Reverse: return {-speed, actualCycleRate};
        case Intent::Idle: return {0.0, 0.0};
        }
        throw std::invalid_argument("unknown locomotion intent");
    }

private:
    static void requirePositiveFinite(double value, const char* message) {
        if (!std::isfinite(value) || value <= 0.0) throw std::invalid_argument(message);
    }
};

} // namespace domain
} // namespace stalthos
