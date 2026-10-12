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

// Procedural blade generator appending geometry for one curved/tapered blade to a batch.
// Blade geometry: 2 segments (root to mid, mid to arched tip).
// 5 vertices: 2 root (y=ground), 2 mid (y=ground + midH), 1 tip (y=ground + tipH).
// 3 triangles: 2 for lower trapezoid/quad, 1 for upper triangle tapering to sharp tip.
inline void appendFoliageBlade(
    MeadowBatchGeometry& batch,
    double worldX, double groundY, double worldZ,
    float azimuth, float bladeHeight, float baseWidth,
    float archReach, float leanAngle,
    uint32_t colRoot, uint32_t colMid, uint32_t colTip
) {
    const float cosA = std::cos(azimuth);
    const float sinA = std::sin(azimuth);

    // Tangent unit vectors perpendicular to outward blade direction in XZ plane
    const float tx = -sinA;
    const float tz =  cosA;

    // Arch and radial progression:
    // Root has small spread offset around clump center
    const float rRoot = 0.02f * bladeHeight;
    const float rMid  = 0.40f * archReach;
    const float rTip  = archReach;

    // Width tapering
    const float wRoot = baseWidth;
    const float wMid  = baseWidth * 0.58f;

    // Heights
    const float y0 = static_cast<float>(groundY);
    const float yMid = y0 + bladeHeight * 0.52f;
    // Arch droop at tip
    const float yTip = y0 + std::max(0.04f, bladeHeight * (0.85f - 0.25f * leanAngle));

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

    // Sharp tip vertex (1)
    const float cx2 = static_cast<float>(worldX) + cosA * rTip;
    const float cz2 = static_cast<float>(worldZ) + sinA * rTip;
    batch.positions.push_back(XMFLOAT3(cx2, yTip, cz2));
    batch.vertexColors.push_back(colTip);

    // 3 double-sided triangles
    // Lower quad (v0, v1, v2, v3)
    const uint32_t v0 = baseIdx + 0;
    const uint32_t v1 = baseIdx + 1;
    const uint32_t v2 = baseIdx + 2;
    const uint32_t v3 = baseIdx + 3;
    const uint32_t vTip = baseIdx + 4;

    addDoubleSidedBladeTriangle(batch.indices, v0, v1, v2);
    addDoubleSidedBladeTriangle(batch.indices, v1, v3, v2);

    // Upper triangle (v2, v3, vTip)
    addDoubleSidedBladeTriangle(batch.indices, v2, v3, vTip);
}

