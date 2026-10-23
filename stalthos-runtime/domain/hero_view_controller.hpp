#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include "foundation.hpp"
#include "camera_framing.hpp"

namespace stalthos {
namespace domain {

// Typed exception thrown when terrain clearance and tether hard radius cannot both be satisfied
class TetherTerrainConflictException : public std::runtime_error {
public:
    explicit TetherTerrainConflictException(const std::string& msg)
        : std::runtime_error(msg) {}
};

enum class InputFocus {
    BodySteering, // W/A/S/D drives the animal, camera passively follows or manually orbits
    FreeCamera    // W/A/S/D flies the camera anchor locally, animal acts autonomously
};

enum class CameraMode {
    TetheredFollow,  // Camera strictly anchored to animal body
    LocallyDetached  // Camera anchored to an independent detached point, tethered smoothly to animal
};

struct ControllerOutput {
    CameraFraming::Output cameraState;
    InputFocus currentFocus;
    CameraMode currentMode;
    std::optional<Vec3> activeTravelDestination;
};

// Pure deterministic state machine for hero-view camera and intent routing.
// Enforces local bounding, transactional valid updates, and preserves state on invalid input.
class HeroViewController {
public:
    // Provisional defaults: limits on free-camera travel distance from the hero's body
    static constexpr double kDefaultComfortRadius = 30.0; // soft limit, attenuation outward
    static constexpr double kDefaultHardRadius = 50.0;    // strict physical cap
    static constexpr double kComfortRadius = 30.0;        // backward compatibility alias
    static constexpr double kHardRadius = 50.0;           // backward compatibility alias

    static constexpr double kMaxTimeStep = 0.5;           // caps max simulation delta
    static constexpr double kPanSpeed = 10.0;             // m/s detached pan
    static constexpr double kZoomSpeed = 1.0;
    static constexpr double kOrbitSpeedAzimuth = 0.005;
    static constexpr double kOrbitSpeedElevation = 0.005;
    static constexpr double kEnvelopeFollowRate = 3.0;    // 1/s smooth follow rate towards comfort boundary

    struct State {
        CameraMode mode = CameraMode::TetheredFollow;
        InputFocus focus = InputFocus::BodySteering;
        CameraFraming::SphericalOrbit orbit = CameraFraming::followPreset();
        Vec3 detachedAnchor{0.0, 0.0, 0.0};
        std::optional<Vec3> pendingDestination = std::nullopt;
        double comfortRadius = kDefaultComfortRadius;
        double hardRadius = kDefaultHardRadius;
    };

private:
    State state_;
    bool dangerRecall_ = false; // Session-only; no additional camera authority or save schema.
    std::optional<Vec3> previousRecallBody_;

