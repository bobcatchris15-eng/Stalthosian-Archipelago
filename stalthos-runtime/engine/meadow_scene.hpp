#pragma once

#include "WickedEngine.h"
#include "domain/reference_landscape.hpp"
#include "domain/environment_palette.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace stalthos::engine {

// Summary of created meadow groundcover batch objects and truthful counts.
struct MeadowSceneCounts {
    std::size_t batchObjectCount = 0;
    std::size_t tuftCount = 0;
    std::size_t vertexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t rejectedWetSamples = 0;
};

namespace meadow_detail {

using namespace DirectX;

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

// Evaluates a double-precision unit uniform in [0.0, 1.0) using domain::hashMix and hashUnit
inline double meadowHashUnit(std::uint64_t v) noexcept {
    return domain::hashUnit(domain::hashMix(v));
}

// Single-precision unit uniform in [0.0, 1.0)
inline float meadowHashUnitF(std::uint64_t v) noexcept {
    return static_cast<float>(meadowHashUnit(v));
}

// Emits a double-sided triangle (i0, i1, i2).
// Foliage material has SetDoubleSided(true); single face emitted to prevent normal cancellation.
inline void addDoubleSidedBladeTriangle(
    std::vector<uint32_t>& indices,
    uint32_t i0, uint32_t i1, uint32_t i2
) {
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);
}

// Raw geometry buffer accumulating batched tufts for a specific habitat / material grouping.
struct MeadowBatchGeometry {
    std::vector<XMFLOAT3> positions;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> vertexColors; // Packed RGBA via wi::Color::rgba
    std::size_t tuftCount = 0;
};

// Morphotype 1: Slender curved blade with natural arching droop.
// 5 vertices (2 root, 2 mid, 1 tip), 3 double-sided triangles.
inline void appendSlenderCurvedBlade(
    MeadowBatchGeometry& batch,
    double worldX, double groundY, double worldZ,
    float azimuth, float bladeHeight, float baseWidth,
    float archReach, float droopFactor,
    uint32_t colRoot, uint32_t colMid, uint32_t colTip
) {
    const float cosA = std::cos(azimuth);
    const float sinA = std::sin(azimuth);

    const float tx = -sinA;
    const float tz =  cosA;

    const float rRoot = 0.015f * archReach;
    const float rMid  = 0.55f * archReach;
    const float rTip  = archReach;

    const float wRoot = baseWidth;
    const float wMid  = baseWidth * 0.48f;

    const float y0 = static_cast<float>(groundY);
    // Peak of graceful curve at mid-span
    const float yMid = y0 + bladeHeight * 0.72f;
    // Drooping arch at tip curving down towards soil
    const float yTip = y0 + std::max(0.018f, bladeHeight * (0.32f - 0.18f * droopFactor));

    const uint32_t baseIdx = static_cast<uint32_t>(batch.positions.size());

    // Root vertices (2)
    const float cx0 = static_cast<float>(worldX) + cosA * rRoot;
    const float cz0 = static_cast<float>(worldZ) + sinA * rRoot;
    batch.positions.push_back(XMFLOAT3(cx0 - tx * wRoot, y0, cz0 - tz * wRoot));
    batch.positions.push_back(XMFLOAT3(cx0 + tx * wRoot, y0, cz0 + tz * wRoot));
    batch.vertexColors.push_back(colRoot);
    batch.vertexColors.push_back(colRoot);

    // Mid vertices (2)
    const float cx1 = static_cast<float>(worldX) + cosA * rMid;
    const float cz1 = static_cast<float>(worldZ) + sinA * rMid;
    batch.positions.push_back(XMFLOAT3(cx1 - tx * wMid, yMid, cz1 - tz * wMid));
    batch.positions.push_back(XMFLOAT3(cx1 + tx * wMid, yMid, cz1 + tz * wMid));
    batch.vertexColors.push_back(colMid);
    batch.vertexColors.push_back(colMid);

    // Tip vertex (1)
    const float cx2 = static_cast<float>(worldX) + cosA * rTip;
    const float cz2 = static_cast<float>(worldZ) + sinA * rTip;
    batch.positions.push_back(XMFLOAT3(cx2, yTip, cz2));
    batch.vertexColors.push_back(colTip);

    // 3 double-sided triangles
    const uint32_t v0 = baseIdx + 0;
    const uint32_t v1 = baseIdx + 1;
    const uint32_t v2 = baseIdx + 2;
    const uint32_t v3 = baseIdx + 3;
    const uint32_t vTip = baseIdx + 4;

    addDoubleSidedBladeTriangle(batch.indices, v0, v1, v2);
    addDoubleSidedBladeTriangle(batch.indices, v1, v3, v2);
    addDoubleSidedBladeTriangle(batch.indices, v2, v3, vTip);
}

