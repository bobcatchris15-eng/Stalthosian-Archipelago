#include "../engine/foot_terrain_diagnostic.hpp"
#include "../domain/reference_landscape.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stalthos::engine;
using namespace stalthos::domain;

static bool near(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) < eps;
}

// Release-active check function (active regardless of NDEBUG)
static void check(bool condition, const std::string& msg) {
    if (!condition) {
        std::cerr << "FAIL: " << msg << std::endl;
        throw std::runtime_error(msg);
    }
}

// 1. Verify four distinct foot identities, Blender bone names, and authored selectors.
static void testFootIdentityAndMappings() {
    check(kFootCount == 4, "foot count must be 4");

    check(std::string(footIdToString(FootId::FrontLeft)) == "front_left", "front_left string mismatch");
    check(std::string(footIdToString(FootId::FrontRight)) == "front_right", "front_right string mismatch");
    check(std::string(footIdToString(FootId::RearLeft)) == "rear_left", "rear_left string mismatch");
    check(std::string(footIdToString(FootId::RearRight)) == "rear_right", "rear_right string mismatch");

    check(std::string(footIdToBlenderBoneName(FootId::FrontLeft)) == "toe_front_L", "front_left bone mismatch");
    check(std::string(footIdToBlenderBoneName(FootId::FrontRight)) == "toe_front_R", "front_right bone mismatch");
    check(std::string(footIdToBlenderBoneName(FootId::RearLeft)) == "toe_rear_L", "rear_left bone mismatch");
    check(std::string(footIdToBlenderBoneName(FootId::RearRight)) == "toe_rear_R", "rear_right bone mismatch");

    check(std::string(footIdToToolkitSelector(FootId::FrontLeft)) == "diagnostic:manus_hand_l", "front_left selector mismatch");
    check(std::string(footIdToToolkitSelector(FootId::FrontRight)) == "diagnostic:manus_hand_r", "front_right selector mismatch");
    check(std::string(footIdToToolkitSelector(FootId::RearLeft)) == "bone:pes_foot_l", "rear_left selector mismatch");
    check(std::string(footIdToToolkitSelector(FootId::RearRight)) == "bone:pes_foot_r", "rear_right selector mismatch");

    check(isFrontFoot(FootId::FrontLeft) && isFrontFoot(FootId::FrontRight), "front foot check failed");
    check(!isFrontFoot(FootId::RearLeft) && !isFrontFoot(FootId::RearRight), "rear foot misidentified as front");
    check(isHindFoot(FootId::RearLeft) && isHindFoot(FootId::RearRight), "hind foot check failed");
    check(!isHindFoot(FootId::FrontLeft) && !isHindFoot(FootId::FrontRight), "front foot misidentified as hind");

    check(parseFootId("toe_front_L") == FootId::FrontLeft, "parse toe_front_L failed");
    check(parseFootId("toe_front_R") == FootId::FrontRight, "parse toe_front_R failed");
    check(parseFootId("toe_rear_L") == FootId::RearLeft, "parse toe_rear_L failed");
    check(parseFootId("toe_rear_R") == FootId::RearRight, "parse toe_rear_R failed");
    check(!parseFootId("nonexistent_bone").has_value(), "parse invalid should return nullopt");
}

// 2. Pure math matrix and point transform cases.
static void testMatrix4x4Transform() {
    auto id = Matrix4x4::identity();
    Vector3 p{1.0, 2.0, 3.0};
    auto tp = id.transformPoint(p);
    check(near(tp.x, 1.0) && near(tp.y, 2.0) && near(tp.z, 3.0), "identity transform failed");

    auto tr = Matrix4x4::translation(10.0, -5.0, 20.0);
    auto ttr = tr.transformPoint(p);
    check(near(ttr.x, 11.0) && near(ttr.y, -3.0) && near(ttr.z, 23.0), "translation transform failed");

    const double pi = 3.14159265358979323846;
    auto rot = Matrix4x4::rotationY(pi * 0.5); // +90 deg around Y: (1, 0, 0) -> (0, 0, -1) in LH
    Vector3 px{1.0, 0.0, 0.0};
    auto rpx = rot.transformPoint(px);
    check(near(rpx.x, 0.0) && near(rpx.y, 0.0) && near(rpx.z, -1.0), "rotation Y failed");

    // Compose rotation and translation
    auto comp = Matrix4x4::multiply(rot, tr);
    auto pcomp = comp.transformPoint(px);
    check(near(pcomp.x, 10.0) && near(pcomp.y, -5.0) && near(pcomp.z, 19.0), "composite transform failed");
}

