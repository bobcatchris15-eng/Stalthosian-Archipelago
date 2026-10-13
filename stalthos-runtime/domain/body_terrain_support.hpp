#pragma once

#include <cmath>
#include <functional>
#include <stdexcept>
#include <algorithm>

namespace stalthos::domain {

struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct TerrainSupport {
    Vector3 right;
    Vector3 up;
    Vector3 forward;
    double centerHeight = 0.0;
    double slopeAngleRadians = 0.0;
    bool supported = true;
    Vector3 surfaceNormal = {0.0, 1.0, 0.0};
};

inline Vector3 cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline double dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 normalize(const Vector3& v) {
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) {
        return {0.0, 0.0, 0.0};
    }
    double maxComp = std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
    if (maxComp == 0.0 || !std::isfinite(maxComp)) {
        return {0.0, 0.0, 0.0};
    }
    double invMax = 1.0 / maxComp;
    double sx = v.x * invMax;
    double sy = v.y * invMax;
    double sz = v.z * invMax;
    double scaledLenSq = sx * sx + sy * sy + sz * sz;
    if (scaledLenSq <= 0.0 || !std::isfinite(scaledLenSq)) {
        return {0.0, 0.0, 0.0};
    }
    double len = maxComp * std::sqrt(scaledLenSq);
    if (len <= 0.0 || !std::isfinite(len)) {
        return {0.0, 0.0, 0.0};
    }
    Vector3 result = {v.x / len, v.y / len, v.z / len};
    if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z)) {
        return {0.0, 0.0, 0.0};
    }
    return result;
}

constexpr double kMaxSlopeAngleRadians = 20.0 * 3.14159265358979323846 / 180.0;
constexpr double kDefaultLongitudinalHalfspan = 0.5;
constexpr double kDefaultLateralHalfspan = 0.25;

