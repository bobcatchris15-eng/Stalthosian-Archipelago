#pragma once

#include "resource_ledger.hpp"
#include <cmath>
#include <optional>

namespace stalthos {

struct ShrubVisualState {
    bool valid;
    bool woodyStemVisible;
    double foliageFraction;
    int leafClusterCount;
};

class ShrubPresentation {
public:
    static ShrubVisualState evaluate(const ResourceDto& dto, const std::optional<ShrubVisualState>& previousState = std::nullopt) {
        ShrubVisualState result = {false, true, 0.0, 0};

        if (dto.kind != ResourceKind::Shrub) {
            return result;
        }

        if (!std::isfinite(dto.initialQuantity) || !std::isfinite(dto.consumedQuantity)) {
            return result;
        }

        if (dto.initialQuantity < 0.0 || dto.consumedQuantity < 0.0 || dto.consumedQuantity > dto.initialQuantity) {
             return result;
        }

        result.valid = true;
        result.woodyStemVisible = true; // Stem is strictly preserved

        if (dto.initialQuantity == 0.0) {
            result.foliageFraction = 0.0;
            result.leafClusterCount = 0;
            return result;
        }

        const double remaining = dto.initialQuantity - dto.consumedQuantity;
        result.foliageFraction = remaining / dto.initialQuantity;

        const int maxClusters = 4;
        const double scaled = result.foliageFraction * maxClusters;
        int targetClusters = static_cast<int>(std::round(scaled));

        if (previousState && previousState->valid &&
            previousState->leafClusterCount >= 0 && previousState->leafClusterCount <= maxClusters &&
            std::isfinite(previousState->foliageFraction) &&
            previousState->foliageFraction >= 0.0 && previousState->foliageFraction <= 1.0) {
            double diff = scaled - previousState->leafClusterCount;
            // 0.1 hysteresis margin around the 0.5 rounding boundary
            if (diff > 0.0 && diff < 0.6) {
                targetClusters = previousState->leafClusterCount;
            } else if (diff < 0.0 && diff > -0.6) {
                targetClusters = previousState->leafClusterCount;
            }
        }

        result.leafClusterCount = targetClusters;

        if (result.leafClusterCount > maxClusters) result.leafClusterCount = maxClusters;
        if (result.leafClusterCount < 0) result.leafClusterCount = 0;

        if (remaining <= 0.0) {
            result.foliageFraction = 0.0;
            result.leafClusterCount = 0;
        }

        return result;
    }
};

} // namespace stalthos
