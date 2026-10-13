#include "../domain/resource_presentation.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cmath>

using namespace stalthos;

void require(bool condition) { if (!condition) throw std::runtime_error("resource presentation expectation failed"); }

void test_evaluate() {
    // 1. Full resource
    ResourceDto fullShrub = {1, ResourceKind::Shrub, 100.0, 0.0};
    auto state = ShrubPresentation::evaluate(fullShrub);
    require(state.valid);
    require(state.woodyStemVisible);
    require(std::abs(state.foliageFraction - 1.0) < 1e-9);
    require(state.leafClusterCount == 4);

    // 2. Partial resource
    ResourceDto halfShrub = {2, ResourceKind::Shrub, 100.0, 50.0};
    state = ShrubPresentation::evaluate(halfShrub);
    require(state.valid);
    require(state.woodyStemVisible);
    require(std::abs(state.foliageFraction - 0.5) < 1e-9);
    require(state.leafClusterCount == 2);

    // 3. Exhausted resource
    ResourceDto emptyShrub = {3, ResourceKind::Shrub, 100.0, 100.0};
    state = ShrubPresentation::evaluate(emptyShrub);
    require(state.valid);
    require(state.woodyStemVisible); // Woody stem preserved at exhaustion
    require(std::abs(state.foliageFraction - 0.0) < 1e-9);
    require(state.leafClusterCount == 0);

    // 4. Zero initial
    ResourceDto zeroShrub = {4, ResourceKind::Shrub, 0.0, 0.0};
    state = ShrubPresentation::evaluate(zeroShrub);
    require(state.valid);
    require(state.woodyStemVisible);
    require(std::abs(state.foliageFraction - 0.0) < 1e-9);
    require(state.leafClusterCount == 0);

    // 5. Invalid - overconsumed
    ResourceDto overShrub = {5, ResourceKind::Shrub, 100.0, 110.0};
    state = ShrubPresentation::evaluate(overShrub);
    require(!state.valid);

    // 6. Invalid - nonfinite
    ResourceDto nanShrub = {6, ResourceKind::Shrub, std::numeric_limits<double>::quiet_NaN(), 0.0};
    state = ShrubPresentation::evaluate(nanShrub);
    require(!state.valid);
    ResourceDto infShrub = {6, ResourceKind::Shrub, 100.0, std::numeric_limits<double>::infinity()};
    state = ShrubPresentation::evaluate(infShrub);
    require(!state.valid);

    // 7. Invalid - negative
    ResourceDto negShrub = {7, ResourceKind::Shrub, -100.0, 0.0};
    state = ShrubPresentation::evaluate(negShrub);
    require(!state.valid);

    // 8. Creek excluded
    ResourceDto creek = {8, ResourceKind::Creek, 100.0, 50.0};
    state = ShrubPresentation::evaluate(creek);
    require(!state.valid);

    // 9. Hysteresis tests
    // A resource around 2.5 clusters
    ResourceDto flutterShrub2 = {9, ResourceKind::Shrub, 100.0, 38.0}; // 62% -> 2.48 clusters (rounds to 2 normally)

    // Without hysteresis, 62.5% could snap to 3, 62% snaps to 2.
    // Let's test with hysteresis maintaining the previous state.
    ShrubVisualState prevState = {true, true, 0.625, 3}; // Previously was 3

    // If it drops to 62% (2.48), diff is 2.48 - 3 = -0.52.
    // It should stay at 3 because it's within the hysteresis deadband (-0.6 to 0.6).
    state = ShrubPresentation::evaluate(flutterShrub2, prevState);
    require(state.valid);
    require(state.leafClusterCount == 3);

    // If it drops to 35% (65 consumed) -> 35% * 4 = 1.4 clusters. Diff = 1.4 - 3 = -1.6
    // This is outside hysteresis deadband, so it should snap down to 1.
    ResourceDto droppedShrub = {9, ResourceKind::Shrub, 100.0, 65.0};
    state = ShrubPresentation::evaluate(droppedShrub, prevState);
    require(state.valid);
    require(state.leafClusterCount == 1);

    ShrubVisualState malformedPrevious = {true, true, std::numeric_limits<double>::quiet_NaN(), 3};
    require(ShrubPresentation::evaluate(flutterShrub2, malformedPrevious).leafClusterCount == 2);
    malformedPrevious = {true, true, 0.5, -100};
    require(ShrubPresentation::evaluate(flutterShrub2, malformedPrevious).leafClusterCount == 2);
    require(!ShrubPresentation::evaluate({7, ResourceKind::Shrub, 100.0, -1.0}).valid);
    require(!ShrubPresentation::evaluate({7, ResourceKind::Shrub, std::numeric_limits<double>::infinity(), 0.0}).valid);
    std::cout << "All resource_presentation tests passed!\n";
}

int main() {
    test_evaluate();
    return 0;
}
