#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include "../domain/camera_framing.hpp"

using namespace stalthos;
using namespace stalthos::domain;

static void require(bool condition, const std::string& msg) {
    if (!condition) {
        throw std::runtime_error("Assertion failed: " + msg);
    }
}

void testFiniteValidsAndClamps() {
    // Zoom / Distance clamp 2..18m
    require(CameraFraming::clampDistance(1.0) == 2.0, "clampDistance min");
    require(CameraFraming::clampDistance(2.0) == 2.0, "clampDistance 2m");
    require(CameraFraming::clampDistance(8.0) == 8.0, "clampDistance 8m");
    require(CameraFraming::clampDistance(18.0) == 18.0, "clampDistance 18m");
    require(CameraFraming::clampDistance(25.0) == 18.0, "clampDistance max");

    // Zoom update
    double z = CameraFraming::updateZoom(8.0, 1.0, 1.0); // zoom in by 1m -> 7m
    require(std::abs(z - 7.0) < 1e-6, "zoom in 1m");
    z = CameraFraming::updateZoom(8.0, -1.0, 1.0); // zoom out by 1m -> 9m
    require(std::abs(z - 9.0) < 1e-6, "zoom out 1m");
    z = CameraFraming::updateZoom(2.0, 5.0, 1.0); // zoom in beyond min -> clamped to 2m
    require(z == 2.0, "zoom in clamped");
    z = CameraFraming::updateZoom(18.0, -10.0, 1.0); // zoom out beyond max -> clamped to 18m
    require(z == 18.0, "zoom out clamped");

    // Elevation clamp -10..75 degrees
    double elLow = CameraFraming::clampElevation(-20.0 * CameraFraming::kDegToRad);
    require(std::abs(elLow - (-10.0 * CameraFraming::kDegToRad)) < 1e-6, "elevation min clamp");
    double elHigh = CameraFraming::clampElevation(85.0 * CameraFraming::kDegToRad);
    require(std::abs(elHigh - (75.0 * CameraFraming::kDegToRad)) < 1e-6, "elevation max clamp");
}

void testNaNAndInvalidRejection() {
    double nan = std::numeric_limits<double>::quiet_NaN();
    double inf = std::numeric_limits<double>::infinity();

    // Distance clamping on NaN/Inf throws
    bool caught = false;
    try { CameraFraming::clampDistance(nan); } catch (const std::invalid_argument&) { caught = true; }
    require(caught, "clampDistance NaN");

    caught = false;
    try { CameraFraming::clampElevation(nan); } catch (const std::invalid_argument&) { caught = true; }
    require(caught, "clampElevation NaN");

    caught = false;
    try { CameraFraming::updateZoom(8.0, nan); } catch (const std::invalid_argument&) { caught = true; }
    require(caught, "updateZoom NaN");

    caught = false;
    try { CameraFraming::wrapAngle(nan); } catch (const std::invalid_argument&) { caught = true; }
    require(caught, "wrapAngle NaN");

    // computeFraming rejects NaN / Inf
    caught = false;
    try {
        CameraFraming::computeFraming({nan, 0, 0}, 0.0, {0, 0, 8.0});
    } catch (const std::invalid_argument&) { caught = true; }
    require(caught, "computeFraming bodyPos NaN");

    caught = false;
    try {
        CameraFraming::computeFraming({0, 0, 0}, inf, {0, 0, 8.0});
    } catch (const std::invalid_argument&) { caught = true; }
    require(caught, "computeFraming heading Inf");

    caught = false;
    try {
        CameraFraming::computeFraming({0, 0, 0}, 0.0, {0, 0, nan});
    } catch (const std::invalid_argument&) { caught = true; }
    require(caught, "computeFraming orbit NaN");
}

