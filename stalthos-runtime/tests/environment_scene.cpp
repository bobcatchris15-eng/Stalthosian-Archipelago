#include "../engine/environment_scene.hpp"
#include <iostream>

using stalthos::engine::populateEnvironmentScene;
using stalthos::engine::EnvironmentSceneCounts;
using stalthos::domain::ReferenceLandscape;
using stalthos::domain::HabitatDressingBudget;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main() {
    try {
        std::cout << "[test] environment_scene starting...\n";

        wi::scene::Scene scene;
        ReferenceLandscape landscape(42);
        HabitatDressingBudget budget;
        budget.maxFallenLogs = 5;
        budget.maxRiverPebbles = 5;
        budget.maxDryBankClumps = 5;
        budget.maxBluffAccents = 5;

        EnvironmentSceneCounts counts = populateEnvironmentScene(scene, landscape, 42, budget);

        require(counts.objectCount > 0, "Should generate some objects");
        require(counts.vertexCount > 0, "Should generate some vertices");
        require(counts.triangleCount > 0, "Should generate some triangles");
        require(counts.logCount <= 5, "Log count within budget");
        require(counts.pebbleCount <= 5, "Pebble count within budget");
        require(counts.bankClumpCount <= 5, "Bank clump count within budget");
        require(counts.bluffAccentCount <= 5, "Bluff accent count within budget");

        // Second generation with same seed should be identical
        wi::scene::Scene scene2;
        EnvironmentSceneCounts counts2 = populateEnvironmentScene(scene2, landscape, 42, budget);

        require(counts.objectCount == counts2.objectCount, "Determinism: object count");
        require(counts.vertexCount == counts2.vertexCount, "Determinism: vertex count");
        require(counts.triangleCount == counts2.triangleCount, "Determinism: triangle count");
        require(counts.logCount == counts2.logCount, "Determinism: log count");
        require(counts.pebbleCount == counts2.pebbleCount, "Determinism: pebble count");

        // Ensure that meshes are generated for the scene
        require(scene.meshes.components.size() == counts.objectCount, "Each generated object has a mesh component");

        // Basic limits test
        HabitatDressingBudget zeroBudget;
        zeroBudget.maxFallenLogs = 0;
        zeroBudget.maxRiverPebbles = 0;
        zeroBudget.maxDryBankClumps = 0;
        zeroBudget.maxBluffAccents = 0;

        wi::scene::Scene sceneZero;
        EnvironmentSceneCounts countsZero = populateEnvironmentScene(sceneZero, landscape, 42, zeroBudget);
        require(countsZero.objectCount == 0, "Zero budget yields zero objects");
        require(countsZero.vertexCount == 0, "Zero budget yields zero vertices");

        std::cout << "[test] environment_scene passed successfully.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[test-failure] " << e.what() << "\n";
        return 1;
    }
}
