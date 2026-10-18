#include "../domain/watershed_terrain.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace stalthos;
using namespace stalthos::world;
namespace {
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template<class Exception, class F> void rejects(F operation) {
    try { operation(); } catch (const Exception&) { return; }
    check(false,"expected rejection");
}
void same(const TerrainVertex& a, const TerrainVertex& b) {
    check(a.position.x==b.position.x && a.position.y==b.position.y && a.position.z==b.position.z,
          "identical world positions");
    check(a.normal.x==b.normal.x && a.normal.y==b.normal.y && a.normal.z==b.normal.z,
          "identical normals");
    check(a.moisture==b.moisture && a.sea==b.sea,"identical ancillary field");
}
void interiorAndBoundary() {
    for (const std::uint64_t seed : {1ULL,42ULL,1337ULL}) {
        WatershedTerrain field(seed);
        domain::ReferenceLandscape original(seed);
        TerrainTiles coarse(seed,{16,8,1});
        for (double z=-96;z<=96;z+=3)
            for (double x=-96;x<=96;x+=3)
                check(field.height(x,z)==original.height(x,z),"exact original interior and border");
        for (const double x : {-96.0,-95.999,0.0001,95.999,96.0})
            for (const double z : {-96.0,-95.999,0.0001,95.999,96.0})
                check(field.height(x,z)==original.height(x,z),"fractional interior identity");
        for (double p=-300;p<=300;p+=7.5) {
            same(field.sample(-256,p),coarse.sample(-256,p));
            same(field.sample(256,p),coarse.sample(256,p));
            same(field.sample(p,-256),coarse.sample(p,-256));
            same(field.sample(p,256),coarse.sample(p,256));
        }
        for (double p=97;p<=256;p+=0.25) {
            const auto v=field.sample(p,24.3);
            check(std::isfinite(v.position.y) && std::isfinite(v.normal.y),"finite transition");
            check(v.normal.y>0,"upward normal");
            const double dx=(field.height(p+1,24.3)-field.height(p-1,24.3))/2;
            check(std::abs(v.normal.x/v.normal.y+dx)<1e-12,"normal from overlay height");
        }
    }
}
void tilesAndOrder() {
    WatershedTerrain native(42,{64,2.0,1.0});
    domain::ReferenceLandscape nativeReference(42);
    for (const auto key : {TerrainTileKey{-1,-1},TerrainTileKey{-1,0},
                           TerrainTileKey{0,-1},TerrainTileKey{0,0}}) {
        const auto tile=native.build(key);
        check(tile.vertices.size()==4225,"native 2m grid vertex count");
        for (const auto& vertex : tile.vertices)
            if (nativeReference.inBounds(vertex.position.x,vertex.position.z))
                check(vertex.position.y==nativeReference.height(vertex.position.x,vertex.position.z),
                      "native 2m mesh vertices preserve watershed datum");
    }
    for (double spacing : {8.0,0.3}) {
        WatershedTerrain field(42,{16,spacing,1}), again(42,{16,spacing,1});
        for (const std::int64_t z : {-54LL,-21LL,-3LL,-1LL,0LL,2LL,20LL,53LL})
            for (const std::int64_t x : {-54LL,-21LL,-3LL,-1LL,0LL,2LL,20LL,53LL}) {
            const auto east=field.build({x+1,z}), north=field.build({x,z+1});
            const auto tile=field.build({x,z}), repeat=again.build({x,z});
            for (std::size_t i=0;i<17;++i) {
                same(tile.vertices[i*17+16],east.vertices[i*17]);
                same(tile.vertices[16*17+i],north.vertices[i]);
            }
            for (std::size_t i=0;i<tile.vertices.size();++i) same(tile.vertices[i],repeat.vertices[i]);
        }
    }
    WatershedTerrain field(42);
    check(field.spanMeters()==128 && field.config().cells==16,"native default grid");
    check(field.bodyTile({-0.1,0,-128.1}).z==-2,"negative body floor");
    const auto keys=field.neighborhood({-1,0,-1});
    check(keys.size()==9 && keys.front().x==-2 && keys.back().z==0,"deterministic neighborhood");
    WatershedTerrain custom(42,{7,0.3,0.3},333.25);
    TerrainTiles customCoarse(42,{7,0.3,0.3});
    for (double p=-350;p<=350;p+=17.25) {
        same(custom.sample(-333.25,p),customCoarse.sample(-333.25,p));
        same(custom.sample(333.25,p),customCoarse.sample(333.25,p));
        same(custom.sample(p,-333.25),customCoarse.sample(p,-333.25));
        same(custom.sample(p,333.25),customCoarse.sample(p,333.25));
    }
}
void guards() {
    WatershedTerrain field(1);
    const double nan=std::numeric_limits<double>::quiet_NaN(), inf=std::numeric_limits<double>::infinity();
    rejects<std::invalid_argument>([&]{field.height(nan,0);});
    rejects<std::invalid_argument>([&]{field.sample(0,inf);});
    rejects<std::out_of_range>([&]{field.height(14000.1,0);});
    rejects<std::invalid_argument>([&]{field.neighborhood({0,nan,0});});
    rejects<std::invalid_argument>([&]{field.neighborhood({0,0,0},5);});
    rejects<std::out_of_range>([&]{field.bodyTile({1e300,0,0});});
    rejects<std::out_of_range>([&]{field.build({std::numeric_limits<std::int64_t>::max(),0});});
    for (const double extent : {nan,inf,96.0,97.0,14001.0})
        rejects<std::invalid_argument>([&]{WatershedTerrain invalid(1,{16,8,1},extent);});
    for (const auto config : {TerrainTileConfig{0,8,1},TerrainTileConfig{16,nan,1},
                             TerrainTileConfig{16,8,0},TerrainTileConfig{256,256,1}})
        rejects<std::invalid_argument>([&]{WatershedTerrain invalid(1,config);});
    for (const double x : {-14000.0,14000.0})
        check(std::isfinite(field.sample(x,x).normal.y),"finite world perimeter");
}
}
int main() {
    interiorAndBoundary(); tilesAndOrder(); guards();
    std::cout << "{\"status\":\"success\",\"suite\":\"watershed_terrain\",\"groups\":3}\n";
}
