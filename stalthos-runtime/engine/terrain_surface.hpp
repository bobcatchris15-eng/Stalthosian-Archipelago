#pragma once

#include "WickedEngine.h"
#include "domain/reference_landscape.hpp"
#include "domain/terrain_texture.hpp"

#include <cstdint>
#include <mutex>
#include <vector>

namespace stalthos::engine {

enum class TerrainSurfaceStatus : std::uint8_t {
    Applied,
    MissingMesh,
    GraphicsDeviceUnavailable,
    TextureCreationFailed
};

struct TerrainSurfaceResult {
    TerrainSurfaceStatus status = TerrainSurfaceStatus::MissingMesh;
    std::size_t uvVertexCount = 0;
    std::uint32_t textureWidth = 0;
    std::size_t textureBytes = 0;
    bool applied() const noexcept { return status == TerrainSurfaceStatus::Applied; }
};

// Applies one bounded, seed-stable repeating ground material. The near-neutral
// albedo multiplies the habitat vertex colors already assigned to the terrain.
inline TerrainSurfaceResult applyTerrainSurface(
    wi::scene::Scene& scene,
    wi::ecs::Entity groundEntity,
    const domain::ReferenceLandscape& landscape,
    std::uint64_t seed
) {
    (void)landscape; // Habitat color and wet-bank shading remain authored by the caller.
    auto* object = scene.objects.GetComponent(groundEntity);
    auto* mesh = object ? scene.meshes.GetComponent(object->meshID) : nullptr;
    if (mesh == nullptr || mesh->vertex_positions.empty()) return {};
    if (wi::graphics::GetDevice() == nullptr) {
        return {TerrainSurfaceStatus::GraphicsDeviceUnavailable, 0, 0, 0};
    }

    // World-space UVs repeat every 3 m, preventing the 192 m ground from being stretched.
    mesh->vertex_uvset_0.resize(mesh->vertex_positions.size());
    constexpr float kMetersPerTile = 3.0f;
    for (std::size_t i = 0; i < mesh->vertex_positions.size(); ++i) {
        const auto& p = mesh->vertex_positions[i];
        mesh->vertex_uvset_0[i] = DirectX::XMFLOAT2(p.x / kMetersPerTile, p.z / kMetersPerTile);
    }
    const std::size_t uvVertexCount = mesh->vertex_positions.size();

    static std::mutex cacheMutex;
    static bool cached = false;
    static std::uint64_t cachedSeed = 0;
    static wi::graphics::Texture cachedTexture;
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (!cached || cachedSeed != seed || !cachedTexture.IsValid()) {
        const domain::TerrainTexture pixels = domain::makeTerrainTexture(seed);
        wi::graphics::Texture candidate;
        if (!wi::texturehelper::CreateTexture(candidate, pixels.rgba.data(), domain::TerrainTexture::kSize,
            domain::TerrainTexture::kSize, wi::graphics::Format::R8G8B8A8_UNORM_SRGB)) {
            return {TerrainSurfaceStatus::TextureCreationFailed, uvVertexCount, 0, 0};
        }
        cachedTexture = std::move(candidate);
        cachedSeed = seed;
        cached = true;
    }

    for (auto& subset : mesh->subsets) {
        auto* material = scene.materials.GetComponent(subset.materialID);
        if (material == nullptr) continue;
        material->SetBaseColor(DirectX::XMFLOAT4(1, 1, 1, 1));
        material->SetRoughness(0.96f);
        material->SetMetalness(0.0f);
        material->SetUseVertexColors(true);
        material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource.SetTexture(cachedTexture);
        material->textures[wi::scene::MaterialComponent::BASECOLORMAP].uvset = 0;
    }
    mesh->CreateRenderData();
    return {TerrainSurfaceStatus::Applied, uvVertexCount, domain::TerrainTexture::kSize,
        domain::TerrainTexture::kMaxBytes};
}

} // namespace stalthos::engine
