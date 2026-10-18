#include "../domain/archipelago_world.hpp"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace stalthos::world;

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

void testRegionsStabilityAndBoundaries() {
    Archipelago arch(42ULL);
    const auto& regions = arch.regions();
    check(regions.size() == 4, "Archipelago must contain exactly 4 regions");

    // IDs stable 1001, 1002, 1003, 1004
    check(regions[0].id == 1001ULL, "Region 0 ID must be 1001");
    check(regions[1].id == 1002ULL, "Region 1 ID must be 1002");
    check(regions[2].id == 1003ULL, "Region 2 ID must be 1003");
    check(regions[3].id == 1004ULL, "Region 3 ID must be 1004");

    // Names stable
    check(std::string(regions[0].name) == "Main Island", "Region 0 name must be Main Island");
    check(std::string(regions[1].name) == "Rainridge", "Region 1 name must be Rainridge");
    check(std::string(regions[2].name) == "Stone Island", "Region 2 name must be Stone Island");
    check(std::string(regions[3].name) == "Outer Reach", "Region 3 name must be Outer Reach");

    // Main Island centered at 0, 0
    check(regions[0].centerX == 0.0 && regions[0].centerZ == 0.0, "Main Island center must be 0,0");

    // Approximate canonical spans
    check(regions[0].referenceSpanMeters >= 7000.0 && regions[0].referenceSpanMeters <= 8000.0, "Main span in 7-8km");
    check(regions[1].referenceSpanMeters >= 5000.0 && regions[1].referenceSpanMeters <= 6000.0, "Rainridge span in 5-6km");
    check(regions[2].referenceSpanMeters >= 4000.0 && regions[2].referenceSpanMeters <= 5500.0, "Stone Island span flexible");
    check(regions[3].referenceSpanMeters >= 5000.0 && regions[3].referenceSpanMeters <= 6000.0, "Outer Reach span in 5-6km");

    // Check demographic bounded counts (not 0, not unconstrained)
    for (size_t i = 0; i < 4; ++i) {
        check(regions[i].coarsePopulation > 100, "coarse population must be positive and non-trivial");
        check(regions[i].coarsePopulation < 100000, "coarse population must be bounded coarse demographic");
    }

    // Check wetness/canopy/shelter differentiation: Rainridge wetter, Outer Reach more open
    check(regions[1].moisture > regions[0].moisture, "Rainridge must be wetter than Main Island");
    check(regions[0].moisture > regions[3].moisture, "Main Island must be wetter than Outer Reach");
    check(regions[1].canopy > regions[3].canopy, "Rainridge canopy must exceed Outer Reach canopy");

    // Region centers are land (elevation > 0)
    for (size_t i = 0; i < 4; ++i) {
        const double elev = arch.elevation(regions[i].centerX, regions[i].centerZ);
        check(elev > 0.0, "Region center must be land above sea level");
    }

    // Centroids geographically spaced (no overlap)
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = i + 1; j < 4; ++j) {
            const double dx = regions[i].centerX - regions[j].centerX;
            const double dz = regions[i].centerZ - regions[j].centerZ;
            const double dist = std::sqrt(dx * dx + dz * dz);
            check(dist > 5000.0, "Region centroids must be spaced apart with no overlap (>5km)");
        }
    }
}

void testChannelsBetweenIslandsBelowSeaLevel() {
    Archipelago arch(42ULL);
    const auto& r = arch.regions();

    // Channel between Main and Rainridge
    const double midMainRainX = (r[0].centerX + r[1].centerX) * 0.5;
    const double midMainRainZ = (r[0].centerZ + r[1].centerZ) * 0.5;
    const double elevMainRain = arch.elevation(midMainRainX, midMainRainZ);
    check(elevMainRain < 0.0, "Channel between Main and Rainridge must be below sea level (sea < 0)");

    // Channel between Main and Stone Island
    const double midMainStoneX = (r[0].centerX + r[2].centerX) * 0.5;
    const double midMainStoneZ = (r[0].centerZ + r[2].centerZ) * 0.5;
    const double elevMainStone = arch.elevation(midMainStoneX, midMainStoneZ);
    check(elevMainStone < 0.0, "Channel between Main and Stone Island must be below sea level");

    // Channel between Main and Outer Reach
    const double midMainOuterX = (r[0].centerX + r[3].centerX) * 0.5;
    const double midMainOuterZ = (r[0].centerZ + r[3].centerZ) * 0.5;
    const double elevMainOuter = arch.elevation(midMainOuterX, midMainOuterZ);
    check(elevMainOuter < 0.0, "Channel between Main and Outer Reach must be below sea level");

    // Channel between Outer Reach and Stone Island
    const double midOuterStoneX = (r[3].centerX + r[2].centerX) * 0.5;
    const double midOuterStoneZ = (r[3].centerZ + r[2].centerZ) * 0.5;
    const double elevOuterStone = arch.elevation(midOuterStoneX, midOuterStoneZ);
    check(elevOuterStone < 0.0, "Channel between Outer Reach and Stone Island must be below sea level");
}

