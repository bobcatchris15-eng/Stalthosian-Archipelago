#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "../domain/reference_landscape.hpp"

namespace stalthos::engine {

struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    bool isFinite() const noexcept {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
};

inline double dot(const Vector3& a, const Vector3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline double distanceSq(const Vector3& a, const Vector3& b) noexcept {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

inline double distance(const Vector3& a, const Vector3& b) noexcept {
    return std::sqrt(distanceSq(a, b));
}

// 4x4 matrix for local-to-world bone and landmark transformations.
// Uses DirectX / Left-Handed row-vector convention (v_world = v_local * M).
struct Matrix4x4 {
    double m[4][4] = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };

    static constexpr Matrix4x4 identity() noexcept {
        return Matrix4x4{};
    }

    static Matrix4x4 translation(double tx, double ty, double tz) noexcept {
        Matrix4x4 res = identity();
        res.m[3][0] = tx;
        res.m[3][1] = ty;
        res.m[3][2] = tz;
        return res;
    }

    static Matrix4x4 rotationY(double radians) noexcept {
        Matrix4x4 res = identity();
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        res.m[0][0] = c;
        res.m[0][2] = -s;
        res.m[2][0] = s;
        res.m[2][2] = c;
        return res;
    }

    static Matrix4x4 basis(const Vector3& right, const Vector3& up, const Vector3& forward) noexcept {
        Matrix4x4 res = identity();
        res.m[0][0] = right.x;   res.m[0][1] = right.y;   res.m[0][2] = right.z;
        res.m[1][0] = up.x;      res.m[1][1] = up.y;      res.m[1][2] = up.z;
        res.m[2][0] = forward.x; res.m[2][1] = forward.y; res.m[2][2] = forward.z;
        return res;
    }

    static Matrix4x4 multiply(const Matrix4x4& a, const Matrix4x4& b) noexcept {
        Matrix4x4 res{};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                double sum = 0.0;
                for (int k = 0; k < 4; ++k) {
                    sum += a.m[r][k] * b.m[k][c];
                }
                res.m[r][c] = sum;
            }
        }
        return res;
    }

    Vector3 transformPoint(const Vector3& p) const noexcept {
        double x = p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0];
        double y = p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1];
        double z = p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2];
        double w = p.x * m[0][3] + p.y * m[1][3] + p.z * m[2][3] + m[3][3];
        if (w != 0.0 && w != 1.0 && std::isfinite(w)) {
            x /= w;
            y /= w;
            z /= w;
        }
        return {x, y, z};
    }
};

// Four distinct foot identities matching both Blender rig exports and authored selectors.
enum class FootId : std::uint8_t {
    FrontLeft = 0,
    FrontRight = 1,
    RearLeft = 2,
    RearRight = 3
};

constexpr std::size_t kFootCount = 4;
constexpr std::array<FootId, kFootCount> kAllFootIds = {
    FootId::FrontLeft,
    FootId::FrontRight,
    FootId::RearLeft,
    FootId::RearRight
};

// Maximum bounded vertex capacity per foot pad cloud to enforce bounds safety.
constexpr std::size_t kMaxVerticesPerFootPad = 4096;

inline constexpr bool isValidFootId(FootId id) noexcept {
    return id == FootId::FrontLeft || id == FootId::FrontRight ||
           id == FootId::RearLeft || id == FootId::RearRight;
}

inline constexpr const char* footIdToString(FootId id) noexcept {
    switch (id) {
        case FootId::FrontLeft: return "front_left";
        case FootId::FrontRight: return "front_right";
        case FootId::RearLeft: return "rear_left";
        case FootId::RearRight: return "rear_right";
    }
    return "unknown";
}

// Concrete Blender rig export bone name.
inline constexpr const char* footIdToBlenderBoneName(FootId id) noexcept {
    switch (id) {
        case FootId::FrontLeft: return "toe_front_L";
        case FootId::FrontRight: return "toe_front_R";
        case FootId::RearLeft: return "toe_rear_L";
        case FootId::RearRight: return "toe_rear_R";
    }
    return "";
}