// Morphotype 2: Small Cretaceous ground fern frond (pinna expansion at mid-span).
// 5 vertices (2 base, 2 wide mid-pinnae, 1 apex), 3 double-sided triangles.
inline void appendFernFrond(
    MeadowBatchGeometry& batch,
    double worldX, double groundY, double worldZ,
    float azimuth, float frondLen, float pinnaSpread,
    float droopFactor,
    uint32_t colRoot, uint32_t colMid, uint32_t colTip
) {
    const float cosA = std::cos(azimuth);
    const float sinA = std::sin(azimuth);
    const float tx = -sinA;
    const float tz =  cosA;

    const float y0 = static_cast<float>(groundY);
    // Frond arches low to ground
    const float yMid = y0 + frondLen * 0.52f;
    const float yTip = y0 + std::max(0.015f, frondLen * (0.22f - 0.12f * droopFactor));

    const float rRoot = 0.02f * frondLen;
    const float rMid  = 0.52f * frondLen;
    const float rTip  = frondLen;

    const float wRoot = 0.007f;
    const float wPinna = pinnaSpread * 0.5f;

    const uint32_t baseIdx = static_cast<uint32_t>(batch.positions.size());

    // Stipe/base vertices (2)
    const float cx0 = static_cast<float>(worldX) + cosA * rRoot;
    const float cz0 = static_cast<float>(worldZ) + sinA * rRoot;
    batch.positions.push_back(XMFLOAT3(cx0 - tx * wRoot, y0, cz0 - tz * wRoot));
    batch.positions.push_back(XMFLOAT3(cx0 + tx * wRoot, y0, cz0 + tz * wRoot));
    batch.vertexColors.push_back(colRoot);
    batch.vertexColors.push_back(colRoot);

    // Mid pinnae vertices (2)
    const float cx1 = static_cast<float>(worldX) + cosA * rMid;
    const float cz1 = static_cast<float>(worldZ) + sinA * rMid;
    batch.positions.push_back(XMFLOAT3(cx1 - tx * wPinna, yMid, cz1 - tz * wPinna));
    batch.positions.push_back(XMFLOAT3(cx1 + tx * wPinna, yMid, cz1 + tz * wPinna));
    batch.vertexColors.push_back(colMid);
    batch.vertexColors.push_back(colMid);

    // Frond tip apex (1)
    const float cx2 = static_cast<float>(worldX) + cosA * rTip;
    const float cz2 = static_cast<float>(worldZ) + sinA * rTip;
    batch.positions.push_back(XMFLOAT3(cx2, yTip, cz2));
    batch.vertexColors.push_back(colTip);

    // 3 double-sided triangles
    const uint32_t v0 = baseIdx + 0;
    const uint32_t v1 = baseIdx + 1;
    const uint32_t v2 = baseIdx + 2;
    const uint32_t v3 = baseIdx + 3;
    const uint32_t vTip = baseIdx + 4;

    addDoubleSidedBladeTriangle(batch.indices, v0, v1, v2);
    addDoubleSidedBladeTriangle(batch.indices, v1, v3, v2);
    addDoubleSidedBladeTriangle(batch.indices, v2, v3, vTip);
}

