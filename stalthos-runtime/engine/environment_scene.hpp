#pragma once

#include "WickedEngine.h"
#include "domain/reference_landscape.hpp"
#include "domain/environment_palette.hpp"
#include "domain/habitat_dressing.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <stdexcept>
#include <vector>

namespace stalthos::engine {

// Summary of created scenery objects and truthful geometric primitive counts.
struct EnvironmentSceneCounts {
    std::size_t objectCount = 0;
    std::size_t vertexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t logCount = 0;
    std::size_t pebbleCount = 0;
    std::size_t bankClumpCount = 0;
    std::size_t bluffAccentCount = 0;
};

namespace detail {

using namespace DirectX;

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

// Deterministic 64-bit unit hash helper
inline float hashUnitF(std::uint64_t salt) noexcept {
    return static_cast<float>(domain::hashUnit(domain::hashMix(salt)));
}

// Emits triangle (i0, i1, i2) ensuring Wicked Engine outward winding convention.
// Wicked computes triangle normals via cross(p2 - p0, p1 - p0).
// For closed/convex meshes, (faceCenter - meshCenter) points outward.
inline void addOrientedTriangle(
    std::vector<uint32_t>& indices,
    const std::vector<XMFLOAT3>& positions,
    uint32_t i0, uint32_t i1, uint32_t i2,
    const XMFLOAT3& meshCenter
) {
    const XMFLOAT3& p0 = positions[i0];
    const XMFLOAT3& p1 = positions[i1];
    const XMFLOAT3& p2 = positions[i2];

    const float d10x = p1.x - p0.x, d10y = p1.y - p0.y, d10z = p1.z - p0.z;
    const float d20x = p2.x - p0.x, d20y = p2.y - p0.y, d20z = p2.z - p0.z;

    // cross(p2 - p0, p1 - p0) in left-handed DirectX coordinates
    const float nx = d20y * d10z - d20z * d10y;
    const float ny = d20z * d10x - d20x * d10z;
    const float nz = d20x * d10y - d20y * d10x;

    const float fcx = (p0.x + p1.x + p2.x) * (1.0f / 3.0f) - meshCenter.x;
    const float fcy = (p0.y + p1.y + p2.y) * (1.0f / 3.0f) - meshCenter.y;
    const float fcz = (p0.z + p1.z + p2.z) * (1.0f / 3.0f) - meshCenter.z;

    const float dot = nx * fcx + ny * fcy + nz * fcz;
    if (dot < 0.0f) {
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);
    } else {
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
    }
}

// Double-sided triangle emitter for thin foliage blades (e.g. sedge/grass).
inline void addDoubleSidedTriangle(
    std::vector<uint32_t>& indices,
    uint32_t i0, uint32_t i1, uint32_t i2
) {
    indices.push_back(i0);
    indices.push_back(i1);
    indices.push_back(i2);
    // The foliage material is double-sided; duplicated reversed faces would cancel smooth normals.
}

// Procedural mesh data bundle
struct GeneratedMesh {
    std::vector<XMFLOAT3> positions;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> vertexColors; // Packed RGBA via wi::Color::rgba
    float groundOffset = 0.0f; // Local Y offset so prop sits on terrain
};

// Match TransformComponent::RotateRollPitchYaw in wiScene_Components.cpp.
inline XMFLOAT3 rotatePropVertex(const XMFLOAT3& p, float heading, float pitch, float roll) noexcept {
    XMVECTOR quat = XMQuaternionIdentity();
    const XMVECTOR x = XMQuaternionRotationRollPitchYaw(pitch, 0.0f, 0.0f);
    const XMVECTOR y = XMQuaternionRotationRollPitchYaw(0.0f, heading, 0.0f);
    const XMVECTOR z = XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, roll);
    quat = XMQuaternionMultiply(x, quat);
    quat = XMQuaternionMultiply(quat, y);
    quat = XMQuaternionMultiply(z, quat);
    quat = XMQuaternionNormalize(quat);
    XMFLOAT3 result;
    XMStoreFloat3(&result, XMVector3Rotate(XMLoadFloat3(&p), quat));
    return result;
}