// 3. Flat terrain clearance (geometric threshold classifications).
static void testFlatTerrainClearance() {
    auto flatGround = [](double, double) { return 2.0; };

    // Case A: All 4 feet planted exactly at y = 2.0
    std::array<Vector3, 4> plantedFeet = {{
        {-0.25, 2.0,  0.5}, // FrontLeft
        { 0.25, 2.0,  0.5}, // FrontRight
        {-0.30, 2.0, -0.5}, // RearLeft
        { 0.30, 2.0, -0.5}  // RearRight
    }};

    auto repA = evaluateFourFeetPositions(
        plantedFeet, MeasurementMethod::SolePadSurface, 0.02, flatGround);

    check(repA.allValid, "planted feet should be valid");
    check(!repA.anyPenetrating, "planted feet should not be penetrating");
    check(near(repA.maxPenetrationMeters, 0.0), "max penetration should be 0");
    check(near(repA.minClearanceMeters, 0.0), "min clearance should be 0");
    check(near(repA.maxClearanceMeters, 0.0), "max clearance should be 0");
    check(near(repA.meanClearanceMeters, 0.0), "mean clearance should be 0");
    for (size_t i = 0; i < 4; ++i) {
        check(repA.feet[i].inStanceContact, "foot should meet geometric stance threshold");
        check(!repA.feet[i].inSwing, "foot should not meet swing threshold");
        check(!repA.feet[i].penetrating, "foot should not be penetrating");
    }

    // Case B: Feet elevated geometrically (e.g. y = 2.10 m)
    std::array<Vector3, 4> swingFeet = plantedFeet;
    swingFeet[0].y = 2.10; // FrontLeft elevated
    swingFeet[3].y = 2.08; // RearRight elevated

    auto repB = evaluateFourFeetPositions(
        swingFeet, MeasurementMethod::SolePadSurface, 0.02, flatGround);

    check(repB.allValid, "swing feet should be valid");
    check(!repB.anyPenetrating, "no penetration in elevated feet");
    check(repB.feet[0].inSwing, "FrontLeft should meet swing threshold");
    check(!repB.feet[0].inStanceContact, "FrontLeft should not meet stance contact threshold");
    check(near(repB.feet[0].clearanceMeters, 0.10), "FrontLeft clearance should be 0.10");
    check(repB.feet[1].inStanceContact, "FrontRight should meet stance contact threshold");
    check(repB.feet[2].inStanceContact, "RearLeft should meet stance contact threshold");
    check(repB.feet[3].inSwing, "RearRight should meet swing threshold");

    // Case C: Feet penetrating into terrain (e.g. y = 1.95 m and y = 1.90 m)
    std::array<Vector3, 4> penetratingFeet = plantedFeet;
    penetratingFeet[1].y = 1.95; // 0.05m below ground -> exceeds 0.02m tolerance -> penetrating
    penetratingFeet[2].y = 1.90; // 0.10m below ground -> penetrating

    auto repC = evaluateFourFeetPositions(
        penetratingFeet, MeasurementMethod::SolePadSurface, 0.02, flatGround);

    check(repC.allValid, "penetrating feet should be valid queries");
    check(repC.anyPenetrating, "anyPenetrating should be true");
    check(repC.feet[1].penetrating, "FrontRight should be marked penetrating");
    check(near(repC.feet[1].penetrationDepthMeters, 0.05), "FrontRight penetration depth mismatch");
    check(repC.feet[2].penetrating, "RearLeft should be marked penetrating");
    check(near(repC.feet[2].penetrationDepthMeters, 0.10), "RearLeft penetration depth mismatch");
    check(near(repC.maxPenetrationMeters, 0.10), "max penetration should be 0.10");
}

