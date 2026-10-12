#pragma once

#include "reference_landscape.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace stalthos::domain {

enum class ShoreMaterial : std::uint8_t { Surface = 0, WetBank = 1, WetBed = 2 };

struct ShoreVertex {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    ShoreMaterial material = ShoreMaterial::Surface;
};

struct ShoreMesh {
    std::vector<ShoreVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// Hard output limits keep runtime and memory bounded independently of the
// requested resolution. At most 24 indices and 16 material vertices are
// emitted per cell by clipping two triangles and triangulating each polygon.
inline constexpr std::size_t kShoreMeshMaxCells = 65'536;
inline constexpr std::size_t kShoreMeshMaxVertices = 16 * kShoreMeshMaxCells;
inline constexpr std::size_t kShoreMeshMaxIndices = 24 * kShoreMeshMaxCells;

// Clips each grid cell as two triangles. This avoids the marching-squares
// ambiguous-case fan that can bridge diagonally separated wet corners.
// Crossings are deterministic 28-step bisection approximations of the
// ReferenceLandscape waterContains predicate, not analytic intersections.
template <typename Landscape>
inline ShoreMesh generateShoreMesh(const Landscape& landscape, double resolution = 2.0) {
    if (!std::isfinite(resolution) || resolution < 0.1 || resolution > 10.0) {
        throw std::invalid_argument("resolution must be finite and within [0.1, 10.0]");
    }

    constexpr double extent = ReferenceLandscape::halfExtent;
    constexpr double width = 2.0 * extent;
    const double cellsValue = std::ceil(width / resolution);
    if (!std::isfinite(cellsValue) || cellsValue < 1.0 || cellsValue > static_cast<double>(kShoreMeshMaxCells)) {
        throw std::length_error("shore mesh resolution exceeds the cell budget");
    }
    const auto cells = static_cast<std::size_t>(cellsValue);
    if (cells > kShoreMeshMaxCells / cells) {
        throw std::length_error("shore mesh resolution exceeds the cell budget");
    }
    const std::size_t cellCount = cells * cells;
    const std::size_t side = cells + 1;
    const double step = width / static_cast<double>(cells);

    struct Sample { double x, z, waterY, bedY; bool wet; };
    std::vector<Sample> samples;
    samples.reserve(side * side);
    for (std::size_t z = 0; z < side; ++z) {
        for (std::size_t x = 0; x < side; ++x) {
            const double px = -extent + static_cast<double>(x) * step;
            const double pz = -extent + static_cast<double>(z) * step;
            samples.push_back({px, pz, landscape.waterSurface(px, pz), landscape.height(px, pz), landscape.waterContains(px, pz)});
        }
    }

    ShoreMesh mesh;
    const std::size_t worstVertices = std::min(kShoreMeshMaxVertices, 16 * cellCount);
    const std::size_t worstIndices = std::min(kShoreMeshMaxIndices, 24 * cellCount);
    mesh.vertices.reserve(worstVertices);
    mesh.indices.reserve(worstIndices);

    // A point is either a sampled lattice vertex (a == b) or a crossing on
    // the canonicalized edge between two lattice vertices (a < b).
    using PointKey = std::pair<std::size_t, std::size_t>;
    using MaterialKey = std::tuple<std::size_t, std::size_t, ShoreMaterial>;
    std::map<PointKey, std::pair<double, double>> crossingCache;
    std::map<MaterialKey, std::uint32_t> vertexCache;

    const auto crossing = [&](std::size_t ia, std::size_t ib) -> std::pair<double, double> {
        PointKey key{std::min(ia, ib), std::max(ia, ib)};
        const auto found = crossingCache.find(key);
        if (found != crossingCache.end()) return found->second;

        const Sample& a = samples[key.first];
        const Sample& b = samples[key.second];
        double lo = 0.0;
        double hi = 1.0;
        const bool wetAtLo = a.wet;
        for (int i = 0; i < 28; ++i) {
            const double mid = (lo + hi) * 0.5;
            const double x = a.x + (b.x - a.x) * mid;
            const double z = a.z + (b.z - a.z) * mid;
            if (landscape.waterContains(x, z) == wetAtLo) lo = mid;
            else hi = mid;
        }
        const double t = (lo + hi) * 0.5;
        const std::pair<double, double> value{a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t};
        return crossingCache.emplace(key, value).first->second;
    };

    const auto pointXZ = [&](const PointKey& key) -> std::pair<double, double> {
        if (key.first == key.second) return {samples[key.first].x, samples[key.first].z};
        return crossing(key.first, key.second);
    };

    const auto addVertex = [&](const PointKey& point, ShoreMaterial material) -> std::uint32_t {
        const MaterialKey key{point.first, point.second, material};
        const auto found = vertexCache.find(key);
        if (found != vertexCache.end()) return found->second;
        if (mesh.vertices.size() >= kShoreMeshMaxVertices || mesh.vertices.size() >= std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("shore mesh vertex budget exceeded");
        }
        const auto [x, z] = pointXZ(point);
        const double y = material == ShoreMaterial::Surface ? landscape.waterSurface(x, z) : landscape.height(x, z);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
            throw std::runtime_error("shore mesh generated a non-finite vertex");
        }
        const auto index = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({x, y, z, material});
        vertexCache.emplace(key, index);
        return index;
    };

