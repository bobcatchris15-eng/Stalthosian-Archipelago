#pragma once

#include <cmath>
#include <functional>
#include <stdexcept>
#include <tuple>
#include "foundation.hpp"

namespace stalthos {
namespace domain {

// Pure engine-independent C++17 bounded camera framing helper for
// third-person animal-centered follow, orbit, and close inspection.
// Provisional framing bounds and pure calculations only; this helper does not
// implement the complete hero-view camera system or scene collision handling.
class CameraFraming {
public:
    // Clamping limits according to domain requirements
    static constexpr double kMinDistance = 2.0;       // meters
    static constexpr double kMaxDistance = 18.0;      // meters
    static constexpr double kDefaultDistance = 8.0;   // meters (matches existing runtime follow)
    static constexpr double kCloseInspectionDistance = 3.0; // meters (close inspection preset)

    // Elevation limits: -10 to +75 degrees (in radians)
    static constexpr double kMinElevationDegrees = -10.0;
    static constexpr double kMaxElevationDegrees = 75.0;
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kDegToRad = kPi / 180.0;

    static constexpr double kMinElevation = -10.0 * kDegToRad;
    static constexpr double kMaxElevation = 75.0 * kDegToRad;

    static constexpr double kDefaultTargetHeight = 0.8; // meters relative to body position
    static constexpr double kDefaultTerrainClearance = 0.25; // meters above terrain

    // Coordinate safety bound to prevent overflow in distance/vector calculations
    static constexpr double kMaxCoordinateMagnitude = 1e6; // 1,000 km
    static constexpr double kMaxTargetHeight = 20.0; // provisional helper input bound, meters
    static constexpr double kMaxTerrainClearance = 10.0; // provisional helper input bound, meters

    struct SphericalOrbit {
        double azimuth = 0.0;     // radians, 0 = viewing towards +Z or facing from +Z
        double elevation = 0.0;   // radians, pitch above horizontal
        double distance = kDefaultDistance; // meters slant distance from target to eye

        SphericalOrbit() = default;
        SphericalOrbit(double az, double el, double dist)
            : azimuth(az), elevation(el), distance(dist) {}
    };

    struct Output {
        Vec3 eye{0.0, 0.0, 0.0};
        Vec3 target{0.0, 0.0, 0.0};
        SphericalOrbit orbit{};
    };

    // Callback querying terrain height at (x, z). Returns ground Y.
    using TerrainHeightFn = std::function<double(double x, double z)>;

    // Utility: wrap angle into [-pi, +pi) or [0, 2*pi) safely without while-loops
    static double wrapAngle(double radians) {
        if (!std::isfinite(radians)) {
            throw std::invalid_argument("radians must be finite");
        }
        constexpr double twoPi = 2.0 * kPi;
        double rem = std::fmod(radians, twoPi);
        if (rem < -kPi) {
            rem += twoPi;
        } else if (rem >= kPi) {
            rem -= twoPi;
        }
        return rem;
    }

    // Preset generator for standard follow camera (8m distance, level/slight elevation)
    static SphericalOrbit followPreset(double azimuth = 0.0, double elevationDeg = 15.0) {
        return SphericalOrbit(wrapAngle(azimuth), clampElevation(elevationDeg * kDegToRad), kDefaultDistance);
    }

    // Preset generator for close inspection (3m distance, three-quarter frame angle)
    // In a three-quarter perspective, azimuth is offset ~45 degrees (pi/4) from animal heading.
    static SphericalOrbit closeInspectionPreset(double bodyHeading = 0.0, double elevationDeg = 20.0) {
        if (!std::isfinite(bodyHeading)) {
            throw std::invalid_argument("bodyHeading must be finite");
        }
        // With forward +Z at heading zero, +pi/4 places the camera in the
        // front three-quarter region. computeFraming uses sin(az), cos(az).
        double az = wrapAngle(wrapAngle(bodyHeading) + 0.25 * kPi);
        return SphericalOrbit(az, clampElevation(elevationDeg * kDegToRad), kCloseInspectionDistance);
    }

