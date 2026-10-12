#include <iostream>
#include <stdexcept>
#include <cmath>
#include <string>
#include "../domain/hero_view_controller.hpp"

using namespace stalthos;
using namespace stalthos::domain;

// Strict expectation macro active under NDEBUG (never compiled away)
#define EXPECT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            throw std::runtime_error(std::string("Assertion failed: ") + (msg)); \
        } \
    } while (false)

void test_initial_state() {
    HeroViewController controller;
    auto state = controller.getState();
    EXPECT(state.mode == CameraMode::TetheredFollow, "Initial mode should be tethered");
    EXPECT(state.focus == InputFocus::BodySteering, "Initial focus should be steering");
    EXPECT(std::abs(controller.getComfortRadius() - 30.0) < 1e-6, "Default comfort radius should be 30m");
    EXPECT(std::abs(controller.getHardRadius() - 50.0) < 1e-6, "Default hard radius should be 50m");
}

void test_independent_setters() {
    HeroViewController controller;

    // Independently set mode without forcing focus
    controller.setCameraMode(CameraMode::LocallyDetached);
    EXPECT(controller.getState().mode == CameraMode::LocallyDetached, "CameraMode should be LocallyDetached");
    EXPECT(controller.getState().focus == InputFocus::BodySteering, "InputFocus should remain BodySteering");

    // Independently set focus
    controller.setInputFocus(InputFocus::FreeCamera);
    EXPECT(controller.getState().focus == InputFocus::FreeCamera, "InputFocus should be FreeCamera");

    // Explicitly set detached anchor
    Vec3 newAnchor{15.0, 0.0, -25.0};
    controller.setDetachedAnchor(newAnchor);
    auto state = controller.getState();
    EXPECT(std::abs(state.detachedAnchor.x - 15.0) < 1e-6 &&
           std::abs(state.detachedAnchor.z - (-25.0)) < 1e-6, "Detached anchor should match setter");

    // Return to animal helper
    controller.returnToAnimal();
    state = controller.getState();
    EXPECT(state.mode == CameraMode::TetheredFollow, "Mode should be TetheredFollow after returnToAnimal");
    EXPECT(state.focus == InputFocus::BodySteering, "Focus should be BodySteering after returnToAnimal");
    EXPECT(!state.pendingDestination.has_value(), "Pending destination cleared on returnToAnimal");
}

void test_configurable_tether_bounds() {
    HeroViewController controller;
    controller.setTetherBounds(20.0, 40.0);
    EXPECT(std::abs(controller.getComfortRadius() - 20.0) < 1e-6, "Comfort radius should be 20m");
    EXPECT(std::abs(controller.getHardRadius() - 40.0) < 1e-6, "Hard radius should be 40m");

    // Invalid bounds rejection: negative, comfort > hard, or exceeding coordinate caps
    try {
        controller.setTetherBounds(-5.0, 40.0);
        EXPECT(false, "Should throw for negative comfort radius");
    } catch (const std::invalid_argument&) {}

    try {
        controller.setTetherBounds(50.0, 40.0);
        EXPECT(false, "Should throw when comfort > hard");
    } catch (const std::invalid_argument&) {}

    try {
        controller.setTetherBounds(10.0, 2e6);
        EXPECT(false, "Should throw when hard exceeds coordinate cap");
    } catch (const std::invalid_argument&) {}

    // State preserved after rejected bounds update
    EXPECT(std::abs(controller.getComfortRadius() - 20.0) < 1e-6, "Comfort radius should remain unchanged");
    EXPECT(std::abs(controller.getHardRadius() - 40.0) < 1e-6, "Hard radius should remain unchanged");
}