    struct ClipPoint { PointKey key; bool wet; };
    const auto emitPolygon = [&](const std::array<std::size_t, 3>& tri) {
        std::vector<ClipPoint> polygon;
        polygon.reserve(4);
        for (std::size_t e = 0; e < 3; ++e) {
            const std::size_t a = tri[e];
            const std::size_t b = tri[(e + 1) % 3];
            const bool wa = samples[a].wet;
            const bool wb = samples[b].wet;
            if (wa) polygon.push_back({{a, a}, true});
            if (wa != wb) polygon.push_back({{std::min(a, b), std::max(a, b)}, true});
        }
        if (polygon.size() < 3) return;

        for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
            const PointKey points[3] = {polygon[0].key, polygon[i].key, polygon[i + 1].key};
            const auto p0 = pointXZ(points[0]);
            const auto p1 = pointXZ(points[1]);
            const auto p2 = pointXZ(points[2]);
            const double sx1 = p1.first - p0.first;
            const double sz1 = p1.second - p0.second;
            const double sx2 = p2.first - p0.first;
            const double sz2 = p2.second - p0.second;
            const double area2XZ = sz1 * sx2 - sx1 * sz2;
            if (std::abs(area2XZ) <= 1e-12) continue;

            if (mesh.indices.size() + 6 > kShoreMeshMaxIndices) {
                throw std::length_error("shore mesh index budget exceeded");
            }
            const auto s0 = addVertex(points[0], ShoreMaterial::Surface);
            const auto s1 = addVertex(points[1], ShoreMaterial::Surface);
            const auto s2 = addVertex(points[2], ShoreMaterial::Surface);
            // With this X/Z order, both the visible water surface and the
            // creek bed face +Y so a top-down renderer can show the substrate.
            mesh.indices.insert(mesh.indices.end(), {s0, s1, s2});
            const auto b0 = addVertex(points[0], ShoreMaterial::WetBed);
            const auto b1 = addVertex(points[1], ShoreMaterial::WetBed);
            const auto b2 = addVertex(points[2], ShoreMaterial::WetBed);
            mesh.indices.insert(mesh.indices.end(), {b0, b1, b2});
        }
    };

    for (std::size_t z = 0; z < cells; ++z) {
        for (std::size_t x = 0; x < cells; ++x) {
            const std::size_t i00 = z * side + x;
            const std::size_t i10 = i00 + 1;
            const std::size_t i01 = i00 + side;
            const std::size_t i11 = i01 + 1;
            const bool w00 = samples[i00].wet;
            const bool w10 = samples[i10].wet;
            const bool w01 = samples[i01].wet;
            const bool w11 = samples[i11].wet;

            // Split ambiguous checkerboards along the dry diagonal. Each wet
            // corner then clips as its own triangle instead of being joined
            // by a fan across the dry center.
            if (w00 == w11 && w10 == w01 && w00 != w10) {
                if (w00) {
                    emitPolygon({i00, i01, i10});
                    emitPolygon({i01, i11, i10});
                } else {
                    emitPolygon({i00, i01, i11});
                    emitPolygon({i00, i11, i10});
                }
            } else {
                emitPolygon({i00, i01, i11});
                emitPolygon({i00, i11, i10});
            }
        }
    }
    return mesh;
}

} // namespace stalthos::domain