// Authoritative toolkit selector name.
inline constexpr const char* footIdToToolkitSelector(FootId id) noexcept {
    switch (id) {
        case FootId::FrontLeft: return "diagnostic:manus_hand_l";
        case FootId::FrontRight: return "diagnostic:manus_hand_r";
        case FootId::RearLeft: return "bone:pes_foot_l";
        case FootId::RearRight: return "bone:pes_foot_r";
    }
    return "";
}

inline constexpr bool isHindFoot(FootId id) noexcept {
    return id == FootId::RearLeft || id == FootId::RearRight;
}

inline constexpr bool isFrontFoot(FootId id) noexcept {
    return id == FootId::FrontLeft || id == FootId::FrontRight;
}

// Parse string identifier to FootId.
inline std::optional<FootId> parseFootId(std::string_view name) noexcept {
    if (name == "front_left" || name == "toe_front_L" || name == "diagnostic:manus_hand_l")
        return FootId::FrontLeft;
    if (name == "front_right" || name == "toe_front_R" || name == "diagnostic:manus_hand_r")
        return FootId::FrontRight;
    if (name == "rear_left" || name == "toe_rear_L" || name == "bone:pes_foot_l")
        return FootId::RearLeft;
    if (name == "rear_right" || name == "toe_rear_R" || name == "bone:pes_foot_r")
        return FootId::RearRight;
    return std::nullopt;
}

// Method used to obtain the sampled world-space contact position.
enum class MeasurementMethod : std::uint8_t {
    Unresolved = 0,
    SolePadSurface = 1, // Skinned mesh surface vertices / contact geometry bounds
    BoneProxy = 2       // Named bone landmark (head/tail) adjusted by vertical pad thickness offset
};

inline constexpr bool isValidMeasurementMethod(MeasurementMethod m) noexcept {
    return m == MeasurementMethod::SolePadSurface || m == MeasurementMethod::BoneProxy;
}

inline constexpr const char* measurementMethodToString(MeasurementMethod m) noexcept {
    switch (m) {
        case MeasurementMethod::SolePadSurface: return "sole_pad_surface";
        case MeasurementMethod::BoneProxy: return "bone_proxy";
        case MeasurementMethod::Unresolved: return "unresolved";
    }
    return "unknown";
}

// Per-foot terrain clearance sample and validation status.
//
// NOTE: Clearance classifications (inStanceContact, inSwing, penetrating) are PURELY
// GEOMETRIC elevation threshold checks relative to contactToleranceMeters.
// They do NOT represent authored walk-cycle gait phases or dynamic contact schedules.
// Bone proxy measurements evaluate rigid joint landmark offsets, NOT actual sole-pad contact geometry.
// Foot clearance evaluation is a diagnostic metric and NEVER establishes foot contact acceptance.
struct FootClearanceSample {
    FootId foot = FootId::FrontLeft;
    MeasurementMethod method = MeasurementMethod::Unresolved;
    Vector3 worldPosition{0.0, 0.0, 0.0};
    double terrainHeight = 0.0;
    double clearanceMeters = 0.0; // worldPosition.y - terrainHeight
    bool inBounds = false;
    bool isFinite = false;
    bool valid = false; // inBounds && isFinite && terrainHeight is finite && clearance is finite
    // Purely geometric threshold flags (NOT authored gait phases or locomotion schedules):
    bool inStanceContact = false; // std::abs(clearanceMeters) <= contactTolerance
    bool inSwing = false;         // clearanceMeters > contactTolerance
    bool penetrating = false;     // clearanceMeters < -contactTolerance
    double penetrationDepthMeters = 0.0; // std::max(0.0, -clearanceMeters)
    std::size_t vertexCount = 0;  // Number of vertices sampled if from pad cloud
    std::string issue;            // Explicit typed diagnostic error if invalid
};

// Input representation for bone-proxy diagnostic.
struct FootBoneProxyInput {
    FootId foot = FootId::FrontLeft;
    Vector3 boneJointWorldPosition{0.0, 0.0, 0.0};
    double verticalPadOffsetMeters = 0.02; // Vertical distance from bone landmark to outer sole surface
};