inline float groundContactOffset(
    const GeneratedMesh& mesh, const domain::ReferenceLandscape& landscape,
    double worldX, double worldZ, float heading, float pitch, float roll
) {
    double required = -std::numeric_limits<double>::infinity();
    for (const XMFLOAT3& vertex : mesh.positions) {
        const XMFLOAT3 rotated = rotatePropVertex(vertex, heading, pitch, roll);
        const double x = worldX + rotated.x;
        const double z = worldZ + rotated.z;
        if (!landscape.inBounds(x, z)) continue;
        required = std::max(required, landscape.height(x, z) - rotated.y);
    }
    if (!std::isfinite(required)) return mesh.groundOffset;
    return static_cast<float>(required - landscape.height(worldX, worldZ));
}

// Generates an organic, tapered, multi-segment faceted log (fallen tree trunk).
inline GeneratedMesh createFallenLogMesh(double scale, std::uint64_t itemSeed) {
    GeneratedMesh mesh;
    const float s = static_cast<float>(std::clamp(scale, 0.4, 2.5));

    // Finite procedural scale parameters
    // Calibrated against a 1.8 m juvenile: mostly branches, occasional larger wood.
    const float length = s * (0.7f + 0.75f * hashUnitF(itemSeed + 10U));
    const float baseRadius = s * (0.08f + 0.04f * hashUnitF(itemSeed + 11U));
    const float tipRadius = baseRadius * (0.45f + 0.18f * hashUnitF(itemSeed + 12U)); // Natural tapering
    const float bowAmount = length * 0.04f * (hashUnitF(itemSeed + 13U) - 0.5f);

    constexpr int kRings = 4;
    constexpr int kSectors = 8; // Octagonal faceted cross section

    mesh.positions.reserve(kRings * kSectors + 2);
    mesh.indices.reserve((kRings - 1) * kSectors * 6 + 2 * kSectors * 3);

    const XMFLOAT3 center(0.0f, 0.0f, 0.0f);
    float minY = 1e6f;

    // Generate rings along local Z axis [-length/2, +length/2]
    for (int r = 0; r < kRings; ++r) {
        const float tr = static_cast<float>(r) / static_cast<float>(kRings - 1);
        const float z = -length * 0.5f + length * tr;
        const float currentRadius = baseRadius * (1.0f - tr) + tipRadius * tr;
        // Camber / bow curve in Y
        const float bowY = std::sin(kPi * tr) * bowAmount;

        for (int sIdx = 0; sIdx < kSectors; ++sIdx) {
            const float angle = kTwoPi * (static_cast<float>(sIdx) / static_cast<float>(kSectors));
            const std::uint64_t vSalt = itemSeed + 100U + static_cast<std::uint64_t>(r * kSectors + sIdx);
            const float radJitter = 1.0f + (hashUnitF(vSalt) - 0.5f) * 0.14f;
            const float rad = currentRadius * radJitter;

            const float x = rad * std::cos(angle);
            const float y = rad * std::sin(angle) + bowY;
            mesh.positions.push_back(XMFLOAT3(x, y, z));
            if (y < minY) minY = y;
        }
    }

    // Side wall quads
    for (int r = 0; r < kRings - 1; ++r) {
        const int ring0 = r * kSectors;
        const int ring1 = (r + 1) * kSectors;
        for (int sIdx = 0; sIdx < kSectors; ++sIdx) {
            const int nextSIdx = (sIdx + 1) % kSectors;
            const uint32_t v00 = static_cast<uint32_t>(ring0 + sIdx);
            const uint32_t v01 = static_cast<uint32_t>(ring0 + nextSIdx);
            const uint32_t v10 = static_cast<uint32_t>(ring1 + sIdx);
            const uint32_t v11 = static_cast<uint32_t>(ring1 + nextSIdx);

            addOrientedTriangle(mesh.indices, mesh.positions, v00, v10, v01, center);
            addOrientedTriangle(mesh.indices, mesh.positions, v10, v11, v01, center);
        }
    }

    // End cap at base (r = 0, z = -length/2)
    const uint32_t baseCenterIdx = static_cast<uint32_t>(mesh.positions.size());
    mesh.positions.push_back(XMFLOAT3(0.0f, 0.0f, -length * 0.5f));
    for (int sIdx = 0; sIdx < kSectors; ++sIdx) {
        const int nextSIdx = (sIdx + 1) % kSectors;
        addOrientedTriangle(mesh.indices, mesh.positions, baseCenterIdx, static_cast<uint32_t>(sIdx), static_cast<uint32_t>(nextSIdx), center);
    }

    // End cap at tip (r = kRings - 1, z = +length/2)
    const uint32_t tipCenterIdx = static_cast<uint32_t>(mesh.positions.size());
    mesh.positions.push_back(XMFLOAT3(0.0f, bowAmount, length * 0.5f));
    const int tipRingStart = (kRings - 1) * kSectors;
    for (int sIdx = 0; sIdx < kSectors; ++sIdx) {
        const int nextSIdx = (sIdx + 1) % kSectors;
        addOrientedTriangle(mesh.indices, mesh.positions, tipCenterIdx, static_cast<uint32_t>(tipRingStart + nextSIdx), static_cast<uint32_t>(tipRingStart + sIdx), center);
    }

    mesh.groundOffset = -minY;
    return mesh;
}