// Computes an orthonormal frame adapted to the terrain geometry at the center point (x, z).
// The calculation keeps a Left-Handed basis compatible with Wicked/DirectX, where flat forward is (sin(yaw), 0, cos(yaw)).
inline TerrainSupport calculateTerrainSupport(
    double x, double z, double yaw,
    double longitudinalHalfspan, double lateralHalfspan,
    const std::function<double(double, double)>& heightCallback)
{
    if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(yaw)) {
        throw std::invalid_argument("calculateTerrainSupport: nonfinite position or yaw");
    }
    if (!std::isfinite(longitudinalHalfspan) || !std::isfinite(lateralHalfspan) ||
        longitudinalHalfspan <= 0.0 || lateralHalfspan <= 0.0 ||
        !std::isfinite(2.0 * longitudinalHalfspan) || !std::isfinite(2.0 * lateralHalfspan)) {
        throw std::invalid_argument("calculateTerrainSupport: invalid sample halfspans");
    }
    if (!heightCallback) {
        throw std::invalid_argument("calculateTerrainSupport: missing heightCallback");
    }

    // Flat basis (LH / DirectX convention: forward is +Z, right is +X, up is +Y)
    Vector3 flatForward = {std::sin(yaw), 0.0, std::cos(yaw)};
    Vector3 flatRight = {std::cos(yaw), 0.0, -std::sin(yaw)};
    Vector3 flatUp = {0.0, 1.0, 0.0};

    // Sample points in world space
    double cx = x;
    double cz = z;
    double fx = x + flatForward.x * longitudinalHalfspan;
    double fz = z + flatForward.z * longitudinalHalfspan;
    double bx = x - flatForward.x * longitudinalHalfspan;
    double bz = z - flatForward.z * longitudinalHalfspan;
    double rx = x + flatRight.x * lateralHalfspan;
    double rz = z + flatRight.z * lateralHalfspan;
    double lx = x - flatRight.x * lateralHalfspan;
    double lz = z - flatRight.z * lateralHalfspan;

    if (!std::isfinite(fx) || !std::isfinite(fz) ||
        !std::isfinite(bx) || !std::isfinite(bz) ||
        !std::isfinite(rx) || !std::isfinite(rz) ||
        !std::isfinite(lx) || !std::isfinite(lz)) {
        throw std::invalid_argument("calculateTerrainSupport: sample coordinates nonfinite");
    }

    double hc = 0.0, hf = 0.0, hb = 0.0, hr = 0.0, hl = 0.0;
    try {
        hc = heightCallback(cx, cz);
        hf = heightCallback(fx, fz);
        hb = heightCallback(bx, bz);
        hr = heightCallback(rx, rz);
        hl = heightCallback(lx, lz);
    } catch (...) {
        throw std::runtime_error("calculateTerrainSupport: height callback threw an exception");
    }

    if (!std::isfinite(hc) || !std::isfinite(hf) || !std::isfinite(hb) ||
        !std::isfinite(hr) || !std::isfinite(hl)) {
        throw std::runtime_error("calculateTerrainSupport: height callback returned nonfinite value");
    }

    double diffF = hf - hb;
    double diffR = hr - hl;
    if (!std::isfinite(diffF) || !std::isfinite(diffR)) {
        throw std::runtime_error("calculateTerrainSupport: height difference overflow");
    }

    double dH_dF = diffF / (2.0 * longitudinalHalfspan);
    double dH_dR = diffR / (2.0 * lateralHalfspan);
    if (!std::isfinite(dH_dF) || !std::isfinite(dH_dR)) {
        throw std::runtime_error("calculateTerrainSupport: finite difference slopes nonfinite");
    }

    Vector3 vecF = {flatForward.x, dH_dF, flatForward.z};
    Vector3 vecR = {flatRight.x, dH_dR, flatRight.z};

    // Up is cross(vecF, vecR) in LH convention (Z forward, X right, Y up).
    // Note rawUp.y = flatForward.z * flatRight.x - flatForward.x * flatRight.z = cos^2(yaw) + sin^2(yaw) = 1.0.
    Vector3 rawUp = cross(vecF, vecR);
    Vector3 up = normalize(rawUp);

    // Guard against degenerate normal
    if (!std::isfinite(up.x) || !std::isfinite(up.y) || !std::isfinite(up.z) || up.y <= 0.0) {
        TerrainSupport fallback;
        fallback.right = flatRight;
        fallback.up = flatUp;
        fallback.forward = flatForward;
        fallback.centerHeight = hc;
        fallback.slopeAngleRadians = 3.14159265358979323846 * 0.5;
        fallback.supported = false;
        fallback.surfaceNormal = flatUp;
        return fallback;
    }

    // Clamp up.y for acos to strictly prevent NaN from floating point inaccuracies
    double clampedUpY = std::clamp(up.y, -1.0, 1.0);
    double slopeAngle = std::acos(clampedUpY);
    if (!std::isfinite(slopeAngle)) {
        slopeAngle = 0.0;
    }

    TerrainSupport result;
    result.centerHeight = hc;
    result.slopeAngleRadians = slopeAngle;
    result.surfaceNormal = up;

    if (slopeAngle > kMaxSlopeAngleRadians) {
        // Explicitly falls back to yaw when slope exceeds 20 degrees
        result.supported = false;
        result.forward = flatForward;
        result.up = flatUp;
        result.right = flatRight;
        return result;
    }

    // Project flat forward onto the slope plane
    double fDotN = dot(flatForward, up);
    Vector3 projectedForward = {
        flatForward.x - fDotN * up.x,
        flatForward.y - fDotN * up.y,
        flatForward.z - fDotN * up.z
    };
    Vector3 forward = normalize(projectedForward);
    Vector3 right = normalize(cross(up, forward));

    // Verify basis validity
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) || !std::isfinite(forward.z) ||
        !std::isfinite(right.x) || !std::isfinite(right.y) || !std::isfinite(right.z)) {
        result.supported = false;
        result.forward = flatForward;
        result.up = flatUp;
        result.right = flatRight;
        return result;
    }

    result.forward = forward;
    result.up = up;
    result.right = right;
    result.supported = true;
    return result;
}

} // namespace stalthos::domain
