#pragma once

#include "foundation.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace stalthos::world {

struct Region {
    RegionId id;
    const char* name;
    double centerX;
    double centerZ;
    double referenceSpanMeters;
    double moisture;
    double canopy;
    double shelter;
    uint32_t coarsePopulation;
};

class Archipelago {
public:
    static constexpr double worldBoundsMeters = 14000.0;

    explicit Archipelago(uint64_t seed) : seed_(seed) {
        // IDs: 1001, 1002, 1003, 1004.
        // Main Island center at (0, 0).
        // Canonical spans from geography.md:
        // Main Island: ~7-8 km (e.g. 7500 m)
        // Rainridge: ~5-6 km (e.g. 5500 m)
        // Stone Island: flexible, 4-5 km (e.g. 4500 m)
        // Outer Reach: ~5-6 km (e.g. 5500 m)
        // Coarse demographic counts bounded (deterministic based on seed and baseline capacity).
        // Rainridge: wetter, high canopy/shelter. Outer Reach: more open, lower moisture. Stone Island: rocky platform, moderate.
        const uint32_t popMain  = static_cast<uint32_t>(1800 + (mix(seed_ ^ 1001ULL) % 401)); // [1800, 2200]
        const uint32_t popRain  = static_cast<uint32_t>(900  + (mix(seed_ ^ 1002ULL) % 251)); // [900, 1150]
        const uint32_t popStone = static_cast<uint32_t>(450  + (mix(seed_ ^ 1003ULL) % 151)); // [450, 600]
        const uint32_t popOuter = static_cast<uint32_t>(600  + (mix(seed_ ^ 1004ULL) % 201)); // [600, 800]

        regions_ = {{
            Region{1001ULL, "Main Island",      0.0,      0.0, 7500.0, 0.58, 0.62, 0.55, popMain},
            Region{1002ULL, "Rainridge",    -6200.0,   4500.0, 5500.0, 0.88, 0.78, 0.82, popRain},
            Region{1003ULL, "Stone Island",  5800.0,  -4200.0, 4500.0, 0.42, 0.38, 0.48, popStone},
            Region{1004ULL, "Outer Reach",   6800.0,   5200.0, 5500.0, 0.28, 0.22, 0.35, popOuter}
        }};
    }

    const std::array<Region, 4>& regions() const {
        return regions_;
    }

    double elevation(double x, double z) const {
        validate(x, z);
        return computeElevation(x, z);
    }

    double moisture(double x, double z) const {
        validate(x, z);
        return computeMoisture(x, z);
    }

private:
    uint64_t seed_;
    std::array<Region, 4> regions_;

    static void validate(double x, double z) {
        if (!std::isfinite(x) || !std::isfinite(z)) {
            throw std::invalid_argument("archipelago query coordinates must be finite");
        }
        if (std::abs(x) > worldBoundsMeters || std::abs(z) > worldBoundsMeters) {
            throw std::out_of_range("archipelago query coordinates exceed world bounds (+-14000m)");
        }
    }