// Generates an oblate, water-smoothed, faceted river pebble / cobble stone.
inline GeneratedMesh createRiverPebbleMesh(double scale, std::uint64_t itemSeed) {
    GeneratedMesh mesh;
    const float s = static_cast<float>(std::clamp(scale, 0.1, 1.0));

    // Oblate ellipsoid radii with deterministic asymmetry
    const float rx = s * (0.45f + 0.18f * hashUnitF(itemSeed + 20U));
    const float ry = s * (0.24f + 0.08f * hashUnitF(itemSeed + 21U)); // Flattened oblate height
    const float rz = s * (0.38f + 0.14f * hashUnitF(itemSeed + 22U));

    constexpr int kBandVertices = 6;
    mesh.positions.reserve(2 + 2 * kBandVertices);
    mesh.indices.reserve(4 * kBandVertices * 3);

    const XMFLOAT3 center(0.0f, 0.0f, 0.0f);

    // Top and bottom pole vertices
    const uint32_t topPole = 0;
    const uint32_t bottomPole = 1;
    mesh.positions.push_back(XMFLOAT3(0.0f, ry, 0.0f));
    mesh.positions.push_back(XMFLOAT3(0.0f, -ry, 0.0f));

    // Upper ring at +0.45 ry
    const uint32_t upperStart = static_cast<uint32_t>(mesh.positions.size());
    for (int i = 0; i < kBandVertices; ++i) {
        const float angle = kTwoPi * (static_cast<float>(i) / static_cast<float>(kBandVertices));
        const float j = 1.0f + (hashUnitF(itemSeed + 200U + static_cast<std::uint64_t>(i)) - 0.5f) * 0.18f;
        mesh.positions.push_back(XMFLOAT3(rx * std::cos(angle) * j, ry * 0.45f, rz * std::sin(angle) * j));
    }

    // Lower ring at -0.45 ry with staggered phase
    const uint32_t lowerStart = static_cast<uint32_t>(mesh.positions.size());
    for (int i = 0; i < kBandVertices; ++i) {
        const float angle = kTwoPi * ((static_cast<float>(i) + 0.5f) / static_cast<float>(kBandVertices));
        const float j = 1.0f + (hashUnitF(itemSeed + 300U + static_cast<std::uint64_t>(i)) - 0.5f) * 0.18f;
        mesh.positions.push_back(XMFLOAT3(rx * std::cos(angle) * j, -ry * 0.45f, rz * std::sin(angle) * j));
    }

    // Triangles: Top pole to upper ring
    for (int i = 0; i < kBandVertices; ++i) {
        const uint32_t u0 = upperStart + i;
        const uint32_t u1 = upperStart + ((i + 1) % kBandVertices);
        addOrientedTriangle(mesh.indices, mesh.positions, topPole, u0, u1, center);
    }

    // Triangles: Upper ring to lower ring
    for (int i = 0; i < kBandVertices; ++i) {
        const uint32_t u0 = upperStart + i;
        const uint32_t u1 = upperStart + ((i + 1) % kBandVertices);
        const uint32_t l0 = lowerStart + i;
        const uint32_t l1 = lowerStart + ((i + 1) % kBandVertices);

        addOrientedTriangle(mesh.indices, mesh.positions, u0, l0, u1, center);
        addOrientedTriangle(mesh.indices, mesh.positions, u1, l0, l1, center);
    }

    // Triangles: Lower ring to bottom pole
    for (int i = 0; i < kBandVertices; ++i) {
        const uint32_t l0 = lowerStart + i;
        const uint32_t l1 = lowerStart + ((i + 1) % kBandVertices);
        addOrientedTriangle(mesh.indices, mesh.positions, bottomPole, l1, l0, center);
    }

    mesh.groundOffset = ry; // Center sits at +ry so base touches ground
    return mesh;
}