// Input representation for skinned sole-pad vertex cloud.
struct FootPadVertexCloud {
    FootId foot = FootId::FrontLeft;
    std::vector<Vector3> worldVertices; // Skinned vertex positions in world space (max 4096)
};

// Aggregate four-foot terrain clearance diagnostic report.
struct FourFootDiagnosticReport {
    std::array<FootClearanceSample, kFootCount> feet{};
    bool allValid = false;
    bool anyPenetrating = false;
    double maxPenetrationMeters = 0.0;
    double minClearanceMeters = 0.0;
    double maxClearanceMeters = 0.0;
    double meanClearanceMeters = 0.0;
    double contactToleranceMeters = 0.02; // Authored contact tolerance threshold
    MeasurementMethod overallMethod = MeasurementMethod::Unresolved;
    std::vector<std::string> limitations;
    bool footContactAccepted = false; // Strictly false; kinematic diagnostic only, never foot acceptance

    const FootClearanceSample& get(FootId id) const {
        return feet[static_cast<std::size_t>(id)];
    }
    FootClearanceSample& get(FootId id) {
        return feet[static_cast<std::size_t>(id)];
    }
};

// Standard limitations list for bone-proxy evaluations.
inline std::vector<std::string> defaultBoneProxyLimitations() {
    return {
        "Bone proxy uses rigid toe joint landmarks with estimated vertical pad thickness offset; dynamic CPU skinning deformation is omitted.",
        "Body support orientation tilts the root transform to an approximate terrain plane; individual feet may hover or penetrate local terrain relief.",
        "Front limbs (manus) are kinematic diagnostic only; authored contact schedule and acceptance apply only to hind limbs (pes).",
        "Clearance distance is signed vertical elevation relative to authoritative terrain; horizontal slip and lateral ground collision forces are unmodeled.",
        "Geometric proximity thresholds (inStanceContact / inSwing) are distance metrics, not authored gait cycle phases.",
        "Kinematic diagnostic only; does NOT establish foot contact acceptance or terrain foot-locking."
    };
}

// Standard limitations list for sole-pad surface evaluations.
inline std::vector<std::string> defaultSolePadSurfaceLimitations() {
    return {
        "Sole pad surface evaluates actual skinned contact vertices against authoritative terrain height field.",
        "Body support orientation tilts root transform approximate to terrain; per-foot IK and ground clamping are not active.",
        "Front limbs (manus) are kinematic diagnostic only; authored contact schedule and acceptance apply only to hind limbs (pes).",
        "Clearance is signed vertical distance; lateral friction, soft-substrate compaction, and foot planting dynamics remain unresolved.",
        "Geometric proximity thresholds (inStanceContact / inSwing) are distance metrics, not authored gait cycle phases.",
        "Kinematic diagnostic only; does NOT establish foot contact acceptance or terrain foot-locking."
    };
}

// Stable finite running mean: opposite signs must not subtract huge magnitudes.
inline double finiteMeanStep(double mean, double value, std::size_t count) noexcept {
    const double n = static_cast<double>(count);
    if (std::signbit(mean) != std::signbit(value)) return mean * ((n - 1.0) / n) + value / n;
    return mean + (value - mean) / n;
}

