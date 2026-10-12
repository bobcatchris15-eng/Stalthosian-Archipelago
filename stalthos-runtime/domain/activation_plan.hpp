#pragma once

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <cmath>
#include "foundation.hpp"

namespace stalthos::activation {

enum class Tier { Physical, Presentation, Distant };

struct ActivationConfig {
    double physicalRadius;
    double physicalHysteresis;
    std::size_t physicalBudget;

    double presentationRadius;
    double presentationHysteresis;
    std::size_t presentationBudget;
};

struct RegionInput {
    stalthos::RegionId id;
    stalthos::Vec3 position;
    Tier currentTier;
};

struct RegionPlan {
    stalthos::RegionId id;
    Tier targetTier;
};

struct ActivationPlan {
    std::vector<RegionPlan> regions;
};

class ActivationPlanner {
    static double distance(const stalthos::Vec3& a, const stalthos::Vec3& b) {
        double dx = a.x - b.x;
        double dy = a.y - b.y;
        double dz = a.z - b.z;
        if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) {
            throw std::overflow_error("region distance overflow");
        }
        const double result = std::hypot(dx, dy, dz);
        if (!std::isfinite(result)) throw std::overflow_error("region distance overflow");
        return result;
    }

public:
    static constexpr std::size_t MaxRegions = 100'000;

    static ActivationPlan plan(
        stalthos::Vec3 bodyPosition,
        stalthos::Vec3 cameraPosition,
        const ActivationConfig& config,
        const std::vector<RegionInput>& regions)
    {
        if (!stalthos::finite(bodyPosition) || !stalthos::finite(cameraPosition)) {
            throw std::invalid_argument("invalid positions");
        }
        if (!std::isfinite(config.physicalRadius) || config.physicalRadius < 0 ||
            !std::isfinite(config.physicalHysteresis) || config.physicalHysteresis < 0 ||
            !std::isfinite(config.presentationRadius) || config.presentationRadius < 0 ||
            !std::isfinite(config.presentationHysteresis) || config.presentationHysteresis < 0) {
            throw std::invalid_argument("invalid config");
        }
        const double physicalLimit = config.physicalRadius + config.physicalHysteresis;
        const double presentationLimit = config.presentationRadius + config.presentationHysteresis;
        if (!std::isfinite(physicalLimit) || !std::isfinite(presentationLimit)) {
            throw std::overflow_error("activation radius overflow");
        }
        if (regions.size() > MaxRegions || config.physicalBudget > MaxRegions ||
            config.presentationBudget > MaxRegions) {
            throw std::invalid_argument("activation input exceeds bounds");
        }

        std::vector<stalthos::RegionId> seenIds;
        seenIds.reserve(regions.size());
        for (const auto& r : regions) {
            if (!stalthos::finite(r.position)) throw std::invalid_argument("invalid region position");
            if (static_cast<unsigned>(r.currentTier) > static_cast<unsigned>(Tier::Distant)) {
                throw std::invalid_argument("invalid tier");
            }
            seenIds.push_back(r.id);
        }
        std::sort(seenIds.begin(), seenIds.end());
        if (std::unique(seenIds.begin(), seenIds.end()) != seenIds.end()) {
            throw std::invalid_argument("duplicate region IDs");
        }

        struct Eval {
            stalthos::RegionId id;
            double distance;
        };

        std::vector<Eval> physQualifying;
        for (const auto& r : regions) {
            double pMax = config.physicalRadius + (r.currentTier == Tier::Physical ? config.physicalHysteresis : 0.0);
            double bodyDistance = distance(r.position, bodyPosition);
            if (bodyDistance <= pMax) {
                physQualifying.push_back({r.id, bodyDistance});
            }
        }

        auto sortByDistId = [](const Eval& a, const Eval& b) {
            if (a.distance != b.distance) return a.distance < b.distance;
            return a.id < b.id;
        };

        std::sort(physQualifying.begin(), physQualifying.end(), sortByDistId);

        ActivationPlan plan;
        std::vector<stalthos::RegionId> assignedPhys;

        for (std::size_t i = 0; i < physQualifying.size() && i < config.physicalBudget; ++i) {
            plan.regions.push_back({physQualifying[i].id, Tier::Physical});
            assignedPhys.push_back(physQualifying[i].id);
        }

        std::sort(assignedPhys.begin(), assignedPhys.end());

        std::vector<Eval> presQualifying;
        for (const auto& r : regions) {
            if (std::binary_search(assignedPhys.begin(), assignedPhys.end(), r.id)) {
                continue;
            }

            double vMax = config.presentationRadius + ((r.currentTier == Tier::Physical || r.currentTier == Tier::Presentation) ? config.presentationHysteresis : 0.0);
            double cameraDistance = distance(r.position, cameraPosition);
            if (cameraDistance <= vMax) {
                presQualifying.push_back({r.id, cameraDistance});
            }
        }

        std::sort(presQualifying.begin(), presQualifying.end(), sortByDistId);

        for (std::size_t i = 0; i < presQualifying.size() && i < config.presentationBudget; ++i) {
            plan.regions.push_back({presQualifying[i].id, Tier::Presentation});
        }

        std::sort(plan.regions.begin(), plan.regions.end(), [](const RegionPlan& a, const RegionPlan& b) {
            return a.id < b.id;
        });

        return plan;
    }
};

} // namespace stalthos::activation
