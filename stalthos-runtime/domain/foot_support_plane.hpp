#pragma once

#include "body_terrain_support.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <limits>

namespace stalthos::domain {

struct FootSample {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct FootSupportPlane {
    Vector3 right;
    Vector3 up;
    Vector3 forward;
    double centerHeight = 0.0;
    double slopeAngleRadians = 0.0;
    bool supported = true;
    Vector3 surfaceNormal = {0.0, 1.0, 0.0};
    std::array<double, 4> residuals = {0.0, 0.0, 0.0, 0.0};
};

// Fits an optimal least-squares support plane to exactly four foot samples.
// Uses a centered and rescaled 2x2 least squares formulation around the centroid
// to improve conditioning and reject numerically degenerate layouts.
// Precision remains limited by representable input coordinates.
inline FootSupportPlane calculateFootSupportPlane(
    const std::array<FootSample, 4>& samples,
    double yaw)
{
    if (!std::isfinite(yaw)) {
        throw std::invalid_argument("calculateFootSupportPlane: yaw is nonfinite");
    }

    // 1. Verify all sample coordinates are finite and guard against quadratic accumulation overflow
    for (const auto& s : samples) {
        if (!std::isfinite(s.x) || !std::isfinite(s.y) || !std::isfinite(s.z)) {
            throw std::invalid_argument("calculateFootSupportPlane: nonfinite sample coordinate");
        }
        if (!std::isfinite(s.x * s.x) || !std::isfinite(s.y * s.y) || !std::isfinite(s.z * s.z)) {
            throw std::runtime_error("calculateFootSupportPlane: overflow during accumulation");
        }
    }

    // 2. Verify four distinct points: no two points may share identical (x, y, z) or identical (x, z) footprint
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            if ((samples[i].x == samples[j].x && samples[i].z == samples[j].z) ||
                (samples[i].x == samples[j].x && samples[i].y == samples[j].y && samples[i].z == samples[j].z)) {
                throw std::invalid_argument("calculateFootSupportPlane: duplicate or degenerate layout");
            }
        }
    }

    // 3. Compute centroid (mean of the four foot samples)
    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;
    for (const auto& s : samples) {
        sumX += s.x;
        sumY += s.y;
        sumZ += s.z;
    }

    if (!std::isfinite(sumX) || !std::isfinite(sumY) || !std::isfinite(sumZ)) {
        throw std::runtime_error("calculateFootSupportPlane: overflow during accumulation");
    }

    const double cx = sumX * 0.25;
    const double cy = sumY * 0.25;
    const double cz = sumZ * 0.25;

    // 4. Centered offsets relative to centroid
    std::array<double, 4> dx;
    std::array<double, 4> dy;
    std::array<double, 4> dz;
    double maxSpanXZ = 0.0;

    for (size_t i = 0; i < 4; ++i) {
        dx[i] = samples[i].x - cx;
        dy[i] = samples[i].y - cy;
        dz[i] = samples[i].z - cz;
        if (!std::isfinite(dx[i]) || !std::isfinite(dy[i]) || !std::isfinite(dz[i])) {
            throw std::runtime_error("calculateFootSupportPlane: overflow during centroid centering");
        }
        const double absX = std::abs(dx[i]);
        const double absZ = std::abs(dz[i]);
        if (absX > maxSpanXZ) maxSpanXZ = absX;
        if (absZ > maxSpanXZ) maxSpanXZ = absZ;
    }

    if (maxSpanXZ == 0.0 || !std::isfinite(maxSpanXZ)) {
        throw std::invalid_argument("calculateFootSupportPlane: duplicate or degenerate layout");
    }

