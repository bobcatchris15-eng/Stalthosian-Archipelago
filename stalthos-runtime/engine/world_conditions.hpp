#pragma once

#include "WickedEngine.h"
#include "domain/archipelago_world.hpp"
#include "domain/world_environment.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace stalthos::engine {

struct RegionConditionsSnapshot {
    world::Region region;
    double elevationMeters = 0;
    double moisture = 0;
    environment::Conditions conditions{};
};

inline const char* weatherName(environment::Weather weather) {
    switch (weather) {
    case environment::Weather::Clear: return "clear";
    case environment::Weather::Overcast: return "overcast";
    case environment::Weather::Rain: return "rain";
    case environment::Weather::Storm: return "storm";
    }
    throw std::invalid_argument("invalid weather value");
}

inline std::vector<RegionConditionsSnapshot> sampleRegionConditions(
    const world::Archipelago& archipelago,
    std::uint64_t worldSeed,
    double calendarSeconds
) {
    std::vector<RegionConditionsSnapshot> snapshots;
    snapshots.reserve(archipelago.regions().size());
    for (const auto& region : archipelago.regions()) {
        const double localMoisture = archipelago.moisture(region.centerX, region.centerZ);
        const environment::Site site{
            region.canopy,
            region.shelter,
            localMoisture,
            0.0 // Inland regional sites are provisionally freshwater.
        };
        snapshots.push_back({
            region,
            archipelago.elevation(region.centerX, region.centerZ),
            localMoisture,
            environment::sample(worldSeed, region.id, calendarSeconds, site)
        });
    }
    return snapshots;
}

inline std::string regionConditionsJson(const std::vector<RegionConditionsSnapshot>& snapshots) {
    std::ostringstream out;
    out << std::setprecision(17);
    out << "{\"regions\":[";
    for (std::size_t i = 0; i < snapshots.size(); ++i) {
        const auto& item = snapshots[i];
        if (i != 0) out << ',';
        out << "{\"id\":" << item.region.id
            << ",\"name\":\"" << item.region.name << '\"'
            << ",\"centerX\":" << item.region.centerX
            << ",\"centerZ\":" << item.region.centerZ
            << ",\"elevationMeters\":" << item.elevationMeters
            << ",\"moisture\":" << item.moisture
            << ",\"canopy\":" << item.region.canopy
            << ",\"shelter\":" << item.region.shelter
            << ",\"coarsePopulation\":" << item.region.coarsePopulation
            << ",\"dayFraction\":" << item.conditions.dayFraction
            << ",\"seasonFraction\":" << item.conditions.seasonFraction
            << ",\"cloud\":" << item.conditions.cloud
            << ",\"rainfall\":" << item.conditions.rainfall
            << ",\"windMetersPerSecond\":" << item.conditions.windMetersPerSecond
            << ",\"temperatureC\":" << item.conditions.temperatureC
            << ",\"wetness\":" << item.conditions.wetness
            << ",\"floodRisk\":" << item.conditions.floodRisk
            << ",\"drinkable\":" << (item.conditions.drinkable ? "true" : "false")
            << ",\"weather\":\"" << weatherName(item.conditions.weather) << "\"}";
    }
    out << "]}";
    return out.str();
}

// Maps sampled regional wind and rain into Wicked's documented Scene::weather fields.
// The caller owns the selected site and wind direction; this does not create a solver.
inline void applyWeatherToScene(
    wi::scene::Scene& scene,
    const environment::Conditions& conditions,
    XMFLOAT3 windDirection
) {
    const double directionLength = std::sqrt(
        static_cast<double>(windDirection.x) * windDirection.x +
        static_cast<double>(windDirection.y) * windDirection.y +
        static_cast<double>(windDirection.z) * windDirection.z);
    if (!std::isfinite(conditions.rainfall) || conditions.rainfall < 0 || conditions.rainfall > 1 ||
        !std::isfinite(conditions.windMetersPerSecond) || conditions.windMetersPerSecond < 0 ||
        !std::isfinite(directionLength) || directionLength <= 1e-6) {
        throw std::invalid_argument("invalid scene weather input");
    }
    scene.weather.rain_amount = static_cast<float>(conditions.rainfall);
    scene.weather.windSpeed = static_cast<float>(conditions.windMetersPerSecond);
    scene.weather.windDirection = XMFLOAT3(
        static_cast<float>(windDirection.x / directionLength),
        static_cast<float>(windDirection.y / directionLength),
        static_cast<float>(windDirection.z / directionLength));
}

} // namespace stalthos::engine
