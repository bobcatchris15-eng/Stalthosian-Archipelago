#include "../domain/foot_support_plane.hpp"
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

// Group 1: Flat plane fitting
static void testFlatPlane() {
    std::array<FootSample, 4> samples = {{
        {1.0, 5.0, 1.0},
        {1.0, 5.0, -1.0},
        {-1.0, 5.0, 1.0},
        {-1.0, 5.0, -1.0}
    }};

    auto p = calculateFootSupportPlane(samples, 0.0);
    check(p.supported, "flat plane should be supported");
    check(near(p.centerHeight, 5.0), "flat plane height should be exactly 5.0");
    check(near(p.up.x, 0.0) && near(p.up.y, 1.0) && near(p.up.z, 0.0), "flat plane up is (0,1,0)");
    check(near(p.slopeAngleRadians, 0.0), "flat slope should be 0");
    for(double r : p.residuals) {
        check(near(r, 0.0), "flat plane should have zero residuals");
    }
    check(near(p.forward.x, 0.0) && near(p.forward.y, 0.0) && near(p.forward.z, 1.0), "yaw=0 forward is (0,0,1)");
    check(near(p.right.x, 1.0) && near(p.right.y, 0.0) && near(p.right.z, 0.0), "yaw=0 right is (1,0,0)");
}

// Group 2: Known slope fitting
static void testKnownSlope() {
    // Plane: y = 0.2*x - 0.1*z
    auto evalY = [](double x, double z) { return 0.2*x - 0.1*z; };
    std::array<FootSample, 4> samples = {{
        {1.0, evalY(1.0, 2.0), 2.0},
        {1.5, evalY(1.5, -0.5), -0.5},
        {-0.5, evalY(-0.5, 1.0), 1.0},
        {-2.0, evalY(-2.0, -2.0), -2.0}
    }};

    auto p = calculateFootSupportPlane(samples, 0.0);
    check(p.supported, "moderate slope should be supported");
    for(double r : p.residuals) {
        check(near(r, 0.0), "points on exact plane should have zero residuals");
    }

    // Normal for Ax + Bz - y + C = 0 where A=0.2, B=-0.1, C=0 -> N = (-0.2, 1, 0.1)
    Vector3 expectedNormal = normalize({-0.2, 1.0, 0.1});
    check(near(p.up.x, expectedNormal.x) && near(p.up.y, expectedNormal.y) && near(p.up.z, expectedNormal.z), "incorrect up vector for known slope");
    check(near(dot(p.up, p.forward), 0), "up and forward not orthogonal");
    check(near(dot(p.up, p.right), 0), "up and right not orthogonal");
    check(near(dot(p.forward, p.right), 0), "forward and right not orthogonal");
}

// Group 3: Uneven ground residuals
static void testUnevenGroundResiduals() {
    // True underlying plane y = 2.0 with symmetric perturbations
    std::array<FootSample, 4> samples = {{
        {1.0, 2.1, 1.0},   // +0.1
        {1.0, 1.9, -1.0},  // -0.1
        {-1.0, 1.9, 1.0},  // -0.1
        {-1.0, 2.1, -1.0}  // +0.1
    }};
    auto p = calculateFootSupportPlane(samples, 0.0);
    check(p.supported, "uneven but flat average should be supported");
    check(near(p.centerHeight, 2.0), "center height should be exactly 2.0");
    check(near(p.up.x, 0.0) && near(p.up.y, 1.0) && near(p.up.z, 0.0), "best fit is flat");

    check(near(p.residuals[0], 0.1), "residual 0");
    check(near(p.residuals[1], -0.1), "residual 1");
    check(near(p.residuals[2], -0.1), "residual 2");
    check(near(p.residuals[3], 0.1), "residual 3");
}

// Group 4: Extreme slope fallback (20-degree tilt policy)
static void testExtremeSlopeFallback() {
    // Plane y = 1.0 * x (45 deg slope)
    std::array<FootSample, 4> samples = {{
        {1.0, 1.0, 1.0},
        {1.0, 1.0, -1.0},
        {-1.0, -1.0, 1.0},
        {-1.0, -1.0, -1.0}
    }};

    double yaw = 0.5;
    auto p = calculateFootSupportPlane(samples, yaw);
    check(!p.supported, "45 deg slope should be unsupported");
    check(p.slopeAngleRadians > 20.0 * 3.1415926535 / 180.0, "slope angle should exceed 20 deg");

    // Check fallback logic applied
    check(near(p.up.x, 0.0) && near(p.up.y, 1.0) && near(p.up.z, 0.0), "fallback up should be flat");
    check(near(p.forward.x, std::sin(yaw)) && near(p.forward.y, 0.0) && near(p.forward.z, std::cos(yaw)), "fallback forward should be flat yaw");

    // Residuals still calculated from the *fitted* steep plane to expose exact discrepancy, not the fallback plane!
    for(double r : p.residuals) {
        check(near(r, 0.0), "residuals against true steep plane should be 0");
    }
}