// Generates an angular, faceted rock scree/talus fragment for bluff zones.
inline GeneratedMesh createBluffAccentMesh(double scale, std::uint64_t itemSeed) {
    GeneratedMesh mesh;
    const float s = static_cast<float>(std::clamp(scale, 0.3, 2.0));

    // Base icosahedron golden ratio coordinates
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

    const float sx = s * (0.48f + 0.16f * hashUnitF(itemSeed + 40U));
    const float sy = s * (0.30f + 0.12f * hashUnitF(itemSeed + 41U));
    const float sz = s * (0.42f + 0.14f * hashUnitF(itemSeed + 42U));

    const XMFLOAT3 center(0.0f, 0.0f, 0.0f);
    mesh.positions.reserve(12);
    mesh.indices.reserve(60);

    float minY = 1e6f;
    for (int i = 0; i < 12; ++i) {
        const auto& bv = baseVerts[i];
        const float len = std::sqrt(bv.x * bv.x + bv.y * bv.y + bv.z * bv.z);
        const float j = 1.0f + (hashUnitF(itemSeed + 400U + static_cast<std::uint64_t>(i)) - 0.5f) * 0.28f;
        const float x = (bv.x / len) * sx * j;
        const float y = (bv.y / len) * sy * j;
        const float z = (bv.z / len) * sz * j;
        mesh.positions.push_back(XMFLOAT3(x, y, z));
        if (y < minY) minY = y;
    }

    for (const auto& tri : baseTris) {
        addOrientedTriangle(mesh.indices, mesh.positions, tri[0], tri[1], tri[2], center);
    }

    mesh.groundOffset = -minY;
    return mesh;
}