    static bool isFiniteVec3(const Vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    static bool isCoordWithinCap(const Vec3& v) {
        return std::abs(v.x) <= CameraFraming::kMaxCoordinateMagnitude &&
               std::abs(v.y) <= CameraFraming::kMaxCoordinateMagnitude &&
               std::abs(v.z) <= CameraFraming::kMaxCoordinateMagnitude;
    }

    static void validateState(const State& s) {
        if (s.mode != CameraMode::TetheredFollow && s.mode != CameraMode::LocallyDetached) {
            throw std::invalid_argument("invalid camera mode");
        }
        if (s.focus != InputFocus::BodySteering && s.focus != InputFocus::FreeCamera) {
            throw std::invalid_argument("invalid input focus");
        }
        if (!isFiniteVec3(s.detachedAnchor) || !isCoordWithinCap(s.detachedAnchor)) {
            throw std::invalid_argument("detachedAnchor must be finite and within coordinate bounds");
        }
        if (!std::isfinite(s.orbit.azimuth) || !std::isfinite(s.orbit.elevation) || !std::isfinite(s.orbit.distance)) {
            throw std::invalid_argument("orbit parameters must be finite");
        }
        if (s.orbit.distance < 0.0 || s.orbit.distance > CameraFraming::kMaxCoordinateMagnitude) {
            throw std::invalid_argument("orbit distance out of bounds");
        }
        if (!std::isfinite(s.comfortRadius) || !std::isfinite(s.hardRadius) ||
            s.comfortRadius <= 0.0 || s.hardRadius < s.comfortRadius ||
            s.hardRadius > CameraFraming::kMaxCoordinateMagnitude) {
            throw std::invalid_argument("tether bounds must be finite, positive, and comfort <= hard <= cap");
        }
        if (s.pendingDestination.has_value()) {
            if (!isFiniteVec3(*s.pendingDestination) || !isCoordWithinCap(*s.pendingDestination)) {
                throw std::invalid_argument("pendingDestination must be finite and within coordinate bounds");
            }
        }
    }

public:
    HeroViewController() = default;

    bool dangerRecallActive() const { return dangerRecall_; }
    void cancelDangerRecall() { dangerRecall_ = false; previousRecallBody_.reset(); }
    void requestDangerRecall() {
        if (state_.mode == CameraMode::LocallyDetached && !dangerRecall_) {
            dangerRecall_ = true;
            previousRecallBody_.reset();
        }
    }

    // Direct access to state for testing and serialization
    State getState() const { return state_; }
    void setState(const State& s) {
        validateState(s);
        state_ = s;
        cancelDangerRecall();
    }

    // Explicit independent setters
    void setCameraMode(CameraMode mode) {
        if (mode != CameraMode::TetheredFollow && mode != CameraMode::LocallyDetached) {
            throw std::invalid_argument("invalid camera mode");
        }
        state_.mode = mode;
    }

    void setInputFocus(InputFocus focus) {
        if (focus != InputFocus::BodySteering && focus != InputFocus::FreeCamera) {
            throw std::invalid_argument("invalid input focus");
        }
        state_.focus = focus;
    }

    void setDetachedAnchor(Vec3 anchor) {
        if (!isFiniteVec3(anchor) || !isCoordWithinCap(anchor)) {
            throw std::invalid_argument("detached anchor must be finite and within coordinate bounds");
        }
        state_.detachedAnchor = anchor;
    }

    void setTetherBounds(double comfortRadius, double hardRadius) {
        if (!std::isfinite(comfortRadius) || !std::isfinite(hardRadius) ||
            comfortRadius <= 0.0 || hardRadius < comfortRadius ||
            hardRadius > CameraFraming::kMaxCoordinateMagnitude) {
            throw std::invalid_argument("tether bounds must be finite, positive, and comfort <= hard <= cap");
        }
        state_.comfortRadius = comfortRadius;
        state_.hardRadius = hardRadius;
    }

    double getComfortRadius() const { return state_.comfortRadius; }
    double getHardRadius() const { return state_.hardRadius; }

    // Return to animal helper
    void returnToAnimal() {
        cancelDangerRecall();
        state_.mode = CameraMode::TetheredFollow;
        state_.focus = InputFocus::BodySteering;
        state_.pendingDestination = std::nullopt;
    }

    // Toggle camera mode and switch focus
    void toggleDetachedMode(Vec3 currentBodyPosition) {
        if (!isFiniteVec3(currentBodyPosition) || !isCoordWithinCap(currentBodyPosition)) {
            throw std::invalid_argument("body position must be finite and within coordinate bounds");
        }
        cancelDangerRecall();
        if (state_.mode == CameraMode::TetheredFollow) {
            state_.mode = CameraMode::LocallyDetached;
            state_.focus = InputFocus::FreeCamera;
            state_.detachedAnchor = currentBodyPosition;
        } else {
            returnToAnimal();
        }
    }

    // Handles pure orbit input (e.g. mouse drag)
    void applyOrbitInput(double deltaX, double deltaY) {
        if (!std::isfinite(deltaX) || !std::isfinite(deltaY)) {
            throw std::invalid_argument("orbit deltas must be finite");
        }
        if (std::abs(deltaX) > CameraFraming::kMaxCoordinateMagnitude ||
            std::abs(deltaY) > CameraFraming::kMaxCoordinateMagnitude) {
            throw std::overflow_error("orbit delta exceeds coordinate safety magnitude");
        }
        double nextAz = state_.orbit.azimuth + deltaX * kOrbitSpeedAzimuth;
        double nextEl = state_.orbit.elevation + deltaY * kOrbitSpeedElevation;
        if (!std::isfinite(nextAz) || !std::isfinite(nextEl)) {
            throw std::overflow_error("orbit delta calculation overflowed");
        }
        if (deltaX != 0 || deltaY != 0) cancelDangerRecall();
        state_.orbit.azimuth = CameraFraming::wrapAngle(nextAz);
        state_.orbit.elevation = CameraFraming::clampElevation(nextEl);
    }

    // Handles zoom input (e.g. mouse wheel)
    void applyZoomInput(double wheelDelta) {
        if (!std::isfinite(wheelDelta)) {
            throw std::invalid_argument("zoom delta must be finite");
        }
        if (std::abs(wheelDelta) > CameraFraming::kMaxCoordinateMagnitude) {
            throw std::overflow_error("zoom delta exceeds coordinate safety magnitude");
        }
        if (wheelDelta != 0) cancelDangerRecall();
        state_.orbit.distance = CameraFraming::updateZoom(state_.orbit.distance, wheelDelta, kZoomSpeed);
    }

    // Sets a destination intent (e.g. ground click in free camera mode)
    void setTravelDestination(Vec3 destination) {
        if (!isFiniteVec3(destination) || !isCoordWithinCap(destination)) {
            throw std::invalid_argument("destination must be finite and within coordinate bounds");
        }
        if (state_.focus == InputFocus::FreeCamera) {
            state_.pendingDestination = destination;
        }
    }

    // Explicit clear of travel intention
    void cancelTravelDestination() {
        state_.pendingDestination = std::nullopt;
    }

    // Calculates pan movement for detached camera considering bounds and azimuth.
    // Retains lateral and inward motion unattenuated; attenuates outward motion beyond comfort radius.
    void applyDetachedPan(double input_x, double input_z, double dt, Vec3 bodyPosition) {
        if (!std::isfinite(input_x) || !std::isfinite(input_z) || !std::isfinite(dt)) {
            throw std::invalid_argument("pan inputs must be finite");
        }
        if (dt < 0.0) {
            throw std::invalid_argument("dt must be non-negative");
        }
        if (!isFiniteVec3(bodyPosition) || !isCoordWithinCap(bodyPosition)) {
            throw std::invalid_argument("body position must be finite and within coordinate bounds");
        }

        if (state_.mode != CameraMode::LocallyDetached || state_.focus != InputFocus::FreeCamera) {
            return;
        }
        if (input_x != 0 || input_z != 0) cancelDangerRecall();

        double mag = std::hypot(input_x, input_z);
        if (!std::isfinite(mag)) {
            throw std::overflow_error("input magnitude overflowed");
        }
        if (mag > 1.0) {
            input_x /= mag;
            input_z /= mag;
        }

        double cappedDt = std::min(dt, kMaxTimeStep);

        double cosAz = std::cos(state_.orbit.azimuth);
        double sinAz = std::sin(state_.orbit.azimuth);
        double fwdX = -sinAz;
        double fwdZ = -cosAz;

        double moveX = (input_z * fwdX) + (input_x * -cosAz);
        double moveZ = (input_z * fwdZ) + (input_x * sinAz);

        double deltaX = moveX * kPanSpeed * cappedDt;
        double deltaZ = moveZ * kPanSpeed * cappedDt;

        // Vector from body to current anchor in horizontal XZ plane
        double curDx = state_.detachedAnchor.x - bodyPosition.x;
        double curDz = state_.detachedAnchor.z - bodyPosition.z;
        double curDist = std::hypot(curDx, curDz);

        double effDeltaX = deltaX;
        double effDeltaZ = deltaZ;

        if (curDist > 1e-9) {
            double uRadialX = curDx / curDist;
            double uRadialZ = curDz / curDist;

            // Radial component of motion (positive = outward, negative = inward)
            double radialComp = deltaX * uRadialX + deltaZ * uRadialZ;

            // Tangential (lateral) component
            double tanX = deltaX - radialComp * uRadialX;
            double tanZ = deltaZ - radialComp * uRadialZ;

            if (radialComp > 0.0) {
                // Outward motion: apply soft attenuation if beyond comfort radius
                double comfort = state_.comfortRadius;
                double hard = state_.hardRadius;
                double alpha = 1.0;
                if (curDist >= hard) {
                    alpha = 0.0;
                } else if (curDist > comfort) {
                    alpha = (hard - curDist) / (hard - comfort);
                    if (alpha < 0.0) alpha = 0.0;
                    else if (alpha > 1.0) alpha = 1.0;
                }
                radialComp *= alpha;
            }
            // Inward and lateral motion retain full unattenuated controls
            effDeltaX = tanX + radialComp * uRadialX;
            effDeltaZ = tanZ + radialComp * uRadialZ;
        }

        Vec3 proposedAnchor = state_.detachedAnchor;
        proposedAnchor.x += effDeltaX;
        proposedAnchor.z += effDeltaZ;

        // Hard radius clamp on anchor position
        double nextDx = proposedAnchor.x - bodyPosition.x;
        double nextDz = proposedAnchor.z - bodyPosition.z;
        double nextDist = std::hypot(nextDx, nextDz);

        if (nextDist > state_.hardRadius && nextDist > 1e-9) {
            double scale = state_.hardRadius / nextDist;
            proposedAnchor.x = bodyPosition.x + nextDx * scale;
            proposedAnchor.z = bodyPosition.z + nextDz * scale;
        }

        state_.detachedAnchor = proposedAnchor;
    }

    // Core update loop for frame progression. Preserves state transactional on failure.
    // Bounding bounds ACTUAL rendered camera eye accounting for orbit offset, target height, and terrain lift.
    ControllerOutput update(TimeStep dt, Vec3 bodyPosition, double bodyHeading, CameraFraming::TerrainHeightFn terrainSampler = nullptr) {
        if (!isFiniteVec3(bodyPosition) || !isCoordWithinCap(bodyPosition)) {
            throw std::invalid_argument("body position must be finite and within coordinate bounds");
        }
        if (!std::isfinite(bodyHeading)) {
            throw std::invalid_argument("body heading must be finite");
        }
        if (!std::isfinite(dt.physicalSeconds) || dt.physicalSeconds < 0.0) {
            throw std::invalid_argument("dt must be finite and non-negative");
        }

        State rollback = state_;
        const bool recallRollback = dangerRecall_;
        const auto recallBodyRollback = previousRecallBody_;

        try {
            if (state_.focus == InputFocus::BodySteering) {
                state_.pendingDestination = std::nullopt;
            }

            double cappedDt = std::min(dt.physicalSeconds, kMaxTimeStep);

            if (dangerRecall_ && state_.mode == CameraMode::LocallyDetached) {
                // Carry the anchor with actual body displacement before closing the
                // residual. Otherwise a moving body creates an asymptotic lag and
                // even ordinary walking can prevent recall from ever completing.
                if (previousRecallBody_) {
                    state_.detachedAnchor.x += bodyPosition.x-previousRecallBody_->x;
                    state_.detachedAnchor.z += bodyPosition.z-previousRecallBody_->z;
                }
                previousRecallBody_=bodyPosition;
                const double alpha = 1-std::exp(-2*cappedDt);
                state_.detachedAnchor.x += (bodyPosition.x-state_.detachedAnchor.x)*alpha;
                state_.detachedAnchor.z += (bodyPosition.z-state_.detachedAnchor.z)*alpha;
                if (std::hypot(state_.detachedAnchor.x-bodyPosition.x,state_.detachedAnchor.z-bodyPosition.z)<.03) {
                    state_.mode = CameraMode::TetheredFollow;
                    state_.focus = InputFocus::BodySteering;
                    cancelDangerRecall();
                }
            }

            if (state_.mode == CameraMode::LocallyDetached) {
                // Smooth bounded follow of the moving current body
                double dx = state_.detachedAnchor.x - bodyPosition.x;
                double dz = state_.detachedAnchor.z - bodyPosition.z;
                double dist = std::hypot(dx, dz);

                if (dist > state_.comfortRadius && dist > 1e-9 && cappedDt > 0.0) {
                    // Smooth envelope follow towards comfort radius
                    double targetDist = state_.comfortRadius;
                    double targetX = bodyPosition.x + (dx / dist) * targetDist;
                    double targetZ = bodyPosition.z + (dz / dist) * targetDist;

                    double factor = 1.0 - std::exp(-kEnvelopeFollowRate * cappedDt);
                    state_.detachedAnchor.x += (targetX - state_.detachedAnchor.x) * factor;
                    state_.detachedAnchor.z += (targetZ - state_.detachedAnchor.z) * factor;
                }

                // Hard bound check on anchor directly
                dx = state_.detachedAnchor.x - bodyPosition.x;
                dz = state_.detachedAnchor.z - bodyPosition.z;
                dist = std::hypot(dx, dz);
                if (dist > state_.hardRadius && dist > 1e-9) {
                    double scale = state_.hardRadius / dist;
                    state_.detachedAnchor.x = bodyPosition.x + dx * scale;
                    state_.detachedAnchor.z = bodyPosition.z + dz * scale;
                }

                // Hard tether bounding on ACTUAL rendered camera eye
                CameraFraming::SphericalOrbit boundedOrbit;
                boundedOrbit.azimuth = CameraFraming::wrapAngle(state_.orbit.azimuth);
                boundedOrbit.elevation = CameraFraming::clampElevation(state_.orbit.elevation);
                boundedOrbit.distance = CameraFraming::clampDistance(state_.orbit.distance);

                double cosEl = std::cos(boundedOrbit.elevation);
                double sinEl = std::sin(boundedOrbit.elevation);
                double sinAz = std::sin(boundedOrbit.azimuth);
                double cosAz = std::cos(boundedOrbit.azimuth);

                double orbitOffX = boundedOrbit.distance * cosEl * sinAz;
                double orbitOffY = boundedOrbit.distance * sinEl;
                double orbitOffZ = boundedOrbit.distance * cosEl * cosAz;
                double targetH = CameraFraming::kDefaultTargetHeight;

                // Candidate eye position
                Vec3 targetPos{state_.detachedAnchor.x, bodyPosition.y + targetH, state_.detachedAnchor.z};
                Vec3 eyePos{targetPos.x + orbitOffX, targetPos.y + orbitOffY, targetPos.z + orbitOffZ};

                if (terrainSampler) {
                    double groundY = terrainSampler(eyePos.x, eyePos.z);
                    if (!std::isfinite(groundY) || std::abs(groundY) > CameraFraming::kMaxCoordinateMagnitude) {
                        throw std::runtime_error("terrain sampler returned invalid height");
                    }
                    double minEyeY = groundY + CameraFraming::kDefaultTerrainClearance;
                    if (!std::isfinite(minEyeY)) {
                        throw std::overflow_error("terrain clearance overflowed");
                    }
                    if (eyePos.y < minEyeY) {
                        eyePos.y = minEyeY;
                    }
                }

                // Vertical eye displacement relative to body
                double dEyeY = eyePos.y - bodyPosition.y;
                if (std::abs(dEyeY) > state_.hardRadius) {
                    throw TetherTerrainConflictException(
                        "impossible terrain/tether clearance: vertical displacement from terrain exceeds hard tether radius");
                }

                // Horizontal eye displacement relative to body
                double dEyeX = eyePos.x - bodyPosition.x;
                double dEyeZ = eyePos.z - bodyPosition.z;
                double curHoriz = std::hypot(dEyeX, dEyeZ);

                // Maximum horizontal distance keeping 3D distance within hard radius
                double maxHorizSq = state_.hardRadius * state_.hardRadius - dEyeY * dEyeY;
                if (maxHorizSq < 0.0) {
                    throw TetherTerrainConflictException(
                        "impossible terrain/tether clearance: 3D tether bound exceeded by terrain lift");
                }
                double maxHoriz = std::sqrt(maxHorizSq);

                if (curHoriz > maxHoriz && curHoriz > 1e-9) {
                    double horizScale = maxHoriz / curHoriz;
                    eyePos.x = bodyPosition.x + dEyeX * horizScale;
                    eyePos.z = bodyPosition.z + dEyeZ * horizScale;

                    // Pull anchor so target + orbit offset lands at clamped eye
                    state_.detachedAnchor.x = eyePos.x - orbitOffX;
                    state_.detachedAnchor.z = eyePos.z - orbitOffZ;

                    // If terrain clearance at new location is different, verify
                    if (terrainSampler) {
                        double groundY2 = terrainSampler(eyePos.x, eyePos.z);
                        if (!std::isfinite(groundY2) || std::abs(groundY2) > CameraFraming::kMaxCoordinateMagnitude) {
                            throw std::runtime_error("terrain sampler returned invalid height");
                        }
                        double minEyeY2 = groundY2 + CameraFraming::kDefaultTerrainClearance;
                        if (eyePos.y < minEyeY2) {
                            eyePos.y = minEyeY2;
                            dEyeY = eyePos.y - bodyPosition.y;
                            if (std::abs(dEyeY) > state_.hardRadius) {
                                throw TetherTerrainConflictException(
                                    "impossible terrain/tether clearance: re-evaluated terrain lift exceeds hard tether radius");
                            }
                            maxHorizSq = state_.hardRadius * state_.hardRadius - dEyeY * dEyeY;
                            if (maxHorizSq < 0.0) {
                                throw TetherTerrainConflictException(
                                    "impossible terrain/tether clearance: 3D tether bound exceeded by re-evaluated terrain lift");
                            }
                            maxHoriz = std::sqrt(maxHorizSq);
                            dEyeX = eyePos.x - bodyPosition.x;
                            dEyeZ = eyePos.z - bodyPosition.z;
                            curHoriz = std::hypot(dEyeX, dEyeZ);
                            if (curHoriz > maxHoriz && curHoriz > 1e-9) {
                                horizScale = maxHoriz / curHoriz;
                                eyePos.x = bodyPosition.x + dEyeX * horizScale;
                                eyePos.z = bodyPosition.z + dEyeZ * horizScale;
                                state_.detachedAnchor.x = eyePos.x - orbitOffX;
                                state_.detachedAnchor.z = eyePos.z - orbitOffZ;
                            }
                        }
                    }
                }
            }

            // In locally detached mode, activeTarget is detachedAnchor with body Y to ensure stable ground reference;
            // body heading changes do not alter detached look-target or orbit orientation.
            Vec3 activeTarget = (state_.mode == CameraMode::LocallyDetached)
                ? Vec3{state_.detachedAnchor.x, bodyPosition.y, state_.detachedAnchor.z}
                : bodyPosition;

            CameraFraming::Output framingOut = CameraFraming::computeFraming(
                activeTarget,
                bodyHeading,
                state_.orbit,
                CameraFraming::kDefaultTargetHeight,
                terrainSampler
            );

            // In LocallyDetached mode, ensure framingOut eye strictly respects the hard tether bound
            if (state_.mode == CameraMode::LocallyDetached) {
                double eyeDistFromHost = std::hypot(framingOut.eye.x - bodyPosition.x, framingOut.eye.z - bodyPosition.z);
                if (eyeDistFromHost > state_.hardRadius + 1e-4) {
                    double scale = state_.hardRadius / eyeDistFromHost;
                    framingOut.eye.x = bodyPosition.x + (framingOut.eye.x - bodyPosition.x) * scale;
                    framingOut.eye.z = bodyPosition.z + (framingOut.eye.z - bodyPosition.z) * scale;
                }
            }

            if (state_.mode == CameraMode::LocallyDetached) {
                const double actualEyeDistance=std::hypot(
                    std::hypot(framingOut.eye.x-bodyPosition.x,framingOut.eye.z-bodyPosition.z),
                    framingOut.eye.y-bodyPosition.y);
                if(actualEyeDistance>state_.hardRadius+1e-6)
                    throw TetherTerrainConflictException("final terrain lift exceeds actual 3D eye tether");
            }
            return ControllerOutput{
                framingOut,
                state_.focus,
                state_.mode,
                state_.pendingDestination
            };

        } catch (...) {
            state_ = rollback;
            dangerRecall_ = recallRollback;
            previousRecallBody_ = recallBodyRollback;
            throw;
        }
    }
};

} // namespace domain
} // namespace stalthos
