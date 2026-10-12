#include "../domain/shore_mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

using namespace stalthos::domain;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

static double crossY(const ShoreVertex& a, const ShoreVertex& b, const ShoreVertex& c) {
    const double abx = b.x - a.x, abz = b.z - a.z;
    const double acx = c.x - a.x, acz = c.z - a.z;
    return abz * acx - abx * acz;
}

static double areaSquared(const ShoreVertex& a, const ShoreVertex& b, const ShoreVertex& c) {
    const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    const double nx = uy * vz - uz * vy;
    const double ny = uz * vx - ux * vz;
    const double nz = ux * vy - uy * vx;
    return nx * nx + ny * ny + nz * nz;
}

static bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

static void testMeshAndTopology() {
    const ReferenceLandscape landscape(42);
    const ShoreMesh mesh = generateShoreMesh(landscape, 4.0);
    const ShoreMesh repeat = generateShoreMesh(landscape, 4.0);
    require(!mesh.vertices.empty() && !mesh.indices.empty(), "mesh is populated");
    require(mesh.indices.size() % 3 == 0, "indices form triangles");
    require(mesh.vertices.size() <= kShoreMeshMaxVertices, "vertex budget respected");
    require(mesh.indices.size() <= kShoreMeshMaxIndices, "index budget respected");
    require(mesh.vertices.size() == repeat.vertices.size() && mesh.indices == repeat.indices, "same seed and resolution reproduce topology");

    bool hasSurface = false, hasBed = false;
    std::set<std::pair<double, double>> surfaceXZ, bedXZ;
    for (const ShoreVertex& v : mesh.vertices) {
        require(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z), "vertices are finite");
        require(landscape.inBounds(v.x, v.z), "vertices stay in patch bounds");
        if (v.material == ShoreMaterial::Surface) {
            hasSurface = true;
            surfaceXZ.emplace(v.x, v.z);
        } else if (v.material == ShoreMaterial::WetBed) {
            hasBed = true;
            bedXZ.emplace(v.x, v.z);
        }
    }
    require(hasSurface && hasBed, "surface and bed are both present");
    require(surfaceXZ.size() == bedXZ.size(), "surface and bed use matching seam coordinates");
    require(std::equal(surfaceXZ.begin(), surfaceXZ.end(), bedXZ.begin(), [](const auto& a, const auto& b) {
        return near(a.first, b.first) && near(a.second, b.second);
    }), "surface and bed seam coordinates coincide");
    require(surfaceXZ.size() * 2 == mesh.vertices.size(), "shared-edge cache avoids duplicate same-material vertices");

    std::size_t surfaceTriangles = 0, bedTriangles = 0;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const std::uint32_t ia = mesh.indices[i], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
        require(ia < mesh.vertices.size() && ib < mesh.vertices.size() && ic < mesh.vertices.size(), "indices are valid");
        const ShoreVertex& a = mesh.vertices[ia];
        const ShoreVertex& b = mesh.vertices[ib];
        const ShoreVertex& c = mesh.vertices[ic];
        require(areaSquared(a, b, c) > 1e-20, "triangle has nonzero 3D area");
        require(a.material == b.material && b.material == c.material, "triangle has one material");
        if (a.material == ShoreMaterial::Surface) {
            ++surfaceTriangles;
            require(crossY(a, b, c) > 0.0, "water surface winding faces upward");
        } else if (a.material == ShoreMaterial::WetBed) {
            ++bedTriangles;
            require(crossY(a, b, c) > 0.0, "visible bed winding faces upward");
        }
    }
    require(surfaceTriangles > 0 && bedTriangles > 0, "both mesh layers contain triangles");

}

struct CheckerboardLandscape {
    static constexpr double halfExtent = 96.0;
    static constexpr double step = 9.6;
    bool waterContains(double x, double z) const noexcept {
        return x >= 0.0 && x <= step && z >= 0.0 && z <= step &&
               (x - step * 0.5) * (z - step * 0.5) > 0.0;
    }
    double waterSurface(double, double) const noexcept { return 1.0; }
    double height(double, double) const noexcept { return 0.0; }
};

static void testDiagonalWetCorners() {
    const CheckerboardLandscape landscape;
    const ShoreMesh mesh = generateShoreMesh(landscape, 10.0); // 20x20 grid, target cell is [0, 9.6]^2
    const bool w00 = landscape.waterContains(0.0, 0.0);
    const bool w10 = landscape.waterContains(CheckerboardLandscape::step, 0.0);
    const bool w01 = landscape.waterContains(0.0, CheckerboardLandscape::step);
    const bool w11 = landscape.waterContains(CheckerboardLandscape::step, CheckerboardLandscape::step);
    require(w00 && !w10 && !w01 && w11, "synthetic fixture has diagonal wet corners");

    std::size_t localTriangles = 0;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const ShoreVertex& a = mesh.vertices[mesh.indices[i]];
        const ShoreVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const ShoreVertex& c = mesh.vertices[mesh.indices[i + 2]];
        if (a.material != ShoreMaterial::Surface) continue;
        const auto at = [](const ShoreVertex& v, double x, double z) { return near(v.x, x) && near(v.z, z); };
        const bool wet00 = at(a, 0.0, 0.0) || at(b, 0.0, 0.0) || at(c, 0.0, 0.0);
        const bool wet11 = at(a, CheckerboardLandscape::step, CheckerboardLandscape::step) ||
                           at(b, CheckerboardLandscape::step, CheckerboardLandscape::step) ||
                           at(c, CheckerboardLandscape::step, CheckerboardLandscape::step);
        if (wet00 || wet11) {
            ++localTriangles;
            require(!(wet00 && wet11), "diagonal wet components are not joined by a fan");
        }
    }
    require(localTriangles >= 2, "both diagonal wet components are represented");
}

static void testResolutionBounds() {
    const ReferenceLandscape landscape(42);
    for (double bad : {0.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        bool threw = false;
        try { (void)generateShoreMesh(landscape, bad); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "invalid resolution is rejected");
    }
    bool overBudget = false;
    try { (void)generateShoreMesh(landscape, 0.1); } catch (const std::length_error&) { overBudget = true; }
    require(overBudget, "resolution requiring more than the cell budget is rejected before allocation");

    const ShoreMesh atCellLimit = generateShoreMesh(landscape, 0.75);
    require(atCellLimit.vertices.size() <= kShoreMeshMaxVertices, "maximum admitted grid respects vertex budget");
    require(atCellLimit.indices.size() <= kShoreMeshMaxIndices, "maximum admitted grid respects index budget");
}

int main() {
    try {
        testMeshAndTopology();
        testDiagonalWetCorners();
        testResolutionBounds();
        std::cout << "shore mesh: winding, area, shared seams, diagonal topology, determinism, and finite budgets passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "shore mesh test failed: " << e.what() << '\n';
        return 1;
    }
}
