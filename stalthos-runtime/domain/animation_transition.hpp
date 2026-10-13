#pragma once

#include "locomotion_pace.hpp"
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

namespace stalthos {
namespace domain {

// Plain C++17 bounded animation transition schedule and phase controller.
// Governs the deterministic 5-phase probe: idle2s -> forward2s -> idle2s -> reverse2s -> idle2s.
class AnimationTransitionSchedule {
public:
    static constexpr double kPhaseDuration = 2.0;
    static constexpr int kPhaseCount = 5;
    static constexpr double kTotalDuration = 10.0;

    enum class ClipKind {
        Idle,
        Walk
    };

    struct PhaseDefinition {
        int index;
        const char* name;
        LocomotionPace::Intent intent;
        ClipKind activeClip;
        bool resetsIdleToNeutral;
    };

    struct PhaseEvaluation {
        int phaseIndex;
        std::string phaseName;
        LocomotionPace::Intent intent;
        ClipKind activeClip;
        double phaseElapsed;
        double phaseRemaining;
        bool isTransitionBoundary;
        bool resetsIdleToNeutral;
        float activeClipAmount;
        float inactiveClipAmount;
    };

    static const PhaseDefinition& phaseDefinition(int index) {
        if (index < 0 || index >= kPhaseCount) {
            throw std::out_of_range("phase index must be within 0..4");
        }
        static const PhaseDefinition kPhases[kPhaseCount] = {
            {0, "idle_1", LocomotionPace::Intent::Idle, ClipKind::Idle, true},
            {1, "forward_walk", LocomotionPace::Intent::Forward, ClipKind::Walk, false},
            {2, "idle_2", LocomotionPace::Intent::Idle, ClipKind::Idle, true},
            {3, "reverse_walk", LocomotionPace::Intent::Reverse, ClipKind::Walk, false},
            {4, "idle_3", LocomotionPace::Intent::Idle, ClipKind::Idle, true}
        };
        return kPhases[index];
    }

    static PhaseEvaluation evaluate(double elapsedSeconds) {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0) {
            throw std::invalid_argument("elapsed time must be finite non-negative");
        }

        int index = elapsedSeconds >= kTotalDuration ? kPhaseCount - 1 : static_cast<int>(elapsedSeconds / kPhaseDuration);
        if (index >= kPhaseCount) {
            index = kPhaseCount - 1;
        }

        const auto& def = phaseDefinition(index);
        double phaseStart = index * kPhaseDuration;
        double phaseElapsed = elapsedSeconds - phaseStart;
        if (phaseElapsed < 0.0) phaseElapsed = 0.0;
        if (phaseElapsed > kPhaseDuration && index < kPhaseCount - 1) phaseElapsed = kPhaseDuration;
        double phaseRemaining = std::max(0.0, kPhaseDuration - phaseElapsed);

        return {
            index,
            def.name,
            def.intent,
            def.activeClip,
            phaseElapsed,
            phaseRemaining,
            (phaseElapsed == 0.0),
            def.resetsIdleToNeutral,
            1.0f,
            0.0f
        };
    }

    static bool isComplete(double elapsedSeconds) {
        return std::isfinite(elapsedSeconds) && elapsedSeconds >= kTotalDuration;
    }
};

} // namespace domain
} // namespace stalthos
