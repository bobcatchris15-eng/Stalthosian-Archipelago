#include "../domain/locomotion_clip.hpp"
#include <iostream>
#include <cmath>
#include <stdexcept>

void assert_close(double a, double b, const char* msg) {
    if (std::abs(a - b) > 1e-5) {
        std::cerr << "Assertion failed: " << a << " != " << b << " - " << msg << std::endl;
        throw std::runtime_error("test failed");
    }
}

void assert_eq(long long a, long long b, const char* msg) {
    if (a != b) {
        std::cerr << "Assertion failed: " << a << " != " << b << " - " << msg << std::endl;
        throw std::runtime_error("test failed");
    }
}

int main() {
    using namespace stalthos::domain;

    try {
        // Test basic forward
        LocomotionClip clip(2.0, 0.162);
        auto res = clip.advance(0.0, 0.162 / 2.0); // half stride
        assert_close(res.phase, 1.0, "half stride forward phase");
        assert_close(res.rootDisplacement, 0.081, "half stride root displacement");
        assert_eq(res.cycles, 0, "half stride cycles");

        // Test loop boundary exact
        res = clip.advance(1.0, 0.162 / 2.0);
        assert_close(res.phase, 0.0, "exact loop phase");
        assert_eq(res.cycles, 1, "exact loop cycles");

        // Test backward reversal
        res = clip.advance(0.5, -0.162);
        assert_close(res.phase, 0.5, "backward wrap phase");
        assert_eq(res.cycles, -1, "backward wrap cycles");
        assert_close(res.rootDisplacement, -0.162, "backward displacement");

        // Test multiple loops forward
        res = clip.advance(0.0, 0.162 * 3.5);
        assert_close(res.phase, 1.0, "multi loop phase");
        assert_eq(res.cycles, 3, "multi loop cycles");

        // Test zero distance (frozen dead/rest)
        res = clip.advance(1.0, 0.0);
        assert_close(res.phase, 1.0, "zero distance phase");
        assert_eq(res.cycles, 0, "zero distance cycles");

        // Test NaN / Inf stride
        try { LocomotionClip c(2.0, NAN); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}
        try { LocomotionClip c(2.0, INFINITY); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}

        // Test Zero stride
        try { LocomotionClip c(2.0, 0.0); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}

        // Test NaN / Inf duration
        try { LocomotionClip c(NAN, 0.162); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}
        try { LocomotionClip c(INFINITY, 0.162); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}

        // Test Zero duration
        try { LocomotionClip c(0.0, 0.162); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}

        // Test NaN / Inf phase
        try { clip.advance(NAN, 0.1); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}
        try { clip.advance(INFINITY, 0.1); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}

        // Test out of bounds phase
        try { clip.advance(2.1, 0.1); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}
        try { clip.advance(-0.1, 0.1); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}

        // Test NaN / Inf distance
        try { clip.advance(0.0, NAN); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}
        try { clip.advance(0.0, INFINITY); throw std::logic_error("missed"); }
        catch (const std::invalid_argument&) {}

        // Large finite loop (stress test no while-loop unbounded timeout)
        res = clip.advance(0.0, 0.162 * 1e9);
        assert_close(res.rootDisplacement, 0.162 * 1e9, "large root");
        assert_eq(res.cycles, 1000000000LL, "large cycles");

        // 2^63 is exactly representable as double but cannot fit signed cycles.
        LocomotionClip unitClip(1.0, 1.0);
        try { unitClip.advance(0.0, 9223372036854775808.0); throw std::logic_error("positive cycle overflow missed"); }
        catch (const std::overflow_error&) {}
        res = unitClip.advance(0.0, -9223372036854775808.0);
        assert_eq(res.cycles, (-9223372036854775807LL - 1LL), "minimum signed cycles");
        std::cout << "All locomotion tests passed." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