// Morphotype 3: Low herbaceous rosette leaf (creeping broadleaf / Beadflower analogue).
// 4 vertices (1 base, 2 mid-leaf flanks, 1 tip), 2 double-sided triangles.
inline void appendLowHerbLeaf(
    MeadowBatchGeometry& batch,
    double worldX, double groundY, double worldZ,
    float azimuth, float leafLen, float leafWidth,
    uint32_t colBase, uint32_t colBody, uint32_t colTip
) {
    const float cosA = std::cos(azimuth);
    const float sinA = std::sin(azimuth);
    const float tx = -sinA;
    const float tz =  cosA;

    const float y0 = static_cast<float>(groundY);
    // Hugs the soil with gentle upward tilt
    const float yMid = y0 + std::max(0.012f, leafLen * 0.28f);
    const float yTip = y0 + std::max(0.008f, leafLen * 0.14f);

    const float rMid = 0.50f * leafLen;
    const float rTip = leafLen;

    const float halfW = leafWidth * 0.5f;

    const uint32_t baseIdx = static_cast<uint32_t>(batch.positions.size());

    // Base vertex at rosette center (1)
    batch.positions.push_back(XMFLOAT3(static_cast<float>(worldX), y0, static_cast<float>(worldZ)));
    batch.vertexColors.push_back(colBase);

    // Mid flanks (2)
    const float cx1 = static_cast<float>(worldX) + cosA * rMid;
    const float cz1 = static_cast<float>(worldZ) + sinA * rMid;
    batch.positions.push_back(XMFLOAT3(cx1 - tx * halfW, yMid, cz1 - tz * halfW));
    batch.positions.push_back(XMFLOAT3(cx1 + tx * halfW, yMid, cz1 + tz * halfW));
    batch.vertexColors.push_back(colBody);
    batch.vertexColors.push_back(colBody);

    // Tip vertex (1)
    const float cx2 = static_cast<float>(worldX) + cosA * rTip;
    const float cz2 = static_cast<float>(worldZ) + sinA * rTip;
    batch.positions.push_back(XMFLOAT3(cx2, yTip, cz2));
    batch.vertexColors.push_back(colTip);

    // 2 double-sided triangles: diamond blade
    const uint32_t vBase = baseIdx + 0;
    const uint32_t vL    = baseIdx + 1;
    const uint32_t vR    = baseIdx + 2;
    const uint32_t vTip  = baseIdx + 3;

    addDoubleSidedBladeTriangle(batch.indices, vBase, vL, vR);
    addDoubleSidedBladeTriangle(batch.indices, vL, vTip, vR);
}