// Evaluate clearance for a single point against a height function.
inline FootClearanceSample evaluateSinglePoint(
    FootId foot,
    const Vector3& worldPos,
    MeasurementMethod method,
    double contactToleranceMeters,
    const std::function<double(double, double)>& heightFunc,
    const std::function<bool(double, double)>& inBoundsFunc = nullptr
) {
    FootClearanceSample sample;
    sample.foot = foot;
    sample.method = method;
    sample.worldPosition = worldPos;
    sample.vertexCount = 1;

    if (!isValidFootId(foot)) {
        sample.valid = false;
        sample.issue = "invalid foot identity";
        return sample;
    }

    if (!isValidMeasurementMethod(method)) {
        sample.valid = false;
        sample.issue = "unresolved or invalid measurement method";
        return sample;
    }

    if (!std::isfinite(contactToleranceMeters) || contactToleranceMeters < 0.0) {
        sample.valid = false;
        sample.issue = "contact tolerance must be finite and non-negative";
        return sample;
    }

    if (!worldPos.isFinite()) {
        sample.isFinite = false;
        sample.inBounds = false;
        sample.valid = false;
        sample.issue = "foot world position coordinates non-finite";
        return sample;
    }
    sample.isFinite = true;

    if (inBoundsFunc) {
        try {
            sample.inBounds = inBoundsFunc(worldPos.x, worldPos.z);
        } catch (const std::exception& e) {
            sample.inBounds = false;
            sample.valid = false;
            sample.issue = std::string("inBounds callback threw: ") + e.what();
            return sample;
        } catch (...) {
            sample.inBounds = false;
            sample.valid = false;
            sample.issue = "inBounds callback threw unknown exception";
            return sample;
        }
        if (!sample.inBounds) {
            sample.valid = false;
            sample.issue = "foot coordinates outside authoritative terrain patch bounds";
            return sample;
        }
    } else {
        sample.inBounds = true;
    }

    if (!heightFunc) {
        sample.valid = false;
        sample.issue = "missing height callback";
        return sample;
    }

    double h = 0.0;
    try {
        h = heightFunc(worldPos.x, worldPos.z);
    } catch (const std::exception& e) {
        sample.valid = false;
        sample.issue = std::string("height callback threw: ") + e.what();
        return sample;
    } catch (...) {
        sample.valid = false;
        sample.issue = "height callback threw unknown exception";
        return sample;
    }

    if (!std::isfinite(h)) {
        sample.valid = false;
        sample.issue = "height callback returned non-finite value";
        return sample;
    }

    sample.terrainHeight = h;

    const double clearance = worldPos.y - h;
    if (!std::isfinite(clearance)) {
        sample.valid = false;
        sample.clearanceMeters = std::numeric_limits<double>::quiet_NaN();
        sample.penetrationDepthMeters = std::numeric_limits<double>::quiet_NaN();
        sample.issue = "clearance subtraction non-finite or overflow";
        return sample;
    }

    sample.clearanceMeters = clearance;
    sample.valid = true;

    // Solely geometric threshold classification against tolerance:
    if (std::abs(clearance) <= contactToleranceMeters) {
        sample.inStanceContact = true;
        sample.inSwing = false;
        sample.penetrating = false;
        sample.penetrationDepthMeters = 0.0;
    } else if (clearance > contactToleranceMeters) {
        sample.inStanceContact = false;
        sample.inSwing = true;
        sample.penetrating = false;
        sample.penetrationDepthMeters = 0.0;
    } else {
        sample.inStanceContact = false;
        sample.inSwing = false;
        sample.penetrating = true;
        const double depth = -clearance;
        sample.penetrationDepthMeters = std::isfinite(depth) ? depth : 0.0;
    }

    return sample;
}

