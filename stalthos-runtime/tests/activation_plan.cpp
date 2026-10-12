#include <iostream>
#include <vector>
#include <stdexcept>
#include <cmath>
#include "../domain/activation_plan.hpp"

using namespace stalthos;
using namespace stalthos::activation;

void assertPlan(const std::vector<RegionPlan>& actual, const std::vector<RegionPlan>& expected) {
    if (actual.size() != expected.size()) {
        throw std::runtime_error("Size mismatch in plan");
    }
    for (size_t i = 0; i < actual.size(); ++i) {
        if (actual[i].id != expected[i].id || actual[i].targetTier != expected[i].targetTier) {
            throw std::runtime_error("Mismatch at index " + std::to_string(i));
        }
    }
}

void testBodyCameraSeparation() {
    ActivationConfig config{
        10.0, 2.0, 5,
        20.0, 5.0, 10
    };
    Vec3 body{0, 0, 0};
    Vec3 camera{100, 0, 0};
    std::vector<RegionInput> inputs = {
        {1, {5, 0, 0}, Tier::Distant},   // near body -> Physical
        {2, {110, 0, 0}, Tier::Distant}, // near camera -> Presentation
        {3, {50, 0, 0}, Tier::Distant}   // far from both
    };

    ActivationPlan plan = ActivationPlanner::plan(body, camera, config, inputs);
    assertPlan(plan.regions, {
        {1, Tier::Physical},
        {2, Tier::Presentation}
    });
}

void testTiesAndOrderIndependence() {
    ActivationConfig config{
        10.0, 2.0, 1, // Only 1 budget for physical
        10.0, 2.0, 1  // Only 1 budget for presentation
    };
    Vec3 pos{0, 0, 0};
    std::vector<RegionInput> inputs = {
        {2, {5, 0, 0}, Tier::Distant},
        {1, {5, 0, 0}, Tier::Distant}
    };
    // Distance tie -> sort by ID -> ID 1 gets physical, ID 2 gets presentation
    ActivationPlan plan = ActivationPlanner::plan(pos, pos, config, inputs);
    assertPlan(plan.regions, {
        {1, Tier::Physical},
        {2, Tier::Presentation}
    });

    // Reversing input order should give exact same result
    std::vector<RegionInput> inputsRev = {
        {1, {5, 0, 0}, Tier::Distant},
        {2, {5, 0, 0}, Tier::Distant}
    };
    ActivationPlan planRev = ActivationPlanner::plan(pos, pos, config, inputsRev);
    assertPlan(planRev.regions, {
        {1, Tier::Physical},
        {2, Tier::Presentation}
    });
}

void testHysteresis() {
    ActivationConfig config{
        10.0, 2.0, 5,
        20.0, 5.0, 5
    };
    Vec3 pos{0, 0, 0};

    // Dist = 11, config radius = 10.
    // If currently Distant, won't make physical.
    std::vector<RegionInput> inputNoHysteresis = {
        {1, {11, 0, 0}, Tier::Distant}
    };
    ActivationPlan plan1 = ActivationPlanner::plan(pos, pos, config, inputNoHysteresis);
    assertPlan(plan1.regions, {
        {1, Tier::Presentation} // dist 11 <= 20, presentation radius
    });

    // If currently Physical, will remain physical because 10 + 2 = 12 >= 11
    std::vector<RegionInput> inputWithHysteresis = {
        {1, {11, 0, 0}, Tier::Physical}
    };
    ActivationPlan plan2 = ActivationPlanner::plan(pos, pos, config, inputWithHysteresis);
    assertPlan(plan2.regions, {
        {1, Tier::Physical}
    });

    // Dist = 23, config presentation radius = 20, hysteresis = 5. (20+5 = 25 >= 23).
    std::vector<RegionInput> inputPresHysteresis = {
        {1, {23, 0, 0}, Tier::Presentation}
    };
    ActivationPlan plan3 = ActivationPlanner::plan(pos, pos, config, inputPresHysteresis);
    assertPlan(plan3.regions, {
        {1, Tier::Presentation}
    });
}

void testBudgetSaturation() {
    ActivationConfig config{
        10.0, 0.0, 2, // Physical budget: 2
        20.0, 0.0, 1  // Presentation budget: 1
    };
    Vec3 pos{0, 0, 0};
    std::vector<RegionInput> inputs = {
        {1, {1, 0, 0}, Tier::Distant},
        {2, {2, 0, 0}, Tier::Distant},
        {3, {3, 0, 0}, Tier::Distant}, // Physical budget exceeded, drops to Presentation
        {4, {4, 0, 0}, Tier::Distant}  // Presentation budget exceeded, drops to Distant
    };

    ActivationPlan plan = ActivationPlanner::plan(pos, pos, config, inputs);
    assertPlan(plan.regions, {
        {1, Tier::Physical},
        {2, Tier::Physical},
        {3, Tier::Presentation}
        // 4 is distant
    });
}

