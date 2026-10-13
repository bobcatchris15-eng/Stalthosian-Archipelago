#include "../domain/animation_transition.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << "Failed: " << message << '\n'; throw std::runtime_error(message); }
}
bool close(double a, double b) { return std::abs(a - b) < 1e-12; }
template <typename Exception, typename F>
void throws(F&& f, const char* message) {
    try { f(); } catch (const Exception&) { return; }
    throw std::runtime_error(message);
}
}

int main() {
    using stalthos::domain::AnimationTransitionSchedule;
    using stalthos::domain::LocomotionPace;
    using ClipKind = AnimationTransitionSchedule::ClipKind;

    try {
        // Phase 0: 0.0s to 2.0s -> idle_1
        auto p0_start = AnimationTransitionSchedule::evaluate(0.0);
        check(p0_start.phaseIndex == 0, "0.0s is phase 0");
        check(p0_start.phaseName == "idle_1", "phase 0 name is idle_1");
        check(p0_start.intent == LocomotionPace::Intent::Idle, "phase 0 is idle intent");
        check(p0_start.activeClip == ClipKind::Idle, "phase 0 active clip is idle");
        check(p0_start.isTransitionBoundary, "0.0s is transition boundary");
        check(p0_start.resetsIdleToNeutral, "phase 0 resets idle to neutral");
        check(close(p0_start.phaseElapsed, 0.0), "phase 0 elapsed 0.0");
        check(close(p0_start.phaseRemaining, 2.0), "phase 0 remaining 2.0");

        auto p0_mid = AnimationTransitionSchedule::evaluate(1.0);
        check(p0_mid.phaseIndex == 0, "1.0s is phase 0");
        check(!p0_mid.isTransitionBoundary, "1.0s is not transition boundary");
        check(close(p0_mid.phaseElapsed, 1.0), "phase 0 elapsed 1.0");

        // Phase 1: 2.0s to 4.0s -> forward_walk
        auto p1_start = AnimationTransitionSchedule::evaluate(2.0);
        check(p1_start.phaseIndex == 1, "2.0s is phase 1");
        check(p1_start.phaseName == "forward_walk", "phase 1 name is forward_walk");
        check(p1_start.intent == LocomotionPace::Intent::Forward, "phase 1 is forward intent");
        check(p1_start.activeClip == ClipKind::Walk, "phase 1 active clip is walk");
        check(p1_start.isTransitionBoundary, "2.0s is transition boundary");
        check(!p1_start.resetsIdleToNeutral, "phase 1 does not reset idle to neutral");
        check(close(p1_start.phaseElapsed, 0.0), "phase 1 elapsed 0.0");

        // Phase 2: 4.0s to 6.0s -> idle_2
        auto p2_start = AnimationTransitionSchedule::evaluate(4.0);
        check(p2_start.phaseIndex == 2, "4.0s is phase 2");
        check(p2_start.phaseName == "idle_2", "phase 2 name is idle_2");
        check(p2_start.intent == LocomotionPace::Intent::Idle, "phase 2 is idle intent");
        check(p2_start.activeClip == ClipKind::Idle, "phase 2 active clip is idle");
        check(p2_start.resetsIdleToNeutral, "phase 2 resets idle to neutral");

        // Phase 3: 6.0s to 8.0s -> reverse_walk
        auto p3_start = AnimationTransitionSchedule::evaluate(6.0);
        check(p3_start.phaseIndex == 3, "6.0s is phase 3");
        check(p3_start.phaseName == "reverse_walk", "phase 3 name is reverse_walk");
        check(p3_start.intent == LocomotionPace::Intent::Reverse, "phase 3 is reverse intent");
        check(p3_start.activeClip == ClipKind::Walk, "phase 3 active clip is walk");
        check(!p3_start.resetsIdleToNeutral, "phase 3 does not reset idle to neutral");

        // Phase 4: 8.0s to 10.0s -> idle_3
        auto p4_start = AnimationTransitionSchedule::evaluate(8.0);
        check(p4_start.phaseIndex == 4, "8.0s is phase 4");
        check(p4_start.phaseName == "idle_3", "phase 4 name is idle_3");
        check(p4_start.intent == LocomotionPace::Intent::Idle, "phase 4 is idle intent");
        check(p4_start.activeClip == ClipKind::Idle, "phase 4 active clip is idle");
        check(p4_start.resetsIdleToNeutral, "phase 4 resets idle to neutral");

        // Completion at 10.0s
        check(!AnimationTransitionSchedule::isComplete(9.99), "9.99s is not complete");
        check(AnimationTransitionSchedule::isComplete(10.0), "10.0s is complete");
        check(AnimationTransitionSchedule::isComplete(12.0), "12.0s is complete");

        check(AnimationTransitionSchedule::evaluate(std::numeric_limits<double>::max()).phaseIndex == 4, "huge finite elapsed clamps before integer conversion");

        // Invalid inputs
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        throws<std::invalid_argument>([=] { AnimationTransitionSchedule::evaluate(-0.1); }, "negative time rejected");
        throws<std::invalid_argument>([=] { AnimationTransitionSchedule::evaluate(nan); }, "nan rejected");
        throws<std::invalid_argument>([=] { AnimationTransitionSchedule::evaluate(inf); }, "inf rejected");
        throws<std::out_of_range>([=] { AnimationTransitionSchedule::phaseDefinition(-1); }, "negative index rejected");
        throws<std::out_of_range>([=] { AnimationTransitionSchedule::phaseDefinition(5); }, "out of bounds index rejected");

        std::cout << "All animation transition schedule tests passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