// 4. Sloped terrain plane clearance.
static void testSlopedTerrainClearance() {
    auto slopedGround = [](double x, double z) {
        return 0.1 * x + 0.05 * z + 3.0;
    };

    // Evaluate feet positioned on an inclined plane
    std::array<Vector3, 4> feet = {{
        { 2.0, slopedGround(2.0,  1.0),  1.0}, // FrontLeft exactly on slope
        { 3.0, slopedGround(3.0,  1.0),  1.0}, // FrontRight exactly on slope
        { 2.0, slopedGround(2.0, -1.0), -1.0}, // RearLeft exactly on slope
        { 3.0, slopedGround(3.0, -1.0), -1.0}  // RearRight exactly on slope
    }};

    auto report = evaluateFourFeetPositions(
        feet, MeasurementMethod::SolePadSurface, 0.02, slopedGround);

    check(report.allValid, "sloped feet should be valid");
    check(!report.anyPenetrating, "exact slope feet should not penetrate");
    for (size_t i = 0; i < 4; ++i) {
        check(near(report.feet[i].clearanceMeters, 0.0), "clearance on slope should be 0");
        check(report.feet[i].inStanceContact, "foot on slope should meet stance contact threshold");
    }

    // Now test a horizontally flat foot placement over the slope (unaligned creature)
    std::array<Vector3, 4> unalignedFeet = feet;
    const double meanY = (feet[0].y + feet[1].y + feet[2].y + feet[3].y) * 0.25;
    for (size_t i = 0; i < 4; ++i) unalignedFeet[i].y = meanY;

    auto unalignedReport = evaluateFourFeetPositions(
        unalignedFeet, MeasurementMethod::SolePadSurface, 0.02, slopedGround);

    check(unalignedReport.allValid, "unaligned query should be valid");
    check(unalignedReport.anyPenetrating, "unaligned body over slope must penetrate downhill/uphill");
    check(unalignedReport.minClearanceMeters < 0.0, "min clearance must be negative");
    check(unalignedReport.maxClearanceMeters > 0.0, "max clearance must be positive");
}

// 5. Authoritative ReferenceLandscape sampling.
static void testAuthoritativeReferenceLandscape() {
    ReferenceLandscape landscape(1);

    const double cx = 0.0;
    const double cz = 0.0;
    check(landscape.inBounds(cx, cz), "center is in bounds");
    const double ch = landscape.height(cx, cz);
    check(std::isfinite(ch), "center height must be finite");

    std::array<Vector3, 4> feet = {{
        {cx - 0.25, landscape.height(cx - 0.25, cz + 0.50), cz + 0.50},
        {cx + 0.25, landscape.height(cx + 0.25, cz + 0.50), cz + 0.50},
        {cx - 0.30, landscape.height(cx - 0.30, cz - 0.50), cz - 0.50},
        {cx + 0.30, landscape.height(cx + 0.30, cz - 0.50), cz - 0.50}
    }};

    auto report = evaluateFourFeetOnLandscape(
        feet, MeasurementMethod::SolePadSurface, 0.02, landscape);

    check(report.allValid, "landscape query should be valid");
    check(!report.anyPenetrating, "exact sampled feet should not penetrate");
    for (size_t i = 0; i < 4; ++i) {
        check(near(report.feet[i].clearanceMeters, 0.0, 1e-9), "clearance should match landscape height");
        check(report.feet[i].inStanceContact, "foot should meet stance contact threshold");
    }
}

