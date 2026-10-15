#pragma once

#include "reference_landscape.hpp"
#include "deterministic_hash.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace stalthos::domain {

enum class DressingKind {
    FallenLog,
    RiverPebble,
    DryBankClump,
    BluffAccent
};

[[nodiscard]] constexpr const char* toString(DressingKind kind) noexcept {
    switch (kind) {
        case DressingKind::FallenLog: return "FallenLog";
        case DressingKind::RiverPebble: return "RiverPebble";
        case DressingKind::DryBankClump: return "DryBankClump";
        case DressingKind::BluffAccent: return "BluffAccent";
    }
    return "Unknown";
}

struct DressingProposal {
    std::string id;
    DressingKind kind{};
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double scale = 1.0;
    double headingRadians = 0.0;
    double pitchRadians = 0.0;
    double rollRadians = 0.0;
};

struct HabitatDressingBudget {
    std::size_t maxFallenLogs = 32;
    std::size_t maxRiverPebbles = 64;
    std::size_t maxDryBankClumps = 48;
    std::size_t maxBluffAccents = 32;

    [[nodiscard]] std::size_t totalMax() const noexcept {
        std::size_t total = 0;
        for (const std::size_t value : {maxFallenLogs, maxRiverPebbles, maxDryBankClumps, maxBluffAccents}) {
            if (value > std::numeric_limits<std::size_t>::max() - total) {
                return std::numeric_limits<std::size_t>::max();
            }
            total += value;
        }
        return total;
    }
};

class HabitatDressing {
public:
    HabitatDressing(const ReferenceLandscape& landscape,
                    std::uint64_t seed = 1,
                    const HabitatDressingBudget& budget = {})
        : landscape_(landscape), seed_(seed), budget_(budget) {
        generateProposals();
    }

    [[nodiscard]] const std::vector<DressingProposal>& proposals() const noexcept {
        return proposals_;
    }

    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
    [[nodiscard]] const HabitatDressingBudget& budget() const noexcept { return budget_; }

    [[nodiscard]] std::size_t countByKind(DressingKind kind) const noexcept {
        std::size_t count = 0;
        for (const auto& p : proposals_) {
            if (p.kind == kind) {
                ++count;
            }
        }
        return count;
    }

private:
    const ReferenceLandscape& landscape_;
    std::uint64_t seed_;
    HabitatDressingBudget budget_;
    std::vector<DressingProposal> proposals_;

    static double creekCenter(double z) noexcept {
        return 8.0 * std::sin(z / 43.0) + 2.0 * std::sin(z / 17.0);
    }

    void generateProposals() {
        proposals_.clear();
        // The fixed cluster loops can produce at most this many candidates,
        // regardless of caller-supplied budgets. Keep reserve bounded even if
        // a caller passes SIZE_MAX or a budget whose sum would overflow.
        constexpr std::size_t maxCandidates = 12U * 9U + 14U * 3U + 16U * 7U + 10U * 6U;
        proposals_.reserve(std::min(budget_.totalMax(), maxCandidates));

        generateRiverPebbles();
        generateFallenLogs();
        generateDryBankClumps();
        generateBluffAccents();
    }

    void generateRiverPebbles() {
        std::size_t count = 0;
        const std::size_t targetMax = budget_.maxRiverPebbles;
        if (targetMax == 0) return;

        // River pebbles: clustered irregular placement along creek bed, gravel bars, and wetted margins.
        // We define cluster seeds along the creek corridor (z: -90..90).
        constexpr int clusterCount = 12;
        for (int c = 0; c < clusterCount && count < targetMax; ++c) {
            const std::uint64_t clusterSalt = seed_ ^ hashMix(0xFEED01ULL + static_cast<std::uint64_t>(c));
            const double centerZ = -80.0 + hashUnit(hashMix(clusterSalt)) * 160.0;
            const double channelCenter = creekCenter(centerZ);
            // Cluster center offset across the channel (-5.0 to +5.0)
            const double clusterOffsetX = (hashUnit(hashMix(clusterSalt + 1U)) - 0.5) * 10.0;
            const double clusterX = channelCenter + clusterOffsetX;

            const std::size_t pebblesInCluster = 4 + static_cast<std::size_t>(hashUnit(hashMix(clusterSalt + 2U)) * 6.0);
            for (std::size_t p = 0; p < pebblesInCluster && count < targetMax; ++p) {
                const std::uint64_t pSalt = hashMix(clusterSalt ^ (static_cast<std::uint64_t>(p) << 4U));
                // Cluster dispersion with irregular radial offset
                const double angle = hashUnit(hashMix(pSalt)) * 6.283185307179586;
                const double dist = std::sqrt(hashUnit(hashMix(pSalt + 1U))) * 3.5;
                const double px = clusterX + std::cos(angle) * dist;
                const double pz = centerZ + std::sin(angle) * dist;

                if (!landscape_.inBounds(px, pz)) continue;

                // River pebbles belong in Creek bed or Riparian margins
                const Habitat hab = landscape_.habitat(px, pz);
                const double dx = std::abs(px - creekCenter(pz));

                // Must be within creek or immediately adjacent bed (dx <= 6.5)
                if (hab != Habitat::Creek && hab != Habitat::Riparian) continue;
                if (dx > 6.5) continue;

                const double py = landscape_.height(px, pz);
                const double scale = 0.15 + hashUnit(hashMix(pSalt + 2U)) * 0.45;
                const double heading = hashUnit(hashMix(pSalt + 3U)) * 6.283185307179586;
                const double pitch = (hashUnit(hashMix(pSalt + 4U)) - 0.5) * 0.3;
                const double roll = (hashUnit(hashMix(pSalt + 5U)) - 0.5) * 0.3;

                // Stable deterministic ID
                std::string id = "pebble_" + std::to_string(count);
                proposals_.push_back({std::move(id), DressingKind::RiverPebble, px, py, pz, scale, heading, pitch, roll});
                ++count;
            }
        }
    }

