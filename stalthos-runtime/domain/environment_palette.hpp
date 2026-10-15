#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include "math_util.hpp"

namespace stalthos::domain {

// Physical/authoring material role identifiers for scenery in the reference landscape
// and runtime terrain/water presentation.
enum class SceneryMaterialRole : std::uint8_t {
    DryBank = 0,
    WetBank = 1,
    CreekBed = 2,
    Bluff = 3,
    Rock = 4,
    WoodyDebris = 5,
    WaterSurface = 6
};

inline constexpr std::array<std::string_view, 7> sceneryMaterialRoleNames{
    "DryBank",
    "WetBank",
    "CreekBed",
    "Bluff",
    "Rock",
    "WoodyDebris",
    "WaterSurface"
};

inline constexpr std::string_view roleName(SceneryMaterialRole role) {
    const auto idx = static_cast<std::size_t>(role);
    if (idx >= sceneryMaterialRoleNames.size()) {
        throw std::invalid_argument("unknown scenery material role");
    }
    return sceneryMaterialRoleNames[idx];
}

// Linear color representation (RGBA in [0.0, 1.0]).
struct ColorLinear {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    bool isFinite() const noexcept {
        return std::isfinite(r) && std::isfinite(g) && std::isfinite(b) && std::isfinite(a);
    }

    constexpr bool inUnitRange() const noexcept {
        return r >= 0.0f && r <= 1.0f &&
               g >= 0.0f && g <= 1.0f &&
               b >= 0.0f && b <= 1.0f &&
               a >= 0.0f && a <= 1.0f;
    }
};

// Ready-to-consume Wicked Engine material parameter payload.
// Directly maps to:
//   wi::scene::MaterialComponent::SetBaseColor(XMFLOAT4(baseColor.r, baseColor.g, baseColor.b, baseColor.a))
//   wi::scene::MaterialComponent::SetRoughness(roughness)
//   wi::scene::MaterialComponent::SetMetalness(metalness)
//   wi::scene::MaterialComponent::SetRefractionAmount(refractionAmount)
//   wi::scene::MaterialComponent::SetTransmissionAmount(transmissionAmount)
//   wi::scene::MaterialComponent::shaderType (PBR vs SHADERTYPE_WATER)
struct SceneryMaterialParameters {
    SceneryMaterialRole role = SceneryMaterialRole::DryBank;
    ColorLinear baseColor{};
    float roughness = 0.85f;
    float metalness = 0.0f;
    float refractionAmount = 0.0f;
    float transmissionAmount = 0.0f;
    bool isWaterShader = false;

    bool isFinite() const noexcept {
        return baseColor.isFinite() &&
               std::isfinite(roughness) &&
               std::isfinite(metalness) &&
               std::isfinite(refractionAmount) &&
               std::isfinite(transmissionAmount);
    }
};

// Bounded deterministic environment palette generator.
// Generates plausible, restrained non-neon linear color values and roughness ranges
// with strict separation between bed and water surface.
class EnvironmentPalette {
public:
    static constexpr float kMinColorLuminance = 0.01f;
    static constexpr float kMaxColorLuminance = 0.90f;