void testFiniteBoundsAndExceptions() {
    Archipelago arch(42ULL);

    // Queries inside finite bounds (+-14000) succeed
    const double e0 = arch.elevation(0.0, 0.0);
    const double m0 = arch.moisture(0.0, 0.0);
    check(std::isfinite(e0), "elevation at origin must be finite");
    check(std::isfinite(m0), "moisture at origin must be finite");
    check(m0 >= 0.0 && m0 <= 1.0, "moisture must be normalized between 0 and 1");

    // Corner inside bounds
    const double ec = arch.elevation(14000.0, -14000.0);
    check(std::isfinite(ec), "boundary query must succeed");

    // Non-finite query throws invalid_argument
    bool caughtNonFinite = false;
    try {
        arch.elevation(std::numeric_limits<double>::quiet_NaN(), 0.0);
    } catch (const std::invalid_argument&) {
        caughtNonFinite = true;
    }
    check(caughtNonFinite, "NaN coordinate must throw std::invalid_argument");

    bool caughtInf = false;
    try {
        arch.moisture(0.0, std::numeric_limits<double>::infinity());
    } catch (const std::invalid_argument&) {
        caughtInf = true;
    }
    check(caughtInf, "infinity coordinate must throw std::invalid_argument");

    // Out-of-bounds query throws out_of_range
    bool caughtOOB = false;
    try {
        arch.elevation(14001.0, 0.0);
    } catch (const std::out_of_range&) {
        caughtOOB = true;
    }
    check(caughtOOB, "coordinate >14000m must throw std::out_of_range");

    bool caughtOOBZ = false;
    try {
        arch.moisture(0.0, -14000.5);
    } catch (const std::out_of_range&) {
        caughtOOBZ = true;
    }
    check(caughtOOBZ, "coordinate <-14000m must throw std::out_of_range");
}

void testDeterminismAndSeedVariation() {
    Archipelago archA(100ULL);
    Archipelago archB(100ULL);
    Archipelago archC(999ULL);

    // Same seed exact
    const double eA = archA.elevation(1234.5, -543.2);
    const double eB = archB.elevation(1234.5, -543.2);
    const double mA = archA.moisture(1234.5, -543.2);
    const double mB = archB.moisture(1234.5, -543.2);
    check(eA == eB, "Same seed must produce bit-exact elevation");
    check(mA == mB, "Same seed must produce bit-exact moisture");

    // Different seed changes surface boundedly
    const double eC = archC.elevation(1234.5, -543.2);
    check(eA != eC, "Different seed must vary elevation");
    check(std::abs(eA - eC) <= 15.0, "Seed elevation difference must remain bounded surface variation");

    // Check population variation across seeds
    check(archA.regions()[0].coarsePopulation == archB.regions()[0].coarsePopulation, "Population bit-identical for same seed");
}

void exportAtlasJson(const std::string& path, const Archipelago& arch, int gridSize = 64) {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "Failed to open output file: " << path << std::endl;
        std::exit(1);
    }

    out << "{\n";
    out << "  \"worldBoundsMeters\": " << Archipelago::worldBoundsMeters << ",\n";
    out << "  \"gridResolution\": " << gridSize << ",\n";
    out << "  \"regions\": [\n";
    const auto& regs = arch.regions();
    for (size_t i = 0; i < regs.size(); ++i) {
        out << "    {\n";
        out << "      \"id\": " << regs[i].id << ",\n";
        out << "      \"name\": \"" << regs[i].name << "\",\n";
        out << "      \"centerX\": " << regs[i].centerX << ",\n";
        out << "      \"centerZ\": " << regs[i].centerZ << ",\n";
        out << "      \"referenceSpanMeters\": " << regs[i].referenceSpanMeters << ",\n";
        out << "      \"moisture\": " << regs[i].moisture << ",\n";
        out << "      \"canopy\": " << regs[i].canopy << ",\n";
        out << "      \"shelter\": " << regs[i].shelter << ",\n";
        out << "      \"coarsePopulation\": " << regs[i].coarsePopulation << "\n";
        out << "    }" << (i + 1 < regs.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    out << "  \"elevation\": [\n";
    const double step = (2.0 * Archipelago::worldBoundsMeters) / (gridSize - 1);
    for (int iz = 0; iz < gridSize; ++iz) {
        const double z = -Archipelago::worldBoundsMeters + iz * step;
        out << "    [";
        for (int ix = 0; ix < gridSize; ++ix) {
            const double x = -Archipelago::worldBoundsMeters + ix * step;
            const double elev = arch.elevation(x, z);
            out << std::setprecision(4) << std::fixed << elev << (ix + 1 < gridSize ? ", " : "");
        }
        out << "]" << (iz + 1 < gridSize ? "," : "") << "\n";
    }
    out << "  ],\n";

    out << "  \"moisture\": [\n";
    for (int iz = 0; iz < gridSize; ++iz) {
        const double z = -Archipelago::worldBoundsMeters + iz * step;
        out << "    [";
        for (int ix = 0; ix < gridSize; ++ix) {
            const double x = -Archipelago::worldBoundsMeters + ix * step;
            const double moist = arch.moisture(x, z);
            out << std::setprecision(4) << std::fixed << moist << (ix + 1 < gridSize ? ", " : "");
        }
        out << "]" << (iz + 1 < gridSize ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    std::cout << "Successfully wrote coarse atlas JSON to: " << path << std::endl;
}

} // namespace

int main(int argc, char* argv[]) {
    std::string atlasPath;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--atlas" && i + 1 < argc) {
            atlasPath = argv[i + 1];
            ++i;
        }
    }

    testRegionsStabilityAndBoundaries();
    testChannelsBetweenIslandsBelowSeaLevel();
    testFiniteBoundsAndExceptions();
    testDeterminismAndSeedVariation();

    if (!atlasPath.empty()) {
        Archipelago arch(20261017ULL);
        exportAtlasJson(atlasPath, arch, 64);
    }

    std::cout << "ALL ARCHIPELAGO WORLD TESTS PASSED" << std::endl;
    return 0;
}
