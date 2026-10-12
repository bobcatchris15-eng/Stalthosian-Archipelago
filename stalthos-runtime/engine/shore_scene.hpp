#pragma once

#include "WickedEngine.h"
#include "domain/reference_landscape.hpp"
#include "domain/shore_mesh.hpp"
#include "domain/environment_palette.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace stalthos::engine {

// Summary of created shore scenery entities and truthful geometric primitive counts.
struct ShoreSceneCounts {
    std::size_t objectCount = 0;
    std::size_t vertexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t surfaceTriangleCount = 0;
    std::size_t bedTriangleCount = 0;
    std::size_t bankTriangleCount = 0;
};

namespace detail {

// Subdivided mesh data container for a single material role
struct RoleMeshBuilder {
    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> remap;
};

// Creates a scene entity with MeshComponent, TransformComponent, ObjectComponent,
// and binds the specified material entity.
inline wi::ecs::Entity createShoreMeshEntity(
    wi::scene::Scene& scene,
    const std::string& name,
    std::vector<DirectX::XMFLOAT3>&& positions,
    std::vector<std::uint32_t>&& indices,
    wi::ecs::Entity materialId
) {
    if (positions.empty() || indices.empty()) {
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

    meshComp.vertex_positions = std::move(positions);
    meshComp.indices = std::move(indices);

    auto& subset = meshComp.subsets.emplace_back();
    subset.indexCount = static_cast<std::uint32_t>(meshComp.indices.size());
    subset.materialID = materialId;

    meshComp.ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);
    if (wi::graphics::GetDevice() != nullptr) {
        meshComp.CreateRenderData();
    }

    transform.ClearTransform();
    transform.UpdateTransform();

    return entity;
}

} // namespace detail