void test_soft_outward_attenuation_and_controls() {
    HeroViewController controller;
    Vec3 bodyPos{0.0, 0.0, 0.0};
    auto s = controller.getState();
    s.mode = CameraMode::LocallyDetached;
    s.focus = InputFocus::FreeCamera;
    s.orbit.azimuth = 0.0; // Forward view is -Z, right view is -X
    s.detachedAnchor = Vec3{0.0, 0.0, 39.0};
    controller.setState(s);

    // 1. Outward pan in soft zone [30m, 50m]: input (0, -1) moves anchor in +Z (away from body).
    // Original buggy code moved full unattenuated 5.0m.
    controller.applyDetachedPan(0.0, -1.0, 0.5, bodyPos);
    double stepOutward = controller.getState().detachedAnchor.z - 39.0;
    EXPECT(stepOutward < 4.999, "Outward step in soft zone must be attenuated (was 5.0 in failed probe)");
    EXPECT(stepOutward > 0.0, "Outward step must make positive progress");
    EXPECT(std::abs(stepOutward - 2.75) < 1e-3, "Outward attenuation factor at 39m should be 0.55 * 5m = 2.75m");

    // 2. Inward pan from same position: input (0, 1) moves anchor in -Z (towards body).
    // Inward motion must retain FULL unattenuated control (5.0m step for dt=0.5, speed=10).
    s.detachedAnchor = Vec3{0.0, 0.0, 39.0};
    controller.setState(s);
    controller.applyDetachedPan(0.0, 1.0, 0.5, bodyPos);
    double stepInward = 39.0 - controller.getState().detachedAnchor.z;
    EXPECT(std::abs(stepInward - 5.0) < 1e-3, "Inward step towards body must retain 100% unattenuated control");

    // 3. Lateral pan: input (-1, 0) moves anchor along +X (perpendicular to radial line).
    // Lateral motion must retain FULL control.
    s.detachedAnchor = Vec3{0.0, 0.0, 39.0};
    controller.setState(s);
    controller.applyDetachedPan(-1.0, 0.0, 0.5, bodyPos);
    double stepLateral = controller.getState().detachedAnchor.x;
    EXPECT(std::abs(stepLateral - 5.0) < 1e-3, "Lateral step must retain 100% unattenuated control");
}

void test_actual_rendered_eye_hard_bound() {
    HeroViewController controller;
    Vec3 bodyPos{0.0, 0.0, 0.0};
    auto s = controller.getState();
    s.mode = CameraMode::LocallyDetached;
    s.focus = InputFocus::FreeCamera;
    s.orbit.azimuth = 0.0;
    // Set anchor at 50m hard radius: previously, camera eye ended up at 57.7274m
    s.detachedAnchor = Vec3{0.0, 0.0, 50.0};
    controller.setState(s);

    TimeStep dt{0.0, 0.0};
    auto out = controller.update(dt, bodyPos, 0.0);
    double eyeHoriz = std::hypot(out.cameraState.eye.x - bodyPos.x, out.cameraState.eye.z - bodyPos.z);
    double eye3D = std::hypot(eyeHoriz, out.cameraState.eye.y - bodyPos.y);

    EXPECT(eyeHoriz <= 50.0 + 1e-4, "Horizontal rendered eye distance must not exceed 50m hard anchor");
    EXPECT(eye3D <= 50.0 + 1e-4, "3D rendered eye distance must not exceed 50m hard anchor");
    EXPECT(eyeHoriz < 55.0, "Rendered eye must strictly eliminate original 57.7274m violation");
}