// Evaluate four feet from discrete world-space positions in canonical FootId order.
inline FourFootDiagnosticReport evaluateFourFeetPositions(
    const std::array<Vector3, kFootCount>& worldPositions,
    MeasurementMethod method,
    double contactToleranceMeters,
    const std::function<double(double, double)>& heightFunc,
    const std::function<bool(double, double)>& inBoundsFunc = nullptr
) {
    FourFootDiagnosticReport report;
    report.contactToleranceMeters = contactToleranceMeters;
    report.overallMethod = method;
    report.footContactAccepted = false; // Strictly false

    if (method == MeasurementMethod::BoneProxy) {
        report.limitations = defaultBoneProxyLimitations();
    } else if (method == MeasurementMethod::SolePadSurface) {
        report.limitations = defaultSolePadSurfaceLimitations();
    } else {
        report.limitations.clear();
    }

    const bool toleranceValid = std::isfinite(contactToleranceMeters) && contactToleranceMeters >= 0.0;
    const bool methodValid = isValidMeasurementMethod(method);

    bool allValid = toleranceValid && methodValid;
    bool anyPenetrating = false;
    double maxPen = 0.0;
    double minClear = std::numeric_limits<double>::infinity();
    double maxClear = -std::numeric_limits<double>::infinity();
    double meanClear = 0.0;
    std::size_t validCount = 0;

    for (std::size_t i = 0; i < kFootCount; ++i) {
        const FootId fid = kAllFootIds[i];
        report.feet[i] = evaluateSinglePoint(
            fid, worldPositions[i], method, contactToleranceMeters, heightFunc, inBoundsFunc);

        if (!report.feet[i].valid) {
            allValid = false;
        } else {
            ++validCount;
            // Incremental mean avoids finite-sum overflow
            meanClear = finiteMeanStep(meanClear, report.feet[i].clearanceMeters, validCount);
            minClear = std::min(minClear, report.feet[i].clearanceMeters);
            maxClear = std::max(maxClear, report.feet[i].clearanceMeters);
            if (report.feet[i].penetrating) {
                anyPenetrating = true;
                maxPen = std::max(maxPen, report.feet[i].penetrationDepthMeters);
            }
        }
    }

    report.allValid = allValid && (validCount == kFootCount);
    report.anyPenetrating = anyPenetrating;
    report.maxPenetrationMeters = maxPen;
    if (validCount > 0) {
        report.minClearanceMeters = minClear;
        report.maxClearanceMeters = maxClear;
        report.meanClearanceMeters = meanClear;
    } else {
        report.minClearanceMeters = 0.0;
        report.maxClearanceMeters = 0.0;
        report.meanClearanceMeters = 0.0;
    }

    return report;
}

// Evaluate four feet from bone proxy inputs (mapping foot identities and applying vertical pad offset).
inline FourFootDiagnosticReport evaluateFourFeetFromBoneProxies(
    const std::array<FootBoneProxyInput, kFootCount>& boneInputs,
    double contactToleranceMeters,
    const std::function<double(double, double)>& heightFunc,
    const std::function<bool(double, double)>& inBoundsFunc = nullptr
) {
    FourFootDiagnosticReport report;
    report.contactToleranceMeters = contactToleranceMeters;
    report.overallMethod = MeasurementMethod::BoneProxy;
    report.limitations = defaultBoneProxyLimitations();
    report.footContactAccepted = false;

    const bool toleranceValid = std::isfinite(contactToleranceMeters) && contactToleranceMeters >= 0.0;

    // Validate and map 4 distinct foot identities to avoid duplicate/mislabeled limbs
    std::array<int, kFootCount> footIndexMap;
    footIndexMap.fill(-1);
    bool identityError = false;
    std::string identityIssue;

    for (std::size_t i = 0; i < kFootCount; ++i) {
        const auto& inp = boneInputs[i];
        if (!isValidFootId(inp.foot)) {
            identityError = true;
            identityIssue = "invalid foot identity in bone proxy input";
            break;
        }
        const auto footVal = static_cast<std::size_t>(inp.foot);
        if (footIndexMap[footVal] != -1) {
            identityError = true;
            identityIssue = "duplicate foot identity in bone proxy inputs";
            break;
        }
        footIndexMap[footVal] = static_cast<int>(i);
    }

    if (!identityError) {
        for (std::size_t f = 0; f < kFootCount; ++f) {
            if (footIndexMap[f] == -1) {
                identityError = true;
                identityIssue = "missing foot identity in bone proxy inputs";
                break;
            }
        }
    }

    if (identityError) {
        report.allValid = false;
        for (std::size_t f = 0; f < kFootCount; ++f) {
            report.feet[f].foot = kAllFootIds[f];
            report.feet[f].method = MeasurementMethod::BoneProxy;
            report.feet[f].valid = false;
            report.feet[f].issue = identityIssue;
        }
        return report;
    }

    bool allValid = toleranceValid;
    bool anyPenetrating = false;
    double maxPen = 0.0;
    double minClear = std::numeric_limits<double>::infinity();
    double maxClear = -std::numeric_limits<double>::infinity();
    double meanClear = 0.0;
    std::size_t validCount = 0;

    for (std::size_t f = 0; f < kFootCount; ++f) {
        const auto fid = kAllFootIds[f];
        const auto& inp = boneInputs[static_cast<std::size_t>(footIndexMap[f])];

        // Reject negative or non-finite pad offset
        if (!std::isfinite(inp.verticalPadOffsetMeters) || inp.verticalPadOffsetMeters < 0.0) {
            report.feet[f].foot = fid;
            report.feet[f].method = MeasurementMethod::BoneProxy;
            report.feet[f].worldPosition = inp.boneJointWorldPosition;
            report.feet[f].valid = false;
            report.feet[f].issue = "negative or non-finite vertical pad offset";
            allValid = false;
            continue;
        }

        const double padY = inp.boneJointWorldPosition.y - inp.verticalPadOffsetMeters;
        if (!std::isfinite(padY)) {
            report.feet[f].foot = fid;
            report.feet[f].method = MeasurementMethod::BoneProxy;
            report.feet[f].worldPosition = inp.boneJointWorldPosition;
            report.feet[f].valid = false;
            report.feet[f].issue = "vertical pad offset calculation overflow";
            allValid = false;
            continue;
        }

        const Vector3 padPos{inp.boneJointWorldPosition.x, padY, inp.boneJointWorldPosition.z};
        report.feet[f] = evaluateSinglePoint(
            fid, padPos, MeasurementMethod::BoneProxy, contactToleranceMeters, heightFunc, inBoundsFunc);

        if (!report.feet[f].valid) {
            allValid = false;
        } else {
            ++validCount;
            // Incremental mean avoids finite-sum overflow
            meanClear = finiteMeanStep(meanClear, report.feet[f].clearanceMeters, validCount);
            minClear = std::min(minClear, report.feet[f].clearanceMeters);
            maxClear = std::max(maxClear, report.feet[f].clearanceMeters);
            if (report.feet[f].penetrating) {
                anyPenetrating = true;
                maxPen = std::max(maxPen, report.feet[f].penetrationDepthMeters);
            }
        }
    }

    report.allValid = allValid && (validCount == kFootCount);
    report.anyPenetrating = anyPenetrating;
    report.maxPenetrationMeters = maxPen;
    if (validCount > 0) {
        report.minClearanceMeters = minClear;
        report.maxClearanceMeters = maxClear;
        report.meanClearanceMeters = meanClear;
    } else {
        report.minClearanceMeters = 0.0;
        report.maxClearanceMeters = 0.0;
        report.meanClearanceMeters = 0.0;
    }

    return report;
}