    void generateFallenLogs() {
        std::size_t count = 0;
        const std::size_t targetMax = budget_.maxFallenLogs;
        if (targetMax == 0) return;

        // Fallen logs: terrestrial only! Must respect wet channel exclusion (waterContains must be false).
        // Placed in woodland shoulders, riparian banks, and fallen tree zones.
        // Bounded clusters along the banks and outer riparian / woodland transition.
        constexpr int clusterCount = 14;
        for (int c = 0; c < clusterCount && count < targetMax; ++c) {
            const std::uint64_t clusterSalt = seed_ ^ hashMix(0xFEED02ULL + static_cast<std::uint64_t>(c));
            // Choose bank side: -1 (west) or +1 (east)
            const double side = (hashUnit(hashMix(clusterSalt)) > 0.5) ? 1.0 : -1.0;
            const double centerZ = -85.0 + hashUnit(hashMix(clusterSalt + 1U)) * 170.0;
            const double channelCenter = creekCenter(centerZ);
            // Placed at 5.5 to 28.0m from channel center (dry riparian to woodland)
            const double distFromCenter = 5.5 + hashUnit(hashMix(clusterSalt + 2U)) * 22.0;
            const double clusterX = channelCenter + side * distFromCenter;

            const std::size_t logsInCluster = 1 + static_cast<std::size_t>(hashUnit(hashMix(clusterSalt + 3U)) * 3.0);
            for (std::size_t l = 0; l < logsInCluster && count < targetMax; ++l) {
                const std::uint64_t lSalt = hashMix(clusterSalt ^ (static_cast<std::uint64_t>(l) << 5U));
                const double lx = clusterX + (hashUnit(hashMix(lSalt)) - 0.5) * 6.0;
                const double lz = centerZ + (hashUnit(hashMix(lSalt + 1U)) - 0.5) * 6.0;

                if (!landscape_.inBounds(lx, lz)) continue;

                // STRICT: Terrestrial item wet channel exclusion
                if (landscape_.waterContains(lx, lz)) continue;

                const Habitat hab = landscape_.habitat(lx, lz);
                // Fallen logs fit in Woodland, Riparian dry banks, or OpenLowland
                if (hab != Habitat::Woodland && hab != Habitat::Riparian && hab != Habitat::OpenLowland) continue;

                const double ly = landscape_.height(lx, lz);
                const double scale = 0.8 + hashUnit(hashMix(lSalt + 2U)) * 1.4;
                const double heading = hashUnit(hashMix(lSalt + 3U)) * 6.283185307179586;
                // Log pitch follows minor slope or terrain variation
                const double pitch = (hashUnit(hashMix(lSalt + 4U)) - 0.5) * 0.25;
                const double roll = (hashUnit(hashMix(lSalt + 5U)) - 0.5) * 0.2;

                std::string id = "log_" + std::to_string(count);
                proposals_.push_back({std::move(id), DressingKind::FallenLog, lx, ly, lz, scale, heading, pitch, roll});
                ++count;
            }
        }
    }

