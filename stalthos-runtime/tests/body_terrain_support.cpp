#include "../domain/body_terrain_support.hpp"
#include <iostream>
#include <cmath>
#include <limits>
#include <functional>
#include <string>

using namespace stalthos::domain;

static bool near(double a, double b, double epsilon = 1e-6) {
    return std::abs(a - b) < epsilon;
}

static void rejects(std::function<void()> f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception&) {
        caught = true;
    }
    if (!caught) {
        throw std::runtime_error("Expected exception was not thrown.");
    }
}

static void check(bool condition, const std::string& msg) {
    if (!condition) throw std::runtime_error(msg);
}

static void testHorizontal() {
    auto hc = [](double, double) { return 10.0; };
    auto s = calculateTerrainSupport(0, 0, 0, 1.0, 1.0, hc);
    check(near(s.up.x, 0) && near(s.up.y, 1) && near(s.up.z, 0), "horizontal up is not (0,1,0)");
    check(near(s.forward.x, 0) && near(s.forward.y, 0) && near(s.forward.z, 1), "yaw=0 forward is not (0,0,1)");
    check(near(s.right.x, 1) && near(s.right.y, 0) && near(s.right.z, 0), "yaw=0 right is not (1,0,0)");
    check(near(s.slopeAngleRadians, 0), "slope should be 0");
    check(s.supported, "horizontal should be supported");
    check(near(s.centerHeight, 10.0), "center height wrong");

    const double pi = std::acos(-1.0);
    auto s2 = calculateTerrainSupport(0, 0, pi / 2.0, 1.0, 1.0, hc);
    check(near(s2.forward.x, 1) && near(s2.forward.y, 0) && near(s2.forward.z, 0), "yaw=pi/2 forward is not (1,0,0)");
    check(near(s2.right.x, 0) && near(s2.right.y, 0) && near(s2.right.z, -1), "yaw=pi/2 right is not (0,0,-1)");

    auto s3 = calculateTerrainSupport(5.0, -12.0, pi, 0.5, 0.25, hc);
    check(near(s3.forward.x, 0) && near(s3.forward.y, 0) && near(s3.forward.z, -1), "yaw=pi forward is not (0,0,-1)");
    check(near(s3.right.x, -1) && near(s3.right.y, 0) && near(s3.right.z, 0), "yaw=pi right is not (-1,0,0)");
}

static void testInclinedPlane() {
    // Plane: y = 0.1 * z + 0.05 * x
    // Slope: dz = 0.1, dx = 0.05 => sqrt(0.01 + 0.0025) ≈ 0.1118 rad ≈ 6.4 degrees <= 20 deg
    auto hc = [](double x, double z) { return 0.1 * z + 0.05 * x; };
    auto s = calculateTerrainSupport(0, 0, 0, 1.0, 1.0, hc);
    check(s.supported, "moderate slope should be supported");
    check(near(dot(s.up, s.forward), 0), "up and forward not orthogonal");
    check(near(dot(s.up, s.right), 0), "up and right not orthogonal");
    check(near(dot(s.forward, s.right), 0), "forward and right not orthogonal");

    // Check lengths are 1
    check(near(dot(s.up, s.up), 1.0), "up length not 1");
    check(near(dot(s.forward, s.forward), 1.0), "forward length not 1");
    check(near(dot(s.right, s.right), 1.0), "right length not 1");

    // Check LH basis cross products: Forward x Right = Up, Up x Forward = Right
    Vector3 crossFR = cross(s.forward, s.right);
    check(near(crossFR.x, s.up.x) && near(crossFR.y, s.up.y) && near(crossFR.z, s.up.z), "LH cross(forward, right) != up");

    Vector3 crossUF = cross(s.up, s.forward);
    check(near(crossUF.x, s.right.x) && near(crossUF.y, s.right.y) && near(crossUF.z, s.right.z), "LH cross(up, forward) != right");
}

static void testSteepPlaneAndFallback() {
    // Steep plane: y = x, slope is 45 deg, which > 20 deg
    const double pi = std::acos(-1.0);
    double yaw = 0.35;
    auto hc = [](double x, double) { return x; };
    auto s = calculateTerrainSupport(0, 0, yaw, 0.5, 0.25, hc);
    check(!s.supported, "steep slope should be marked unsupported");
    check(s.slopeAngleRadians > 20.0 * pi / 180.0, "slope angle must exceed 20 degrees");
    check(std::isfinite(s.slopeAngleRadians), "slope angle must remain finite");

    // Must explicitly fall back to flat yaw basis
    Vector3 expectedFlatForward = {std::sin(yaw), 0.0, std::cos(yaw)};
    Vector3 expectedFlatRight = {std::cos(yaw), 0.0, -std::sin(yaw)};
    check(near(s.forward.x, expectedFlatForward.x) && near(s.forward.y, 0.0) && near(s.forward.z, expectedFlatForward.z),
          "unsupported fallback forward did not match flat yaw");
    check(near(s.right.x, expectedFlatRight.x) && near(s.right.y, 0.0) && near(s.right.z, expectedFlatRight.z),
          "unsupported fallback right did not match flat yaw");
    check(near(s.up.x, 0.0) && near(s.up.y, 1.0) && near(s.up.z, 0.0),
          "unsupported fallback up did not match flat (0,1,0)");

    // surfaceNormal should still record the steep normal
    check(s.surfaceNormal.y < 0.8, "surface normal should reflect steep ground");
}