    static uint64_t mix(uint64_t v) {
        v += 0x9e3779b97f4a7c15ULL;
        v = (v ^ (v >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        v = (v ^ (v >> 27U)) * 0x94d049bb133111ebULL;
        return v ^ (v >> 31U);
    }

    static double smoothstep(double edge0, double edge1, double x) {
        double t = (x - edge0) / (edge1 - edge0);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        return t * t * (3.0 - 2.0 * t);
    }

    // Coarse continuous structural field
    double computeElevation(double x, double z) const {
        // Drowned upland continuous field:
        // 1. Regional tectonic/fault trend (ancient resistant ridge lines running roughly SW-NE)
        // Shared structural strike across the archipelago (angle ~ 0.58 rad, ~33 degrees)
        const double strikeX =  0.83867;
        const double strikeZ =  0.54464;
        const double dipX    = -0.54464;
        const double dipZ    =  0.83867;

        const double u = x * strikeX + z * strikeZ; // along strike
        const double v = x * dipX    + z * dipZ;    // across strike

        // Shared structural corrugations (faulted ridges and valleys)
        const double ridge1 = std::sin(v / 2100.0) * 45.0;
        const double ridge2 = std::sin((v + 800.0) / 1050.0) * 25.0;
        const double crossSpur = std::sin(u / 3200.0) * std::cos(v / 2800.0) * 35.0;
        const double structuralUpland = ridge1 + ridge2 + crossSpur;

        // 2. Continuous sea floor / drowned basin subsidence term:
        // Deep open ocean drop-off as we approach world perimeter
        const double distFromOrigin = std::sqrt(x * x + z * z);
        const double basinMarginDrop = smoothstep(6500.0, 13500.0, distFromOrigin) * 220.0;

        // 3. Highland masses centered near the 4 regions:
        // Each highland mass decays smoothly with distance, reflecting the four major surviving drowned uplands.
        // Main Island (radius ~ 3600m)
        const double dMain = std::sqrt(x * x + z * z);
        const double massMain = (1.0 - smoothstep(1800.0, 4200.0, dMain)) * 145.0;

        // Rainridge (-6200, 4500) (radius ~ 2800m)
        const double dxRain = x - (-6200.0);
        const double dzRain = z - (4500.0);
        const double dRain = std::sqrt(dxRain * dxRain + dzRain * dzRain);
        const double massRain = (1.0 - smoothstep(1200.0, 3200.0, dRain)) * 190.0;

        // Stone Island (5800, -4200) (radius ~ 2400m)
        const double dxStone = x - (5800.0);
        const double dzStone = z - (-4200.0);
        const double dStone = std::sqrt(dxStone * dxStone + dzStone * dzStone);
        const double massStone = (1.0 - smoothstep(1000.0, 2700.0, dStone)) * 110.0;

        // Outer Reach (6800, 5200) (radius ~ 2800m)
        const double dxOuter = x - (6800.0);
        const double dzOuter = z - (5200.0);
        const double dOuter = std::sqrt(dxOuter * dxOuter + dzOuter * dzOuter);
        const double massOuter = (1.0 - smoothstep(1100.0, 3100.0, dOuter)) * 160.0;

        // Combined highland uplift
        const double highlands = massMain + massRain + massStone + massOuter;

        // 4. Inundated channels / tectonic straits between island masses:
        // Channel between Main and Rainridge (midpoint around -3100, 2250)
        // Channel between Main and Stone (midpoint around 2900, -2100)
        // Channel between Main and Outer (midpoint around 3400, 2600)
        // Deep strait between Outer Reach and Stone (x around 6300, z around 500)
        // Tectonic drowned valleys ensure inter-island straits sit comfortably below sea level (elevation < 0)
        // Base drowned shelf baseline is below sea level (-65m)
        double elevation = -65.0 + highlands + structuralUpland * (0.35 + 0.65 * (highlands / 190.0)) - basinMarginDrop;

        // 5. Seed-derived bounded low-frequency surface variation (low amplitude, smooth sinusoidal mix with seed-derived phases)
        const double p0 = static_cast<double>(mix(seed_) % 10000ULL) / 10000.0 * 6.2831853;
        const double p1 = static_cast<double>(mix(seed_ ^ 0xa5a5ULL) % 10000ULL) / 10000.0 * 6.2831853;
        const double p2 = static_cast<double>(mix(seed_ ^ 0x5c5cULL) % 10000ULL) / 10000.0 * 6.2831853;
        const double seedJitter = (std::sin(x / 1400.0 + p0) * std::cos(z / 1400.0 + p1) +
                                   std::sin((x + z) / 2200.0 + p2)) * 6.0;

        elevation += seedJitter;

        // 6. Scattered coastal remnants / islets (shoals / rocks)
        // Minor resistant promontories along the shelf
        const double skerry1 = std::exp(-((x + 2200.0) * (x + 2200.0) + (z - 2800.0) * (z - 2800.0)) / (700.0 * 700.0)) * 28.0;
        const double skerry2 = std::exp(-((x - 2600.0) * (x - 2600.0) + (z + 2800.0) * (z + 2800.0)) / (600.0 * 600.0)) * 24.0;
        elevation += (skerry1 + skerry2);

        return elevation;
    }

    double computeMoisture(double x, double z) const {
        // Continuous moisture field combining regional macro gradient (rainward northwest / maritime)
        // with topographical rain-shadow and island affinity.
        // Rainridge is distinctly wettest (~0.88), Outer Reach is more open/arid (~0.28).
        // Macro moisture gradient: higher towards north-west (-X, +Z), lower towards south-east (+X, -Z).
        const double macro = 0.52 - (x / 24000.0) + (z / 26000.0);

        // Island regional influences:
        const double dMain = std::sqrt(x * x + z * z);
        const double wMain = (1.0 - smoothstep(1200.0, 4800.0, dMain));

        const double dRain = std::sqrt((x + 6200.0) * (x + 6200.0) + (z - 4500.0) * (z - 4500.0));
        const double wRain = (1.0 - smoothstep(800.0, 4200.0, dRain));

        const double dStone = std::sqrt((x - 5800.0) * (x - 5800.0) + (z + 4200.0) * (z + 4200.0));
        const double wStone = (1.0 - smoothstep(800.0, 3800.0, dStone));

        const double dOuter = std::sqrt((x - 6800.0) * (x - 6800.0) + (z - 5200.0) * (z - 5200.0));
        const double wOuter = (1.0 - smoothstep(800.0, 4200.0, dOuter));

        double m = macro + (wRain * 0.32) - (wOuter * 0.22) - (wStone * 0.08) + (wMain * 0.06);

        // Bounded variation from seed
        const double p0 = static_cast<double>(mix(seed_ ^ 0xbeefULL) % 10000ULL) / 10000.0 * 6.2831853;
        m += std::sin((x - z) / 3200.0 + p0) * 0.04;

        if (m < 0.05) m = 0.05;
        if (m > 0.98) m = 0.98;
        return m;
    }
};

} // namespace stalthos::world