    // Check for near-duplicate points in normalized footprint coordinates
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            const double du = (dx[i] - dx[j]) / maxSpanXZ;
            const double dw = (dz[i] - dz[j]) / maxSpanXZ;
            if (du * du + dw * dw < 1e-14) {
                throw std::invalid_argument("calculateFootSupportPlane: duplicate or degenerate layout");
            }
        }
    }

    // 5. Centered, rescaled 2x2 least squares system:
    // Solve for plane dy = A*dx + B*dz.
    // Let u_i = dx_i / maxSpanXZ, w_i = dz_i / maxSpanXZ, with u_i, w_i in [-1, 1].
    // Then dy_i = A'*u_i + B'*w_i where A' = A*maxSpanXZ, B' = B*maxSpanXZ.
    // System:
    // [ Muu  Muw ] [ A' ]   [ Ru ]
    // [ Muw  Mww ] [ B' ] = [ Rw ]
    double Muu = 0.0;
    double Mww = 0.0;
    double Muw = 0.0;
    double Ru = 0.0;
    double Rw = 0.0;

    for (size_t i = 0; i < 4; ++i) {
        const double u = dx[i] / maxSpanXZ;
        const double w = dz[i] / maxSpanXZ;
        Muu += u * u;
        Mww += w * w;
        Muw += u * w;
        Ru += u * dy[i];
        Rw += w * dy[i];
    }

    if (!std::isfinite(Ru) || !std::isfinite(Rw) ||
        !std::isfinite(Muu) || !std::isfinite(Mww) || !std::isfinite(Muw)) {
        throw std::runtime_error("calculateFootSupportPlane: overflow during accumulation");
    }

    const double det = Muu * Mww - Muw * Muw;

    // Check for collinearity or singularity in normalized coordinates.
    // In normalized coordinates, det is bounded in [0, 16].
    // If det <= 1e-12, the points are collinear or degenerate in the horizontal plane.
    if (det <= 1e-12 || !std::isfinite(det)) {
        throw std::invalid_argument("calculateFootSupportPlane: duplicate or degenerate layout");
    }

    const double A_prime = (Mww * Ru - Muw * Rw) / det;
    const double B_prime = (Muu * Rw - Muw * Ru) / det;

    const double A = A_prime / maxSpanXZ;
    const double B = B_prime / maxSpanXZ;

    if (!std::isfinite(A) || !std::isfinite(B)) {
        throw std::runtime_error("calculateFootSupportPlane: ill-conditioned fitting");
    }

    // Normal vector from plane equation A*x + B*z - y + C = 0 -> N_raw = (-A, 1, -B)
    const Vector3 rawNormal = {-A, 1.0, -B};
    const Vector3 normal = normalize(rawNormal);

    FootSupportPlane result;

    // Residuals against the fitted plane:
    // residual_i = dy_i - (A*dx_i + B*dz_i) = dy_i - (A'*u_i + B'*w_i)
    // Centered evaluation eliminates catastrophic subtraction cancellation across large world coordinates.
    for (size_t i = 0; i < 4; ++i) {
        const double u = dx[i] / maxSpanXZ;
        const double w = dz[i] / maxSpanXZ;
        const double fitDeltaY = A_prime * u + B_prime * w;
        result.residuals[i] = dy[i] - fitDeltaY;
        if (!std::isfinite(result.residuals[i])) {
            throw std::runtime_error("calculateFootSupportPlane: residual overflow");
        }
    }

    // Height of fitted plane evaluated at the centroid (cx, cz)
    result.centerHeight = cy;

    const Vector3 flatForward = {std::sin(yaw), 0.0, std::cos(yaw)};
    const Vector3 flatRight = {std::cos(yaw), 0.0, -std::sin(yaw)};
    const Vector3 flatUp = {0.0, 1.0, 0.0};

    const double clampedNy = std::clamp(normal.y, -1.0, 1.0);
    double slopeAngle = std::acos(clampedNy);
    if (!std::isfinite(slopeAngle)) {
        slopeAngle = 0.0;
    }

    result.slopeAngleRadians = slopeAngle;
    result.surfaceNormal = normal;

    // 20-degree maximum tilt policy and upward-facing normal requirement
    if (slopeAngle > kMaxSlopeAngleRadians || normal.y <= 0.0) {
        result.supported = false;
        result.forward = flatForward;
        result.up = flatUp;
        result.right = flatRight;
        return result;
    }

    // Orthonormal yaw-preserving frame construction
    const Vector3 projectedForward = {
        flatForward.x,
        -(normal.x * flatForward.x + normal.z * flatForward.z) / normal.y,
        flatForward.z
    };
    const Vector3 forward = normalize(projectedForward);
    const Vector3 right = normalize(cross(normal, forward));

    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) || !std::isfinite(forward.z) ||
        !std::isfinite(right.x) || !std::isfinite(right.y) || !std::isfinite(right.z)) {
        result.supported = false;
        result.forward = flatForward;
        result.up = flatUp;
        result.right = flatRight;
        return result;
    }

    result.forward = forward;
    result.up = normal;
    result.right = right;
    result.supported = true;

    return result;
}

} // namespace stalthos::domain