    // Evaluate material parameters for a given role and deterministic seed.
    // Throws std::invalid_argument if the role is out of range.
    static SceneryMaterialParameters evaluate(SceneryMaterialRole role, std::uint64_t seed) {
        validateRole(role);

        // Derive independent salt per role to prevent correlated cross-role variation
        const std::uint64_t roleSeed = hashMix(seed ^ (UINT64_C(0x517cc1b727220a95) * (static_cast<std::uint64_t>(role) + 1U)));

        // Unit randoms for hue/brightness and roughness jitter
        const float uCol = static_cast<float>(hashUnit(hashMix(roleSeed + 1U)));
        const float uJitter = static_cast<float>(hashUnit(hashMix(roleSeed + 2U)));
        const float uRough = static_cast<float>(hashUnit(hashMix(roleSeed + 3U)));

        SceneryMaterialParameters params{};
        params.role = role;
        params.metalness = 0.0f; // Scenery materials are non-metallic dielectrics

        switch (role) {
        case SceneryMaterialRole::DryBank: {
            // Earthy loamy bank: muted brownish-olive silt and dry soil.
            // Restrained variation: +/- 8% luminance, subtle warm/cool shift.
            const float tint = (uCol - 0.5f) * 0.04f;
            const float scale = 0.92f + uJitter * 0.16f;
            params.baseColor = ColorLinear{
                std::clamp((0.26f + tint) * scale, 0.18f, 0.35f),
                std::clamp((0.24f + tint * 0.5f) * scale, 0.17f, 0.33f),
                std::clamp((0.17f - tint) * scale, 0.11f, 0.25f),
                1.0f
            };
            // Dry soil: matte, roughness [0.82, 0.94]
            params.roughness = 0.82f + uRough * 0.12f;
            params.refractionAmount = 0.0f;
            params.transmissionAmount = 0.0f;
            params.isWaterShader = false;
            break;
        }

        case SceneryMaterialRole::WetBank: {
            // Moistened riparian sediment: darkened silt, higher specular gloss (lower roughness).
            // Moisture deepens albedo by ~40% and lowers roughness towards ~0.45 - 0.65.
            const float tint = (uCol - 0.5f) * 0.03f;
            const float scale = 0.92f + uJitter * 0.16f;
            params.baseColor = ColorLinear{
                std::clamp((0.14f + tint) * scale, 0.08f, 0.20f),
                std::clamp((0.12f + tint * 0.5f) * scale, 0.07f, 0.18f),
                std::clamp((0.09f - tint) * scale, 0.05f, 0.14f),
                1.0f
            };
            // Wet soil: semi-gloss sheen, roughness [0.45, 0.65]
            params.roughness = 0.45f + uRough * 0.20f;
            params.refractionAmount = 0.0f;
            params.transmissionAmount = 0.0f;
            params.isWaterShader = false;
            break;
        }

        case SceneryMaterialRole::CreekBed: {
            // Submerged gravel, sand, and submerged rock under the creek channel.
            // Distinct from water surface: opaque solid substrate, dark waterlogged pebbles.
            const float tint = (uCol - 0.5f) * 0.03f;
            const float scale = 0.90f + uJitter * 0.20f;
            params.baseColor = ColorLinear{
                std::clamp((0.11f + tint) * scale, 0.06f, 0.17f),
                std::clamp((0.10f + tint * 0.4f) * scale, 0.05f, 0.16f),
                std::clamp((0.08f - tint * 0.5f) * scale, 0.04f, 0.14f),
                1.0f
            };
            // Submerged substrate: smoothed pebble roughness [0.50, 0.75]
            params.roughness = 0.50f + uRough * 0.25f;
            params.refractionAmount = 0.0f;
            params.transmissionAmount = 0.0f;
            params.isWaterShader = false;
            break;
        }

        case SceneryMaterialRole::Bluff: {
            // Resistant sedimentary / mudstone cliff face.
            // Weathered warm-grey / olive-khaki mineral tones.
            const float tint = (uCol - 0.5f) * 0.05f;
            const float scale = 0.92f + uJitter * 0.16f;
            params.baseColor = ColorLinear{
                std::clamp((0.27f + tint) * scale, 0.20f, 0.36f),
                std::clamp((0.26f + tint * 0.6f) * scale, 0.19f, 0.35f),
                std::clamp((0.23f - tint * 0.3f) * scale, 0.16f, 0.32f),
                1.0f
            };
            // Rough fractured stone face: roughness [0.75, 0.92]
            params.roughness = 0.75f + uRough * 0.17f;
            params.refractionAmount = 0.0f;
            params.transmissionAmount = 0.0f;
            params.isWaterShader = false;
            break;
        }

        case SceneryMaterialRole::Rock: {
            // Outcrop boulders and detached limestone / silicified caprock.
            // Mineral grey with very subtle lichen / weathering cool undertone.
            const float tint = (uCol - 0.5f) * 0.04f;
            const float scale = 0.92f + uJitter * 0.16f;
            params.baseColor = ColorLinear{
                std::clamp((0.22f + tint) * scale, 0.15f, 0.30f),
                std::clamp((0.23f + tint * 0.8f) * scale, 0.16f, 0.31f),
                std::clamp((0.22f + tint * 0.2f) * scale, 0.15f, 0.30f),
                1.0f
            };
            // Weathered rock face: roughness [0.70, 0.88]
            params.roughness = 0.70f + uRough * 0.18f;
            params.refractionAmount = 0.0f;
            params.transmissionAmount = 0.0f;
            params.isWaterShader = false;
            break;
        }

        case SceneryMaterialRole::WoodyDebris: {
            // Weathered driftwood, branch litter, and fallen bark.
            // Desaturated weathered lignified grey-brown.
            const float tint = (uCol - 0.5f) * 0.04f;
            const float scale = 0.92f + uJitter * 0.16f;
            params.baseColor = ColorLinear{
                std::clamp((0.18f + tint) * scale, 0.11f, 0.26f),
                std::clamp((0.15f + tint * 0.5f) * scale, 0.09f, 0.22f),
                std::clamp((0.11f - tint * 0.4f) * scale, 0.06f, 0.18f),
                1.0f
            };
            // Fibrous matte bark: roughness [0.80, 0.95]
            params.roughness = 0.80f + uRough * 0.15f;
            params.refractionAmount = 0.0f;
            params.transmissionAmount = 0.0f;
            params.isWaterShader = false;
            break;
        }

        case SceneryMaterialRole::WaterSurface: {
            // Physical water surface mesh: transparent/refractive fluid boundary,
            // strictly separated from creek bed.
            // Subdued organic freshwater coloration (absorptive greenish-teal tint).
            const float tint = (uCol - 0.5f) * 0.02f;
            const float scale = 0.95f + uJitter * 0.10f;
            params.baseColor = ColorLinear{
                std::clamp((0.04f + tint * 0.3f) * scale, 0.02f, 0.07f),
                std::clamp((0.12f + tint) * scale, 0.08f, 0.18f),
                std::clamp((0.14f - tint * 0.5f) * scale, 0.09f, 0.20f),
                0.75f // Alpha represents base water body extinction/opacity
            };
            // Fluid surface: smooth specular specular reflection, roughness [0.08, 0.22]
            params.roughness = 0.08f + uRough * 0.14f;
            // Freshwater refraction index delta (~1.333) mapped into Wicked refraction scale
            params.refractionAmount = 0.04f + uJitter * 0.02f;
            params.transmissionAmount = 0.85f;
            params.isWaterShader = true;
            break;
        }
        }

        // Defensive validation against finite and unit range limits
        if (!params.isFinite()) {
            throw std::runtime_error("non-finite material parameter generated");
        }
        if (!params.baseColor.inUnitRange()) {
            throw std::runtime_error("material base color out of [0, 1] unit range");
        }
        if (params.roughness < 0.0f || params.roughness > 1.0f) {
            throw std::runtime_error("material roughness out of [0, 1] unit range");
        }

        return params;
    }

private:
    static void validateRole(SceneryMaterialRole role) {
        const auto idx = static_cast<std::size_t>(role);
        if (idx >= sceneryMaterialRoleNames.size()) {
            throw std::invalid_argument("scenery material role out of range");
        }
    }
};

} // namespace stalthos::domain
