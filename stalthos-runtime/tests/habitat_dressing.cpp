#include "../domain/habitat_dressing.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

using stalthos::domain::DressingKind;
using stalthos::domain::DressingProposal;
using stalthos::domain::Habitat;
using stalthos::domain::HabitatDressing;
using stalthos::domain::HabitatDressingBudget;
using stalthos::domain::ReferenceLandscape;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    try {
        const ReferenceLandscape landscape(4242);
        const HabitatDressing dressingA(landscape, 90210);
        const HabitatDressing dressingSame(landscape, 90210);
        const HabitatDressing dressingOtherSeed(landscape, 90211);

        // 1. Proposals populated
        const auto& propsA = dressingA.proposals();
        const auto& propsSame = dressingSame.proposals();
        const auto& propsOther = dressingOtherSeed.proposals();

        require(!propsA.empty(), "proposals should not be empty");
        require(propsA.size() == propsSame.size(), "proposals size must match for same seed");
        require(dressingA.seed() == 90210, "seed getter matches");

        // 2. Budget limits respected
        const auto& budget = dressingA.budget();
        require(dressingA.countByKind(DressingKind::FallenLog) <= budget.maxFallenLogs, "fallen logs within budget");
        require(dressingA.countByKind(DressingKind::RiverPebble) <= budget.maxRiverPebbles, "river pebbles within budget");
        require(dressingA.countByKind(DressingKind::DryBankClump) <= budget.maxDryBankClumps, "dry bank clumps within budget");
        require(dressingA.countByKind(DressingKind::BluffAccent) <= budget.maxBluffAccents, "bluff accents within budget");
        require(propsA.size() <= budget.totalMax(), "total count within totalMax budget");

        // 3. Determinism and exact repeatability
        std::set<std::string> idsSeen;
        for (std::size_t i = 0; i < propsA.size(); ++i) {
            const auto& p1 = propsA[i];
            const auto& p2 = propsSame[i];

            require(p1.id == p2.id, "ID repeats deterministically");
            require(p1.kind == p2.kind, "kind repeats deterministically");
            require(p1.x == p2.x && p1.y == p2.y && p1.z == p2.z, "coords repeat deterministically");
            require(p1.scale == p2.scale, "scale repeats deterministically");
            require(p1.headingRadians == p2.headingRadians, "heading repeats deterministically");
            require(p1.pitchRadians == p2.pitchRadians, "pitch repeats deterministically");
            require(p1.rollRadians == p2.rollRadians, "roll repeats deterministically");

            // No duplicate IDs
            require(idsSeen.find(p1.id) == idsSeen.end(), "no duplicate IDs across proposals");
            idsSeen.insert(p1.id);

            // Bounded coordinates within patch
            require(landscape.inBounds(p1.x, p1.z), "placement is within patch bounds");
            require(std::isfinite(p1.x) && std::isfinite(p1.y) && std::isfinite(p1.z), "coords are finite");
            require(std::isfinite(p1.scale) && p1.scale > 0.0, "scale is finite and positive");
            require(std::isfinite(p1.headingRadians) && std::isfinite(p1.pitchRadians) && std::isfinite(p1.rollRadians), "rotations are finite");

            // Height matches landscape surface
            const double terrainY = landscape.height(p1.x, p1.z);
            require(std::abs(p1.y - terrainY) < 1e-9, "placement y matches landscape height");

            // Wet channel exclusions for terrestrial items
            if (p1.kind == DressingKind::FallenLog ||
                p1.kind == DressingKind::DryBankClump ||
                p1.kind == DressingKind::BluffAccent) {
                require(!landscape.waterContains(p1.x, p1.z), "terrestrial item must not be in water");
            }

            // Habitat checks
            const Habitat hab = landscape.habitat(p1.x, p1.z);
            if (p1.kind == DressingKind::BluffAccent) {
                require(hab == Habitat::Bluff || hab == Habitat::Outcrop, "bluff accent must be on bluff or outcrop");
            } else if (p1.kind == DressingKind::DryBankClump) {
                require(hab == Habitat::Riparian || hab == Habitat::OpenLowland, "dry bank clump on riparian or bank lowland");
            } else if (p1.kind == DressingKind::RiverPebble) {
                require(hab == Habitat::Creek || hab == Habitat::Riparian, "river pebble on creek or riparian bed margin");
            }
        }

        // 4. Seed variation produces different proposals
        bool variationDetected = false;
        if (propsA.size() != propsOther.size()) {
            variationDetected = true;
        } else {
            for (std::size_t i = 0; i < propsA.size(); ++i) {
                if (propsA[i].x != propsOther[i].x || propsA[i].z != propsOther[i].z) {
                    variationDetected = true;
                    break;
                }
            }
        }
        require(variationDetected, "different seed must vary dressing proposals");

        // 5. Zero or custom budget limits respected
        HabitatDressingBudget zeroBudget{0, 0, 0, 0};
        HabitatDressing zeroDressing(landscape, 12345, zeroBudget);
        require(zeroDressing.proposals().empty(), "zero budget produces empty proposals");

        HabitatDressingBudget tightBudget{2, 3, 4, 1};
        HabitatDressing tightDressing(landscape, 12345, tightBudget);
        require(tightDressing.countByKind(DressingKind::FallenLog) <= 2, "tight budget fallen logs");
        require(tightDressing.countByKind(DressingKind::RiverPebble) <= 3, "tight budget river pebbles");
        require(tightDressing.countByKind(DressingKind::DryBankClump) <= 4, "tight budget dry bank clumps");
        require(tightDressing.countByKind(DressingKind::BluffAccent) <= 1, "tight budget bluff accents");

        // 7. Adversarial budgets cannot overflow totalMax or cause an
        // unbounded reserve before the fixed candidate loops run.
        const auto largest = std::numeric_limits<std::size_t>::max();
        HabitatDressingBudget hugeBudget{largest, largest, largest, largest};
        require(hugeBudget.totalMax() == largest, "totalMax saturates on overflow");
        HabitatDressing hugeDressing(landscape, 90210, hugeBudget);
        require(hugeDressing.proposals().size() <= 322, "huge caller budget remains bounded by candidate loops");

        // 6. Invalid input handling on ReferenceLandscape queries
        bool rejectedNaN = false;
        try {
            (void)landscape.height(std::numeric_limits<double>::quiet_NaN(), 0.0);
        } catch (const std::invalid_argument&) {
            rejectedNaN = true;
        }
        require(rejectedNaN, "ReferenceLandscape rejects NaN coordinate");

        bool rejectedOOB = false;
        try {
            (void)landscape.habitat(100.0, 0.0);
        } catch (const std::invalid_argument&) {
            rejectedOOB = true;
        }
        require(rejectedOOB, "ReferenceLandscape rejects out of bounds coordinate");

        std::cout << "habitat dressing: proposals=" << propsA.size()
                  << " (logs=" << dressingA.countByKind(DressingKind::FallenLog)
                  << ", pebbles=" << dressingA.countByKind(DressingKind::RiverPebble)
                  << ", bank_clumps=" << dressingA.countByKind(DressingKind::DryBankClump)
                  << ", bluff_accents=" << dressingA.countByKind(DressingKind::BluffAccent)
                  << ") determinism, bounds, exclusions, and budget passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "habitat dressing test failed: " << e.what() << '\n';
        return 1;
    }
}