    void generateDryBankClumps() {
        std::size_t count = 0;
        const std::size_t targetMax = budget_.maxDryBankClumps;
        if (targetMax == 0) return;

        // Dry bank clumps: clusters of sedge/drift/scrub clumps strictly on dry ground near the creek banks.
        // STRICT: Exclude wetted channel.
        constexpr int clusterCount = 16;
        for (int c = 0; c < clusterCount && count < targetMax; ++c) {
            const std::uint64_t clusterSalt = seed_ ^ hashMix(0xFEED03ULL + static_cast<std::uint64_t>(c));
            const double side = (hashUnit(hashMix(clusterSalt)) > 0.5) ? 1.0 : -1.0;
            const double centerZ = -85.0 + hashUnit(hashMix(clusterSalt + 1U)) * 170.0;
            const double channelCenter = creekCenter(centerZ);
            // Bank zone: 4.8m to 12.0m from channel center
            const double bankDist = 4.8 + hashUnit(hashMix(clusterSalt + 2U)) * 7.2;
            const double clusterX = channelCenter + side * bankDist;

            const std::size_t clumpsInCluster = 3 + static_cast<std::size_t>(hashUnit(hashMix(clusterSalt + 3U)) * 5.0);
            for (std::size_t k = 0; k < clumpsInCluster && count < targetMax; ++k) {
                const std::uint64_t kSalt = hashMix(clusterSalt ^ (static_cast<std::uint64_t>(k) << 4U));
                const double angle = hashUnit(hashMix(kSalt)) * 6.283185307179586;
                const double dist = std::sqrt(hashUnit(hashMix(kSalt + 1U))) * 2.8;
                const double kx = clusterX + std::cos(angle) * dist;
                const double kz = centerZ + std::sin(angle) * dist;

                if (!landscape_.inBounds(kx, kz)) continue;

                // STRICT: Exclude wetted channel
                if (landscape_.waterContains(kx, kz)) continue;

                const Habitat hab = landscape_.habitat(kx, kz);
                // Must be Riparian or adjacent OpenLowland bank
                if (hab != Habitat::Riparian && hab != Habitat::OpenLowland) continue;

                const double ky = landscape_.height(kx, kz);
                const double scale = 0.4 + hashUnit(hashMix(kSalt + 2U)) * 0.6;
                const double heading = hashUnit(hashMix(kSalt + 3U)) * 6.283185307179586;
                const double pitch = (hashUnit(hashMix(kSalt + 4U)) - 0.5) * 0.15;
                const double roll = (hashUnit(hashMix(kSalt + 5U)) - 0.5) * 0.15;

                std::string id = "bank_clump_" + std::to_string(count);
                proposals_.push_back({std::move(id), DressingKind::DryBankClump, kx, ky, kz, scale, heading, pitch, roll});
                ++count;
            }
        }
    }

    void generateBluffAccents() {
        std::size_t count = 0;
        const std::size_t targetMax = budget_.maxBluffAccents;
        if (targetMax == 0) return;

        // Bluff accents: rock scree, talus, and cliff-edge accents located around Bluff and Outcrop habitats.
        // West side high relief region.
        constexpr int clusterCount = 10;
        for (int c = 0; c < clusterCount && count < targetMax; ++c) {
            const std::uint64_t clusterSalt = seed_ ^ hashMix(0xFEED04ULL + static_cast<std::uint64_t>(c));
            // Bluff center is roughly around x ~ -58 m
            const double centerZ = -70.0 + hashUnit(hashMix(clusterSalt)) * 140.0;
            const double centerX = -58.0 + (hashUnit(hashMix(clusterSalt + 1U)) - 0.5) * 20.0;

            const std::size_t accentsInCluster = 3 + static_cast<std::size_t>(hashUnit(hashMix(clusterSalt + 2U)) * 4.0);
            for (std::size_t a = 0; a < accentsInCluster && count < targetMax; ++a) {
                const std::uint64_t aSalt = hashMix(clusterSalt ^ (static_cast<std::uint64_t>(a) << 4U));
                const double angle = hashUnit(hashMix(aSalt)) * 6.283185307179586;
                const double dist = std::sqrt(hashUnit(hashMix(aSalt + 1U))) * 5.0;
                const double ax = centerX + std::cos(angle) * dist;
                const double az = centerZ + std::sin(angle) * dist;

                if (!landscape_.inBounds(ax, az)) continue;

                // STRICT: Terrestrial item wet exclusion
                if (landscape_.waterContains(ax, az)) continue;

                const Habitat hab = landscape_.habitat(ax, az);
                if (hab != Habitat::Bluff && hab != Habitat::Outcrop) continue;

                const double ay = landscape_.height(ax, az);
                const double scale = 0.5 + hashUnit(hashMix(aSalt + 2U)) * 1.1;
                const double heading = hashUnit(hashMix(aSalt + 3U)) * 6.283185307179586;
                const double pitch = (hashUnit(hashMix(aSalt + 4U)) - 0.5) * 0.4;
                const double roll = (hashUnit(hashMix(aSalt + 5U)) - 0.5) * 0.4;

                std::string id = "bluff_accent_" + std::to_string(count);
                proposals_.push_back({std::move(id), DressingKind::BluffAccent, ax, ay, az, scale, heading, pitch, roll});
                ++count;
            }
        }
    }
};

} // namespace stalthos::domain