// Primary reusable shore scene renderer entry point.
// Consumes domain::ShoreMesh generated from ReferenceLandscape, splits the geometry
// into separate meshes by material role without unsafe index out-of-bounds,
// reverses all triangle windings to match Wicked Engine's reversed cross-product normal
// convention ensuring upward host normals, applies shared palette materials,
// and returns truthful object, vertex, and triangle counts.
inline ShoreSceneCounts populateShoreScene(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    double resolution = 2.0,
    std::uint64_t seed = 1
) {
    if (!std::isfinite(resolution) || resolution < 0.1 || resolution > 10.0) {
        throw std::invalid_argument("resolution must be finite and within [0.1, 10.0]");
    }

    ShoreSceneCounts counts{};

    // 1. Generate domain shore mesh
    const domain::ShoreMesh shoreMesh = domain::generateShoreMesh(landscape, resolution);
    if (shoreMesh.vertices.empty() || shoreMesh.indices.empty()) {
        return counts;
    }

    if (shoreMesh.indices.size() % 3 != 0) {
        throw std::runtime_error("domain shore mesh indices count must be a multiple of 3");
    }

    // 2. Setup role builders with index remap tables sized to input vertex count
    detail::RoleMeshBuilder surfaceBuilder;
    detail::RoleMeshBuilder bedBuilder;
    detail::RoleMeshBuilder bankBuilder;

    surfaceBuilder.remap.assign(shoreMesh.vertices.size(), std::numeric_limits<std::uint32_t>::max());
    bedBuilder.remap.assign(shoreMesh.vertices.size(), std::numeric_limits<std::uint32_t>::max());
    bankBuilder.remap.assign(shoreMesh.vertices.size(), std::numeric_limits<std::uint32_t>::max());

    const std::size_t totalTriangles = shoreMesh.indices.size() / 3;

    for (std::size_t t = 0; t < totalTriangles; ++t) {
        const std::size_t idx0 = t * 3;
        const std::uint32_t i0 = shoreMesh.indices[idx0];
        const std::uint32_t i1 = shoreMesh.indices[idx0 + 1];
        const std::uint32_t i2 = shoreMesh.indices[idx0 + 2];

        // Validate source indices against vertex buffer bounds
        if (i0 >= shoreMesh.vertices.size() ||
            i1 >= shoreMesh.vertices.size() ||
            i2 >= shoreMesh.vertices.size()) {
            throw std::runtime_error("shore source index outside vertex buffer");
        }

        const auto& v0 = shoreMesh.vertices[i0];
        const auto& v1 = shoreMesh.vertices[i1];
        const auto& v2 = shoreMesh.vertices[i2];

        // Validate finite coordinates
        if (!std::isfinite(v0.x) || !std::isfinite(v0.y) || !std::isfinite(v0.z) ||
            !std::isfinite(v1.x) || !std::isfinite(v1.y) || !std::isfinite(v1.z) ||
            !std::isfinite(v2.x) || !std::isfinite(v2.y) || !std::isfinite(v2.z)) {
            throw std::runtime_error("shore source contains nonfinite coordinates");
        }

        // Validate material consistency across triangle
        if (v0.material != v1.material || v0.material != v2.material) {
            throw std::runtime_error("shore source triangle mixes material roles");
        }

        detail::RoleMeshBuilder* builder = nullptr;
        switch (v0.material) {
        case domain::ShoreMaterial::Surface:
            builder = &surfaceBuilder;
            break;
        case domain::ShoreMaterial::WetBed:
            builder = &bedBuilder;
            break;
        case domain::ShoreMaterial::WetBank:
            builder = &bankBuilder;
            break;
        }

        if (!builder) {
            throw std::runtime_error("shore source has unknown material role");
        }

        auto addVertex = [&](std::uint32_t origIdx, const domain::ShoreVertex& v) -> std::uint32_t {
            if (builder->remap[origIdx] != std::numeric_limits<std::uint32_t>::max()) {
                return builder->remap[origIdx];
            }
            const auto newIdx = static_cast<std::uint32_t>(builder->positions.size());
            builder->positions.emplace_back(
                static_cast<float>(v.x),
                // Lift the decorative wet bed 10 mm above the matching terrain to avoid coplanar depth flicker.
                static_cast<float>(v.y + (v.material == domain::ShoreMaterial::WetBed ? 0.01 : 0.0)),
                static_cast<float>(v.z)
            );
            builder->remap[origIdx] = newIdx;
            return newIdx;
        };

        const std::uint32_t u0 = addVertex(i0, v0);
        const std::uint32_t u1 = addVertex(i1, v1);
        const std::uint32_t u2 = addVertex(i2, v2);

        // Reverse triangle winding: Wicked computes normals with cross(p2 - p0, p1 - p0).
        // The domain winding (u0, u1, u2) has upward (p1 - p0) x (p2 - p0).
        // By inserting (u0, u2, u1), Wicked's cross(p2' - p0', p1' - p0') evaluates to
        // cross(u1 - u0, u2 - u0), ensuring host normals point upward for both
        // water surface and submerged bed.
        builder->indices.push_back(u0);
        builder->indices.push_back(u2);
        builder->indices.push_back(u1);
    }

    // 3. Create shared materials from domain::EnvironmentPalette
    // Water surface material: plausible mildly transparent tinted surface with SHADERTYPE_WATER
    const wi::ecs::Entity waterMatEntity = scene.Entity_CreateMaterial("shore_mat_water_surface");
    if (auto* mat = scene.materials.GetComponent(waterMatEntity)) {
        const auto p = domain::EnvironmentPalette::evaluate(domain::SceneryMaterialRole::WaterSurface, seed);
        mat->shaderType = wi::scene::MaterialComponent::SHADERTYPE_WATER;
        mat->SetBaseColor(DirectX::XMFLOAT4(p.baseColor.r, p.baseColor.g, p.baseColor.b, p.baseColor.a));
        mat->SetRoughness(p.roughness);
        mat->SetMetalness(p.metalness);
        mat->SetRefractionAmount(p.refractionAmount);
        mat->SetTransmissionAmount(p.transmissionAmount);
        mat->SetDoubleSided(false);
        mat->SetCastShadow(false);
    }

    // Wet bed material: darker rough opaque substrate
    const wi::ecs::Entity bedMatEntity = scene.Entity_CreateMaterial("shore_mat_wet_bed");
    if (auto* mat = scene.materials.GetComponent(bedMatEntity)) {
        const auto p = domain::EnvironmentPalette::evaluate(domain::SceneryMaterialRole::CreekBed, seed);
        mat->shaderType = wi::scene::MaterialComponent::SHADERTYPE_PBR;
        mat->SetBaseColor(DirectX::XMFLOAT4(p.baseColor.r, p.baseColor.g, p.baseColor.b, p.baseColor.a));
        mat->SetRoughness(p.roughness);
        mat->SetMetalness(p.metalness);
        mat->SetRefractionAmount(0.0f);
        mat->SetTransmissionAmount(0.0f);
        mat->SetDoubleSided(false);
        mat->SetCastShadow(true);
    }

    // Wet bank material: darkened moist riparian sediment
    const wi::ecs::Entity bankMatEntity = scene.Entity_CreateMaterial("shore_mat_wet_bank");
    if (auto* mat = scene.materials.GetComponent(bankMatEntity)) {
        const auto p = domain::EnvironmentPalette::evaluate(domain::SceneryMaterialRole::WetBank, seed);
        mat->shaderType = wi::scene::MaterialComponent::SHADERTYPE_PBR;
        mat->SetBaseColor(DirectX::XMFLOAT4(p.baseColor.r, p.baseColor.g, p.baseColor.b, p.baseColor.a));
        mat->SetRoughness(p.roughness);
        mat->SetMetalness(p.metalness);
        mat->SetRefractionAmount(0.0f);
        mat->SetTransmissionAmount(0.0f);
        mat->SetDoubleSided(false);
        mat->SetCastShadow(true);
    }

    // 4. Construct scene entities for each non-empty role
    if (!surfaceBuilder.indices.empty()) {
        const auto vCount = surfaceBuilder.positions.size();
        const auto tCount = surfaceBuilder.indices.size() / 3;
        detail::createShoreMeshEntity(
            scene,
            "shore_water_surface",
            std::move(surfaceBuilder.positions),
            std::move(surfaceBuilder.indices),
            waterMatEntity
        );
        ++counts.objectCount;
        counts.vertexCount += vCount;
        counts.triangleCount += tCount;
        counts.surfaceTriangleCount += tCount;
    }

    // The terrain already renders the creek bed. Duplicating clipped bed triangles
    // here produces coplanar/interpolated depth interference; retain domain bed data
    // for a future jointly clipped terrain adapter without rendering it twice.

    if (!bankBuilder.indices.empty()) {
        const auto vCount = bankBuilder.positions.size();
        const auto tCount = bankBuilder.indices.size() / 3;
        detail::createShoreMeshEntity(
            scene,
            "shore_wet_bank",
            std::move(bankBuilder.positions),
            std::move(bankBuilder.indices),
            bankMatEntity
        );
        ++counts.objectCount;
        counts.vertexCount += vCount;
        counts.triangleCount += tCount;
        counts.bankTriangleCount += tCount;
    }

    return counts;
}

// Convenience alias matching PascalCase convention
inline ShoreSceneCounts PopulateShoreScene(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    double resolution = 2.0,
    std::uint64_t seed = 1
) {
    return populateShoreScene(scene, landscape, resolution, seed);
}

} // namespace stalthos::engine

namespace stalthos {
    using engine::populateShoreScene;
    using engine::PopulateShoreScene;
    using engine::ShoreSceneCounts;
}