// Group 5: Degenerate collinear and duplicate inputs
static void testDegenerateCollinear() {
    // All points on a line: x = z, y = 0
    std::array<FootSample, 4> samples = {{
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 1.0},
        {2.0, 0.0, 2.0},
        {-1.0, 0.0, -1.0}
    }};
    rejects([&]() { calculateFootSupportPlane(samples, 0.0); });

    // Duplicate points
    std::array<FootSample, 4> dupSamples = {{
        {1.0, 1.0, 1.0},
        {1.0, 1.0, 1.0},
        {2.0, 2.0, 2.0},
        {2.0, 2.0, 2.0}
    }};
    rejects([&]() { calculateFootSupportPlane(dupSamples, 0.0); });
}

// Group 6: Invalid and overflowing inputs
static void testInvalidInputs() {
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    std::array<FootSample, 4> validSamples = {{
        {1.0, 0.0, 1.0}, {1.0, 0.0, -1.0}, {-1.0, 0.0, 1.0}, {-1.0, 0.0, -1.0}
    }};

    // Invalid yaw
    rejects([&]() { calculateFootSupportPlane(validSamples, qnan); });

    // Invalid sample coord
    auto badSamples = validSamples;
    badSamples[1].y = inf;
    rejects([&]() { calculateFootSupportPlane(badSamples, 0.0); });

    // Overflow triggering in accumulation
    auto hugeSamples = validSamples;
    hugeSamples[0].x = 1.0e160;
    rejects([&]() { calculateFootSupportPlane(hugeSamples, 0.0); });
}

// Group 7: Tiny inputs
static void testTinyInputs() {
    std::array<FootSample, 4> samples = {{
        {1.0e-15, 5.0e-15, 1.0e-15},
        {1.0e-15, 5.0e-15, -1.0e-15},
        {-1.0e-15, 5.0e-15, 1.0e-15},
        {-1.0e-15, 5.0e-15, -1.0e-15}
    }};

    auto p = calculateFootSupportPlane(samples, 0.0);
    check(p.supported, "tiny flat plane should be supported");
    check(near(p.centerHeight, 5.0e-15, 1e-20), "tiny flat plane height");
    check(near(p.up.x, 0.0) && near(p.up.y, 1.0) && near(p.up.z, 0.0), "tiny flat plane up is (0,1,0)");
}