// Generates a multi-blade, narrow, segmented, drooping riparian bank sedge clump (basal crown).
// Features a central crown rosette, 3-segment drooping strap-like blades, narrow widths,
// bounded juvenile-scale arches, double-sided quads/triangles, and organic vertex colors.
// Strict geometry budget per clump: <= 256 triangles and <= 512 vertices.
inline GeneratedMesh createBankVegetationClumpMesh(double scale, std::uint64_t itemSeed) {
    GeneratedMesh mesh;
    if (!std::isfinite(scale)) throw std::invalid_argument("bank vegetation scale must be finite");
    // Scale clamped to [0.25, 1.40]; theoretical arch crest range is 0.023 - 0.460m.
    const float s = static_cast<float>(std::clamp(scale, 0.25, 1.4));

    // Number of radiating blades in the basal crown (10 to 16 blades)
    const int bladeCount = 10 + static_cast<int>(hashUnitF(itemSeed + 50U) * 6.99f);

    // Each blade has 7 vertices and 5 triangles:
    // With max 16 blades: 16 * 7 = 112 vertices (<= 512) and 16 * 5 = 80 triangles (<= 256).
    mesh.positions.reserve(static_cast<std::size_t>(bladeCount * 7));
    mesh.indices.reserve(static_cast<std::size_t>(bladeCount * 15));
    mesh.vertexColors.reserve(static_cast<std::size_t>(bladeCount * 7));

    // Vertex color palette: earthy basal crown root -> deep wetland olive -> sunlit drooping tip
    const uint32_t colRoot = wi::Color(52, 60, 32, 255).rgba; // Earthy brown-olive basal crown
    const uint32_t colLow  = wi::Color(68, 96, 38, 255).rgba; // Lower sheath olive
    const uint32_t colMid  = wi::Color(88, 126, 46, 255).rgba; // Arch crest green
    const uint32_t colTip  = wi::Color(118, 152, 58, 255).rgba; // Drooping sunlit tip

    // Base crown radius where blade sheaths emerge
    const float crownRadius = s * (0.015f + 0.010f * hashUnitF(itemSeed + 52U));

    for (int b = 0; b < bladeCount; ++b) {
        // Varied azimuths around the basal crown with natural jitter
        const float baseAzimuth = kTwoPi * (static_cast<float>(b) / static_cast<float>(bladeCount));
        const float azimuthJitter = (hashUnitF(itemSeed + 500U + static_cast<std::uint64_t>(b)) - 0.5f) * 0.38f;
        const float azimuth = baseAzimuth + azimuthJitter;
        const float cosA = std::cos(azimuth);
        const float sinA = std::sin(azimuth);

        // Blade length and arch dimensions; actual height depends on scale and seed.
        const float lengthVariation = 0.75f + 0.50f * hashUnitF(itemSeed + 600U + static_cast<std::uint64_t>(b));
        const float bladeArcLen = s * (0.22f + 0.14f * hashUnitF(itemSeed + 51U)) * lengthVariation;

        // Narrow strap-like sedge blade width (0.008m to 0.016m at base, tapering towards apex)
        const float wBase = s * 0.012f * (0.80f + 0.40f * hashUnitF(itemSeed + 700U + static_cast<std::uint64_t>(b)));
        const float wLow  = wBase * 0.82f;
        const float wMid  = wBase * 0.55f;

        // Tangent unit vectors perpendicular to radial direction in XZ
        const float tx = -sinA;
        const float tz = cosA;

        // Segment radial reaches along outward direction
        const float r0 = crownRadius;
        const float r1 = crownRadius + bladeArcLen * 0.32f;
        const float r2 = crownRadius + bladeArcLen * 0.68f;
        const float rTip = crownRadius + bladeArcLen * (0.95f + 0.15f * hashUnitF(itemSeed + 800U + static_cast<std::uint64_t>(b)));

        // Heights forming a realistic drooping arch profile:
        // Segment 0: Base crown sheath sitting on ground
        // Segment 1: Steep ascending rise
        // Segment 2: Arch crest peaking at mid-span
        // Segment 3: Gracefully drooping tip descending towards ground
        const float archPeak = bladeArcLen * (0.55f + 0.18f * hashUnitF(itemSeed + 900U + static_cast<std::uint64_t>(b)));
        const float droopFactor = 0.45f + 0.45f * hashUnitF(itemSeed + 1000U + static_cast<std::uint64_t>(b));

        const float y0 = 0.0f;
        const float y1 = archPeak * 0.62f;
        const float y2 = archPeak;
        const float yTip = std::max(0.015f, archPeak * (0.28f - 0.22f * droopFactor));

        const uint32_t baseIdx = static_cast<uint32_t>(mesh.positions.size());

        // Segment 0: Crown base vertices (2)
        mesh.positions.push_back(XMFLOAT3(cosA * r0 - tx * wBase, y0, sinA * r0 - tz * wBase));
        mesh.positions.push_back(XMFLOAT3(cosA * r0 + tx * wBase, y0, sinA * r0 + tz * wBase));
        mesh.vertexColors.push_back(colRoot);
        mesh.vertexColors.push_back(colRoot);

        // Segment 1: Lower ascending blade vertices (2)
        mesh.positions.push_back(XMFLOAT3(cosA * r1 - tx * wLow, y1, sinA * r1 - tz * wLow));
        mesh.positions.push_back(XMFLOAT3(cosA * r1 + tx * wLow, y1, sinA * r1 + tz * wLow));
        mesh.vertexColors.push_back(colLow);
        mesh.vertexColors.push_back(colLow);

        // Segment 2: Arch crest vertices (2)
        mesh.positions.push_back(XMFLOAT3(cosA * r2 - tx * wMid, y2, sinA * r2 - tz * wMid));
        mesh.positions.push_back(XMFLOAT3(cosA * r2 + tx * wMid, y2, sinA * r2 + tz * wMid));
        mesh.vertexColors.push_back(colMid);
        mesh.vertexColors.push_back(colMid);

        // Segment 3: Drooping tip apex vertex (1)
        mesh.positions.push_back(XMFLOAT3(cosA * rTip, yTip, sinA * rTip));
        mesh.vertexColors.push_back(colTip);

        // Triangles for the 3 segments (5 double-sided triangles total):
        // Segment 0->1 quad: (v0, v1, v2, v3) -> 2 triangles
        const uint32_t v0 = baseIdx + 0;
        const uint32_t v1 = baseIdx + 1;
        const uint32_t v2 = baseIdx + 2;
        const uint32_t v3 = baseIdx + 3;
        addDoubleSidedTriangle(mesh.indices, v0, v1, v2);
        addDoubleSidedTriangle(mesh.indices, v1, v3, v2);

        // Segment 1->2 quad: (v2, v3, v4, v5) -> 2 triangles
        const uint32_t v4 = baseIdx + 4;
        const uint32_t v5 = baseIdx + 5;
        addDoubleSidedTriangle(mesh.indices, v2, v3, v4);
        addDoubleSidedTriangle(mesh.indices, v3, v5, v4);

        // Segment 2->Tip triangle: (v4, v5, vTip) -> 1 triangle
        const uint32_t vTip = baseIdx + 6;
        addDoubleSidedTriangle(mesh.indices, v4, v5, vTip);
    }

    mesh.groundOffset = 0.0f; // Root rests directly on the terrain
    return mesh;
}