void test_terrain_clearance_and_impossible_conflict_rollback() {
    HeroViewController controller;
    Vec3 bodyPos{0.0, 0.0, 0.0};
    auto s = controller.getState();
    s.mode = CameraMode::LocallyDetached;
    s.focus = InputFocus::FreeCamera;
    s.detachedAnchor = Vec3{0.0, 0.0, 30.0};
    controller.setState(s);

    // 1. Feasible terrain lift: lifts eye.y moderately within hard radius
    auto feasibleTerrain = [](double, double) -> double {
        return 10.0; // ground at Y=10m
    };
    TimeStep dt{0.0, 0.0};
    auto out = controller.update(dt, bodyPos, 0.0, feasibleTerrain);
    EXPECT(out.cameraState.eye.y >= 10.0 + CameraFraming::kDefaultTerrainClearance,
           "Rendered eye must respect terrain clearance");
    double dist3D = std::hypot(
        std::hypot(out.cameraState.eye.x, out.cameraState.eye.z),
        out.cameraState.eye.y
    );
    EXPECT(dist3D <= 50.0 + 1e-4, "Feasible terrain lift must still bound eye to 3D hard radius");

    // 2. Impossible terrain lift: terrain is at 100m, exceeding 50m hard radius from bodyPos (Y=0)
    auto impossibleTerrain = [](double, double) -> double {
        return 100.0;
    };
    auto beforeState = controller.getState();
    bool threwTyped = false;
    try {
        controller.update(dt, bodyPos, 0.0, impossibleTerrain);
    } catch (const TetherTerrainConflictException&) {
        threwTyped = true;
    } catch (const std::runtime_error&) {
        threwTyped = true;
    }
    EXPECT(threwTyped, "Must throw typed exception when terrain and tether hard bound cannot both be satisfied");

    // Verify transactional rollback on conflict
    auto afterState = controller.getState();
    EXPECT(std::abs(afterState.detachedAnchor.z - beforeState.detachedAnchor.z) < 1e-6,
           "State must roll back cleanly on impossible terrain clearance");
}

void test_moving_body_smooth_bounded_follow() {
    HeroViewController controller;
    Vec3 bodyPos{0.0, 0.0, 0.0};
    controller.toggleDetachedMode(bodyPos);

    // Set anchor at 45m (outside 30m comfort radius)
    auto s = controller.getState();
    s.detachedAnchor = Vec3{0.0, 0.0, 45.0};
    controller.setState(s);

    // Advance frame with dt=0.1s
    TimeStep dt{0.1, 0.1};
    controller.update(dt, bodyPos, 0.0);
    double distAfter1 = std::hypot(controller.getState().detachedAnchor.x, controller.getState().detachedAnchor.z);
    EXPECT(distAfter1 < 45.0, "Moving body tether must smoothly ease anchor towards comfort boundary");
    EXPECT(distAfter1 > 30.0, "Smooth follow must not snap instantly on small dt");

    // Huge dt simulation (e.g. 1000s): must be bounded with NO overshoot below comfort radius
    TimeStep hugeDt{1000.0, 1000.0};
    controller.update(hugeDt, bodyPos, 0.0);
    double distHuge = std::hypot(controller.getState().detachedAnchor.x, controller.getState().detachedAnchor.z);
    EXPECT(distHuge >= 30.0 - 1e-4, "Anchor follow must never overshoot below comfort radius on large dt");
}

void test_heading_changes_do_not_shift_detached_anchor() {
    HeroViewController controller;
    Vec3 bodyPos{0.0, 0.0, 0.0};
    controller.toggleDetachedMode(bodyPos);

    auto s = controller.getState();
    s.detachedAnchor = Vec3{12.0, 0.0, 18.0};
    controller.setState(s);

    TimeStep dt{0.0, 0.0};
    auto outHeading0 = controller.update(dt, bodyPos, 0.0);
    auto outHeading90 = controller.update(dt, bodyPos, CameraFraming::kPi * 0.5);
    auto outHeading180 = controller.update(dt, bodyPos, CameraFraming::kPi);

    EXPECT(std::abs(outHeading0.cameraState.eye.x - outHeading90.cameraState.eye.x) < 1e-6 &&
           std::abs(outHeading0.cameraState.eye.z - outHeading90.cameraState.eye.z) < 1e-6,
           "Body heading change must produce zero detached camera eye movement");
    EXPECT(std::abs(outHeading0.cameraState.eye.x - outHeading180.cameraState.eye.x) < 1e-6 &&
           std::abs(outHeading0.cameraState.eye.z - outHeading180.cameraState.eye.z) < 1e-6,
           "Body heading change must produce zero detached camera eye movement");
}