// Group 8: Translated copies of same layout at reference and larger bounded coordinates
static void testTranslatedCopies() {
    // Base layout on a known slope: y = 0.15*x - 0.08*z + 1.2
    auto evalY = [](double x, double z) { return 0.15 * x - 0.08 * z + 1.2; };
    std::array<FootSample, 4> baseSamples = {{
        {0.8, evalY(0.8, 1.2) + 0.02, 1.2},
        {0.9, evalY(0.9, -1.1) - 0.02, -1.1},
        {-0.7, evalY(-0.7, 1.0) - 0.02, 1.0},
        {-1.0, evalY(-1.0, -0.9) + 0.02, -0.9}
    }};
    const double yaw = 0.42;
    const auto pBase = calculateFootSupportPlane(baseSamples, yaw);
    check(pBase.supported, "base slope should be supported");

    // Copy 1: Reference island world coordinates: (x, y, z) + (240.0, 18.5, -310.0)
    const double refTx = 240.0;
    const double refTy = 18.5;
    const double refTz = -310.0;
    std::array<FootSample, 4> refSamples;
    for (size_t i = 0; i < 4; ++i) {
        refSamples[i] = {baseSamples[i].x + refTx, baseSamples[i].y + refTy, baseSamples[i].z + refTz};
    }
    const auto pRef = calculateFootSupportPlane(refSamples, yaw);
    check(pRef.supported == pBase.supported, "supported flag invariant under reference translation");
    check(near(pRef.slopeAngleRadians, pBase.slopeAngleRadians, 1e-12), "slope angle invariant under reference translation");
    check(near(pRef.up.x, pBase.up.x, 1e-12) && near(pRef.up.y, pBase.up.y, 1e-12) && near(pRef.up.z, pBase.up.z, 1e-12), "up invariant under reference translation");
    check(near(pRef.forward.x, pBase.forward.x, 1e-12) && near(pRef.forward.y, pBase.forward.y, 1e-12) && near(pRef.forward.z, pBase.forward.z, 1e-12), "forward invariant under reference translation");
    check(near(pRef.right.x, pBase.right.x, 1e-12) && near(pRef.right.y, pBase.right.y, 1e-12) && near(pRef.right.z, pBase.right.z, 1e-12), "right invariant under reference translation");
    check(near(pRef.centerHeight, pBase.centerHeight + refTy, 1e-12), "center height shifts exactly by Ty at reference coordinates");
    for (size_t i = 0; i < 4; ++i) {
        check(near(pRef.residuals[i], pBase.residuals[i], 1e-12), "residuals invariant under reference translation");
    }

    // Copy 2: Larger bounded world coordinates: (x, y, z) + (500000.0, 4200.0, -800000.0)
    const double bndTx = 500000.0;
    const double bndTy = 4200.0;
    const double bndTz = -800000.0;
    std::array<FootSample, 4> bndSamples;
    for (size_t i = 0; i < 4; ++i) {
        bndSamples[i] = {baseSamples[i].x + bndTx, baseSamples[i].y + bndTy, baseSamples[i].z + bndTz};
    }
    const auto pBnd = calculateFootSupportPlane(bndSamples, yaw);
    check(pBnd.supported == pBase.supported, "supported flag invariant under large bounded translation");
    check(near(pBnd.slopeAngleRadians, pBase.slopeAngleRadians, 1e-10), "slope angle invariant under large bounded translation");
    check(near(pBnd.up.x, pBase.up.x, 1e-10) && near(pBnd.up.y, pBase.up.y, 1e-10) && near(pBnd.up.z, pBase.up.z, 1e-10), "up invariant under large bounded translation");
    check(near(pBnd.forward.x, pBase.forward.x, 1e-10) && near(pBnd.forward.y, pBase.forward.y, 1e-10) && near(pBnd.forward.z, pBase.forward.z, 1e-10), "forward invariant under large bounded translation");
    check(near(pBnd.right.x, pBase.right.x, 1e-10) && near(pBnd.right.y, pBase.right.y, 1e-10) && near(pBnd.right.z, pBase.right.z, 1e-10), "right invariant under large bounded translation");
    check(near(pBnd.centerHeight, pBase.centerHeight + bndTy, 1e-9), "center height shifts exactly by Ty at large bounded coordinates");
    for (size_t i = 0; i < 4; ++i) {
        check(near(pBnd.residuals[i], pBase.residuals[i], 1e-10), "residuals invariant under large bounded translation");
    }
}

// Group 9: Near-collinear layout and narrow stance track
static void testNearCollinearAndNarrowTrack() {
    // Narrow footprint: length 1.6m, lateral track width 0.04m (x in [-0.02, 0.02])
    // Cross-slope of 0.06m height difference across 0.04m track width => slope = 1.5 (~56.3 deg > 20 deg)
    std::array<FootSample, 4> narrowSamples = {{
        {0.02, 0.03, 0.8},
        {0.02, 0.03, -0.8},
        {-0.02, -0.03, 0.8},
        {-0.02, -0.03, -0.8}
    }};
    const double yaw = 0.25;
    auto p = calculateFootSupportPlane(narrowSamples, yaw);
    // Must explicitly bound unsupported steep data rather than inventing fitted success
    check(!p.supported, "narrow layout with steep cross-slope must be unsupported");
    check(p.slopeAngleRadians > kMaxSlopeAngleRadians, "slope angle must exceed 20 degrees");
    check(near(p.up.x, 0.0) && near(p.up.y, 1.0) && near(p.up.z, 0.0), "fallback up vector must be vertical flat");
    check(near(p.forward.x, std::sin(yaw)) && near(p.forward.y, 0.0) && near(p.forward.z, std::cos(yaw)), "fallback forward must preserve flat yaw");
    check(near(p.right.x, std::cos(yaw)) && near(p.right.y, 0.0) && near(p.right.z, -std::sin(yaw)), "fallback right must preserve flat yaw");
    // Residuals must still be reported against the actual fitted plane
    for (double r : p.residuals) {
        check(near(r, 0.0), "residuals against fitted steep plane must be zero for planar samples");
    }

    // Extremely near-collinear layout below numerical determinant tolerance must be rejected cleanly
    std::array<FootSample, 4> almostCollinear = {{
        {0.0, 0.0, 1.0},
        {1e-9, 0.0, 0.5},
        {-1e-9, 0.0, -0.5},
        {0.0, 0.0, -1.0}
    }};
    rejects([&]() { calculateFootSupportPlane(almostCollinear, 0.0); });
}