// 6. Out-of-bounds, non-finite, and throwing callback error cases.
static void testOutOfBoundsAndInvalidCoordinates() {
    ReferenceLandscape landscape(1);

    // Coordinates outside the 192 m patch (|x| > 96 or |z| > 96)
    std::array<Vector3, 4> outOfBoundsFeet = {{
        { 100.0, 2.0,   0.0}, // FrontLeft out of bounds
        {   0.0, 2.0,   0.0},
        {   0.0, 2.0,   0.0},
        {   0.0, 2.0,   0.0}
    }};

    auto repOob = evaluateFourFeetOnLandscape(
        outOfBoundsFeet, MeasurementMethod::SolePadSurface, 0.02, landscape);

    check(!repOob.allValid, "allValid should be false for out-of-bounds query");
    check(!repOob.feet[0].valid, "out of bounds foot must be invalid");
    check(!repOob.feet[0].inBounds, "foot should be marked out of bounds");
    check(!repOob.feet[0].issue.empty(), "issue message must be set");
    check(repOob.feet[1].valid, "in-bounds foot should be valid");

    // Non-finite coordinates (NaN and Inf)
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    std::array<Vector3, 4> nonFiniteFeet = {{
        {qnan, 2.0,  0.0},
        { 0.0, inf,  0.0},
        { 0.0, 2.0, -inf},
        { 0.0, 2.0,  0.0}
    }};

    auto repNf = evaluateFourFeetPositions(
        nonFiniteFeet, MeasurementMethod::SolePadSurface, 0.02,
        [](double, double) { return 0.0; });

    check(!repNf.allValid, "non-finite query should be invalid");
    check(!repNf.feet[0].valid && !repNf.feet[0].isFinite, "NaN position should be flagged non-finite");
    check(!repNf.feet[1].valid && !repNf.feet[1].isFinite, "Inf Y should be flagged non-finite");
    check(!repNf.feet[2].valid && !repNf.feet[2].isFinite, "-Inf Z should be flagged non-finite");
    check(repNf.feet[3].valid, "finite foot should be valid");

    // Throwing height callback
    auto throwingCallback = [](double, double) -> double {
        throw std::runtime_error("simulated height query crash");
    };

    std::array<Vector3, 4> regularFeet = {{
        {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 1.0}, {1.0, 1.0, 1.0}
    }};

    auto repThrow = evaluateFourFeetPositions(
        regularFeet, MeasurementMethod::SolePadSurface, 0.02, throwingCallback);

    check(!repThrow.allValid, "throwing callback should set allValid=false");
    for (size_t i = 0; i < 4; ++i) {
        check(!repThrow.feet[i].valid, "foot must be marked invalid on thrown callback");
        check(repThrow.feet[i].issue.find("simulated height query crash") != std::string::npos,
              "issue must contain exception description");
    }
}

// 7. Distinction between SolePadSurface and BoneProxy methods, and limitations reporting.
static void testSolePadSurfaceVsBoneProxyDistinction() {
    auto flatGround = [](double, double) { return 0.0; };

    // Bone proxy: Joint located at y = 0.02 m; with 0.02 m offset, pad contacts at y = 0.0 m
    std::array<FootBoneProxyInput, 4> boneInputs = {{
        {FootId::FrontLeft,  {-0.25, 0.02,  0.5}, 0.02},
        {FootId::FrontRight, { 0.25, 0.02,  0.5}, 0.02},
        {FootId::RearLeft,   {-0.30, 0.02, -0.5}, 0.02},
        {FootId::RearRight,  { 0.30, 0.02, -0.5}, 0.02}
    }};

    auto boneReport = evaluateFourFeetFromBoneProxies(boneInputs, 0.02, flatGround);

    check(boneReport.allValid, "bone proxy report should be valid");
    check(boneReport.overallMethod == MeasurementMethod::BoneProxy, "overall method must be BoneProxy");
    check(!boneReport.limitations.empty(), "bone proxy limitations must be non-empty");
    check(!boneReport.footContactAccepted, "footContactAccepted must strictly be false");

    for (size_t i = 0; i < 4; ++i) {
        check(boneReport.feet[i].method == MeasurementMethod::BoneProxy, "sample method must be BoneProxy");
        check(near(boneReport.feet[i].worldPosition.y, 0.0), "pad world Y should be joint Y - pad offset");
        check(near(boneReport.feet[i].clearanceMeters, 0.0), "pad clearance should be 0");
        check(boneReport.feet[i].inStanceContact, "pad should meet stance contact threshold");
    }

    // Skinned vertex cloud: multiple vertices per foot pad
    std::array<FootPadVertexCloud, 4> clouds;
    for (size_t i = 0; i < 4; ++i) {
        clouds[i].foot = kAllFootIds[i];
        // 3 vertices: top of toe (0.05), middle (0.02), sole pad bottom (0.00)
        clouds[i].worldVertices = {
            {0.0, 0.05, 0.0},
            {0.0, 0.02, 0.0},
            {0.0, 0.00, 0.0}
        };
    }

    auto cloudReport = evaluateFourFeetFromVertexClouds(clouds, 0.02, flatGround);

    check(cloudReport.allValid, "cloud report should be valid");
    check(cloudReport.overallMethod == MeasurementMethod::SolePadSurface, "method must be SolePadSurface");
    check(!cloudReport.footContactAccepted, "footContactAccepted must strictly be false");
    for (size_t i = 0; i < 4; ++i) {
        check(cloudReport.feet[i].vertexCount == 3, "vertex count must match 3");
        check(near(cloudReport.feet[i].clearanceMeters, 0.0), "lowest vertex clearance must be 0");
        check(cloudReport.feet[i].inStanceContact, "lowest vertex must meet stance contact threshold");
    }
}