// Evaluate four feet from skinned vertex clouds (bounded <= 4096 vertices, selecting lowest clearance vertex per pad).
inline FourFootDiagnosticReport evaluateFourFeetFromVertexClouds(
    const std::array<FootPadVertexCloud, kFootCount>& clouds,
    double contactToleranceMeters,
    const std::function<double(double, double)>& heightFunc,
    const std::function<bool(double, double)>& inBoundsFunc = nullptr
) {
    FourFootDiagnosticReport report;
    report.contactToleranceMeters = contactToleranceMeters;
    report.overallMethod = MeasurementMethod::SolePadSurface;
    report.limitations = defaultSolePadSurfaceLimitations();
    report.footContactAccepted = false;

    const bool toleranceValid = std::isfinite(contactToleranceMeters) && contactToleranceMeters >= 0.0;

    // Validate and map 4 distinct foot identities
    std::array<int, kFootCount> cloudIndexMap;
    cloudIndexMap.fill(-1);
    bool identityError = false;
    std::string identityIssue;

    for (std::size_t i = 0; i < kFootCount; ++i) {
        const auto& cloud = clouds[i];
        if (!isValidFootId(cloud.foot)) {
            identityError = true;
            identityIssue = "invalid foot identity in vertex cloud";
            break;
        }
        const auto footVal = static_cast<std::size_t>(cloud.foot);
        if (cloudIndexMap[footVal] != -1) {
            identityError = true;
            identityIssue = "duplicate foot identity in vertex clouds";
            break;
        }
        cloudIndexMap[footVal] = static_cast<int>(i);
    }

    if (!identityError) {
        for (std::size_t f = 0; f < kFootCount; ++f) {
            if (cloudIndexMap[f] == -1) {
                identityError = true;
                identityIssue = "missing foot identity in vertex clouds";
                break;
            }
        }
    }

    if (identityError) {
        report.allValid = false;
        for (std::size_t f = 0; f < kFootCount; ++f) {
            report.feet[f].foot = kAllFootIds[f];
            report.feet[f].method = MeasurementMethod::SolePadSurface;
            report.feet[f].valid = false;
            report.feet[f].issue = identityIssue;
        }
        return report;
    }

    bool allValid = toleranceValid;
    bool anyPenetrating = false;
    double maxPen = 0.0;
    double minClear = std::numeric_limits<double>::infinity();
    double maxClear = -std::numeric_limits<double>::infinity();
    double meanClear = 0.0;
    std::size_t validCount = 0;

    for (std::size_t f = 0; f < kFootCount; ++f) {
        const auto fid = kAllFootIds[f];
        const auto& cloud = clouds[static_cast<std::size_t>(cloudIndexMap[f])];

        if (cloud.worldVertices.empty()) {
            report.feet[f].foot = fid;
            report.feet[f].method = MeasurementMethod::SolePadSurface;
            report.feet[f].valid = false;
            report.feet[f].vertexCount = 0;
            report.feet[f].issue = "empty vertex cloud for foot pad";
            allValid = false;
            continue;
        }

        // Bounded vertex capacity check
        if (cloud.worldVertices.size() > kMaxVerticesPerFootPad) {
            report.feet[f].foot = fid;
            report.feet[f].method = MeasurementMethod::SolePadSurface;
            report.feet[f].valid = false;
            report.feet[f].vertexCount = cloud.worldVertices.size();
            report.feet[f].issue = "vertex cloud exceeds maximum capacity of 4096 vertices per foot";
            allValid = false;
            continue;
        }

        bool cloudHasInvalidVertex = false;
        FootClearanceSample firstInvalidSample;
        FootClearanceSample bestSample;
        double bestClearance = std::numeric_limits<double>::infinity();

        for (const auto& v : cloud.worldVertices) {
            auto s = evaluateSinglePoint(
                fid, v, MeasurementMethod::SolePadSurface, contactToleranceMeters, heightFunc, inBoundsFunc);
            if (!s.valid) {
                if (!cloudHasInvalidVertex) {
                    cloudHasInvalidVertex = true;
                    firstInvalidSample = s;
                }
            } else if (!cloudHasInvalidVertex) {
                if (s.clearanceMeters < bestClearance) {
                    bestClearance = s.clearanceMeters;
                    bestSample = s;
                }
            }
        }

        // Partially invalid cloud must be rejected with explicit typed issue
        if (cloudHasInvalidVertex) {
            report.feet[f] = firstInvalidSample;
            report.feet[f].foot = fid;
            report.feet[f].method = MeasurementMethod::SolePadSurface;
            report.feet[f].valid = false;
            report.feet[f].vertexCount = cloud.worldVertices.size();
            report.feet[f].issue = "vertex cloud contains invalid vertex: " + firstInvalidSample.issue;
            allValid = false;
            continue;
        }

        bestSample.vertexCount = cloud.worldVertices.size();
        report.feet[f] = bestSample;

        if (!bestSample.valid) {
            allValid = false;
        } else {
            ++validCount;
            // Incremental mean avoids finite-sum overflow
            meanClear = finiteMeanStep(meanClear, bestSample.clearanceMeters, validCount);
            minClear = std::min(minClear, bestSample.clearanceMeters);
            maxClear = std::max(maxClear, bestSample.clearanceMeters);
            if (bestSample.penetrating) {
                anyPenetrating = true;
                maxPen = std::max(maxPen, bestSample.penetrationDepthMeters);
            }
        }
    }

    report.allValid = allValid && (validCount == kFootCount);
    report.anyPenetrating = anyPenetrating;
    report.maxPenetrationMeters = maxPen;
    if (validCount > 0) {
        report.minClearanceMeters = minClear;
        report.maxClearanceMeters = maxClear;
        report.meanClearanceMeters = meanClear;
    } else {
        report.minClearanceMeters = 0.0;
        report.maxClearanceMeters = 0.0;
        report.meanClearanceMeters = 0.0;
    }

    return report;
}

