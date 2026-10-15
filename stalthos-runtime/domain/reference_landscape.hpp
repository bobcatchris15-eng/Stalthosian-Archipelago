#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "deterministic_hash.hpp"

namespace stalthos::domain {

enum class Habitat { Creek, Riparian, Woodland, OpenLowland, Bluff, Outcrop };
enum class PlacementKind { Tree, Shrub, Fern, Rock, Boulder };

struct Placement {
    PlacementKind kind{};
    double x = 0.0, y = 0.0, z = 0.0;
    double scale = 1.0;
    double headingRadians = 0.0;
};

// A deterministic 192 m square Y-up terrain fixture. Coordinates outside the
// patch and non-finite queries are rejected so callers cannot silently sample
// an invented continuation of the landscape.
class ReferenceLandscape {
public:
    static constexpr double halfExtent = 96.0;
    explicit ReferenceLandscape(std::uint64_t seed = 1) : seed_(seed) { buildPlacements(); }

    double height(double x, double z) const {
        validate(x, z);
        const double center = creekCenter(z);
        const double dx = std::abs(x - center);
        // Broad lowland, a coherent incised creek, wooded shoulders, and a
        // resistant west-side bluff carrying a modest caprock outcrop.
        double y = 2.0 + 0.003 * z + 0.0015 * x;
        y += 8.0 * shoulder(x, z);
        y += 13.0 * bluff(x, z);
        y += 4.0 * outcrop(x, z);
        y -= 2.5 * (1.0 - smooth(3.8, 9.0, dx)); // incised bed, then rising bank
        y += 0.28 * (noise(x, z) - 0.5); // low-amplitude terrain variation
        return y;
    }

    Habitat habitat(double x, double z) const {
        validate(x, z);
        const double dx = std::abs(x - creekCenter(z));
        if (dx < 4.6) return Habitat::Creek;
        if (dx < 11.0) return Habitat::Riparian;
        if (outcrop(x, z) > 0.45) return Habitat::Outcrop;
        if (bluff(x, z) > 0.52) return Habitat::Bluff;
        if (shoulder(x, z) > 0.35) return Habitat::Woodland;
        return Habitat::OpenLowland;
    }

    bool waterContains(double x, double z) const {
        validate(x, z);
        const double dx = std::abs(x - creekCenter(z));
        // Wet extent follows the intersection of the authored bed and water
        // surface, bounded to the creek's bank-transition corridor.
        return dx < 9.0 && height(x, z) < waterSurface(x, z);
    }

    double waterSurface(double x, double z) const {
        validate(x, z);
        return 1.4 + 0.002 * z;
    }

    bool inBounds(double x, double z) const noexcept {
        return std::isfinite(x) && std::isfinite(z) && std::abs(x) <= halfExtent && std::abs(z) <= halfExtent;
    }

    const std::vector<Placement>& placements() const noexcept { return placements_; }

private:
    std::uint64_t seed_;
    std::vector<Placement> placements_;

    static void validate(double x, double z) {
        if (!std::isfinite(x) || !std::isfinite(z) || std::abs(x) > halfExtent || std::abs(z) > halfExtent)
            throw std::invalid_argument("landscape query outside finite 192m patch");
    }
    static double smooth(double a, double b, double v) {
        const double t = (v - a) / (b - a);
        const double c = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
        return c * c * (3.0 - 2.0 * c);
    }
    static double creekCenter(double z) { return 8.0 * std::sin(z / 43.0) + 2.0 * std::sin(z / 17.0); }
    static double shoulder(double x, double z) {
        const double ridge = 0.5 + 0.5 * std::sin((x + 23.0 + 5.0 * std::sin(z / 55.0)) / 18.0);
        return smooth(18.0, 40.0, std::abs(x - creekCenter(z))) * (0.35 + 0.65 * ridge);
    }
    static double bluff(double x, double z) {
        return smooth(0.0, 1.0, 1.0 - std::abs(x + 58.0 + 4.0 * std::sin(z / 39.0)) / 15.0);
    }
    static double outcrop(double x, double z) {
        const double r = std::sqrt((x + 61.0) * (x + 61.0) + (z - 12.0) * (z - 12.0));
        return 1.0 - smooth(9.0, 19.0, r);
    }
    double noise(double x, double z) const {
        const auto ix = static_cast<std::int64_t>(std::floor(x * 0.17));
        const auto iz = static_cast<std::int64_t>(std::floor(z * 0.17));
        const std::uint64_t h = hashMix(seed_ ^ hashMix(static_cast<std::uint64_t>(ix)) ^ (hashMix(static_cast<std::uint64_t>(iz)) << 1U));
        return hashUnit(h);
    }
    void buildPlacements() {
        // Stable 8m candidate lattice with bounded seed-derived jitter.
        for (int iz = -11; iz <= 11; ++iz) for (int ix = -11; ix <= 11; ++ix) {
            const std::uint64_t cell = seed_ ^ hashMix(static_cast<std::uint64_t>(ix + 32)) ^ (hashMix(static_cast<std::uint64_t>(iz + 32)) << 1U);
            const double x = ix * 8.0 + (hashUnit(hashMix(cell)) - 0.5) * 3.0;
            const double z = iz * 8.0 + (hashUnit(hashMix(cell + 1U)) - 0.5) * 3.0;
            const auto h = habitat(x, z);
            const double choice = hashUnit(hashMix(cell + 2U));
            PlacementKind kind;
            if ((h == Habitat::Bluff || h == Habitat::Outcrop) && choice < 0.42) kind = choice < 0.12 ? PlacementKind::Boulder : PlacementKind::Rock;
            else if (h == Habitat::Woodland && choice < 0.66) kind = choice < 0.22 ? PlacementKind::Tree : PlacementKind::Shrub;
            else if ((h == Habitat::Riparian || h == Habitat::OpenLowland) && choice < 0.23) kind = choice < 0.10 ? PlacementKind::Fern : PlacementKind::Shrub;
            else continue;
            if (kind == PlacementKind::Tree || kind == PlacementKind::Shrub || kind == PlacementKind::Fern) {
                if (waterContains(x, z)) continue;
            }
            const double scale = 0.7 + hashUnit(hashMix(cell + 3U)) * (kind == PlacementKind::Tree ? 1.0 : 0.7);
            placements_.push_back({kind, x, height(x, z), z, scale, hashUnit(hashMix(cell + 4U)) * 6.283185307179586});
        }
    }
};

} // namespace stalthos::domain