// 8. Wicked landmark spec constants and creature transform helper.
static void testWickedLandmarkAndTransformHelper() {
    check(kWickedFootLandmarks.size() == 4, "landmark count must be 4");
    check(std::string(kWickedFootLandmarks[0].boneName) == "toe_front_L", "front left bone name mismatch");
    check(std::string(kWickedFootLandmarks[1].boneName) == "toe_front_R", "front right bone name mismatch");
    check(std::string(kWickedFootLandmarks[2].boneName) == "toe_rear_L", "rear left bone name mismatch");
    check(std::string(kWickedFootLandmarks[3].boneName) == "toe_rear_R", "rear right bone name mismatch");

    Vector3 localToe{0.5, 0.0, 0.0}; // Along +X in Blender rig
    Vector3 rootPos{10.0, 2.0, 20.0};
    // With yaw = 0, asset offset (-pi/2) turns +X into +Z (DirectX host forward)
    auto worldToe = computeCreatureWorldPoint(localToe, rootPos, 0.0, 0.0);
    check(near(worldToe.x, 10.0), "worldToe X mismatch");
    check(near(worldToe.y, 2.0), "worldToe Y mismatch");
    check(near(worldToe.z, 20.5), "worldToe Z mismatch: Blender +X must map to Wicked +Z");
}

