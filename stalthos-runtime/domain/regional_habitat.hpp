#pragma once
#include "fauna_population.hpp"
#include <functional>
#include <optional>

namespace stalthos::fauna {
// Candidate-only contract: the host must query the rendered surface and actual
// resource eligibility. freshwater means accessible drinking water from dry land.
struct HabitatGround {
    double height = 0;
    double slope = 0; // rise/run, nonnegative
    bool dry = false;
    bool wet = false;
    bool browse = false;
    bool freshwater = false;
};
struct HabitatRegionBounds { double minX, minZ, maxX, maxZ; };
struct RegionalHabitatOptions {
    double latticeMeters = 16;
    double routeSampleMeters = 2;
    double maxSlope = 0.6;
    std::size_t queryBudget = 20000;
};
enum class RegionalHabitatStatus { Ready, NoViableHabitat, InvalidInput, WorkLimit };
struct RegionalHabitatCandidate {
    RegionalHabitatStatus status = RegionalHabitatStatus::NoViableHabitat;
    std::vector<HabitatSite> sites;
    std::size_t groundQueries = 0;
};
using HabitatGroundQuery = std::function<std::optional<HabitatGround>(double x, double z)>;

// Independent of request order. Global lattice sites belong to half-open bounds;
// callers must supply non-overlapping region bounds. No cross-region edges yet.
inline RegionalHabitatCandidate regionalHabitatCandidate(
    std::uint64_t seed, RegionId region, HabitatRegionBounds b,
    const HabitatGroundQuery& query, RegionalHabitatOptions o = {}) {
    RegionalHabitatCandidate out;
    auto invalid = [&]() { out.status = RegionalHabitatStatus::InvalidInput; return out; };
    if (!query || !std::isfinite(b.minX) || !std::isfinite(b.minZ) ||
        !std::isfinite(b.maxX) || !std::isfinite(b.maxZ) || b.minX >= b.maxX || b.minZ >= b.maxZ ||
        !std::isfinite(o.latticeMeters) || o.latticeMeters < 0.01 || o.latticeMeters > 10000 ||
        !std::isfinite(o.routeSampleMeters) || o.routeSampleMeters < 0.01 ||
        o.routeSampleMeters > o.latticeMeters || !std::isfinite(o.maxSlope) ||
        o.maxSlope < 0 || o.maxSlope > 10 || o.queryBudget == 0 || o.queryBudget > 100000) return invalid();
    const double firstX = std::ceil(b.minX / o.latticeMeters), firstZ = std::ceil(b.minZ / o.latticeMeters);
    const double endX = std::ceil(b.maxX / o.latticeMeters), endZ = std::ceil(b.maxZ / o.latticeMeters);
    if (std::abs(firstX)>1e9 || std::abs(firstZ)>1e9 || std::abs(endX)>1e9 || std::abs(endZ)>1e9) return invalid();
    const auto nx = static_cast<std::int64_t>(endX-firstX), nz = static_cast<std::int64_t>(endZ-firstZ);
    if (nx == 0 || nz == 0) return out;
    if (nx*nz > 512) { out.status=RegionalHabitatStatus::WorkLimit; return out; }
    bool exhausted=false;
    auto ground = [&](double x,double z)->std::optional<HabitatGround> {
        if (out.groundQueries == o.queryBudget) { exhausted=true; return {}; }
        ++out.groundQueries;
        auto g=query(x,z);
        if (!g || !std::isfinite(g->height) || !std::isfinite(g->slope) ||
            g->slope<0 || g->slope>o.maxSlope || !g->dry || g->wet) return {};
        return g;
    };
    std::vector<HabitatSite> all;
    std::vector<std::size_t> indices(static_cast<std::size_t>(nx*nz),512);
    for (std::int64_t z=0;z<nz;++z) for (std::int64_t x=0;x<nx;++x) {
        double px=(firstX+static_cast<double>(x))*o.latticeMeters,pz=(firstZ+static_cast<double>(z))*o.latticeMeters;
        // Guard floating multiplication at half-open boundaries.
        if (px<b.minX || px>=b.maxX || pz<b.minZ || pz>=b.maxZ) continue;
        if (auto g=ground(px,pz)) {
            indices[static_cast<std::size_t>(z*nx+x)]=all.size();
            all.push_back({{px,g->height,pz},g->browse,g->freshwater,{}});
        }
    }
    const auto steps=static_cast<std::size_t>(std::ceil(o.latticeMeters/o.routeSampleMeters));
    auto link = [&](std::size_t a,std::size_t c) {
        if (a==512 || c==512 || exhausted) return;
        const auto p=all[a].position,q=all[c].position;
        double previous=p.y;
        const double interval=o.latticeMeters/static_cast<double>(steps);
        for (std::size_t k=1;k<=steps;++k) {
            const double t=static_cast<double>(k)/static_cast<double>(steps);
            // Requery endpoint too; the supplied query must be pure for deterministic output.
            auto g=ground(p.x+(q.x-p.x)*t,p.z+(q.z-p.z)*t);
            if (!g || std::abs(g->height-previous)>o.maxSlope*interval) return;
            previous=g->height;
        }
        all[a].neighbors.push_back(c); all[c].neighbors.push_back(a);
    };
    for (std::int64_t z=0;z<nz;++z) for (std::int64_t x=0;x<nx;++x) {
        const auto i=static_cast<std::size_t>(z*nx+x);
        if (x+1<nx) link(indices[i],indices[i+1]);
        if (z+1<nz) link(indices[i],indices[i+static_cast<std::size_t>(nx)]);
    }
    if (exhausted) { out.status=RegionalHabitatStatus::WorkLimit; return out; }
    auto mix=[](std::uint64_t v) { v^=v>>30; v*=UINT64_C(0xbf58476d1ce4e5b9); v^=v>>27; v*=UINT64_C(0x94d049bb133111eb); return v^(v>>31); };
    std::vector<bool> seen(all.size());
    std::vector<std::size_t> chosen;
    std::uint64_t best=UINT64_MAX;
    for (std::size_t i=0;i<all.size();++i) if (!seen[i]) {
        std::vector<std::size_t> component{i}; seen[i]=true;
        bool browse=false,water=false;
        for (std::size_t k=0;k<component.size();++k) {
            const auto n=component[k]; browse|=all[n].browse; water|=all[n].freshwater;
            for (auto next:all[n].neighbors) if (!seen[next]) { seen[next]=true; component.push_back(next); }
        }
        const auto rank=mix(seed^mix(region)^mix(i));
        if (browse && water && (chosen.empty() || rank<best)) { chosen=component; best=rank; }
    }
    if (chosen.empty()) return out;
    std::sort(chosen.begin(),chosen.end());
    std::vector<std::size_t> remap(all.size(),512);
    for (std::size_t i=0;i<chosen.size();++i) remap[chosen[i]]=i;
    for (auto n:chosen) {
        auto site=all[n]; for (auto& neighbor:site.neighbors) neighbor=remap[neighbor];
        std::sort(site.neighbors.begin(),site.neighbors.end()); out.sites.push_back(std::move(site));
    }
    out.status=RegionalHabitatStatus::Ready;
    return out;
}
} // namespace stalthos::fauna
