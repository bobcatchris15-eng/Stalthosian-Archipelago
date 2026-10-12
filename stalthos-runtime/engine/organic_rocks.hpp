#pragma once

#include "WickedEngine.h"
#include "domain/reference_landscape.hpp"
#include "domain/environment_palette.hpp"
#include "engine/environment_scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace stalthos::engine {

// Truthful, bounded counts of generated organic rock meshes.
struct OrganicRockCounts {
    std::size_t objectCount = 0;
    std::size_t vertexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t rockCount = 0;
    std::size_t boulderCount = 0;
};

namespace detail {

// Subdivides an initial icosahedron to produce a rounded, faceted convex-ish rocky volume.
// Every vertex position is perturbed deterministically using domain hash logic.
// Base shapes are flattened horizontally and elongated irregularly to mimic natural bedding planes
// and fracture facets without sharp zero-area poles.
inline GeneratedMesh createOrganicRockMesh(
    domain::PlacementKind kind,
    double baseScale,
    std::uint64_t itemSeed
) {
    GeneratedMesh mesh;

    // Boulder is twice the visual nominal scale of Rock, matching ReferenceLandscape scaling
    const float scaleMultiplier = (kind == domain::PlacementKind::Boulder) ? 2.0f : 1.0f;
    const float s = static_cast<float>(std::clamp(baseScale, 0.4, 3.5)) * scaleMultiplier;

    // Deterministic aspect ratios: rocks in natural rocky meadow / outcrop settings
    // exhibit flattened heights (Y), asymmetric length/width (X/Z), and subtle tilt.
    const float rx = s * (0.85f + 0.35f * hashUnitF(itemSeed + 11U));
    const float ry = s * (0.42f + 0.16f * hashUnitF(itemSeed + 12U)); // flattened oblate height
    const float rz = s * (0.75f + 0.30f * hashUnitF(itemSeed + 13U));

    // Base icosahedron golden ratio geometry
    const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
    const XMFLOAT3 baseVerts[12] = {
        {-1.0f,  phi,  0.0f}, { 1.0f,  phi,  0.0f}, {-1.0f, -phi,  0.0f}, { 1.0f, -phi,  0.0f},
        { 0.0f, -1.0f,  phi}, { 0.0f,  1.0f,  phi}, { 0.0f, -1.0f, -phi}, { 0.0f,  1.0f, -phi},
        {  phi,  0.0f, -1.0f}, { phi,  0.0f,  1.0f}, { -phi,  0.0f, -1.0f}, {-phi,  0.0f,  1.0f}
    };

    const uint32_t baseTris[20][3] = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };

    // Subdivide each icosahedron triangle into 4 sub-triangles (1 subdivision step).
    // Total triangles: 20 * 4 = 80 triangles, 42 unique vertices.
    // 80 triangles is well within the <1500 triangles per rock budget while providing
    // believable organic faceting without boxiness or GPU burden.
    std::vector<XMFLOAT3> unitPoints;
    unitPoints.reserve(42);

    for (int i = 0; i < 12; ++i) {
        const auto& v = baseVerts[i];
        const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        unitPoints.push_back(XMFLOAT3(v.x / len, v.y / len, v.z / len));
    }

    auto getMidPoint = [&](uint32_t p1, uint32_t p2) -> uint32_t {
        const XMFLOAT3& v1 = unitPoints[p1];
        const XMFLOAT3& v2 = unitPoints[p2];
        const float mx = (v1.x + v2.x) * 0.5f;
        const float my = (v1.y + v2.y) * 0.5f;
        const float mz = (v1.z + v2.z) * 0.5f;
        const float len = std::sqrt(mx * mx + my * my + mz * mz);
        const float invLen = (len > 1e-6f) ? (1.0f / len) : 1.0f;
        const XMFLOAT3 mid(mx * invLen, my * invLen, mz * invLen);

        // Deduplicate shared edge midpoints
        for (uint32_t i = 12; i < static_cast<uint32_t>(unitPoints.size()); ++i) {
            const float dx = unitPoints[i].x - mid.x;
            const float dy = unitPoints[i].y - mid.y;
            const float dz = unitPoints[i].z - mid.z;
            if (dx * dx + dy * dy + dz * dz < 1e-8f) {
                return i;
            }
        }
        unitPoints.push_back(mid);
        return static_cast<uint32_t>(unitPoints.size() - 1);
    };

    struct SubTri { uint32_t a, b, c; };
    std::vector<SubTri> subdividedTris;
    subdividedTris.reserve(80);

    for (const auto& tri : baseTris) {
        const uint32_t a = tri[0];
        const uint32_t b = tri[1];
        const uint32_t c = tri[2];

        const uint32_t ab = getMidPoint(a, b);
        const uint32_t bc = getMidPoint(b, c);
        const uint32_t ca = getMidPoint(c, a);

        subdividedTris.push_back({a, ab, ca});
        subdividedTris.push_back({b, bc, ab});
        subdividedTris.push_back({c, ca, bc});
        subdividedTris.push_back({ab, bc, ca});
    }