    // Safe zoom update with wheelDelta:
    // Applies delta scaled by zoomSpeed, clamps safely into [kMinDistance, kMaxDistance].
    // delta > 0 zooms in (decreases distance), delta < 0 zooms out (increases distance).
    static double updateZoom(double currentDistance, double wheelDelta, double zoomSpeed = 1.0) {
        if (!std::isfinite(currentDistance) || !std::isfinite(wheelDelta) || !std::isfinite(zoomSpeed)) {
            throw std::invalid_argument("zoom parameters must be finite");
        }
        if (currentDistance < kMinDistance || currentDistance > kMaxDistance) {
            currentDistance = clampDistance(currentDistance);
        }
        double scaledDelta = wheelDelta * zoomSpeed;
        if (!std::isfinite(scaledDelta)) {
            throw std::overflow_error("zoom delta product overflowed");
        }
        double nextDistance = currentDistance - scaledDelta;
        return clampDistance(nextDistance);
    }

    // Elevation clamp
    static double clampElevation(double elevationRad) {
        if (!std::isfinite(elevationRad)) {
            throw std::invalid_argument("elevation must be finite");
        }
        if (elevationRad < kMinElevation) return kMinElevation;
        if (elevationRad > kMaxElevation) return kMaxElevation;
        return elevationRad;
    }

    // Distance clamp
    static double clampDistance(double dist) {
        if (!std::isfinite(dist)) {
            throw std::invalid_argument("distance must be finite");
        }
        if (dist < kMinDistance) return kMinDistance;
        if (dist > kMaxDistance) return kMaxDistance;
        return dist;
    }

    // Exponential frame-rate independent bounded interpolation for a scalar:
    // next = target + (current - target) * exp(-rate * dt)
    static double interpolateScalar(double current, double target, double rate, double dt) {
        if (!std::isfinite(current) || !std::isfinite(target) ||
            !std::isfinite(rate) || rate < 0.0 ||
            !std::isfinite(dt) || dt < 0.0) {
            throw std::invalid_argument("interpolation inputs must be finite and non-negative");
        }
        if (rate == 0.0 || dt == 0.0) return current;
        double product = rate * dt;
        double factor = std::isinf(product) ? 0.0 : std::exp(-product);
        // Weighted form avoids overflowing (current-target) for opposite
        // large finite values while remaining between the two inputs.
        return current * factor + target * (1.0 - factor);
    }

    // Exponential interpolation for Vec3
    static Vec3 interpolateVec3(Vec3 current, Vec3 target, double rate, double dt) {
        return {
            interpolateScalar(current.x, target.x, rate, dt),
            interpolateScalar(current.y, target.y, rate, dt),
            interpolateScalar(current.z, target.z, rate, dt)
        };
    }

    // Exponential interpolation for angles taking shortest arc
    static double interpolateAngle(double currentRad, double targetRad, double rate, double dt) {
        if (!std::isfinite(currentRad) || !std::isfinite(targetRad) ||
            !std::isfinite(rate) || rate < 0.0 ||
            !std::isfinite(dt) || dt < 0.0) {
            throw std::invalid_argument("angle interpolation inputs must be finite and non-negative");
        }
        double normalizedCurrent = wrapAngle(currentRad);
        double normalizedTarget = wrapAngle(targetRad);
        double diff = wrapAngle(normalizedTarget - normalizedCurrent);
        if (rate == 0.0 || dt == 0.0) return normalizedCurrent;
        double product = rate * dt;
        double factor = std::isinf(product) ? 1.0 : 1.0 - std::exp(-product);
        return wrapAngle(normalizedCurrent + diff * factor);
    }

