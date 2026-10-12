#include "../domain/environment_palette.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using stalthos::domain::ColorLinear;
using stalthos::domain::EnvironmentPalette;
using stalthos::domain::roleName;
using stalthos::domain::SceneryMaterialParameters;
using stalthos::domain::SceneryMaterialRole;

static void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

int main() {
    try {
        std::cout << "[test] environment_palette standalone test starting...\n";

        // 1. Validate role names and enum limits
        require(roleName(SceneryMaterialRole::DryBank) == "DryBank", "DryBank role name");
        require(roleName(SceneryMaterialRole::WetBank) == "WetBank", "WetBank role name");
        require(roleName(SceneryMaterialRole::CreekBed) == "CreekBed", "CreekBed role name");
        require(roleName(SceneryMaterialRole::Bluff) == "Bluff", "Bluff role name");
        require(roleName(SceneryMaterialRole::Rock) == "Rock", "Rock role name");
        require(roleName(SceneryMaterialRole::WoodyDebris) == "WoodyDebris", "WoodyDebris role name");
        require(roleName(SceneryMaterialRole::WaterSurface) == "WaterSurface", "WaterSurface role name");

        // 2. Out-of-range role validation
        bool caughtOutOfRange = false;
        try {
            auto invalidRole = static_cast<SceneryMaterialRole>(99);
            (void)EnvironmentPalette::evaluate(invalidRole, 42);
        } catch (const std::invalid_argument&) {
            caughtOutOfRange = true;
        }
        require(caughtOutOfRange, "out-of-range role must throw invalid_argument");

        // 3. Repeatability: Same seed must produce identical parameter bytes across runs
        const std::uint64_t testSeeds[] = {0, 1, 42, 7123, 0x123456789abcdef0ULL};
        for (auto seed : testSeeds) {
            for (std::uint8_t r = 0; r <= 6; ++r) {
                const auto role = static_cast<SceneryMaterialRole>(r);
                const auto p1 = EnvironmentPalette::evaluate(role, seed);
                const auto p2 = EnvironmentPalette::evaluate(role, seed);

                require(p1.role == p2.role, "same seed preserves role");
                require(p1.baseColor.r == p2.baseColor.r, "same seed preserves color.r");
                require(p1.baseColor.g == p2.baseColor.g, "same seed preserves color.g");
                require(p1.baseColor.b == p2.baseColor.b, "same seed preserves color.b");
                require(p1.baseColor.a == p2.baseColor.a, "same seed preserves color.a");
                require(p1.roughness == p2.roughness, "same seed preserves roughness");
                require(p1.metalness == p2.metalness, "same seed preserves metalness");
                require(p1.refractionAmount == p2.refractionAmount, "same seed preserves refraction");
                require(p1.transmissionAmount == p2.transmissionAmount, "same seed preserves transmission");
                require(p1.isWaterShader == p2.isWaterShader, "same seed preserves isWaterShader");
            }
        }

        // 4. Variation check: Different seeds must produce restrained but distinct variation
        for (std::uint8_t r = 0; r <= 6; ++r) {
            const auto role = static_cast<SceneryMaterialRole>(r);
            const auto pA = EnvironmentPalette::evaluate(role, 100);
            const auto pB = EnvironmentPalette::evaluate(role, 200);

            const bool colorsDiffer = (pA.baseColor.r != pB.baseColor.r) ||
                                      (pA.baseColor.g != pB.baseColor.g) ||
                                      (pA.baseColor.b != pB.baseColor.b);
            const bool roughnessDiffers = (pA.roughness != pB.roughness);

            require(colorsDiffer, "different seeds must produce color variation");
            require(roughnessDiffers, "different seeds must produce roughness variation");
        }

        // 5. Plausibility bounds across 1000 randomized seeds:
        // - Colors must be non-neon, restrained, muted earth/rock/water tones
        // - Finite values
        // - Dry bank roughness > wet bank roughness
        // - Water surface vs creek bed strict separation
        for (std::uint64_t seed = 1; seed <= 1000; ++seed) {
            const auto dryBank = EnvironmentPalette::evaluate(SceneryMaterialRole::DryBank, seed);
            const auto wetBank = EnvironmentPalette::evaluate(SceneryMaterialRole::WetBank, seed);
            const auto creekBed = EnvironmentPalette::evaluate(SceneryMaterialRole::CreekBed, seed);
            const auto bluff = EnvironmentPalette::evaluate(SceneryMaterialRole::Bluff, seed);
            const auto rock = EnvironmentPalette::evaluate(SceneryMaterialRole::Rock, seed);
            const auto woody = EnvironmentPalette::evaluate(SceneryMaterialRole::WoodyDebris, seed);
            const auto water = EnvironmentPalette::evaluate(SceneryMaterialRole::WaterSurface, seed);

            // Bounded finite checks
            require(dryBank.isFinite() && dryBank.baseColor.inUnitRange(), "dryBank finite in [0, 1]");
            require(wetBank.isFinite() && wetBank.baseColor.inUnitRange(), "wetBank finite in [0, 1]");
            require(creekBed.isFinite() && creekBed.baseColor.inUnitRange(), "creekBed finite in [0, 1]");
            require(bluff.isFinite() && bluff.baseColor.inUnitRange(), "bluff finite in [0, 1]");
            require(rock.isFinite() && rock.baseColor.inUnitRange(), "rock finite in [0, 1]");
            require(woody.isFinite() && woody.baseColor.inUnitRange(), "woody finite in [0, 1]");
            require(water.isFinite() && water.baseColor.inUnitRange(), "water finite in [0, 1]");

            // No bright flat neon colors: max component luminance check
            require(dryBank.baseColor.r < 0.40f && dryBank.baseColor.g < 0.40f && dryBank.baseColor.b < 0.30f, "dryBank not neon");
            require(wetBank.baseColor.r < 0.25f && wetBank.baseColor.g < 0.25f && wetBank.baseColor.b < 0.20f, "wetBank darkened/not neon");
            require(creekBed.baseColor.r < 0.20f && creekBed.baseColor.g < 0.20f && creekBed.baseColor.b < 0.18f, "creekBed substrate dark/not neon");
            require(bluff.baseColor.r < 0.40f && bluff.baseColor.g < 0.40f && bluff.baseColor.b < 0.35f, "bluff muted mineral/not neon");
            require(rock.baseColor.r < 0.35f && rock.baseColor.g < 0.35f && rock.baseColor.b < 0.35f, "rock muted grey/not neon");
            require(woody.baseColor.r < 0.30f && woody.baseColor.g < 0.25f && woody.baseColor.b < 0.20f, "woody debris muted/not neon");
            require(water.baseColor.r < 0.10f && water.baseColor.g < 0.20f && water.baseColor.b < 0.22f, "water natural teal/not neon");

            // Non-metallic check: all terrestrial scenery is dielectric (metalness == 0.0)
            require(dryBank.metalness == 0.0f, "dryBank metalness 0");
            require(wetBank.metalness == 0.0f, "wetBank metalness 0");
            require(creekBed.metalness == 0.0f, "creekBed metalness 0");
            require(bluff.metalness == 0.0f, "bluff metalness 0");
            require(rock.metalness == 0.0f, "rock metalness 0");
            require(woody.metalness == 0.0f, "woody metalness 0");
            require(water.metalness == 0.0f, "water metalness 0");

            // Moisture darkening: wet bank albedo must be lower than dry bank albedo
            require(wetBank.baseColor.r < dryBank.baseColor.r, "moisture darkens bank red albedo");
            require(wetBank.baseColor.g < dryBank.baseColor.g, "moisture darkens bank green albedo");
            require(wetBank.baseColor.b < dryBank.baseColor.b, "moisture darkens bank blue albedo");

            // Roughness physical ordering:
            // Dry bank (matte soil) > Wet bank (damp soil sheen)
            require(dryBank.roughness > wetBank.roughness, "dry bank rougher than wet bank");
            require(dryBank.roughness >= 0.82f && dryBank.roughness <= 0.94f, "dry bank roughness bounds");
            require(wetBank.roughness >= 0.45f && wetBank.roughness <= 0.65f, "wet bank roughness bounds");

            // Rock / Bluff roughness
            require(bluff.roughness >= 0.75f && bluff.roughness <= 0.92f, "bluff roughness bounds");
            require(rock.roughness >= 0.70f && rock.roughness <= 0.88f, "rock roughness bounds");
            require(woody.roughness >= 0.80f && woody.roughness <= 0.95f, "woody debris roughness bounds");

            // Strict water surface vs creek bed separation:
            // 1) water uses water shader, bed does not
            require(water.isWaterShader == true, "water surface is water shader");
            require(creekBed.isWaterShader == false, "creek bed is solid PBR mesh");

            // 2) water has low fluid roughness [0.08, 0.22], bed has submerged pebble roughness [0.50, 0.75]
            require(water.roughness < 0.25f, "water surface has low specular roughness");
            require(creekBed.roughness >= 0.50f && creekBed.roughness <= 0.75f, "creek bed has submerged substrate roughness");
            require(creekBed.roughness > water.roughness + 0.25f, "strict roughness gap between water and bed");

            // 3) water has refraction/transmission, bed has none
            require(water.refractionAmount > 0.03f, "water has fluid refraction");
            require(water.transmissionAmount > 0.70f, "water has transmission");
            require(creekBed.refractionAmount == 0.0f, "creek bed has zero refraction");
            require(creekBed.transmissionAmount == 0.0f, "creek bed has zero transmission");
        }

        std::cout << "[test] environment_palette standalone test passed successfully across all checks.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[test-failure] " << e.what() << "\n";
        return 1;
    }
}