void testAngleWrapLargeFinite() {
    constexpr double pi = CameraFraming::kPi;
    // Basic wrap
    require(std::abs(CameraFraming::wrapAngle(0.0) - 0.0) < 1e-6, "wrap 0");
    double w3pi = CameraFraming::wrapAngle(3.0 * pi);
    require(std::abs(w3pi - (-pi)) < 1e-6 || std::abs(w3pi - pi) < 1e-6, "wrap 3pi");

    // Large finite angles (e.g. 1000 full turns)
    double largePos = 1000.0 * 2.0 * pi + 0.5;
    double wrappedPos = CameraFraming::wrapAngle(largePos);
    require(std::abs(wrappedPos - 0.5) < 1e-6, "wrap largePos");

    double largeNeg = -1000.0 * 2.0 * pi - 0.75;
    double wrappedNeg = CameraFraming::wrapAngle(largeNeg);
    require(std::abs(wrappedNeg - (-0.75)) < 1e-6, "wrap largeNeg");
}

void testPresetsAndBodyDecoupling() {
    Vec3 bodyPos{10.0, 5.0, 20.0};
    double heading = 1.23; // rad

    // Follow preset: default 8m
    auto followOrbit = CameraFraming::followPreset(0.0);
    require(followOrbit.distance == 8.0, "follow preset 8m");
    auto followOut = CameraFraming::computeFraming(bodyPos, heading, followOrbit);
    require(followOut.target.x == bodyPos.x, "target X matches body");
    require(followOut.target.y == bodyPos.y + 0.8, "target Y matches body + 0.8");
    require(followOut.target.z == bodyPos.z, "target Z matches body");

    // Check eye distance to target in 3D
    double dx = followOut.eye.x - followOut.target.x;
    double dy = followOut.eye.y - followOut.target.y;
    double dz = followOut.eye.z - followOut.target.z;
    double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    require(std::abs(dist - 8.0) < 1e-5, "eye target distance is 8m");

    // Close inspection preset: 3m distance
    auto inspectOrbit = CameraFraming::closeInspectionPreset(heading);
    require(inspectOrbit.distance == 3.0, "inspect preset 3m");
    auto inspectOut = CameraFraming::computeFraming(bodyPos, heading, inspectOrbit);
    double forwardX = std::sin(heading);
    double forwardZ = std::cos(heading);
    double cameraForwardDot = (inspectOut.eye.x - inspectOut.target.x) * forwardX +
                              (inspectOut.eye.z - inspectOut.target.z) * forwardZ;
    require(cameraForwardDot > 0.0, "inspection camera is in front of animal");
    dx = inspectOut.eye.x - inspectOut.target.x;
    dy = inspectOut.eye.y - inspectOut.target.y;
    dz = inspectOut.eye.z - inspectOut.target.z;
    dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    require(std::abs(dist - 3.0) < 1e-5, "eye target inspect distance is 3m");

    // Body pose is NOT mutated (const input guaranteed, bodyPos remains unchanged)
    require(bodyPos.x == 10.0 && bodyPos.y == 5.0 && bodyPos.z == 20.0, "body position immutability");
}

void testTerrainClearanceSeparation() {
    Vec3 bodyPos{0.0, 1.0, 0.0};
    // Looking from azimuth 0, elevation -5 degrees (pointing slightly upwards from low eye)
    // distance 8m
    CameraFraming::SphericalOrbit orbit(0.0, -5.0 * CameraFraming::kDegToRad, 8.0);

    // Case 1: Flat ground at 0.0, eye will be target.y + 8 * sin(-5 deg)
    // target.y = 1.8. sin(-5 deg) ~= -0.087 -> eye.y ~= 1.8 - 0.697 = 1.103. Above 0.0 + 0.25.
    auto out1 = CameraFraming::computeFraming(bodyPos, 0.0, orbit, 0.8, [](double, double) {
        return 0.0;
    });
    require(out1.eye.y >= 0.25, "eye above ground + minClearance");

    // Case 2: High ground at eye location (e.g. ground Y = 2.0).
    // Eye must be pushed up to ground (2.0) + minClearance (0.25) = 2.25.
    auto out2 = CameraFraming::computeFraming(bodyPos, 0.0, orbit, 0.8, [](double, double) {
        return 2.0;
    });
    require(out2.eye.y >= 2.25, "eye raised above raised terrain");
    // Target authority is unchanged! target Y remains exactly 1.8
    require(std::abs(out2.target.y - 1.8) < 1e-6, "target authority strictly preserved");

    // Note that lifting eye.y changes the vertical separation while slant distance to target might increase,
    // but target position is authoritative and eye height clearance is maintained.
    require(out2.target.x == 0.0 && out2.target.z == 0.0, "target horizontal position preserved");
}