// Binds generated geometry into a new Wicked scene entity and assigns a shared material.
inline wi::ecs::Entity createScenePropEntity(
    wi::scene::Scene& scene,
    const std::string& name,
    GeneratedMesh&& mesh,
    wi::ecs::Entity sharedMaterialId,
    const domain::ReferenceLandscape& landscape,
    double worldX,
    double groundY,
    double worldZ,
    double headingRad,
    double pitchRad,
    double rollRad
) {
    const wi::ecs::Entity entity = wi::ecs::CreateEntity();

    if (!name.empty()) {
        scene.names.Create(entity) = name;
    }

    scene.layers.Create(entity);
    auto& transform = scene.transforms.Create(entity);
    auto& object = scene.objects.Create(entity);
    auto& meshComp = scene.meshes.Create(entity);

    object.meshID = entity;

    meshComp.vertex_positions = mesh.positions;
    meshComp.indices = std::move(mesh.indices);
    if (!mesh.vertexColors.empty()) {
        meshComp.vertex_colors = std::move(mesh.vertexColors);
    }

    auto& subset = meshComp.subsets.emplace_back();
    subset.indexCount = static_cast<uint32_t>(meshComp.indices.size());
    subset.materialID = sharedMaterialId;

    meshComp.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
    if (wi::graphics::GetDevice() != nullptr) {
        meshComp.CreateRenderData();
    }

    // Position prop on terrain with orientation
    const float contactOffset = groundContactOffset(mesh, landscape, worldX, worldZ,
        static_cast<float>(headingRad), static_cast<float>(pitchRad), static_cast<float>(rollRad));
    const float yPos = static_cast<float>(groundY) + contactOffset;
    transform.ClearTransform();
    transform.RotateRollPitchYaw(XMFLOAT3(
        static_cast<float>(pitchRad),
        static_cast<float>(headingRad),
        static_cast<float>(rollRad)
    ));
    transform.Translate(XMFLOAT3(
        static_cast<float>(worldX),
        yPos,
        static_cast<float>(worldZ)
    ));
    transform.UpdateTransform();

    return entity;
}

} // namespace detail

