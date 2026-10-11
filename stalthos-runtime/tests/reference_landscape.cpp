#include "../domain/reference_landscape.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using stalthos::domain::Habitat;
using stalthos::domain::PlacementKind;
using stalthos::domain::ReferenceLandscape;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    try {
        const ReferenceLandscape a(7123), same(7123), other(7124);
        double lo = 1e9, hi = -1e9;
        for (int z = -96; z <= 96; z += 2) for (int x = -96; x <= 96; x += 2) {
            const double y = a.height(x, z);
            require(std::isfinite(y) && y > -5.0 && y < 35.0, "height finite and bounded");
            lo = y < lo ? y : lo; hi = y > hi ? y : hi;
            const double c = 8.0 * std::sin(z / 43.0) + 2.0 * std::sin(z / 17.0);
            const bool wet = std::abs(x - c) < 9.0 && y < a.waterSurface(x, z);
            require(a.waterContains(x, z) == wet, "water extent follows terrain-water intersection");
        }
        require(hi - lo > 8.0, "terrain includes relief");
        for (int z = -90; z <= 90; z += 5) {
            const double c = 8.0 * std::sin(z / 43.0) + 2.0 * std::sin(z / 17.0);
            require(a.waterContains(c, z), "creek center contains water");
            for (int step = -48; step <= 48; ++step) {
                const double x = c + static_cast<double>(step) * 0.25;
                const bool bedBelowSurface = a.height(x, z) < a.waterSurface(x, z);
                if (std::abs(static_cast<double>(step) * 0.25) < 9.0) {
                    require(a.waterContains(x, z) == bedBelowSurface, "wet extent matches bed across channel banks");
                } else {
                    require(!a.waterContains(x, z), "water remains bounded to channel corridor");
                }
                if (a.waterContains(x, z)) require(bedBelowSurface, "wetted bed remains below water");
            }
            require(a.height(c + 9.5, z) > a.waterSurface(c, z), "outer creek bank rises above water");
        }
        require(a.placements().size() > 20, "placement list populated");
        require(a.placements().size() == same.placements().size(), "same seed count");
        bool changed = false;
        for (std::size_t i = 0; i < a.placements().size(); ++i) {
            const auto& p = a.placements()[i]; const auto& q = same.placements()[i];
            require(p.kind == q.kind && p.x == q.x && p.y == q.y && p.z == q.z && p.scale == q.scale && p.headingRadians == q.headingRadians, "same seed placements repeat");
            require(p.y == a.height(p.x, p.z), "placement rests on terrain");
            require(std::isfinite(p.scale) && p.scale > 0.0 && std::isfinite(p.headingRadians), "placement values finite");
            if (p.kind == PlacementKind::Tree) require(a.habitat(p.x, p.z) == Habitat::Woodland && !a.waterContains(p.x, p.z), "trees only on dry woodland");
            if (p.kind == PlacementKind::Fern || p.kind == PlacementKind::Shrub) require(!a.waterContains(p.x, p.z), "vegetation excludes all wetted terrain");
        }
        if (a.placements().size() == other.placements().size()) {
            for (std::size_t i = 0; i < a.placements().size(); ++i) if (a.placements()[i].x != other.placements()[i].x || a.placements()[i].z != other.placements()[i].z) changed = true;
        } else changed = true;
        require(changed, "seed changes fixture placements");
        bool rejected = false;
        try { (void)a.height(std::numeric_limits<double>::quiet_NaN(), 0.0); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "non-finite query rejected");
        rejected = false;
        try { (void)a.habitat(96.1, 0.0); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "out-of-bounds query rejected");
        std::cout << "reference landscape: grid, creek banks, deterministic placements and query bounds passed; range=" << lo << ".." << hi << " m; placements=" << a.placements().size() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "reference landscape failed: " << e.what() << '\n';
        return 1;
    }
}