static void testFiniteArithmeticEdgeCases() {
    auto hc = [](double, double) { return 0.0; };
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    // Nonfinite input coordinates and halfspans
    rejects([&]() { calculateTerrainSupport(qnan, 0, 0, 1, 1, hc); });
    rejects([&]() { calculateTerrainSupport(0, qnan, 0, 1, 1, hc); });
    rejects([&]() { calculateTerrainSupport(0, 0, qnan, 1, 1, hc); });
    rejects([&]() { calculateTerrainSupport(inf, 0, 0, 1, 1, hc); });
    rejects([&]() { calculateTerrainSupport(0, -inf, 0, 1, 1, hc); });
    rejects([&]() { calculateTerrainSupport(0, 0, 0, -1, 1, hc); });
    rejects([&]() { calculateTerrainSupport(0, 0, 0, 1, 0, hc); });
    rejects([&]() { calculateTerrainSupport(0, 0, 0, qnan, 1, hc); });
    rejects([&]() { calculateTerrainSupport(0, 0, 0, 1, inf, hc); });
    rejects([&]() { calculateTerrainSupport(0, 0, 0, 1, 1, nullptr); });

    rejects([&]() { calculateTerrainSupport(0, 0, 0, std::numeric_limits<double>::max(), 1, hc); });
    // Height callback non-finite or throwing
    auto badHc = [](double, double) { return std::numeric_limits<double>::infinity(); };
    rejects([&]() { calculateTerrainSupport(0, 0, 0, 1, 1, badHc); });

    auto nanHc = [](double, double) { return std::numeric_limits<double>::quiet_NaN(); };
    rejects([&]() { calculateTerrainSupport(0, 0, 0, 1, 1, nanHc); });

    auto throwHc = [](double, double) -> double { throw std::runtime_error("err"); };
    rejects([&]() { calculateTerrainSupport(0, 0, 0, 1, 1, throwHc); });

    // Extreme coordinate near overflow
    rejects([&]() { calculateTerrainSupport(1.7e308, 1.7e308, 0, 1.7e308, 1.0, hc); });

    // Extreme height difference overflow
    auto overflowDiffHc = [](double, double z) { return z > 0 ? 1.7e308 : -1.7e308; };
    rejects([&]() { calculateTerrainSupport(0, 0, 0, 1.0, 1.0, overflowDiffHc); });

    // Normalization edge cases: vector with 0 length
    Vector3 zeroVec{0.0, 0.0, 0.0};
    Vector3 normZero = normalize(zeroVec);
    check(normZero.x == 0.0 && normZero.y == 0.0 && normZero.z == 0.0, "normalize zero vector");

    // Normalization with huge components: must not overflow to NaN
    Vector3 hugeVec{1e200, 1e200, 1e200};
    Vector3 normHuge = normalize(hugeVec);
    check(std::isfinite(normHuge.x) && std::isfinite(normHuge.y) && std::isfinite(normHuge.z), "normalize huge vector finite");
    check(near(dot(normHuge, normHuge), 1.0), "normalize huge vector unit length");

    // Very steep height differences: must not NaN, must flag unsupported and fall back
    auto extremeSlopeHc = [](double x, double) { return x * 1e8; };
    auto sExtreme = calculateTerrainSupport(0, 0, 0, 0.5, 0.25, extremeSlopeHc);
    check(!sExtreme.supported, "extreme slope should be unsupported");
    check(std::isfinite(sExtreme.slopeAngleRadians), "extreme slope angle must be finite");
    check(!std::isnan(sExtreme.slopeAngleRadians), "extreme slope angle must not be NaN");
    check(near(sExtreme.up.y, 1.0), "extreme slope fallback up is (0,1,0)");
}

int main() {
    try {
        testHorizontal();
        std::cout << "PASS Horizontal\n";
        testInclinedPlane();
        std::cout << "PASS InclinedPlane\n";
        testSteepPlaneAndFallback();
        std::cout << "PASS SteepPlaneAndFallback\n";
        testFiniteArithmeticEdgeCases();
        std::cout << "PASS FiniteArithmeticEdgeCases\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
