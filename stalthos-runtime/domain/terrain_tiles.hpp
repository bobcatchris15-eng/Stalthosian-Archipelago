#pragma once

#include "archipelago_world.hpp"
#include <algorithm>
#include <vector>

namespace stalthos::world {

struct TerrainTileKey { std::int64_t x = 0, z = 0; };
struct TerrainTileConfig {
    std::uint32_t cells = 32;
    double spacingMeters = 8.0;
    double normalStepMeters = 1.0;
};
struct TerrainVertex {
    Vec3 position;
    Vec3 normal;
    double moisture = 0;
    bool sea = false; // Source elevation <= zero, not a separately sampled mask.
};
struct TerrainTile {
    TerrainTileKey key;
    std::uint32_t cells;
    std::vector<TerrainVertex> vertices; // z-major, x-fast; (cells + 1)^2.
};

// One immutable source/configuration per neighborhood. No cache or activation state.
class TerrainTiles {
public:
    explicit TerrainTiles(std::uint64_t seed, TerrainTileConfig config = {})
        : source_(seed), config_(config) {
        if (config.cells < 1 || config.cells > 256 ||
            !std::isfinite(config.spacingMeters) || config.spacingMeters < 0.25 ||
            config.spacingMeters > 256.0 ||
            !std::isfinite(config.normalStepMeters) || config.normalStepMeters < 0.01 ||
            config.normalStepMeters > 16.0 || spanMeters() > 4096.0)
            throw std::invalid_argument("invalid bounded terrain tile configuration");
    }

    double spanMeters() const { return config_.spacingMeters * config_.cells; }
    const TerrainTileConfig& config() const { return config_; }

    TerrainVertex sample(double x, double z) const {
        // elevation validates finite coordinates and source bounds before any math.
        const double height = source_.elevation(x, z);
        const double h = config_.normalStepMeters;
        const double bound = Archipelago::worldBoundsMeters;
        const double left = std::max(-bound, x - h), right = std::min(bound, x + h);
        const double back = std::max(-bound, z - h), front = std::min(bound, z + h);
        const double dx = (source_.elevation(right, z) - source_.elevation(left, z)) / (right - left);
        const double dz = (source_.elevation(x, front) - source_.elevation(x, back)) / (front - back);
        const double inverseLength = 1.0 / std::sqrt(dx * dx + 1.0 + dz * dz);
        return {{x, height, z}, {-dx * inverseLength, inverseLength, -dz * inverseLength},
                source_.moisture(x, z), height <= 0.0};
    }

    bool containsTile(TerrainTileKey key) const {
        // Reject before integer multiply/add. Valid world indices are much smaller;
        // this conservative bound makes all later int64 arithmetic safe.
        constexpr std::int64_t limit = 1000000;
        if (key.x < -limit || key.x > limit || key.z < -limit || key.z > limit) return false;
        const auto cells = static_cast<std::int64_t>(config_.cells);
        const auto coordinate = [&](std::int64_t index) {
            return static_cast<double>(index) * config_.spacingMeters;
        };
        const double bound = Archipelago::worldBoundsMeters;
        return coordinate(key.x * cells) >= -bound && coordinate((key.x + 1) * cells) <= bound &&
               coordinate(key.z * cells) >= -bound && coordinate((key.z + 1) * cells) <= bound;
    }

    TerrainTile build(TerrainTileKey key) const {
        if (!containsTile(key)) throw std::out_of_range("terrain tile exceeds source bounds");
        TerrainTile tile{key, config_.cells, {}};
        const auto stride = static_cast<std::size_t>(config_.cells) + 1;
        tile.vertices.reserve(stride * stride);
        const auto cells = static_cast<std::int64_t>(config_.cells);
        for (std::uint32_t z = 0; z <= config_.cells; ++z) {
            for (std::uint32_t x = 0; x <= config_.cells; ++x) {
                // Compute from global integer lattice, never tile-origin + offset.
                const auto gx = key.x * cells + x;
                const auto gz = key.z * cells + z;
                tile.vertices.push_back(sample(static_cast<double>(gx) * config_.spacingMeters,
                                               static_cast<double>(gz) * config_.spacingMeters));
            }
        }
        return tile;
    }

    TerrainTileKey bodyTile(Vec3 body) const {
        if (!finite(body)) throw std::invalid_argument("terrain body must be finite");
        const double bound = Archipelago::worldBoundsMeters;
        if (std::abs(body.x) > bound || std::abs(body.z) > bound)
            throw std::out_of_range("terrain body exceeds source bounds");
        // Bounds and minimum span guarantee these casts cannot overflow.
        const TerrainTileKey center{static_cast<std::int64_t>(std::floor(body.x / spanMeters())),
                                    static_cast<std::int64_t>(std::floor(body.z / spanMeters()))};
        if (!containsTile(center)) throw std::out_of_range("body lies outside complete terrain tiles");
        return center;
    }

    std::vector<TerrainTileKey> neighborhood(Vec3 body, std::uint32_t radius = 1) const {
        if (radius > 4) throw std::invalid_argument("terrain neighborhood radius exceeds four");
        const auto center = bodyTile(body);
        std::vector<TerrainTileKey> keys;
        const auto r = static_cast<std::int64_t>(radius);
        keys.reserve(static_cast<std::size_t>((2 * r + 1) * (2 * r + 1)));
        for (auto z = center.z - r; z <= center.z + r; ++z)
            for (auto x = center.x - r; x <= center.x + r; ++x)
                if (containsTile({x, z})) keys.push_back({x, z});
        return keys;
    }

private:
    Archipelago source_;
    TerrainTileConfig config_;
};

} // namespace stalthos::world
