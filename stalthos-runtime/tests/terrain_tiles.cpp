#include "../domain/terrain_tiles.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace stalthos;
using namespace stalthos::world;
namespace {
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template<class Exception, class F> void rejects(F operation, const char* message) {
    try { operation(); } catch (const Exception&) { return; }
    check(false, message);
}
void same(const TerrainVertex& a, const TerrainVertex& b) {
    check(a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z,
          "shared position is exactly equal");
    check(a.normal.x == b.normal.x && a.normal.y == b.normal.y && a.normal.z == b.normal.z,
          "shared source-field normal is exactly equal");
    check(a.moisture == b.moisture && a.sea == b.sea, "shared habitat sample is exactly equal");
}
void edgesAndOrder() {
    // Non-binary spacing exposes tile-origin rounding errors. Both axes, negative
    // and positive indices; alternate visit order; same seed reconstruction.
    for (const double spacing : {8.0, 0.3}) {
        TerrainTiles sampler(42, {16, spacing, 1});
        TerrainTiles again(42, {16, spacing, 1});
        for (std::int64_t z = -3; z <= 2; ++z) {
            for (std::int64_t x = -3; x <= 2; ++x) {
                const auto north = sampler.build({x, z + 1});
                const auto east = sampler.build({x + 1, z});
                const auto tile = sampler.build({x, z});
                const auto repeated = again.build({x, z});
                check(tile.vertices.size() == 289, "bounded grid vertex count");
                for (std::size_t i = 0; i < 17; ++i) {
                    same(tile.vertices[i * 17 + 16], east.vertices[i * 17]);
                    same(tile.vertices[16 * 17 + i], north.vertices[i]);
                }
                for (std::size_t i = 0; i < tile.vertices.size(); ++i) same(tile.vertices[i], repeated.vertices[i]);
            }
        }
    }
}
void fieldAndNormals() {
    TerrainTiles sampler(1337);
    Archipelago source(1337);
    bool sawSea = false, sawLand = false;
    for (const Vec3 point : {Vec3{0,0,0}, Vec3{-6200,0,4500}, Vec3{12000,0,12000},
                             Vec3{-14000,0,14000}, Vec3{321.4,0,-736.2}}) {
        const auto v = sampler.sample(point.x, point.z);
        check(v.position.y == source.elevation(point.x, point.z), "source height preserved");
        check(v.moisture == source.moisture(point.x, point.z), "source moisture at same coordinate");
        check(v.sea == (v.position.y <= 0), "sea derived from source height");
        sawSea = sawSea || v.sea; sawLand = sawLand || !v.sea;
        check(std::abs(v.normal.x*v.normal.x + v.normal.y*v.normal.y + v.normal.z*v.normal.z - 1) < 1e-12,
              "unit finite source normal including world perimeter");
        check(v.normal.y > 0, "normal faces up");
    }
    check(sawSea && sawLand, "land and drowned terrain covered");
    const auto v = sampler.sample(123, -456);
    const double dx = (source.elevation(124,-456) - source.elevation(122,-456))/2;
    const double dz = (source.elevation(123,-455) - source.elevation(123,-457))/2;
    check(std::abs(v.normal.x/v.normal.y + dx) < 1e-14 &&
          std::abs(v.normal.z/v.normal.y + dz) < 1e-14, "normal follows source finite differences");
}
void neighborhoodAndGuards() {
    TerrainTiles sampler(42, {16,8,1});
    check(sampler.bodyTile({-0.001,0,-128.001}).x == -1 &&
          sampler.bodyTile({-0.001,0,-128.001}).z == -2, "negative body uses floor");
    check(sampler.bodyTile({128,0,0}).x == 1, "boundary enters next tile");
    const auto keys = sampler.neighborhood({-1,0,-1});
    check(keys.size() == 9 && keys.front().x == -2 && keys.front().z == -2 &&
          keys.back().x == 0 && keys.back().z == 0, "body neighborhood bounded deterministic order");
    check(sampler.neighborhood({0,0,0},4).size() == 81, "maximum neighborhood 81 keys");
    check(sampler.neighborhood({13900,0,13900},4).size() == 25, "edge neighborhood clipped");
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    rejects<std::invalid_argument>([&]{ sampler.sample(nan,0); }, "sample NaN rejected");
    rejects<std::invalid_argument>([&]{ sampler.neighborhood({0,inf,0}); }, "nonfinite body Y rejected");
    rejects<std::out_of_range>([&]{ sampler.sample(14000.01,0); }, "source bounds rejected");
    rejects<std::out_of_range>([&]{ sampler.neighborhood({14000,0,0}); }, "partial perimeter body rejected");
    rejects<std::out_of_range>([&]{ sampler.neighborhood({1e300,0,0}); }, "large body rejected before cast");
    rejects<std::invalid_argument>([&]{ sampler.neighborhood({0,0,0},5); }, "radius rejected");
    rejects<std::out_of_range>([&]{ sampler.build({std::numeric_limits<std::int64_t>::max(),0}); }, "positive index overflow rejected");
    rejects<std::out_of_range>([&]{ sampler.build({0,std::numeric_limits<std::int64_t>::min()}); }, "negative index overflow rejected");
    for (const auto config : {TerrainTileConfig{0,8,1}, TerrainTileConfig{257,8,1},
            TerrainTileConfig{1,nan,1}, TerrainTileConfig{1,inf,1}, TerrainTileConfig{1,0.1,1},
            TerrainTileConfig{256,256,1}, TerrainTileConfig{16,8,0}, TerrainTileConfig{16,8,nan},
            TerrainTileConfig{16,8,17}})
        rejects<std::invalid_argument>([&]{ TerrainTiles invalid(42,config); }, "invalid config rejected");
    TerrainTiles maximum(42, {256, 1, 1});
    check(maximum.build({0,0}).vertices.size() == 66049, "maximum grid bounded");
    TerrainTiles minimum(42, {1,0.25,0.01});
    check(minimum.build({-56000,-56000}).vertices.size() == 4, "minimum grid at negative world boundary");
}
}
int main() {
    edgesAndOrder(); fieldAndNormals(); neighborhoodAndGuards();
    std::cout << "{\"status\":\"success\",\"suite\":\"terrain_tiles\",\"groups\":3}\n";
}