// 9. Regression tests for Parent Review hardening findings.
static void testRegressionParentReviewHardening() {
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    auto flatZero = [](double, double) { return 0.0; };

    // A. evaluateSinglePoint marks valid even clearance subtraction overflows:
    // worldPos.y = 1e308 and h = -1e308 produces overflow to +inf
    {
        auto overflowSample = evaluateSinglePoint(
            FootId::FrontLeft,
            {0.0, 1e308, 0.0},
            MeasurementMethod::SolePadSurface,
            0.02,
            [](double, double) { return -1e308; }
        );
        check(!overflowSample.valid, "subtraction overflow must mark sample invalid");
        check(overflowSample.issue == "clearance subtraction non-finite or overflow",
              "issue must identify clearance subtraction overflow");
    }

    // B. Negative and nonfinite tolerance rejected with explicit typed issue:
    {
        auto negTol = evaluateSinglePoint(
            FootId::FrontLeft, {0.0, 0.0, 0.0}, MeasurementMethod::SolePadSurface, -0.01, flatZero);
        check(!negTol.valid, "negative tolerance must be rejected");
        check(negTol.issue == "contact tolerance must be finite and non-negative",
              "issue must identify negative tolerance");

        auto nanTol = evaluateSinglePoint(
            FootId::FrontLeft, {0.0, 0.0, 0.0}, MeasurementMethod::SolePadSurface, qnan, flatZero);
        check(!nanTol.valid, "NaN tolerance must be rejected");

        auto infTol = evaluateSinglePoint(
            FootId::FrontLeft, {0.0, 0.0, 0.0}, MeasurementMethod::SolePadSurface, inf, flatZero);
        check(!infTol.valid, "Inf tolerance must be rejected");

        std::array<Vector3, 4> dummyPositions = {{{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}}};
        auto repNegTol = evaluateFourFeetPositions(
            dummyPositions, MeasurementMethod::SolePadSurface, -0.05, flatZero);
        check(!repNegTol.allValid, "negative tolerance must make report invalid");
    }

    // C. Unresolved/invalid enum treated as measured:
    {
        auto unresSample = evaluateSinglePoint(
            FootId::FrontLeft, {0.0, 0.0, 0.0}, MeasurementMethod::Unresolved, 0.02, flatZero);
        check(!unresSample.valid, "Unresolved method must be rejected as invalid");
        check(unresSample.issue == "unresolved or invalid measurement method",
              "issue must identify unresolved method");

        auto bogusMethod = evaluateSinglePoint(
            FootId::FrontLeft, {0.0, 0.0, 0.0}, static_cast<MeasurementMethod>(99), 0.02, flatZero);
        check(!bogusMethod.valid, "bogus method enum must be rejected as invalid");

        auto bogusFoot = evaluateSinglePoint(
            static_cast<FootId>(42), {0.0, 0.0, 0.0}, MeasurementMethod::SolePadSurface, 0.02, flatZero);
        check(!bogusFoot.valid, "bogus foot enum must be rejected as invalid");
        check(bogusFoot.issue == "invalid foot identity", "issue must identify invalid foot ID");
    }

    // D. boneInputs/clouds foot identity ignored so duplicates mislabeled:
    {
        // Duplicate FrontLeft in boneInputs
        std::array<FootBoneProxyInput, 4> dupBoneInputs = {{
            {FootId::FrontLeft, {0.0, 0.02, 0.0}, 0.02},
            {FootId::FrontLeft, {0.0, 0.02, 0.0}, 0.02}, // duplicate!
            {FootId::RearLeft,  {0.0, 0.02, 0.0}, 0.02},
            {FootId::RearRight, {0.0, 0.02, 0.0}, 0.02}
        }};
        auto dupBoneRep = evaluateFourFeetFromBoneProxies(dupBoneInputs, 0.02, flatZero);
        check(!dupBoneRep.allValid, "duplicate foot in bone inputs must reject report");
        check(dupBoneRep.feet[0].issue == "duplicate foot identity in bone proxy inputs",
              "typed issue for duplicate bone input");

        // Duplicate RearLeft in vertex clouds
        std::array<FootPadVertexCloud, 4> dupClouds;
        dupClouds[0] = {FootId::FrontLeft,  {{0.0, 0.0, 0.0}}};
        dupClouds[1] = {FootId::FrontRight, {{0.0, 0.0, 0.0}}};
        dupClouds[2] = {FootId::RearLeft,   {{0.0, 0.0, 0.0}}};
        dupClouds[3] = {FootId::RearLeft,   {{0.0, 0.0, 0.0}}}; // duplicate!
        auto dupCloudRep = evaluateFourFeetFromVertexClouds(dupClouds, 0.02, flatZero);
        check(!dupCloudRep.allValid, "duplicate foot in clouds must reject report");
        check(dupCloudRep.feet[0].issue == "duplicate foot identity in vertex clouds",
              "typed issue for duplicate cloud input");

        // Out-of-order permutation of 4 distinct feet should map correctly to their canonical identities
        std::array<FootBoneProxyInput, 4> permutedInputs = {{
            {FootId::RearRight,  {1.0, 0.02, 1.0}, 0.02},
            {FootId::FrontLeft,  {2.0, 0.02, 2.0}, 0.02},
            {FootId::RearLeft,   {3.0, 0.02, 3.0}, 0.02},
            {FootId::FrontRight, {4.0, 0.02, 4.0}, 0.02}
        }};
        auto permRep = evaluateFourFeetFromBoneProxies(permutedInputs, 0.02, flatZero);
        check(permRep.allValid, "permuted valid inputs must all be valid");
        check(permRep.get(FootId::FrontLeft).worldPosition.x == 2.0, "FrontLeft mapped correctly");
        check(permRep.get(FootId::FrontRight).worldPosition.x == 4.0, "FrontRight mapped correctly");
        check(permRep.get(FootId::RearLeft).worldPosition.x == 3.0, "RearLeft mapped correctly");
        check(permRep.get(FootId::RearRight).worldPosition.x == 1.0, "RearRight mapped correctly");
    }

    // E. Negative/nonfinite pad offset unchecked:
    {
        std::array<FootBoneProxyInput, 4> negOffsetInputs = {{
            {FootId::FrontLeft,  {0.0, 0.02, 0.0}, -0.05}, // negative pad offset!
            {FootId::FrontRight, {0.0, 0.02, 0.0}, 0.02},
            {FootId::RearLeft,   {0.0, 0.02, 0.0}, 0.02},
            {FootId::RearRight,  {0.0, 0.02, 0.0}, 0.02}
        }};
        auto negRep = evaluateFourFeetFromBoneProxies(negOffsetInputs, 0.02, flatZero);
        check(!negRep.allValid, "negative pad offset must mark report invalid");
        check(!negRep.get(FootId::FrontLeft).valid, "FrontLeft with negative offset must be invalid");
        check(negRep.get(FootId::FrontLeft).issue == "negative or non-finite vertical pad offset",
              "issue must identify negative pad offset");
        check(negRep.get(FootId::FrontRight).valid, "other valid feet remain valid");

        // Nonfinite pad offset (NaN)
        negOffsetInputs[0].verticalPadOffsetMeters = qnan;
        auto nanOffsetRep = evaluateFourFeetFromBoneProxies(negOffsetInputs, 0.02, flatZero);
        check(!nanOffsetRep.allValid, "NaN pad offset must mark report invalid");
        check(!nanOffsetRep.get(FootId::FrontLeft).valid, "FrontLeft with NaN offset must be invalid");
    }

    // F. vertexcloud accepts partially invalid vertices and reports allValid:
    {
        ReferenceLandscape landscape(1);
        std::array<FootPadVertexCloud, 4> clouds;
        for (size_t i = 0; i < 4; ++i) {
            clouds[i].foot = kAllFootIds[i];
            clouds[i].worldVertices = {{0.0, 2.0, 0.0}};
        }
        // Add 1 invalid vertex (out of bounds) to FrontLeft's cloud alongside valid ones
        clouds[0].worldVertices = {
            {0.0, 2.0, 0.0},     // valid
            {200.0, 2.0, 0.0},   // OUT OF BOUNDS!
            {0.0, 2.0, 0.0}      // valid
        };
        auto partialRep = evaluateFourFeetOnLandscapeFromVertexClouds(clouds, 0.02, landscape);
        check(!partialRep.allValid, "cloud with partially invalid vertex must NOT report allValid");
        check(!partialRep.get(FootId::FrontLeft).valid, "FrontLeft must be marked invalid");
        check(partialRep.get(FootId::FrontLeft).issue.find("vertex cloud contains invalid vertex") != std::string::npos,
              "issue must specify invalid vertex in cloud");
        check(partialRep.get(FootId::FrontRight).valid, "FrontRight cloud without invalid vertex remains valid");

        // Test with NaN vertex in cloud
        clouds[0].worldVertices = {
            {0.0, 2.0, 0.0},
            {qnan, 2.0, 0.0}
        };
        auto nanCloudRep = evaluateFourFeetFromVertexClouds(clouds, 0.02, flatZero);
        check(!nanCloudRep.allValid, "cloud with NaN vertex must NOT report allValid");
        check(!nanCloudRep.get(FootId::FrontLeft).valid, "FrontLeft with NaN vertex must be invalid");
    }

    // G. Bounded cloud size: cap at 4096 vertices per foot with typed invalid result:
    {
        std::array<FootPadVertexCloud, 4> clouds;
        for (size_t i = 0; i < 4; ++i) {
            clouds[i].foot = kAllFootIds[i];
            clouds[i].worldVertices = {{0.0, 0.0, 0.0}};
        }

        // Test 4096 vertices (exactly at cap)
        clouds[0].worldVertices.assign(4096, Vector3{0.0, 0.0, 0.0});
        auto atCapRep = evaluateFourFeetFromVertexClouds(clouds, 0.02, flatZero);
        check(atCapRep.allValid, "cloud with exactly 4096 vertices must be valid");
        check(atCapRep.get(FootId::FrontLeft).vertexCount == 4096, "vertex count must report 4096");

        // Test 4097 vertices (exceeds cap)
        clouds[0].worldVertices.assign(4097, Vector3{0.0, 0.0, 0.0});
        auto overCapRep = evaluateFourFeetFromVertexClouds(clouds, 0.02, flatZero);
        check(!overCapRep.allValid, "cloud exceeding 4096 vertices must be invalid");
        check(!overCapRep.get(FootId::FrontLeft).valid, "FrontLeft must be marked invalid");
        check(overCapRep.get(FootId::FrontLeft).issue == "vertex cloud exceeds maximum capacity of 4096 vertices per foot",
              "issue must identify vertex cloud capacity exceeded");
        check(overCapRep.get(FootId::FrontLeft).vertexCount == 4097, "vertex count should record 4097");

        // Test empty cloud
        clouds[0].worldVertices.clear();
        auto emptyRep = evaluateFourFeetFromVertexClouds(clouds, 0.02, flatZero);
        check(!emptyRep.allValid, "empty cloud must be invalid");
        check(!emptyRep.get(FootId::FrontLeft).valid, "FrontLeft empty must be invalid");
        check(emptyRep.get(FootId::FrontLeft).issue == "empty vertex cloud for foot pad",
              "issue must identify empty vertex cloud");
    }

    // H. Aggregate means avoid finite-sum overflow:
    {
        // 4 feet with very large finite clearances (1e308 each).
        // 4 * 1e308 = 4e308 > DBL_MAX (~1.79e308), so standard sum overflows to +inf.
        // Incremental mean computation preserves 1e308 without overflowing.
        std::array<Vector3, 4> hugeFeet = {{
            {0.0, 1e308, 0.0},
            {0.0, 1e308, 0.0},
            {0.0, 1e308, 0.0},
            {0.0, 1e308, 0.0}
        }};
        auto hugeRep = evaluateFourFeetPositions(
            hugeFeet, MeasurementMethod::SolePadSurface, 0.02, flatZero);
        check(hugeRep.allValid, "huge valid feet queries should be valid");
        check(std::isfinite(hugeRep.meanClearanceMeters), "mean clearance must remain finite without sum overflow");
        check(near(hugeRep.meanClearanceMeters, 1e308, 1e295), "mean clearance should equal 1e308");
    }

    // I. Standalone callbacks overflow handling:
    {
        // Standalone callback called with large finite coordinates
        auto crashProneCallback = [](double x, double) -> double {
            // Callback that might return huge or non-finite numbers
            return (x > 1e100) ? -1e308 : 0.0;
        };
        auto standaloneSample = evaluateSinglePoint(
            FootId::FrontLeft,
            {1e200, 1e308, 0.0},
            MeasurementMethod::SolePadSurface,
            0.02,
            crashProneCallback
        );
        check(!standaloneSample.valid, "clearance subtraction overflow with standalone callback must be invalid");
        check(standaloneSample.issue == "clearance subtraction non-finite or overflow",
              "issue must identify clearance subtraction overflow");
    }
}

int main() {
    try {
        std::cout << "Running foot terrain diagnostic tests..." << std::endl;
        testFootIdentityAndMappings();
        testMatrix4x4Transform();
        testFlatTerrainClearance();
        testSlopedTerrainClearance();
        testAuthoritativeReferenceLandscape();
        testOutOfBoundsAndInvalidCoordinates();
        testSolePadSurfaceVsBoneProxyDistinction();
        testWickedLandmarkAndTransformHelper();
        testRegressionParentReviewHardening();
        check(std::isfinite(finiteMeanStep(1e308, -1e308, 2)), "opposite-sign finite means must not overflow");
        check(near(finiteMeanStep(1e308, -1e308, 2), 0.0), "opposite-sign mean cancels");
        std::cout << "All 9 foot terrain diagnostic tests (including hardening regressions) PASSED successfully." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