// Builds a multi-blade tuft (5 to 8 curved blades) grounded at (worldX, groundY, worldZ)
inline void appendTuft(
    MeadowBatchGeometry& batch,
    double worldX, double groundY, double worldZ,
    domain::Habitat habitat,
    std::uint64_t tuftSeed
) {
    // Number of blades per tuft: 5 to 7 blades
    const int numBlades = 5 + static_cast<int>(meadowHashUnitF(tuftSeed + 1U) * 2.99f);

    // Habitat-specific height and spread:
    // Riparian sedges: taller, arching, slightly broader [0.30, 0.55m]
    // OpenLowland meadow grasses: medium, lush [0.20, 0.42m]
    // Woodland: sparse, shade-tolerant low tufts [0.15, 0.32m]
    float baseHeight = 0.25f;
    float baseSpread = 0.14f;
    if (habitat == domain::Habitat::Riparian) {
        baseHeight = 0.30f + 0.22f * meadowHashUnitF(tuftSeed + 2U);
        baseSpread = 0.16f + 0.08f * meadowHashUnitF(tuftSeed + 3U);
    } else if (habitat == domain::Habitat::OpenLowland) {
        baseHeight = 0.20f + 0.20f * meadowHashUnitF(tuftSeed + 2U);
        baseSpread = 0.12f + 0.08f * meadowHashUnitF(tuftSeed + 3U);
    } else { // Woodland
        baseHeight = 0.15f + 0.15f * meadowHashUnitF(tuftSeed + 2U);
        baseSpread = 0.10f + 0.06f * meadowHashUnitF(tuftSeed + 3U);
    }

    // Muted earthy olive colors per tuft with deterministic subtle jitter:
    // Base: dark earthy olive-brown
    // Mid: rich muted olive / moss green
    // Tip: warm sunlit olive / straw ochre
    const float jitterHue = (meadowHashUnitF(tuftSeed + 4U) - 0.5f) * 16.0f;
    const float jitterBright = (meadowHashUnitF(tuftSeed + 5U) - 0.5f) * 14.0f;

    auto clampU8 = [](float v) -> uint8_t {
        return static_cast<uint8_t>(std::clamp(std::round(v), 0.0f, 255.0f));
    };

    uint8_t rRoot = 54, gRoot = 64, bRoot = 32;
    uint8_t rMid  = 78, gMid  = 106, bMid = 40;
    uint8_t rTip  = 110, gTip = 138, bTip = 52;

    if (habitat == domain::Habitat::Riparian) {
        // Riparian: slightly richer, deeper olive-green sedge
        rRoot = 48; gRoot = 60; bRoot = 28;
        rMid  = 72; gMid  = 112; bMid = 44;
        rTip  = 102; gTip = 142; bTip = 56;
    } else if (habitat == domain::Habitat::Woodland) {
        // Woodland: deep muted olive, less sunlit
        rRoot = 46; gRoot = 54; bRoot = 26;
        rMid  = 65; gMid  = 88;  bMid = 36;
        rTip  = 88;  gTip = 112; bTip = 46;
    }

    const uint32_t colRoot = wi::Color(
        clampU8(static_cast<float>(rRoot) + jitterBright * 0.5f),
        clampU8(static_cast<float>(gRoot) + jitterBright * 0.5f),
        clampU8(static_cast<float>(bRoot) + jitterBright * 0.3f),
        255
    ).rgba;

    const uint32_t colMid = wi::Color(
        clampU8(static_cast<float>(rMid) + jitterHue + jitterBright),
        clampU8(static_cast<float>(gMid) + jitterBright),
        clampU8(static_cast<float>(bMid) - jitterHue * 0.5f + jitterBright * 0.4f),
        255
    ).rgba;

    const uint32_t colTip = wi::Color(
        clampU8(static_cast<float>(rTip) + jitterHue * 1.5f + jitterBright * 1.2f),
        clampU8(static_cast<float>(gTip) + jitterBright * 1.2f),
        clampU8(static_cast<float>(bTip) - jitterHue * 0.7f + jitterBright * 0.5f),
        255
    ).rgba;

    // Distribute blades radially with uneven azimuth spacing and varying length
    const float baseAzimuth = meadowHashUnitF(tuftSeed + 6U) * kTwoPi;

    for (int b = 0; b < numBlades; ++b) {
        const std::uint64_t bSeed = tuftSeed + 100U + static_cast<std::uint64_t>(b) * 17U;
        const float dAzimuth = (static_cast<float>(b) / static_cast<float>(numBlades)) * kTwoPi +
                               (meadowHashUnitF(bSeed + 1U) - 0.5f) * 0.65f;
        const float bladeAzimuth = baseAzimuth + dAzimuth;

        const float bladeH = baseHeight * (0.80f + 0.35f * meadowHashUnitF(bSeed + 2U));
        const float bladeW = 0.016f + 0.012f * meadowHashUnitF(bSeed + 3U);
        const float archR  = baseSpread * (0.75f + 0.45f * meadowHashUnitF(bSeed + 4U));
        const float lean   = 0.2f + 0.7f * meadowHashUnitF(bSeed + 5U);

        appendFoliageBlade(
            batch,
            worldX, groundY, worldZ,
            bladeAzimuth, bladeH, bladeW,
            archR, lean,
            colRoot, colMid, colTip
        );
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

                // Append tuft to the appropriate batch
                if (tuftHabitat == domain::Habitat::Riparian) {
                    const std::size_t triBefore = riparianBatch.indices.size() / 3;
                    meadow_detail::appendTuft(riparianBatch, tuftX, groundY, tuftZ, tuftHabitat, candSeed);
                    const std::size_t triAdded = (riparianBatch.indices.size() / 3) - triBefore;
                    currentTriangles += triAdded;
                } else if (tuftHabitat == domain::Habitat::OpenLowland) {
                    const std::size_t triBefore = lowlandBatch.indices.size() / 3;
                    meadow_detail::appendTuft(lowlandBatch, tuftX, groundY, tuftZ, tuftHabitat, candSeed);
                    const std::size_t triAdded = (lowlandBatch.indices.size() / 3) - triBefore;
                    currentTriangles += triAdded;
                } else if (tuftHabitat == domain::Habitat::Woodland) {
                    const std::size_t triBefore = woodlandBatch.indices.size() / 3;
                    meadow_detail::appendTuft(woodlandBatch, tuftX, groundY, tuftZ, tuftHabitat, candSeed);
                    const std::size_t triAdded = (woodlandBatch.indices.size() / 3) - triBefore;
                    currentTriangles += triAdded;
                }

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
