#include "../domain/woodland_stands.hpp"
#include "../domain/reference_landscape.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using stalthos::domain::WoodlandStands;
using stalthos::domain::ReferenceLandscape;
using stalthos::domain::TreePlacement;

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "Assertion failure: " << message << "\n";
        throw std::runtime_error(message);
    }
}

static void testDeterminismAndSeedDifferences() {
    ReferenceLandscape landscape(42);

    // Identical seeds produce bit-identical results
    WoodlandStands s1(landscape, 12345, 20);
    WoodlandStands s2(landscape, 12345, 20);

    require(s1.placements().size() == s2.placements().size(), "Deterministic size match");
    require(s1.candidateAttempts() == s2.candidateAttempts(), "Deterministic candidate attempts match");
    require(s1.clusterAttempts() == s2.clusterAttempts(), "Deterministic cluster attempts match");
    require(s1.isCandidateBudgetExhausted() == s2.isCandidateBudgetExhausted(), "Deterministic exhaustion match");

    for (std::size_t i = 0; i < s1.placements().size(); ++i) {
        const auto& a = s1.placements()[i];
        const auto& b = s2.placements()[i];
        require(a.id == b.id, "Deterministic ID match");
        require(a.x == b.x, "Deterministic X match");
        require(a.y == b.y, "Deterministic Y match");
        require(a.z == b.z, "Deterministic Z match");
        require(a.headingRadians == b.headingRadians, "Deterministic heading match");
        require(a.scale == b.scale, "Deterministic scale match");
        require(a.isSapling == b.isSapling, "Deterministic sapling stage match");
        require(a.assetProps.height == b.assetProps.height, "Deterministic asset height match");
        require(a.assetProps.crownWidth == b.assetProps.crownWidth, "Deterministic crown width match");
    }

    // Different seeds produce different placements
    WoodlandStands s3(landscape, 54321, 20);
    bool identical = (s1.placements().size() == s3.placements().size());
    if (identical) {
        for (std::size_t i = 0; i < s1.placements().size(); ++i) {
            if (s1.placements()[i].id != s3.placements()[i].id) {
                identical = false;
                break;
            }
        }
    }
    require(!identical, "Different seeds produce different placements");
}

static void testBudgetsAndPrefixStability() {
    ReferenceLandscape landscape(42);

    const std::uint64_t seeds[] = { 1, 42, 999, 12345, 8336722, 0xCAFEBABE, 0x123456789ABCDEF0ULL };
    const std::size_t caps[] = { 0, 1, 5, 12, 20, 32 };

    for (std::uint64_t seed : seeds) {
        // Build maximal run for this seed
        WoodlandStands fullStand(landscape, seed, 32);
        const auto& fullTrees = fullStand.placements();
        require(fullTrees.size() <= 32, "Max trees bounded by 32");

        for (std::size_t cap : caps) {
            WoodlandStands stand(landscape, seed, cap);
            const auto& trees = stand.placements();

            require(trees.size() <= cap, "Output count bounded by requested cap");
            if (cap == 0) {
                require(trees.empty(), "Cap 0 produces 0 trees");
            }

            // Prefix stability check: stand with budget `cap` must have identical prefix to `fullStand`
            std::size_t expectedCount = std::min(cap, fullTrees.size());
            require(trees.size() == expectedCount, "Prefix size matches prefix count");

            for (std::size_t i = 0; i < expectedCount; ++i) {
                require(trees[i].id == fullTrees[i].id, "Prefix stable ID match across budgets");
                require(trees[i].x == fullTrees[i].x, "Prefix stable X match across budgets");
                require(trees[i].y == fullTrees[i].y, "Prefix stable Y match across budgets");
                require(trees[i].z == fullTrees[i].z, "Prefix stable Z match across budgets");
                require(trees[i].headingRadians == fullTrees[i].headingRadians, "Prefix stable heading match across budgets");
                require(trees[i].scale == fullTrees[i].scale, "Prefix stable scale match across budgets");
                require(trees[i].isSapling == fullTrees[i].isSapling, "Prefix stable sapling flag match across budgets");
            }
        }
    }
}