void testOverflowCoordinateRejection() {
    bool caught = false;
    try {
        CameraFraming::computeFraming({1e7, 0, 0}, 0.0, {0, 0, 8.0});
    } catch (const std::overflow_error&) {
        caught = true;
    }
    require(caught, "reject large X coordinate overflow");

    caught = false;
    try {
        CameraFraming::computeFraming({0, -2e6, 0}, 0.0, {0, 0, 8.0});
    } catch (const std::overflow_error&) {
        caught = true;
    }
    require(caught, "reject large Y coordinate overflow");
}

void testInterpolationRates() {
    double current = 10.0;
    double target = 2.0;
    // dt = 0 -> current
    require(CameraFraming::interpolateScalar(current, target, 5.0, 0.0) == current, "interp dt=0");
    // rate = 0 -> current
    require(CameraFraming::interpolateScalar(current, target, 0.0, 1.0) == current, "interp rate=0");

    // After some time, moves toward target
    double stepped = CameraFraming::interpolateScalar(current, target, 2.0, 0.5);
    require(stepped < current && stepped > target, "interp progress toward target");

    // Angle interpolation across wrap boundary (-pi to +pi)
    double angleCurr = 3.10; // close to pi
    double angleTarget = -3.10; // close to -pi, diff is 0.083
    double angleNext = CameraFraming::interpolateAngle(angleCurr, angleTarget, 5.0, 0.1);
    require(std::isfinite(angleNext), "interp angle finite");

    double huge = std::numeric_limits<double>::max();
    double halfway = CameraFraming::interpolateScalar(-huge, huge, 1.0, std::log(2.0));
    require(std::isfinite(halfway) && halfway == 0.0, "opposite extreme scalar inputs interpolate finitely");
    double hugeAngle = CameraFraming::interpolateAngle(-huge, huge, 1.0, 0.5);
    require(std::isfinite(hugeAngle), "opposite extreme finite angles interpolate finitely");
}

void testBoundedInputsAndTerrainFailure() {
    auto rejects = [](auto&& call, const std::string& label) {
        bool caught = false;
        try { call(); } catch (const std::exception&) { caught = true; }
        require(caught, label);
    };
    rejects([] { CameraFraming::computeFraming({0,0,0}, 0, {0,0,8}, -1); }, "negative target height rejected");
    rejects([] { CameraFraming::computeFraming({0,0,0}, 0, {0,0,8}, 21); }, "excessive target height rejected");
    rejects([] { CameraFraming::computeFraming({0,0,0}, 0, {0,0,8}, 0.8, nullptr, -0.1); }, "negative terrain clearance rejected");
    rejects([] { CameraFraming::computeFraming({0,0,0}, 0, {0,0,8}, 0.8, nullptr, 11); }, "excessive terrain clearance rejected");
    rejects([] { CameraFraming::computeFraming({0,0,0}, 0, {0,0,8}, 0.8, [](double,double) { return std::numeric_limits<double>::quiet_NaN(); }); }, "invalid terrain height fails explicitly");
    rejects([] { CameraFraming::updateZoom(8, std::numeric_limits<double>::max(), std::numeric_limits<double>::max()); }, "overflowing zoom product rejected");
}

int main() {
    std::cout << "Running CameraFraming tests under active NDEBUG..." << std::endl;
    testFiniteValidsAndClamps();
    testNaNAndInvalidRejection();
    testAngleWrapLargeFinite();
    testPresetsAndBodyDecoupling();
    testTerrainClearanceSeparation();
    testOverflowCoordinateRejection();
    testInterpolationRates();
    testBoundedInputsAndTerrainFailure();
    std::cout << "All CameraFraming tests passed successfully." << std::endl;
    return 0;
}
