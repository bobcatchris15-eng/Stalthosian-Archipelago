#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include "foundation.hpp"

namespace stalthos::environment {

enum class Weather { Clear, Overcast, Rain, Storm };

struct Site {
    double canopy = 0;
    double shelter = 0;
    double moisture = 0.5;
    double salinity = 0;
};

struct Conditions {
    double dayFraction;
    double seasonFraction;
    double cloud;
    double rainfall;
    double windMetersPerSecond;
    double temperatureC;
    double wetness;
    double floodRisk;
    bool drinkable;
    Weather weather;
};

inline std::uint64_t mixSeed(std::uint64_t x) {
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

inline double periodic(double value, double period) {
    return std::fmod(value, period);
}

inline double lerp(double a, double b, double t) {
    return a + t * (b - a);
}

inline Conditions sample(std::uint64_t worldSeed, RegionId region, double calendarSeconds, Site site = {}) {
    if (!std::isfinite(calendarSeconds) || calendarSeconds < 0) {
        throw std::invalid_argument("invalid time");
    }
    if (!std::isfinite(site.canopy) || site.canopy < 0 || site.canopy > 1 ||
        !std::isfinite(site.shelter) || site.shelter < 0 || site.shelter > 1 ||
        !std::isfinite(site.moisture) || site.moisture < 0 || site.moisture > 1 ||
        !std::isfinite(site.salinity) || site.salinity < 0 || site.salinity > 1) {
        throw std::invalid_argument("invalid site parameters");
    }

    // Bound time to prevent trig function precision issues for extremely large values
    // Using a large multiple of both day and season periods
    constexpr double DAY_SECONDS = 86400.0;
    constexpr double SEASON_SECONDS = 86400.0 * 90.0; // 90 days per season
    constexpr double WEATHER_PERIOD = 86400.0 * 7.0; // 7 days weather cycle
    
    // LCM of periods, roughly
    constexpr double MACRO_PERIOD = DAY_SECONDS * 90.0 * 7.0; 
    
    double boundedTime = std::fmod(calendarSeconds, MACRO_PERIOD);

    std::uint64_t regionHash = mixSeed(mixSeed(worldSeed) ^ mixSeed(region));
    double regionPhase = (regionHash % 1000000) / 1000000.0;
    
    double dayFraction = std::fmod(boundedTime, DAY_SECONDS) / DAY_SECONDS;
    double seasonFraction = std::fmod(boundedTime, SEASON_SECONDS) / SEASON_SECONDS;

    // Weather phases
    double weatherTime = boundedTime / WEATHER_PERIOD + regionPhase;
    double w1 = (std::sin(weatherTime * 2.0 * 3.141592653589793) + 1.0) * 0.5;
    double w2 = (std::sin(weatherTime * 3.0 * 3.141592653589793 + regionPhase * 10.0) + 1.0) * 0.5;

    double cloudBase = w1 * 0.8 + w2 * 0.2;
    // Rainfall requires clouds
    double rainBase = 0.0;
    if (cloudBase > 0.6) {
        rainBase = (cloudBase - 0.6) / 0.4;
    }

    double cloud = cloudBase;
    double rainfall = rainBase;
    
    Weather weather = Weather::Clear;
    if (rainfall > 0.7) {
        weather = Weather::Storm;
    } else if (rainfall > 0.0) {
        weather = Weather::Rain;
    } else if (cloud > 0.4) {
        weather = Weather::Overcast;
    }

    // Wind
    double windMetersPerSecond = 1.0 + w2 * 4.0 + (weather == Weather::Storm ? 15.0 : 0.0) + (weather == Weather::Rain ? 5.0 : 0.0);
    // Shelter reduces wind locally
    windMetersPerSecond *= (1.0 - site.shelter * 0.8);

    // Temperature (fictional natural-history uncalibrated)
    double baseTemp = 25.0; // 25C
    double diurnalTemp = std::sin((dayFraction - 0.25) * 2.0 * 3.141592653589793) * 5.0;
    double seasonalTemp = std::sin((seasonFraction - 0.25) * 2.0 * 3.141592653589793) * 10.0;
    
    double temperatureC = baseTemp + diurnalTemp + seasonalTemp;
    
    // Canopy shades and cools during day, warms during night
    if (dayFraction > 0.25 && dayFraction < 0.75) {
        temperatureC -= site.canopy * 3.0; // Cooling
    } else {
        temperatureC += site.canopy * 1.5; // Insulation
    }
    // Rain cools
    temperatureC -= rainfall * 3.0;

    // Wetness
    double wetness = site.moisture * 0.5 + rainfall * 0.5 + (site.canopy * 0.2 * rainfall);
    if (wetness > 1.0) wetness = 1.0;
    if (wetness < 0.0) wetness = 0.0;

    // Flood risk
    double floodRisk = (site.moisture * 0.4) + (rainfall * 0.8);
    // Shelter / canopy can reduce or increase flood risk depending on interpretation.
    // The prompt says "High rain+moisture low shelter floodRisk higher than dryridge."
    floodRisk -= site.shelter * 0.3;
    if (floodRisk > 1.0) floodRisk = 1.0;
    if (floodRisk < 0.0) floodRisk = 0.0;

    bool drinkable = (site.salinity == 0.0);

    return Conditions{
        dayFraction,
        seasonFraction,
        cloud,
        rainfall,
        windMetersPerSecond,
        temperatureC,
        wetness,
        floodRisk,
        drinkable,
        weather
    };
}

} // namespace stalthos::environment