// Straightforward convenience overloads evaluating against domain::ReferenceLandscape directly.
inline FourFootDiagnosticReport evaluateFourFeetOnLandscape(
    const std::array<Vector3, kFootCount>& worldPositions,
    MeasurementMethod method,
    double contactToleranceMeters,
    const domain::ReferenceLandscape& landscape
) {
    auto heightFunc = [&landscape](double x, double z) -> double {
        return landscape.height(x, z);
    };
    auto inBoundsFunc = [&landscape](double x, double z) -> bool {
        return landscape.inBounds(x, z);
    };
    return evaluateFourFeetPositions(worldPositions, method, contactToleranceMeters, heightFunc, inBoundsFunc);
}

inline FourFootDiagnosticReport evaluateFourFeetOnLandscapeFromBoneProxies(
    const std::array<FootBoneProxyInput, kFootCount>& boneInputs,
    double contactToleranceMeters,
    const domain::ReferenceLandscape& landscape
) {
    auto heightFunc = [&landscape](double x, double z) -> double {
        return landscape.height(x, z);
    };
    auto inBoundsFunc = [&landscape](double x, double z) -> bool {
        return landscape.inBounds(x, z);
    };
    return evaluateFourFeetFromBoneProxies(boneInputs, contactToleranceMeters, heightFunc, inBoundsFunc);
}

