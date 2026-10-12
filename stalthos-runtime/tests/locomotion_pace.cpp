#include "../domain/locomotion_pace.hpp"

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
    using stalthos::domain::LocomotionPace;
    using Intent = LocomotionPace::Intent;
    using Limits = LocomotionPace::Limits;
    try {
        constexpr double stride = 0.16128;
        const auto forward = LocomotionPace::calculate(stride, 2.0, Intent::Forward);
        check(close(forward.signedSpeed, stride), "one cycle/s derives speed from measured stride");
        check(close(forward.cycleRate, 1.0), "forward cycle rate retained");

        Limits faster{1.5, 1.0};
        const auto reverse = LocomotionPace::calculate(stride, 2.0, Intent::Reverse, faster);
        check(close(reverse.signedSpeed, -stride * 1.5), "reverse uses same magnitude and negative sign");
        check(close(reverse.cycleRate, 1.5), "reverse cycle rate retained");
        const auto idle = LocomotionPace::calculate(stride, 2.0, Intent::Idle, faster);
        check(idle.signedSpeed == 0.0 && idle.cycleRate == 0.0, "idle has zero speed and cadence");

        Limits capped{10.0, 0.5};
        const auto limited = LocomotionPace::calculate(stride, 2.0, Intent::Forward, capped);
        check(limited.signedSpeed == 0.5 && close(limited.cycleRate, 0.5 / stride), "physical speed cap reports resulting cadence");
        Limits reverseCapped{10.0, 0.5};
        const auto limitedReverse = LocomotionPace::calculate(stride, 2.0, Intent::Reverse, reverseCapped);
        check(limitedReverse.signedSpeed == -0.5 && close(limitedReverse.cycleRate, 0.5 / stride), "reverse cap reports resulting cadence");

        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        for (double bad : {0.0, -1.0, nan, inf}) {
            throws<std::invalid_argument>([=] { LocomotionPace::calculate(bad, 2.0, Intent::Forward); }, "invalid stride rejected");
            throws<std::invalid_argument>([=] { LocomotionPace::calculate(stride, bad, Intent::Forward); }, "invalid duration rejected");
        }
        for (double bad : {0.0, -1.0, nan, inf}) {
            Limits badRate{bad, 1.0};
            Limits badCap{1.0, bad};
            throws<std::invalid_argument>([=] { LocomotionPace::calculate(stride, 2.0, Intent::Forward, badRate); }, "invalid cycle rate rejected");
            throws<std::invalid_argument>([=] { LocomotionPace::calculate(stride, 2.0, Intent::Forward, badCap); }, "invalid speed cap rejected");
        }
        Limits overflow{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
        throws<std::overflow_error>([&] { LocomotionPace::calculate(2.0, 2.0, Intent::Forward, overflow); }, "overflowing speed rejected");
        throws<std::invalid_argument>([&] { LocomotionPace::calculate(stride, 2.0, static_cast<Intent>(99)); }, "invalid intent rejected");
        std::cout << "All locomotion pace tests passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