void test_validation_and_transactional_rollback() {
    HeroViewController controller;
    auto initialState = controller.getState();

    // 1. Invalid body position in update
    Vec3 nanPos{NAN, 0.0, 0.0};
    TimeStep validDt{0.1, 0.1};
    try {
        controller.update(validDt, nanPos, 0.0);
        EXPECT(false, "Should throw for NaN body position");
    } catch (const std::invalid_argument&) {}
    EXPECT(controller.getState().mode == initialState.mode, "State must rollback on update failure");

    // 2. Body position exceeding coordinate caps (1e6)
    Vec3 hugePos{2e6, 0.0, 0.0};
    try {
        controller.update(validDt, hugePos, 0.0);
        EXPECT(false, "Should throw for coordinate exceeding 1e6 cap");
    } catch (const std::invalid_argument&) {}

    // 3. setState validation with invalid enum / NaN / huge coords
    HeroViewController::State badState = initialState;
    badState.detachedAnchor = Vec3{1e7, 0.0, 0.0};
    try {
        controller.setState(badState);
        EXPECT(false, "Should throw for detached anchor exceeding 1e6");
    } catch (const std::invalid_argument&) {}

    badState = initialState;
    badState.comfortRadius = -10.0;
    try {
        controller.setState(badState);
        EXPECT(false, "Should throw for negative comfort radius");
    } catch (const std::invalid_argument&) {}

    // 4. Stable hypot normalization without intermediate overflow
    controller.setCameraMode(CameraMode::LocallyDetached);
    controller.setInputFocus(InputFocus::FreeCamera);
    // Extreme inputs that would overflow standard x*x + z*z without stable hypot
    controller.applyDetachedPan(1e150, 1e150, 0.1, Vec3{0.0, 0.0, 0.0});
    auto stateAfterExtreme = controller.getState();
    EXPECT(std::isfinite(stateAfterExtreme.detachedAnchor.x) &&
           std::isfinite(stateAfterExtreme.detachedAnchor.z),
           "Stable hypot must normalize extreme inputs without overflow");
}

void test_travel_destination_intent() {
    HeroViewController controller;
    Vec3 bodyPos{0.0, 0.0, 0.0};

    // Cannot set destination in BodySteering mode
    controller.setTravelDestination(Vec3{10.0, 0.0, 10.0});
    EXPECT(!controller.getState().pendingDestination.has_value(), "Cannot set destination in BodySteering mode");

    // Can set destination in FreeCamera mode
    controller.toggleDetachedMode(bodyPos);
    controller.setTravelDestination(Vec3{10.0, 0.0, 10.0});
    EXPECT(controller.getState().pendingDestination.has_value(), "Destination set in FreeCamera mode");

    // Clear destination on return to animal
    controller.returnToAnimal();
    EXPECT(!controller.getState().pendingDestination.has_value(), "Destination cleared on returnToAnimal");
}

void test_stepped_terrain_final_eye_guard() {
    HeroViewController c;
    auto s=c.getState(); s.mode=CameraMode::LocallyDetached; s.focus=InputFocus::FreeCamera;
    s.orbit.azimuth=0; s.detachedAnchor={0,0,50}; c.setState(s);
    bool rejected=false;
    try { c.update(TimeStep{0,0},{0,0,0},0,[](double, double z){return z>52?0.0:z>45?30.0:49.0;}); }
    catch(const TetherTerrainConflictException&) {rejected=true;}
    EXPECT(rejected,"Final terrain lift beyond actual 3D eye tether must reject");
    EXPECT(c.getState().detachedAnchor.z==50,"Terrain conflict must rollback anchor");
}
int main() {
    try {
        test_initial_state();
        test_stepped_terrain_final_eye_guard();
        test_independent_setters();
        test_configurable_tether_bounds();
        test_soft_outward_attenuation_and_controls();
        test_actual_rendered_eye_hard_bound();
        test_terrain_clearance_and_impossible_conflict_rollback();
        test_moving_body_smooth_bounded_follow();
        test_heading_changes_do_not_shift_detached_anchor();
        test_validation_and_transactional_rollback();
        test_travel_destination_intent();
        std::cout << "All HeroViewController test suites passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