inline FourFootDiagnosticReport evaluateFourFeetOnLandscapeFromVertexClouds(
    const std::array<FootPadVertexCloud, kFootCount>& clouds,
    double contactToleranceMeters,
    const domain::ReferenceLandscape& landscape
) {
    auto heightFunc = [&landscape](double x, double z) -> double {
        return landscape.height(x, z);
    };
    auto inBoundsFunc = [&landscape](double x, double z) -> bool {
        return landscape.inBounds(x, z);
    };
    return evaluateFourFeetFromVertexClouds(clouds, contactToleranceMeters, heightFunc, inBoundsFunc);
}

// Wicked bone landmark specification mapping.
struct WickedBoneLandmarkSpec {
    FootId foot;
    const char* boneName;
    const char* toolkitSelector;
    double defaultSoleThicknessMeters;
};

inline constexpr std::array<WickedBoneLandmarkSpec, kFootCount> kWickedFootLandmarks = {{
    {FootId::FrontLeft,  "toe_front_L", "diagnostic:manus_hand_l", 0.02},
    {FootId::FrontRight, "toe_front_R", "diagnostic:manus_hand_r", 0.02},
    {FootId::RearLeft,   "toe_rear_L",  "bone:pes_foot_l",         0.02},
    {FootId::RearRight,  "toe_rear_R",  "bone:pes_foot_r",         0.02}
}};

// Transform a local bone coordinate using root position, yaw, and optional terrain support frame.
inline Vector3 computeCreatureWorldPoint(
    const Vector3& localPoint,
    const Vector3& rootPosition,
    double yawRadians,
    double soleOffsetMeters,
    const std::optional<Matrix4x4>& terrainSupportBasis = std::nullopt
) {
    // Blender +X longitudinal axis to Wicked +Z forward axis is handled by a -pi/2 yaw offset
    const double pi = 3.14159265358979323846;
    Matrix4x4 assetOffset = Matrix4x4::rotationY(-pi * 0.5);

    Matrix4x4 orientation;
    if (terrainSupportBasis.has_value()) {
        orientation = Matrix4x4::multiply(assetOffset, *terrainSupportBasis);
    } else {
        orientation = Matrix4x4::rotationY(yawRadians - pi * 0.5);
    }

    Matrix4x4 translation = Matrix4x4::translation(
        rootPosition.x, rootPosition.y - soleOffsetMeters, rootPosition.z);

    Matrix4x4 fullTransform = Matrix4x4::multiply(orientation, translation);
    return fullTransform.transformPoint(localPoint);
}

} // namespace stalthos::engine