    // Compute eye and target vectors given body pose, orbit parameters, optional target height,
    // and optional terrain callback.
    // DOES NOT mutate bodyPosition or bodyHeading.
    static Output computeFraming(
        const Vec3& bodyPosition,
        double bodyHeadingRadians,
        const SphericalOrbit& requestedOrbit,
        double targetHeight = kDefaultTargetHeight,
        const TerrainHeightFn& terrainSampler = nullptr,
        double minTerrainClearance = kDefaultTerrainClearance)
    {
        // Strict input validity checks
        if (!finite(bodyPosition) || !std::isfinite(bodyHeadingRadians)) {
            throw std::invalid_argument("body position and heading must be finite");
        }
        if (!std::isfinite(requestedOrbit.azimuth) ||
            !std::isfinite(requestedOrbit.elevation) ||
            !std::isfinite(requestedOrbit.distance)) {
            throw std::invalid_argument("orbit parameters must be finite");
        }
        if (!std::isfinite(targetHeight) || !std::isfinite(minTerrainClearance) ||
            targetHeight < 0.0 || targetHeight > kMaxTargetHeight ||
            minTerrainClearance < 0.0 || minTerrainClearance > kMaxTerrainClearance) {
            throw std::invalid_argument("height offsets must be within provisional non-negative bounds");
        }

        // Coordinate overflow check: bounds rejection for astronomical coordinates
        if (std::abs(bodyPosition.x) > kMaxCoordinateMagnitude ||
            std::abs(bodyPosition.y) > kMaxCoordinateMagnitude ||
            std::abs(bodyPosition.z) > kMaxCoordinateMagnitude) {
            throw std::overflow_error("body position coordinates exceed maximum allowed magnitude");
        }

        // Sanitize and clamp orbit parameters
        SphericalOrbit boundedOrbit;
        boundedOrbit.azimuth = wrapAngle(requestedOrbit.azimuth);
        boundedOrbit.elevation = clampElevation(requestedOrbit.elevation);
        boundedOrbit.distance = clampDistance(requestedOrbit.distance);

        // Compute authoritative look target: positioned at bodyPosition with targetHeight offset along Y
        Vec3 target;
        target.x = bodyPosition.x;
        target.y = bodyPosition.y + targetHeight;
        target.z = bodyPosition.z;

        // Spherical offset from target to eye:
        // elevation: angle above horizontal plane.
        // azimuth: angle in XZ plane.
        // Standard coordinate convention (Y up):
        // offset.y = distance * sin(elevation)
        // planarDistance = distance * cos(elevation)
        // offset.x = planarDistance * sin(azimuth)
        // offset.z = planarDistance * cos(azimuth)
        double cosEl = std::cos(boundedOrbit.elevation);
        double sinEl = std::sin(boundedOrbit.elevation);
        double sinAz = std::sin(boundedOrbit.azimuth);
        double cosAz = std::cos(boundedOrbit.azimuth);

        Vec3 eye;
        eye.x = target.x + boundedOrbit.distance * cosEl * sinAz;
        eye.y = target.y + boundedOrbit.distance * sinEl;
        eye.z = target.z + boundedOrbit.distance * cosEl * cosAz;

        // Terrain clearance enforcement:
        // Cannot cross terrain blindly: if terrainSampler is provided, eye.y must be >= terrainHeight + minTerrainClearance.
        // Target authority is unchanged (target vector is strictly preserved).
        // No magical occlusion claim: we simply lift eye.y above the sampled terrain elevation.
        if (terrainSampler) {
            double groundY = terrainSampler(eye.x, eye.z);
            if (!std::isfinite(groundY) || std::abs(groundY) > kMaxCoordinateMagnitude) {
                throw std::runtime_error("terrain sampler returned invalid or out-of-bounds height; clearance is unknown");
            }
            double minEyeY = groundY + minTerrainClearance;
            if (!std::isfinite(minEyeY)) {
                throw std::overflow_error("terrain clearance height overflowed");
            }
            if (eye.y < minEyeY) {
                eye.y = minEyeY;
            }
        }

        // Final finite check
        if (!finite(eye) || !finite(target)) {
            throw std::runtime_error("failed to produce finite eye/target vectors");
        }

        return Output{eye, target, boundedOrbit};
    }
};

} // namespace domain
} // namespace stalthos