    // Now perturb vertices using deterministic domain hashes.
    // Apply faceted noise, bedding flattening, and subtle vertical offset.
    mesh.positions.reserve(unitPoints.size());
    mesh.indices.reserve(subdividedTris.size() * 3);

    const XMFLOAT3 center(0.0f, 0.0f, 0.0f);
    float minY = 1e6f;

    for (std::size_t i = 0; i < unitPoints.size(); ++i) {
        const auto& p = unitPoints[i];
        const std::uint64_t vSeed = itemSeed + 1000U + static_cast<std::uint64_t>(i) * 17U;

        // Controlled radial perturbation (0.80 to 1.20) for natural mineral faceting
        const float radialJitter = 1.0f + (hashUnitF(vSeed) - 0.5f) * 0.35f;

        // Bedding plane flattening: flatten more toward bottom (p.y < 0) for stable ground contact
        float yFactor = (p.y < 0.0f) ? 0.75f : 1.0f;

        const float vx = p.x * rx * radialJitter;
        const float vy = p.y * ry * radialJitter * yFactor;
        const float vz = p.z * rz * radialJitter;

        mesh.positions.push_back(XMFLOAT3(vx, vy, vz));
        if (vy < minY) {
            minY = vy;
        }
    }

    // Emit oriented triangles ensuring Wicked Engine reversed outward cross convention
    // via detail::addOrientedTriangle
    for (const auto& st : subdividedTris) {
        addOrientedTriangle(mesh.indices, mesh.positions, st.a, st.b, st.c, center);
    }

    mesh.groundOffset = -minY;
    return mesh;
}

} // namespace detail

// Primary helper to populate organic, non-box rocks and boulders from ReferenceLandscape.
// Consumes only PlacementKind::Rock and PlacementKind::Boulder.
// Returns truthful counts of objects, vertices, and triangles created.
inline OrganicRockCounts populateOrganicRocks(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    std::uint64_t seed = 1
) {
    using namespace domain;

    OrganicRockCounts counts{};

    // Hard safety limit: cap maximum rock objects to 128
    constexpr std::size_t kMaxRockObjects = 128;

    // 1. Evaluate shared earthy PBR material from EnvironmentPalette for rocks.
    // Uses SceneryMaterialRole::Rock (dielectric weathered mineral tones).
    const wi::ecs::Entity rockMatEntity = scene.Entity_CreateMaterial("scenery_mat_organic_rock");
    if (auto* mat = scene.materials.GetComponent(rockMatEntity)) {
        const auto p = EnvironmentPalette::evaluate(SceneryMaterialRole::Rock, seed);
        mat->SetBaseColor(DirectX::XMFLOAT4(p.baseColor.r, p.baseColor.g, p.baseColor.b, p.baseColor.a));
        mat->SetRoughness(p.roughness);
        mat->SetMetalness(p.metalness);
    }

    std::uint64_t placementIdx = 0;
    for (const auto& placement : landscape.placements()) {
        ++placementIdx;

        // Consume only Rock and Boulder placements
        if (placement.kind != PlacementKind::Rock && placement.kind != PlacementKind::Boulder) {
            continue;
        }

        if (counts.objectCount >= kMaxRockObjects) {
            break;
        }

        // Defensive checks for patch bounds and finite scale
        if (!landscape.inBounds(placement.x, placement.z)) {
            continue;
        }
        if (!std::isfinite(placement.scale) || placement.scale <= 0.0) {
            continue;
        }

        // Domain hash seed derived per placement
        const std::uint64_t itemSeed = hashMix(seed ^ (UINT64_C(0xD1B54A32D192ED03) * placementIdx));

        // Generate faceted, irregular convex-ish rock volume
        detail::GeneratedMesh mesh = detail::createOrganicRockMesh(placement.kind, placement.scale, itemSeed);
        if (mesh.positions.empty() || mesh.indices.empty()) {
            continue;
        }

        const std::size_t vCount = mesh.positions.size();
        const std::size_t tCount = mesh.indices.size() / 3;

        const std::string name = (placement.kind == PlacementKind::Boulder ? "organic_boulder_" : "organic_rock_")
                               + std::to_string(counts.objectCount);

        // Ground contact positioning using analytic height and groundContactOffset
        detail::createScenePropEntity(
            scene,
            name,
            std::move(mesh),
            rockMatEntity,
            landscape,
            placement.x,
            landscape.height(placement.x, placement.z),
            placement.z,
            placement.headingRadians,
            0.0, // Natural resting orientation without artificial pitch/roll tumble
            0.0
        );

        ++counts.objectCount;
        counts.vertexCount += vCount;
        counts.triangleCount += tCount;

        if (placement.kind == PlacementKind::Boulder) {
            ++counts.boulderCount;
        } else {
            ++counts.rockCount;
        }
    }

    return counts;
}

// Convenience alias matching PascalCase convention
inline OrganicRockCounts PopulateOrganicRocks(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    std::uint64_t seed = 1
) {
    return populateOrganicRocks(scene, landscape, seed);
}

} // namespace stalthos::engine

namespace stalthos {
    using engine::populateOrganicRocks;
    using engine::PopulateOrganicRocks;
    using engine::OrganicRockCounts;
}
