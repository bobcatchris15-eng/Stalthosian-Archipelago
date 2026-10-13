#pragma once

#include "reference_landscape.hpp"
#include "environment_palette.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace stalthos::domain {

struct TreeAssetProps {
    double height = 0.0;
    double crownWidth = 0.0;
};

struct TreePlacement {
    std::uint64_t id = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double headingRadians = 0.0;
    double scale = 1.0;
    bool isSapling = false;
    TreeAssetProps assetProps{};
};

class WoodlandStands {
    std::vector<TreePlacement> placements_;
    std::size_t candidateAttempts_ = 0;
    std::size_t clusterAttempts_ = 0;
    bool candidateBudgetExhausted_ = false;

    bool isValidPosition(const ReferenceLandscape& landscape, double x, double z) const {
        if (!std::isfinite(x) || !std::isfinite(z)) return false;

        constexpr double dProbe = 0.1;
        // Probe points for finite-difference slope: (x, z), (x + dProbe, z), (x, z + dProbe)
        // Ensure all query points stay strictly within landscape bounds
        if (!landscape.inBounds(x, z) ||
            !landscape.inBounds(x + dProbe, z) ||
            !landscape.inBounds(x, z + dProbe)) {
            return false;
        }

        // Background valley z roughly [-65, -12]
        if (z < -65.0 || z > -12.0) return false;

        // Dry banks x <= -15 or x >= 20
        if (x > -15.0 && x < 20.0) return false;

        // Clear inspection corridor around hero (12, 0)
        double dx = x - 12.0;
        double dz = z - 0.0;
        if (std::sqrt(dx * dx + dz * dz) < 8.0) return false; // Corridor radius

        // Creek wet mask
        if (landscape.waterContains(x, z)) return false;

        // Reject steep terrain (> 20 degrees)
        double h0 = landscape.height(x, z);
        double hx = landscape.height(x + dProbe, z);
        double hz = landscape.height(x, z + dProbe);
        if (!std::isfinite(h0) || !std::isfinite(hx) || !std::isfinite(hz)) return false;

        double slopeX = (hx - h0) / dProbe;
        double slopeZ = (hz - h0) / dProbe;
        if (!std::isfinite(slopeX) || !std::isfinite(slopeZ)) return false;

        double denom = std::sqrt(1.0 + slopeX * slopeX + slopeZ * slopeZ);
        if (!std::isfinite(denom) || denom < 1.0) return false;

        double cosTheta = 1.0 / denom;
        if (cosTheta < 0.0 || cosTheta > 1.0 || !std::isfinite(cosTheta)) return false;

        double slopeRadians = std::acos(cosTheta);
        if (!std::isfinite(slopeRadians)) return false;
        if (slopeRadians > (20.0 * 3.14159265358979323846 / 180.0)) return false;

        return true;
    }

public:
    static constexpr std::size_t kMaxClusterAttempts = 50;
    static constexpr std::size_t kMaxTreeAttemptsPerCluster = 15;
    static constexpr std::size_t kMaxTotalCandidateAttempts = kMaxClusterAttempts * kMaxTreeAttemptsPerCluster; // 750
    static constexpr std::size_t kHardCapTrees = 32;

    explicit WoodlandStands(const ReferenceLandscape& landscape, std::uint64_t seed, std::size_t maxTrees = 20) {
        if (maxTrees > kHardCapTrees) {
            throw std::invalid_argument("WoodlandStands hard cap is 32 trees");
        }

        const TreeAssetProps adultProps{14.0, 6.5};
        const TreeAssetProps saplingProps{4.2, 2.4};

        std::uint64_t currentSeed = hashMix(seed ^ UINT64_C(0xDEADBEEF12345678));

        std::size_t placed = 0;

        while (clusterAttempts_ < kMaxClusterAttempts && candidateAttempts_ < kMaxTotalCandidateAttempts) {
            if (placed >= maxTrees) {
                break;
            }

            currentSeed = hashMix(currentSeed);
            double cx = -ReferenceLandscape::halfExtent + 2.0 * ReferenceLandscape::halfExtent * hashUnit(currentSeed);

            currentSeed = hashMix(currentSeed);
            double cz = -ReferenceLandscape::halfExtent + 2.0 * ReferenceLandscape::halfExtent * hashUnit(currentSeed);

            std::size_t treeAttempts = 0;
            currentSeed = hashMix(currentSeed);
            std::size_t clusterSize = 1 + static_cast<std::size_t>(hashUnit(currentSeed) * 4.0);

            std::size_t clusterPlaced = 0;

            while (clusterPlaced < clusterSize && treeAttempts < kMaxTreeAttemptsPerCluster && candidateAttempts_ < kMaxTotalCandidateAttempts) {
                if (placed >= maxTrees) {
                    break;
                }

                candidateAttempts_++;
                treeAttempts++;

                currentSeed = hashMix(currentSeed);
                double tx = cx + (hashUnit(currentSeed) - 0.5) * 12.0; // Cluster spread

                currentSeed = hashMix(currentSeed);
                double tz = cz + (hashUnit(currentSeed) - 0.5) * 12.0;

                if (isValidPosition(landscape, tx, tz)) {
                    currentSeed = hashMix(currentSeed);
                    bool isSapling = hashUnit(currentSeed) < 0.3; // 30% chance for sapling

                    currentSeed = hashMix(currentSeed);
                    double heading = hashUnit(currentSeed) * 2.0 * 3.14159265358979323846;

                    currentSeed = hashMix(currentSeed);
                    double scale = 0.9 + hashUnit(currentSeed) * 0.2; // 0.9 to 1.1

                    double ty = landscape.height(tx, tz);

                    TreePlacement tp;
                    tp.id = hashMix(currentSeed ^ (static_cast<std::uint64_t>(placed) << 32));
                    tp.x = tx;
                    tp.z = tz;
                    tp.y = ty;
                    tp.headingRadians = heading;
                    tp.scale = scale;
                    tp.isSapling = isSapling;
                    tp.assetProps = isSapling ? saplingProps : adultProps;

                    placements_.push_back(tp);
                    placed++;
                    clusterPlaced++;
                }
            }
            clusterAttempts_++;
        }

        if (placed < maxTrees && (clusterAttempts_ >= kMaxClusterAttempts || candidateAttempts_ >= kMaxTotalCandidateAttempts)) {
            candidateBudgetExhausted_ = true;
        }
    }

    const std::vector<TreePlacement>& placements() const noexcept {
        return placements_;
    }

    std::size_t candidateAttempts() const noexcept {
        return candidateAttempts_;
    }

    std::size_t clusterAttempts() const noexcept {
        return clusterAttempts_;
    }

    bool isCandidateBudgetExhausted() const noexcept {
        return candidateBudgetExhausted_;
    }
};

} // namespace stalthos::domain