// Builds a multi-element groundcover unit (curved slender blades, fernlets, or low herbs)
inline void appendTuft(
    MeadowBatchGeometry& batch,
    double worldX, double groundY, double worldZ,
    domain::Habitat habitat,
    std::uint64_t tuftSeed
) {
    auto clampU8 = [](float v) -> uint8_t {
        return static_cast<uint8_t>(std::clamp(std::round(v), 0.0f, 255.0f));
    };

    const float jitterHue = (meadowHashUnitF(tuftSeed + 4U) - 0.5f) * 14.0f;
    const float jitterBright = (meadowHashUnitF(tuftSeed + 5U) - 0.5f) * 12.0f;

    // Seeded morphotype selection with local patch affinity:
    // Morphotypes:
    // 0 = Slender curved blade clump (early monocot / strap sedge)
    // 1 = Small fernlet rosette (disturbance / royal fern analogue)
    // 2 = Low creeping herb rosette (broadleaf ground cover)
    const float morphRoll = meadowHashUnitF(tuftSeed + 8U);
    int morphType = 0;

    if (habitat == domain::Habitat::Riparian) {
        // Riparian: mostly graceful arching sedges, with damp ground fernlets and occasional herbs
        morphType = (morphRoll < 0.60f) ? 0 : ((morphRoll < 0.88f) ? 1 : 2);
    } else if (habitat == domain::Habitat::OpenLowland) {
        // OpenLowland: diverse mosaic of low slender blades, creeping broadleaf herbs, and low fernlets
        morphType = (morphRoll < 0.45f) ? 0 : ((morphRoll < 0.78f) ? 2 : 1);
    } else { // Woodland
        // Woodland: dominated by shade-tolerant fernlets and low ground herbs, sparse fine blades
        morphType = (morphRoll < 0.55f) ? 1 : ((morphRoll < 0.85f) ? 2 : 0);
    }

    const float baseAzimuth = meadowHashUnitF(tuftSeed + 6U) * kTwoPi;

    if (morphType == 0) {
        // --- Morphotype 0: Slender Curved Blade Clump ---
        // 4 to 6 blades, significantly lower and more slender than initial draft
        const int numBlades = 4 + static_cast<int>(meadowHashUnitF(tuftSeed + 1U) * 2.99f);

        float baseHeight = 0.14f;
        float baseSpread = 0.12f;
        if (habitat == domain::Habitat::Riparian) {
            baseHeight = 0.16f + 0.09f * meadowHashUnitF(tuftSeed + 2U);
            baseSpread = 0.13f + 0.07f * meadowHashUnitF(tuftSeed + 3U);
        } else if (habitat == domain::Habitat::OpenLowland) {
            baseHeight = 0.11f + 0.07f * meadowHashUnitF(tuftSeed + 2U);
            baseSpread = 0.10f + 0.05f * meadowHashUnitF(tuftSeed + 3U);
        } else {
            baseHeight = 0.08f + 0.05f * meadowHashUnitF(tuftSeed + 2U);
            baseSpread = 0.08f + 0.04f * meadowHashUnitF(tuftSeed + 3U);
        }

        uint8_t rRoot = 48, gRoot = 56, bRoot = 26;
        uint8_t rMid  = 72, gMid  = 98,  bMid = 38;
        uint8_t rTip  = 98, gTip  = 126, bTip = 46;

        if (habitat == domain::Habitat::Riparian) {
            rRoot = 40; gRoot = 52; bRoot = 24;
            rMid  = 66; gMid  = 104; bMid = 40;
            rTip  = 92; gTip  = 132; bTip = 48;
        } else if (habitat == domain::Habitat::Woodland) {
            rRoot = 38; gRoot = 46; bRoot = 22;
            rMid  = 58; gMid  = 78;  bMid = 32;
            rTip  = 78; gTip  = 100; bTip = 38;
        }

        const uint32_t colRoot = wi::Color(
            clampU8(static_cast<float>(rRoot) + jitterBright * 0.4f),
            clampU8(static_cast<float>(gRoot) + jitterBright * 0.4f),
            clampU8(static_cast<float>(bRoot) + jitterBright * 0.2f),
            255
        ).rgba;

        const uint32_t colMid = wi::Color(
            clampU8(static_cast<float>(rMid) + jitterHue + jitterBright),
            clampU8(static_cast<float>(gMid) + jitterBright),
            clampU8(static_cast<float>(bMid) - jitterHue * 0.4f + jitterBright * 0.3f),
            255
        ).rgba;

        const uint32_t colTip = wi::Color(
            clampU8(static_cast<float>(rTip) + jitterHue * 1.3f + jitterBright * 1.1f),
            clampU8(static_cast<float>(gTip) + jitterBright * 1.1f),
            clampU8(static_cast<float>(bTip) - jitterHue * 0.5f + jitterBright * 0.4f),
            255
        ).rgba;

        for (int b = 0; b < numBlades; ++b) {
            const std::uint64_t bSeed = tuftSeed + 100U + static_cast<std::uint64_t>(b) * 19U;
            const float dAzimuth = (static_cast<float>(b) / static_cast<float>(numBlades)) * kTwoPi +
                                   (meadowHashUnitF(bSeed + 1U) - 0.5f) * 0.55f;
            const float bladeAzimuth = baseAzimuth + dAzimuth;

            const float bladeH = baseHeight * (0.82f + 0.32f * meadowHashUnitF(bSeed + 2U));
            const float bladeW = 0.009f + 0.006f * meadowHashUnitF(bSeed + 3U);
            const float archR  = baseSpread * (0.80f + 0.35f * meadowHashUnitF(bSeed + 4U));
            const float droop  = 0.35f + 0.55f * meadowHashUnitF(bSeed + 5U);

            appendSlenderCurvedBlade(
                batch,
                worldX, groundY, worldZ,
                bladeAzimuth, bladeH, bladeW,
                archR, droop,
                colRoot, colMid, colTip
            );
        }
    } else if (morphType == 1) {
        // --- Morphotype 1: Small Cretaceous Fernlet Rosette ---
        // 4 to 5 low spreading pinnate fronds
        const int numFronds = 4 + static_cast<int>(meadowHashUnitF(tuftSeed + 1U) * 1.99f);

        const float frondLen = 0.10f + 0.06f * meadowHashUnitF(tuftSeed + 2U);
        const float pinnaSpread = 0.032f + 0.016f * meadowHashUnitF(tuftSeed + 3U);

        // Rich deep fern green tones
        const uint8_t rRoot = 36, gRoot = 48, bRoot = 22;
        const uint8_t rMid  = 54, gMid  = 96, bMid = 36;
        const uint8_t rTip  = 80, gTip  = 124, bTip = 44;

        const uint32_t colRoot = wi::Color(
            clampU8(static_cast<float>(rRoot) + jitterBright * 0.3f),
            clampU8(static_cast<float>(gRoot) + jitterBright * 0.3f),
            clampU8(static_cast<float>(bRoot) + jitterBright * 0.2f),
            255
        ).rgba;

        const uint32_t colMid = wi::Color(
            clampU8(static_cast<float>(rMid) + jitterHue * 0.8f + jitterBright * 0.8f),
            clampU8(static_cast<float>(gMid) + jitterBright * 0.9f),
            clampU8(static_cast<float>(bMid) - jitterHue * 0.3f + jitterBright * 0.3f),
            255
        ).rgba;

        const uint32_t colTip = wi::Color(
            clampU8(static_cast<float>(rTip) + jitterHue + jitterBright),
            clampU8(static_cast<float>(gTip) + jitterBright),
            clampU8(static_cast<float>(bTip) - jitterHue * 0.4f + jitterBright * 0.4f),
            255
        ).rgba;

        for (int f = 0; f < numFronds; ++f) {
            const std::uint64_t fSeed = tuftSeed + 200U + static_cast<std::uint64_t>(f) * 23U;
            const float dAzimuth = (static_cast<float>(f) / static_cast<float>(numFronds)) * kTwoPi +
                                   (meadowHashUnitF(fSeed + 1U) - 0.5f) * 0.45f;
            const float frondAzimuth = baseAzimuth + dAzimuth;

            const float curLen = frondLen * (0.85f + 0.28f * meadowHashUnitF(fSeed + 2U));
            const float curSpread = pinnaSpread * (0.85f + 0.25f * meadowHashUnitF(fSeed + 3U));
            const float droop = 0.30f + 0.50f * meadowHashUnitF(fSeed + 4U);

            appendFernFrond(
                batch,
                worldX, groundY, worldZ,
                frondAzimuth, curLen, curSpread,
                droop,
                colRoot, colMid, colTip
            );
        }
    } else {
        // --- Morphotype 2: Low Creeping Herb Rosette ---
        // 4 to 5 flat spreading diamond/oval leaves hugging the earth
        const int numLeaves = 4 + static_cast<int>(meadowHashUnitF(tuftSeed + 1U) * 1.99f);

        const float leafLen = 0.065f + 0.035f * meadowHashUnitF(tuftSeed + 2U);
        const float leafWidth = 0.024f + 0.012f * meadowHashUnitF(tuftSeed + 3U);

        // Soft herbaceous green with warm undertone
        const uint8_t rBase = 44, gBase = 54, bBase = 24;
        const uint8_t rBody = 74, gBody = 104, bBody = 40;
        const uint8_t rApex = 98, gApex = 126, bApex = 48;

        const uint32_t colBase = wi::Color(
            clampU8(static_cast<float>(rBase) + jitterBright * 0.4f),
            clampU8(static_cast<float>(gBase) + jitterBright * 0.4f),
            clampU8(static_cast<float>(bBase) + jitterBright * 0.2f),
            255
        ).rgba;

        const uint32_t colBody = wi::Color(
            clampU8(static_cast<float>(rBody) + jitterHue * 0.9f + jitterBright * 0.9f),
            clampU8(static_cast<float>(gBody) + jitterBright),
            clampU8(static_cast<float>(bBody) - jitterHue * 0.3f + jitterBright * 0.3f),
            255
        ).rgba;

        const uint32_t colApex = wi::Color(
            clampU8(static_cast<float>(rApex) + jitterHue * 1.2f + jitterBright * 1.1f),
            clampU8(static_cast<float>(rApex) + jitterBright * 1.1f),
            clampU8(static_cast<float>(bApex) - jitterHue * 0.4f + jitterBright * 0.4f),
            255
        ).rgba;

        for (int l = 0; l < numLeaves; ++l) {
            const std::uint64_t lSeed = tuftSeed + 300U + static_cast<std::uint64_t>(l) * 29U;
            const float dAzimuth = (static_cast<float>(l) / static_cast<float>(numLeaves)) * kTwoPi +
                                   (meadowHashUnitF(lSeed + 1U) - 0.5f) * 0.40f;
            const float leafAzimuth = baseAzimuth + dAzimuth;

            const float curLen = leafLen * (0.85f + 0.30f * meadowHashUnitF(lSeed + 2U));
            const float curWid = leafWidth * (0.85f + 0.25f * meadowHashUnitF(lSeed + 3U));

            appendLowHerbLeaf(
                batch,
                worldX, groundY, worldZ,
                leafAzimuth, curLen, curWid,
                colBase, colBody, colApex
            );
        }
    }

    ++batch.tuftCount;
}