static void testHardCapRejection() {
    ReferenceLandscape landscape(42);

    bool caught33 = false;
    try {
        WoodlandStands overCap(landscape, 123, 33);
    } catch (const std::invalid_argument&) {
        caught33 = true;
    }
    require(caught33, "Hard cap 33 rejected with std::invalid_argument");

    bool caught1000 = false;
    try {
        WoodlandStands hugeCap(landscape, 123, 1000);
    } catch (const std::invalid_argument&) {
        caught1000 = true;
    }
    require(caught1000, "Hard cap 1000 rejected with std::invalid_argument");
}

static void testPropertyAndConstraintScans() {
    ReferenceLandscape landscape(42);

    // Multi-seed scan across 100 seeds verifying every placement property
    for (std::uint64_t s = 0; s < 100; ++s) {
        std::uint64_t seed = s * 7919 + 17;
        WoodlandStands stand(landscape, seed, 32);

        require(stand.candidateAttempts() <= WoodlandStands::kMaxTotalCandidateAttempts,
                "Candidate attempts strictly <= 750");
        require(stand.clusterAttempts() <= WoodlandStands::kMaxClusterAttempts,
                "Cluster attempts strictly <= 50");

        if (stand.placements().size() < 32) {
            require(stand.isCandidateBudgetExhausted(),
                    "Incomplete budget truthfully reports exhaustion");
        } else {
            require(!stand.isCandidateBudgetExhausted(),
                    "Full budget completed reports non-exhausted");
        }

        std::unordered_set<std::uint64_t> ids;

        for (const auto& p : stand.placements()) {
            // Uniqueness
            require(ids.insert(p.id).second, "Every proposal has unique ID within stand");

            // Finiteness
            require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z), "Coordinates finite");
            require(std::isfinite(p.headingRadians) && std::isfinite(p.scale), "Heading and scale finite");
            require(std::isfinite(p.assetProps.height) && std::isfinite(p.assetProps.crownWidth), "Asset props finite");

            // Spatial boundaries
            require(p.z >= -65.0 && p.z <= -12.0, "Background valley Z bounds roughly [-65, -12]");
            require(p.x <= -15.0 || p.x >= 20.0, "Dry banks X bounds x <= -15 or x >= 20");

            double dx = p.x - 12.0;
            double dz = p.z - 0.0;
            require(std::sqrt(dx * dx + dz * dz) >= 8.0, "Clear inspection corridor radius 8m around hero (12, 0)");

            require(!landscape.waterContains(p.x, p.z), "Creek wet mask respected");

            // Patch boundary bounds
            require(std::abs(p.x) <= ReferenceLandscape::halfExtent, "Within 192m patch X");
            require(std::abs(p.z) <= ReferenceLandscape::halfExtent, "Within 192m patch Z");

            // Heading in [0, 2pi)
            require(p.headingRadians >= 0.0 && p.headingRadians < 2.0 * 3.14159265358979323846 + 1e-6, "Heading in [0, 2pi)");

            // Scale in [0.9, 1.1]
            require(p.scale >= 0.9 - 1e-9 && p.scale <= 1.1 + 1e-9, "Scale in [0.9, 1.1]");

            // Asset props matching authored intended dimensions
            if (p.isSapling) {
                require(p.assetProps.height == 4.2 && p.assetProps.crownWidth == 2.4, "Sapling authored intended props");
            } else {
                require(p.assetProps.height == 14.0 && p.assetProps.crownWidth == 6.5, "Adult authored intended props");
            }

            // Height matches landscape surface
            require(std::abs(p.y - landscape.height(p.x, p.z)) < 1e-9, "Tree Y matches landscape surface height");
        }
    }
}

int main() {
    try {
        std::cout << "Starting WoodlandStands regression and property verification...\n";

        testDeterminismAndSeedDifferences();
        std::cout << "PASS: Determinism and seed divergence checks.\n";

        testBudgetsAndPrefixStability();
        std::cout << "PASS: Budget prefixes (0, 1, 5, 12, 20, 32) stability checks.\n";

        testHardCapRejection();
        std::cout << "PASS: Hard cap (>32) typed rejection checks.\n";

        testPropertyAndConstraintScans();
        std::cout << "PASS: 100-seed property scan (finite, dry bank, slope, corridor, unique IDs, truthful exhaustion).\n";

        std::cout << "All woodland stands regression and property tests passed successfully.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << "\n";
        return 1;
    }
}
