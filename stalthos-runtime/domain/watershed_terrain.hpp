#pragma once

#include "terrain_tiles.hpp"
#include "reference_landscape.hpp"

namespace stalthos::world {

// Immutable world-space overlay: the inhabited fixture retains its exact datum.
class WatershedTerrain {
public:
    explicit WatershedTerrain(std::uint64_t seed,
            TerrainTileConfig config = {16, 8.0, 1.0}, double outerExtentMeters = 256.0)
        : coarse_(seed, config), landscape_(seed), outerExtent_(outerExtentMeters), source_(seed) {
        if (!std::isfinite(outerExtent_) ||
            outerExtent_ <= domain::ReferenceLandscape::halfExtent + config.normalStepMeters ||
            outerExtent_ > Archipelago::worldBoundsMeters)
            throw std::invalid_argument("invalid bounded watershed overlay extent");
    }

    const TerrainTileConfig& config() const { return coarse_.config(); }
    double spanMeters() const { return coarse_.spanMeters(); }
    double outerExtentMeters() const { return outerExtent_; }
    bool containsTile(TerrainTileKey key) const { return coarse_.containsTile(key); }
    TerrainTileKey bodyTile(Vec3 body) const { return coarse_.bodyTile(body); }
    std::vector<TerrainTileKey> neighborhood(Vec3 body, std::uint32_t radius = 1) const {
        return coarse_.neighborhood(body, radius);
    }

    double height(double x, double z) const {
        // Coarse sample supplies the established finite/world-bound checks.
        validate(x, z);
        constexpr double inner = domain::ReferenceLandscape::halfExtent;
        if (std::abs(x) <= inner && std::abs(z) <= inner)
            return landscape_.height(x, z);
        const double weight = axisWeight(x) * axisWeight(z);
        const double base = sourceHeight(x, z);
        if (weight == 0.0) return base;
        const double local = landscape_.height(std::clamp(x, -inner, inner),
                                                std::clamp(z, -inner, inner));
        return base + weight * (local - base);
    }

    TerrainVertex sample(double x, double z) const {
        const double y = height(x, z);
        const double h = config().normalStepMeters;
        const double bound = Archipelago::worldBoundsMeters;
        const double left = std::max(-bound, x - h), right = std::min(bound, x + h);
        const double back = std::max(-bound, z - h), front = std::min(bound, z + h);
        const double dx = (height(right, z) - height(left, z)) / (right - left);
        const double dz = (height(x, front) - height(x, back)) / (front - back);
        const double inv = 1.0 / std::sqrt(dx * dx + 1.0 + dz * dz);
        return {{x,y,z}, {-dx*inv, inv, -dz*inv}, source_.moisture(x,z), y <= 0.0};
    }

    TerrainTile build(TerrainTileKey key) const {
        if (!containsTile(key)) throw std::out_of_range("terrain tile exceeds source bounds");
        TerrainTile tile{key, config().cells, {}};
        const auto cells = static_cast<std::int64_t>(config().cells);
        const auto stride = static_cast<std::size_t>(cells) + 1;
        tile.vertices.reserve(stride * stride);
        for (std::int64_t z = 0; z <= cells; ++z)
            for (std::int64_t x = 0; x <= cells; ++x)
                tile.vertices.push_back(sample(static_cast<double>(key.x*cells+x)*config().spacingMeters,
                                               static_cast<double>(key.z*cells+z)*config().spacingMeters));
        return tile;
    }

private:
    TerrainTiles coarse_;
    domain::ReferenceLandscape landscape_;
    double outerExtent_;
    // Initialized explicitly from the same seed; no coordinate translation.
    Archipelago source_;
    static void validate(double x, double z) {
        if (!std::isfinite(x) || !std::isfinite(z))
            throw std::invalid_argument("watershed coordinates must be finite");
        if (std::abs(x) > Archipelago::worldBoundsMeters || std::abs(z) > Archipelago::worldBoundsMeters)
            throw std::out_of_range("watershed query exceeds source bounds");
    }
    double sourceHeight(double x, double z) const { return source_.elevation(x,z); }
    double axisWeight(double value) const {
        constexpr double inner = domain::ReferenceLandscape::halfExtent;
        // A one-normal-step coarse halo makes the outer boundary's numerical
        // normal exactly the coarse normal, not merely asymptotically close.
        const double end = outerExtent_ - config().normalStepMeters;
        const double t = std::clamp((std::abs(value)-inner)/(end-inner), 0.0, 1.0);
        const double smooth = t*t*t*(t*(t*6.0-15.0)+10.0);
        return 1.0-smooth;
    }
};

} // namespace stalthos::world