// Group 10: Large heights and cliff step
static void testLargeHeightsAndExtremeSlope() {
    // Layout with massive cliff height drop: front feet at 100m, rear feet at 0m across 1m distance
    std::array<FootSample, 4> cliffSamples = {{
        {0.5, 100.0, 0.5},
        {-0.5, 100.0, 0.5},
        {0.5, 0.0, -0.5},
        {-0.5, 0.0, -0.5}
    }};
    auto p = calculateFootSupportPlane(cliffSamples, 0.0);
    check(!p.supported, "cliff step must be unsupported");
    check(p.slopeAngleRadians > kMaxSlopeAngleRadians, "cliff slope angle must exceed 20 deg");
    check(near(p.centerHeight, 50.0), "center height must equal centroid height 50.0");
    check(near(p.up.x, 0.0) && near(p.up.y, 1.0) && near(p.up.z, 0.0), "fallback up vector must be vertical flat");
    // Fitted plane residuals must still be finite and exact
    for (double r : p.residuals) {
        check(near(r, 0.0), "planar cliff step has zero residuals against fitted plane");
    }
}

// Group 11: Four finite distinct points and duplicate rejection
static void testDistinctPointsAndDuplicates() {
    // Point 0 and Point 1 identical in (x, y, z), but Points 2 and 3 distinct
    std::array<FootSample, 4> dupPair = {{
        {1.0, 2.0, 3.0},
        {1.0, 2.0, 3.0},
        {0.0, 1.0, 0.0},
        {-1.0, 0.0, 2.0}
    }};
    rejects([&]() { calculateFootSupportPlane(dupPair, 0.0); });

    // Two points sharing identical (x, z) footprint with different heights (vertical stack)
    std::array<FootSample, 4> verticalStack = {{
        {0.5, 2.0, 0.5},
        {0.5, 4.0, 0.5},
        {-0.5, 1.0, 0.5},
        {0.0, 1.0, -0.5}
    }};
    rejects([&]() { calculateFootSupportPlane(verticalStack, 0.0); });

    // Near-duplicate points within normalized layout tolerance
    std::array<FootSample, 4> nearDup = {{
        {1.0, 1.0, 1.0},
        {1.0 + 1e-12, 1.0, 1.0 + 1e-12},
        {-1.0, 1.0, 1.0},
        {0.0, 1.0, -1.0}
    }};
    rejects([&]() { calculateFootSupportPlane(nearDup, 0.0); });
}

// Group 12: Extreme overflow handling
static void testExtremeOverflow() {
    std::array<FootSample, 4> validSamples = {{
        {1.0, 0.0, 1.0}, {1.0, 0.0, -1.0}, {-1.0, 0.0, 1.0}, {-1.0, 0.0, -1.0}
    }};

    // Extreme coordinate causing quadratic accumulation overflow
    auto hugeY = validSamples;
    hugeY[0].y = 1.0e160;
    rejects([&]() { calculateFootSupportPlane(hugeY, 0.0); });

    auto hugeZ = validSamples;
    hugeZ[2].z = -1.0e160;
    rejects([&]() { calculateFootSupportPlane(hugeZ, 0.0); });
}

int main() {
    try {
        const std::array<FootSample, 4> diagonalSlope = {{{-1, 2.7, -1}, {1, 2.9, -1}, {-1, 3.1, 1}, {1, 3.3, 1}}};
        const auto bearing = calculateFootSupportPlane(diagonalSlope, 0.37);
        check(bearing.supported, "diagonal slope supported");
        check(near(std::atan2(bearing.forward.x, bearing.forward.z), 0.37, 1e-12), "horizontal heading preserved on diagonal slope");
        check(near(dot(bearing.forward, bearing.up), 0.0, 1e-12), "bearing tangent to terrain plane");
        std::cout << "PASS HorizontalHeading\n";
        testTinyInputs();
        std::cout << "PASS TinyInputs\n";
        testFlatPlane();
        std::cout << "PASS FlatPlane\n";
        testKnownSlope();
        std::cout << "PASS KnownSlope\n";
        testUnevenGroundResiduals();
        std::cout << "PASS UnevenGroundResiduals\n";
        testExtremeSlopeFallback();
        std::cout << "PASS ExtremeSlopeFallback\n";
        testDegenerateCollinear();
        std::cout << "PASS DegenerateCollinear\n";
        testInvalidInputs();
        std::cout << "PASS InvalidInputs\n";
        testTranslatedCopies();
        std::cout << "PASS TranslatedCopies\n";
        testNearCollinearAndNarrowTrack();
        std::cout << "PASS NearCollinearAndNarrowTrack\n";
        testLargeHeightsAndExtremeSlope();
        std::cout << "PASS LargeHeightsAndExtremeSlope\n";
        testDistinctPointsAndDuplicates();
        std::cout << "PASS DistinctPointsAndDuplicates\n";
        testExtremeOverflow();
        std::cout << "PASS ExtremeOverflow\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
