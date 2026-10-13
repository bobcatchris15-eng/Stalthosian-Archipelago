#include "../domain/animation_blend.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "Failed: " << message << '\n';
        throw std::runtime_error(message);
    }
}

bool close(double a, double b) {
    return std::abs(a - b) < 1e-6;
}

template <typename Exception, typename F>
void throws(F&& f, const char* message) {
    try {
        f();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}
} // namespace

int main() {
    using stalthos::domain::AnimationBlend;
    using ClipKind = AnimationBlend::ClipKind;

    try {
        // Initial state is Idle with 1.0 progress.
        AnimationBlend::State state;
        check(state.targetClip == ClipKind::Idle, "initial target is idle");
        check(close(state.transitionProgress, 1.0), "initial progress is 1.0");

        auto w0 = AnimationBlend::resolve(state);
        check(close(w0.idleWeight, 1.0), "initial idle weight is 1.0");
        check(close(w0.walkWeight, 0.0), "initial walk weight is 0.0");
        check(close(w0.idleWeight + w0.walkWeight, 1.0), "weights sum to 1.0");

        // Transition to Walk over 1.0s, ticking 0.25s
        state = AnimationBlend::advance(state, ClipKind::Walk, 0.25, 1.0);
        check(state.targetClip == ClipKind::Walk, "target changed to walk");
        check(close(state.transitionProgress, 0.25), "progress inverted and advanced to 0.25");
        auto w1 = AnimationBlend::resolve(state);
        check(close(w1.idleWeight, 0.75), "idle weight at 0.75");
        check(close(w1.walkWeight, 0.25), "walk weight at 0.25");
        check(close(w1.idleWeight + w1.walkWeight, 1.0), "weights sum to 1.0");

        // Paused semantics: dt = 0.0
        state = AnimationBlend::advance(state, ClipKind::Walk, 0.0, 1.0);
        check(close(state.transitionProgress, 0.25), "dt=0 retains previous progress");

        // Interrupt transition: switch back to Idle after being 0.25 into Walk
        // Duration is 2.0s now. We are 0.25 into Walk, so we are 0.75 into Idle.
        state = AnimationBlend::advance(state, ClipKind::Idle, 0.5, 2.0);
        check(state.targetClip == ClipKind::Idle, "target changed back to idle");
        // inverted: 1.0 - 0.25 = 0.75. Then advanced 0.5/2.0 = 0.25. Result = 1.0
        check(close(state.transitionProgress, 1.0), "interrupted transition calculated correctly");
        auto w2 = AnimationBlend::resolve(state);
        check(close(w2.idleWeight, 1.0), "interrupted weights fully back to idle");
        check(close(w2.walkWeight, 0.0), "interrupted weights zero for walk");

        // Zero duration transition (snap)
        state = AnimationBlend::advance(state, ClipKind::Walk, 0.1, 0.0);
        check(state.targetClip == ClipKind::Walk, "target changed to walk");
        check(close(state.transitionProgress, 1.0), "zero duration snapped to 1.0");
        auto w3 = AnimationBlend::resolve(state);
        check(close(w3.idleWeight, 0.0), "snap weight zero for idle");
        check(close(w3.walkWeight, 1.0), "snap weight 1.0 for walk");

        // Bounded max
        state = AnimationBlend::advance(state, ClipKind::Walk, 10.0, 1.0);
        check(close(state.transitionProgress, 1.0), "progress bounded to 1.0");

        // Validation failures
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();

        throws<std::invalid_argument>([&] { AnimationBlend::advance(state, ClipKind::Walk, -0.1, 1.0); }, "negative dt rejected");
        throws<std::invalid_argument>([&] { AnimationBlend::advance(state, ClipKind::Walk, 0.1, -1.0); }, "negative duration rejected");
        throws<std::invalid_argument>([&] { AnimationBlend::advance(state, ClipKind::Walk, nan, 1.0); }, "nan dt rejected");
        throws<std::invalid_argument>([&] { AnimationBlend::advance(state, ClipKind::Walk, 0.1, nan); }, "nan duration rejected");
        throws<std::invalid_argument>([&] { AnimationBlend::advance(state, ClipKind::Walk, inf, 1.0); }, "inf dt rejected");

        AnimationBlend::State invalidState;
        invalidState.transitionProgress = 1.5;
        throws<std::invalid_argument>([&] { AnimationBlend::resolve(invalidState); }, "unbounded state resolved rejected");

        invalidState.transitionProgress = nan;
        throws<std::invalid_argument>([&] { AnimationBlend::advance(invalidState, ClipKind::Walk, 0.1, 1.0); }, "nan prior state rejected by advance");
        invalidState.transitionProgress = -0.1;
        throws<std::invalid_argument>([&] { AnimationBlend::advance(invalidState, ClipKind::Walk, 0.1, 1.0); }, "negative prior state rejected by advance");
        invalidState.transitionProgress = 1.0;
        invalidState.targetClip = static_cast<ClipKind>(99);
        throws<std::invalid_argument>([&] { AnimationBlend::resolve(invalidState); }, "unknown prior clip rejected");
        throws<std::invalid_argument>([&] { AnimationBlend::advance(state, static_cast<ClipKind>(99), 0.1, 1.0); }, "unknown target clip rejected");
        state = AnimationBlend::State{};
        state = AnimationBlend::advance(state, ClipKind::Walk, 0.37, 1.0);
        const auto beforeInterrupt = AnimationBlend::resolve(state);
        state = AnimationBlend::advance(state, ClipKind::Idle, 0.0, 1.0);
        const auto afterInterrupt = AnimationBlend::resolve(state);
        check(close(beforeInterrupt.idleWeight, afterInterrupt.idleWeight) && close(beforeInterrupt.walkWeight, afterInterrupt.walkWeight), "zero-time interruption preserves weights");
        for (int tick = 0; tick < 10000; ++tick) {
            state = AnimationBlend::advance(state, tick % 7 < 3 ? ClipKind::Idle : ClipKind::Walk, 0.013, 0.2);
            const auto weights = AnimationBlend::resolve(state);
            check(std::isfinite(weights.idleWeight) && std::isfinite(weights.walkWeight), "repeated weights finite");
            check(weights.idleWeight >= 0 && weights.walkWeight >= 0 && weights.idleWeight <= 1 && weights.walkWeight <= 1 && close(weights.idleWeight + weights.walkWeight, 1.0), "repeated interruptions remain normalized");
        }
        std::cout << "All animation blend tests passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