void testInvalidInputs() {
    ActivationConfig validConfig{10, 0, 5, 20, 0, 5};
    Vec3 validPos{0, 0, 0};
    Vec3 invalidPos{std::nan(""), 0, 0};

    try {
        ActivationPlanner::plan(invalidPos, validPos, validConfig, {});
        throw std::runtime_error("Failed to throw on invalid body position");
    } catch (const std::invalid_argument&) {}

    try {
        ActivationPlanner::plan(validPos, invalidPos, validConfig, {});
        throw std::runtime_error("Failed to throw on invalid camera position");
    } catch (const std::invalid_argument&) {}

    ActivationConfig invalidConfig{std::nan(""), 0, 5, 20, 0, 5};
    try {
        ActivationPlanner::plan(validPos, validPos, invalidConfig, {});
        throw std::runtime_error("Failed to throw on invalid config");
    } catch (const std::invalid_argument&) {}

    std::vector<RegionInput> duplicateInputs = {
        {1, {0, 0, 0}, Tier::Distant},
        {1, {1, 0, 0}, Tier::Distant}
    };
    try {
        ActivationPlanner::plan(validPos, validPos, validConfig, duplicateInputs);
        throw std::runtime_error("Failed to throw on duplicate IDs");
    } catch (const std::invalid_argument&) {}

    std::vector<RegionInput> invalidRegionPos = {
        {1, {std::nan(""), 0, 0}, Tier::Distant}
    };
    try {
        ActivationPlanner::plan(validPos, validPos, validConfig, invalidRegionPos);
        throw std::runtime_error("Failed to throw on invalid region position");
    } catch (const std::invalid_argument&) {}

    std::vector<RegionInput> invalidTier = {
        {1, {0, 0, 0}, static_cast<Tier>(999)}
    };
    try {
        ActivationPlanner::plan(validPos, validPos, validConfig, invalidTier);
        throw std::runtime_error("Failed to throw on invalid tier");
    } catch (const std::invalid_argument&) {}
}

void testOverflowAndBounds() {
    const Vec3 origin{0, 0, 0};
    const ActivationConfig valid{10, 1, 5, 20, 1, 5};
    const double largest = std::numeric_limits<double>::max();
    std::vector<RegionInput> extremePosition = {
        {1, {largest, largest, largest}, Tier::Distant}
    };
    try {
        ActivationPlanner::plan(origin, origin, valid, extremePosition);
        throw std::runtime_error("Failed to throw on distance overflow");
    } catch (const std::overflow_error&) {}

    ActivationConfig extremeRadius{largest, largest, 5, 20, 1, 5};
    try {
        ActivationPlanner::plan(origin, origin, extremeRadius, {});
        throw std::runtime_error("Failed to throw on radius overflow");
    } catch (const std::overflow_error&) {}

    ActivationConfig extremeBudget{10, 1, ActivationPlanner::MaxRegions + 1, 20, 1, 5};
    try {
        ActivationPlanner::plan(origin, origin, extremeBudget, {});
        throw std::runtime_error("Failed to throw on budget overflow");
    } catch (const std::invalid_argument&) {}
}

void testInputsRemainUnchanged() {
    const Vec3 body{0, 0, 0};
    const Vec3 camera{100, 0, 0};
    const ActivationConfig config{10, 2, 5, 20, 5, 10};
    const std::vector<RegionInput> input = {
        {9, {5, 0, 0}, Tier::Distant},
        {4, {110, 0, 0}, Tier::Presentation},
        {7, {50, 0, 0}, Tier::Distant}
    };
    const auto before = input;
    (void)ActivationPlanner::plan(body, camera, config, input);
    if (input.size() != before.size()) throw std::runtime_error("Input size changed");
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i].id != before[i].id || input[i].position.x != before[i].position.x ||
            input[i].position.y != before[i].position.y || input[i].position.z != before[i].position.z ||
            input[i].currentTier != before[i].currentTier) {
            throw std::runtime_error("Planner mutated its input");
        }
    }
}

void testPhysicalRegionDowngradesWhenOutOfRange() {
    const ActivationConfig config{10, 2, 5, 20, 5, 10};
    const Vec3 body{0, 0, 0};
    const Vec3 camera{2000, 0, 0};
    const std::vector<RegionInput> input = {
        {42, {1000, 0, 0}, Tier::Physical}
    };
    const auto plan = ActivationPlanner::plan(body, camera, config, input);
    if (!plan.regions.empty()) {
        throw std::runtime_error("Out-of-range physical region should be omitted and downgraded to distant");
    }
}

int main() {
    try {
        testBodyCameraSeparation();
        testTiesAndOrderIndependence();
        testHysteresis();
        testBudgetSaturation();
        testInvalidInputs();
        testOverflowAndBounds();
        testInputsRemainUnchanged();
        testPhysicalRegionDowngradesWhenOutOfRange();
        std::cout << "All tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