// Binds a batched meadow mesh into a scene entity and associates the shared material.
inline wi::ecs::Entity createMeadowBatchEntity(
    wi::scene::Scene& scene,
    const std::string& name,
    MeadowBatchGeometry&& batch,
    wi::ecs::Entity materialId
) {
    if (batch.positions.empty() || batch.indices.empty()) {
        return wi::ecs::INVALID_ENTITY;
    }

    const wi::ecs::Entity entity = wi::ecs::CreateEntity();
    if (!name.empty()) {
        scene.names.Create(entity) = name;
    }

    scene.layers.Create(entity);
    auto& transform = scene.transforms.Create(entity);
    auto& object = scene.objects.Create(entity);
    auto& meshComp = scene.meshes.Create(entity);

    object.meshID = entity;

    meshComp.vertex_positions = std::move(batch.positions);
    meshComp.indices = std::move(batch.indices);
    if (!batch.vertexColors.empty()) {
        meshComp.vertex_colors = std::move(batch.vertexColors);
    }

    auto& subset = meshComp.subsets.emplace_back();
    subset.indexCount = static_cast<uint32_t>(meshComp.indices.size());
    subset.materialID = materialId;

    meshComp.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
    if (wi::graphics::GetDevice() != nullptr) {
        meshComp.CreateRenderData();
    }

    // World coordinates are already embedded in the vertex positions; identity transform.
    transform.ClearTransform();
    transform.UpdateTransform();

    return entity;
}

} // namespace meadow_detail