// Primary reusable scenery renderer entry point.
// Takes wi::scene::Scene&, ReferenceLandscape and seed, consumes palette and
// dressing contracts, builds procedural non-box meshes, and reports truthful counts.
inline EnvironmentSceneCounts populateEnvironmentScene(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    std::uint64_t seed,
    const domain::HabitatDressingBudget& budget = {}
) {
    using namespace domain;

    EnvironmentSceneCounts counts{};

    // 1. Evaluate and create shared scenery materials from EnvironmentPalette.
    // Materials are reused across all prop instances (no material per blade or per prop).
    const wi::ecs::Entity logMatEntity = scene.Entity_CreateMaterial("scenery_mat_woody_debris");
    if (auto* mat = scene.materials.GetComponent(logMatEntity)) {
        const auto p = EnvironmentPalette::evaluate(SceneryMaterialRole::WoodyDebris, seed);
        mat->SetBaseColor(DirectX::XMFLOAT4(p.baseColor.r, p.baseColor.g, p.baseColor.b, p.baseColor.a));
        mat->SetRoughness(p.roughness);
        mat->SetMetalness(p.metalness);
    }
    const wi::ecs::Entity pebbleMatEntity = scene.Entity_CreateMaterial("scenery_mat_river_pebble");
    if (auto* mat = scene.materials.GetComponent(pebbleMatEntity)) {
        const auto p = EnvironmentPalette::evaluate(SceneryMaterialRole::Rock, seed);
        mat->SetBaseColor(DirectX::XMFLOAT4(p.baseColor.r, p.baseColor.g, p.baseColor.b, p.baseColor.a));
        mat->SetRoughness(p.roughness);
        mat->SetMetalness(p.metalness);
    }
    const wi::ecs::Entity bluffMatEntity = scene.Entity_CreateMaterial("scenery_mat_bluff_accent");
    if (auto* mat = scene.materials.GetComponent(bluffMatEntity)) {
        const auto p = EnvironmentPalette::evaluate(SceneryMaterialRole::Bluff, seed);
        mat->SetBaseColor(DirectX::XMFLOAT4(p.baseColor.r, p.baseColor.g, p.baseColor.b, p.baseColor.a));
        mat->SetRoughness(p.roughness);
        mat->SetMetalness(p.metalness);
    }
    const wi::ecs::Entity clumpMatEntity = scene.Entity_CreateMaterial("scenery_mat_bank_clump");
    if (auto* mat = scene.materials.GetComponent(clumpMatEntity)) {
        const auto p = EnvironmentPalette::evaluate(SceneryMaterialRole::DryBank, seed);
        // Preserve the clump's green vertex colors with a neutral shared tint.
        mat->SetBaseColor(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, p.baseColor.a));
        mat->SetRoughness(p.roughness);
        mat->SetMetalness(p.metalness);
        mat->SetUseVertexColors(true);
        mat->SetDoubleSided(true);
    }

    // 2. Generate deterministic proposals via HabitatDressing
    HabitatDressing dressing(landscape, seed, budget);

    std::uint64_t propIndex = 0;
    for (const auto& prop : dressing.proposals()) {
        ++propIndex;

        // Defensive coordinate bounds and finite scale checks
        if (!landscape.inBounds(prop.x, prop.z)) continue;
        if (!std::isfinite(prop.scale) || prop.scale <= 0.0) continue;

        // Defensive preservation of watery corridor exclusion for terrestrial dressings
        if (prop.kind != DressingKind::RiverPebble) {
            if (landscape.waterContains(prop.x, prop.z)) continue;
        }

        const std::uint64_t itemSeed = hashMix(seed ^ (UINT64_C(0x9E3779B97F4A7C15) * propIndex));
        detail::GeneratedMesh mesh;
        wi::ecs::Entity materialId = wi::ecs::INVALID_ENTITY;

        switch (prop.kind) {
        case DressingKind::FallenLog:
            mesh = detail::createFallenLogMesh(prop.scale, itemSeed);
            materialId = logMatEntity;
            ++counts.logCount;
            break;

        case DressingKind::RiverPebble:
            mesh = detail::createRiverPebbleMesh(prop.scale, itemSeed);
            materialId = pebbleMatEntity;
            ++counts.pebbleCount;
            break;

        case DressingKind::DryBankClump:
            mesh = detail::createBankVegetationClumpMesh(prop.scale, itemSeed);
            materialId = clumpMatEntity;
            ++counts.bankClumpCount;
            break;

        case DressingKind::BluffAccent:
            mesh = detail::createBluffAccentMesh(prop.scale, itemSeed);
            materialId = bluffMatEntity;
            ++counts.bluffAccentCount;
            break;
        }

        if (mesh.positions.empty() || mesh.indices.empty()) continue;

        const std::size_t vCount = mesh.positions.size();
        const std::size_t tCount = mesh.indices.size() / 3;

        detail::createScenePropEntity(
            scene,
            prop.id,
            std::move(mesh),
            materialId,
            landscape,
            prop.x,
            landscape.height(prop.x, prop.z),
            prop.z,
            prop.headingRadians,
            prop.pitchRadians,
            prop.rollRadians
        );

        ++counts.objectCount;
        counts.vertexCount += vCount;
        counts.triangleCount += tCount;
    }

    return counts;
}

// Convenience alias matching PascalCase convention
inline EnvironmentSceneCounts PopulateEnvironmentScene(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    std::uint64_t seed,
    const domain::HabitatDressingBudget& budget = {}
) {
    return populateEnvironmentScene(scene, landscape, seed, budget);
}

} // namespace stalthos::engine

namespace stalthos {
    using engine::populateEnvironmentScene;
    using engine::PopulateEnvironmentScene;
    using engine::EnvironmentSceneCounts;
}