// Primary entry point for populating habitat-specific low meadow/creek groundcover.
// Places clustered, irregular grass and sedge tufts across central region x/z in [-55, 55].
// Strictly skips Creek, Bluff, Outcrop, and ANY waterContains samples.
// Strictly respects hard bounds:
//   <= 4096 tufts
//   <= 80000 triangles
//   <= 8 scene entities
// Batched by habitat and color into 3 entities (Riparian, OpenLowland, Woodland).
// Double-sided, rough nonmetallic material with vertex colors enabled.
inline MeadowSceneCounts populateMeadowScene(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    std::uint64_t seed
) {
    MeadowSceneCounts counts{};

    // Hard safety limits
    constexpr std::size_t kMaxTufts = 4096;
    constexpr std::size_t kMaxTriangles = 80000;
    constexpr std::size_t kMaxEntities = 8;

    // 1. Create shared foliage materials for each habitat batch:
    // Riparian sedge material
    const wi::ecs::Entity matRiparianEntity = scene.Entity_CreateMaterial("meadow_mat_riparian");
    if (auto* mat = scene.materials.GetComponent(matRiparianEntity)) {
        const auto p = domain::EnvironmentPalette::evaluate(domain::SceneryMaterialRole::DryBank, seed ^ UINT64_C(0x1111));
        mat->SetBaseColor(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        mat->SetRoughness(std::clamp(p.roughness, 0.75f, 0.95f));
        mat->SetMetalness(0.0f);
        mat->SetUseVertexColors(true);
        mat->SetDoubleSided(true);
    }

    // OpenLowland meadow grass material
    const wi::ecs::Entity matLowlandEntity = scene.Entity_CreateMaterial("meadow_mat_open_lowland");
    if (auto* mat = scene.materials.GetComponent(matLowlandEntity)) {
        const auto p = domain::EnvironmentPalette::evaluate(domain::SceneryMaterialRole::DryBank, seed ^ UINT64_C(0x2222));
        mat->SetBaseColor(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        mat->SetRoughness(std::clamp(p.roughness, 0.78f, 0.95f));
        mat->SetMetalness(0.0f);
        mat->SetUseVertexColors(true);
        mat->SetDoubleSided(true);
    }

    // Woodland sparse groundcover material
    const wi::ecs::Entity matWoodlandEntity = scene.Entity_CreateMaterial("meadow_mat_woodland");
    if (auto* mat = scene.materials.GetComponent(matWoodlandEntity)) {
        const auto p = domain::EnvironmentPalette::evaluate(domain::SceneryMaterialRole::WoodyDebris, seed ^ UINT64_C(0x3333));
        mat->SetBaseColor(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        mat->SetRoughness(std::clamp(p.roughness, 0.82f, 0.95f));
        mat->SetMetalness(0.0f);
        mat->SetUseVertexColors(true);
        mat->SetDoubleSided(true);
    }

    // 2. Prepare 3 batch containers for the 3 allowed habitats
    meadow_detail::MeadowBatchGeometry riparianBatch;
    meadow_detail::MeadowBatchGeometry lowlandBatch;
    meadow_detail::MeadowBatchGeometry woodlandBatch;

    // Reserve reasonable capacities
    riparianBatch.positions.reserve(12000);
    riparianBatch.indices.reserve(24000);
    riparianBatch.vertexColors.reserve(12000);

    lowlandBatch.positions.reserve(16000);
    lowlandBatch.indices.reserve(32000);
    lowlandBatch.vertexColors.reserve(16000);

    woodlandBatch.positions.reserve(6000);
    woodlandBatch.indices.reserve(12000);
    woodlandBatch.vertexColors.reserve(6000);

    // 3. Deterministic sampling across central region x/z in [-55.0, 55.0]
    // We walk a regular lattice of cluster anchor centers, spaced 2.5 meters apart (45 x 45 grid),
    // and for each cell generate clustered jittered tuft candidate positions.
    // Total candidate tufts are budget-limited well below kMaxTufts (4096) and kMaxTriangles (80000).
    constexpr double kMinCoord = -55.0;
    constexpr double kMaxCoord = 55.0;
    constexpr double kCellSize = 2.5;
    constexpr int kGridSteps = 44; // 44 * 2.5 = 110m span [-55, +55]

    std::size_t totalTuftsGenerated = 0;
    std::size_t currentTriangles = 0;

    for (int iz = 0; iz < kGridSteps; ++iz) {
        const double cellZ = kMinCoord + (iz + 0.5) * kCellSize;
        for (int ix = 0; ix < kGridSteps; ++ix) {
            const double cellX = kMinCoord + (ix + 0.5) * kCellSize;

            if (totalTuftsGenerated >= kMaxTufts || currentTriangles + 24 >= kMaxTriangles) {
                break;
            }

            // Cell seed
            const std::uint64_t cellSeed = seed ^
                domain::hashMix(static_cast<std::uint64_t>(ix + 1000) ^
                (domain::hashMix(static_cast<std::uint64_t>(iz + 1000)) << 1U));

            // Habitat query at cluster center
            const domain::Habitat centerHabitat = landscape.habitat(cellX, cellZ);

            // Skip forbidden habitats immediately
            if (centerHabitat == domain::Habitat::Creek ||
                centerHabitat == domain::Habitat::Bluff ||
                centerHabitat == domain::Habitat::Outcrop) {
                continue;
            }

            // Cluster density based on habitat:
            // Riparian: high density near stream bank (1 to 2 tufts per 2.5m cell)
            // OpenLowland: medium-dense meadow carpet (0 to 2 tufts, ~60% occupancy)
            // Woodland: sparse dappled understory (0 to 1 tufts, ~25% occupancy)
            int candidatesInCell = 0;
            const float occRoll = meadow_detail::meadowHashUnitF(cellSeed + 1U);
            if (centerHabitat == domain::Habitat::Riparian) {
                candidatesInCell = (occRoll < 0.85f) ? (occRoll < 0.35f ? 2 : 1) : 0;
            } else if (centerHabitat == domain::Habitat::OpenLowland) {
                candidatesInCell = (occRoll < 0.65f) ? (occRoll < 0.20f ? 2 : 1) : 0;
            } else if (centerHabitat == domain::Habitat::Woodland) {
                candidatesInCell = (occRoll < 0.28f) ? 1 : 0;
            }

            // Denser bounded clusters; keep the same cell distribution and wet rejection.
            candidatesInCell *= 4;
            for (int c = 0; c < candidatesInCell; ++c) {
                if (totalTuftsGenerated >= kMaxTufts || currentTriangles + 24 >= kMaxTriangles) {
                    break;
                }

                const std::uint64_t candSeed = cellSeed + 100U + static_cast<std::uint64_t>(c) * 31U;

                // Clustered jitter within cell [-1.1m, +1.1m]
                const double jx = (meadow_detail::meadowHashUnit(candSeed + 1U) - 0.5) * 2.2;
                const double jz = (meadow_detail::meadowHashUnit(candSeed + 2U) - 0.5) * 2.2;
                const double tuftX = cellX + jx;
                const double tuftZ = cellZ + jz;

                // Enforce strict bounding box [-55, 55] and landscape bounds
                if (tuftX < kMinCoord || tuftX > kMaxCoord ||
                    tuftZ < kMinCoord || tuftZ > kMaxCoord ||
                    !landscape.inBounds(tuftX, tuftZ)) {
                    continue;
                }

                // Strictly test water containment: NO vegetation in water
                if (landscape.waterContains(tuftX, tuftZ)) {
                    ++counts.rejectedWetSamples;
                    continue;
                }

                // Query precise habitat at candidate location
                const domain::Habitat tuftHabitat = landscape.habitat(tuftX, tuftZ);
                if (tuftHabitat == domain::Habitat::Creek ||
                    tuftHabitat == domain::Habitat::Bluff ||
                    tuftHabitat == domain::Habitat::Outcrop) {
                    continue;
                }

                const double groundY = landscape.height(tuftX, tuftZ);
                if (!std::isfinite(groundY)) {
                    continue;
                }

                auto& batch = tuftHabitat == domain::Habitat::Riparian ? riparianBatch :
                    tuftHabitat == domain::Habitat::OpenLowland ? lowlandBatch : woodlandBatch;
                const auto positionsBefore = batch.positions.size();
                const auto indicesBefore = batch.indices.size();
                const auto colorsBefore = batch.vertexColors.size();
                const auto tuftsBefore = batch.tuftCount;
                meadow_detail::appendTuft(batch, tuftX, groundY, tuftZ, tuftHabitat, candSeed);
                const auto triAdded = (batch.indices.size() - indicesBefore) / 3;
                if (triAdded > kMaxTriangles - currentTriangles) {
                    batch.positions.resize(positionsBefore);
                    batch.indices.resize(indicesBefore);
                    batch.vertexColors.resize(colorsBefore);
                    batch.tuftCount = tuftsBefore;
                    break;
                }
                currentTriangles += triAdded;
                ++totalTuftsGenerated;
            }
        }
    }

    // 4. Create up to 3 scene batch entities (well within <= 8 scene entities hard limit)
    auto registerBatch = [&](
        const std::string& name,
        meadow_detail::MeadowBatchGeometry&& batch,
        wi::ecs::Entity matEntity
    ) {
        if (batch.positions.empty() || batch.indices.empty()) {
            return;
        }
        if (counts.batchObjectCount >= kMaxEntities) {
            return;
        }

        const std::size_t vCount = batch.positions.size();
        const std::size_t tCount = batch.indices.size() / 3;
        const std::size_t tufts = batch.tuftCount;

        meadow_detail::createMeadowBatchEntity(
            scene,
            name,
            std::move(batch),
            matEntity
        );

        ++counts.batchObjectCount;
        counts.tuftCount += tufts;
        counts.vertexCount += vCount;
        counts.triangleCount += tCount;
    };

    registerBatch("meadow_groundcover_riparian", std::move(riparianBatch), matRiparianEntity);
    registerBatch("meadow_groundcover_open_lowland", std::move(lowlandBatch), matLowlandEntity);
    registerBatch("meadow_groundcover_woodland", std::move(woodlandBatch), matWoodlandEntity);

    return counts;
}

// Convenience alias matching PascalCase convention
inline MeadowSceneCounts PopulateMeadowScene(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    std::uint64_t seed
) {
    return populateMeadowScene(scene, landscape, seed);
}

} // namespace stalthos::engine

namespace stalthos {
    using engine::populateMeadowScene;
    using engine::PopulateMeadowScene;
    using engine::MeadowSceneCounts;
}
