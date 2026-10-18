#include "../domain/regional_habitat.hpp"
#include <iostream>
#include <stdexcept>
#include <set>
using namespace stalthos;
using namespace stalthos::fauna;
static void require(bool ok,const char* why) { if (!ok) throw std::runtime_error(why); }
static bool same(const RegionalHabitatCandidate& a,const RegionalHabitatCandidate& b) {
    if (a.status!=b.status || a.sites.size()!=b.sites.size() || a.groundQueries!=b.groundQueries) return false;
    for (std::size_t i=0;i<a.sites.size();++i) {
        const auto& x=a.sites[i]; const auto& y=b.sites[i];
        if (x.position.x!=y.position.x || x.position.y!=y.position.y || x.position.z!=y.position.z ||
            x.browse!=y.browse || x.freshwater!=y.freshwater || x.neighbors!=y.neighbors) return false;
    }
    return true;
}
int main() { try {
    const HabitatRegionBounds bounds{0,0,48,32};
    HabitatGroundQuery flat=[](double x,double)->std::optional<HabitatGround> { return HabitatGround{3,0,true,false,x==0,x==32}; };
    const auto a=regionalHabitatCandidate(42,1,bounds,flat);
    require(a.status==RegionalHabitatStatus::Ready && a.sites.size()==6,"flat viable connected habitat");
    for (const auto& s:a.sites) require(s.position.y==3 && s.browse==(s.position.x==0) && s.freshwater==(s.position.x==32),"ground height and resource eligibility preserved");
    auto right=regionalHabitatCandidate(42,2,{48,0,96,32},[](double,double)->std::optional<HabitatGround> { return HabitatGround{0,0,true,false,true,true}; });
    require(same(a,regionalHabitatCandidate(42,1,bounds,flat)),"region request order independence and determinism");
    auto left=regionalHabitatCandidate(42,1,bounds,[](double,double)->std::optional<HabitatGround> { return HabitatGround{0,0,true,false,true,true}; });
    std::set<std::pair<double,double>> points;
    for (const auto& s:left.sites) require(points.emplace(s.position.x,s.position.z).second,"unique left sites");
    for (const auto& s:right.sites) require(points.emplace(s.position.x,s.position.z).second,"half-open regions never duplicate boundary sites");
    auto negative=regionalHabitatCandidate(1,4,{-32,-32,0,0},[](double,double)->std::optional<HabitatGround> {return HabitatGround{0,0,true,false,true,true};});
    require(negative.sites.size()==4,"negative lattice coordinates");
    auto noResources=regionalHabitatCandidate(1,1,bounds,[](double,double)->std::optional<HabitatGround> { return HabitatGround{0,0,true,false,false,false}; });
    require(noResources.status==RegionalHabitatStatus::NoViableHabitat && noResources.sites.empty(),"never invent resources");
    for (int mode=0;mode<4;++mode) {
        auto reject=regionalHabitatCandidate(1,1,bounds,[mode](double,double)->std::optional<HabitatGround> {
            if (mode==0) return {};
            if (mode==1) return HabitatGround{0,0,true,true,true,true};
            if (mode==2) return HabitatGround{0,2,true,false,true,true};
            return HabitatGround{std::numeric_limits<double>::quiet_NaN(),0,true,false,true,true};
        });
        require(reject.status==RegionalHabitatStatus::NoViableHabitat && reject.sites.empty(),"reject missing wet steep nonfinite ground");
    }
    for (int mode=0;mode<3;++mode) {
        const auto blocked=regionalHabitatCandidate(1,1,{0,0,32,1},[mode](double x,double)->std::optional<HabitatGround> {
            if (x>6 && x<10) {
                if (mode==0) return {};
                if (mode==1) return HabitatGround{0,0,false,true,false,false};
                return HabitatGround{100,0,true,false,false,false};
            }
            return HabitatGround{0,0,true,false,x==0,x==16};
        });
        require(blocked.status==RegionalHabitatStatus::NoViableHabitat && blocked.sites.empty(),"sampled route blocks missing ground water and hidden cliff");
    }
    auto isolated=regionalHabitatCandidate(9,9,{0,0,48,1},[](double x,double)->std::optional<HabitatGround> {
        if (x>16 && x<32) return {};
        return HabitatGround{0,0,true,false,x==0,x==16};
    });
    require(isolated.status==RegionalHabitatStatus::Ready && isolated.sites.size()==2 && isolated.sites[1].neighbors==std::vector<std::size_t>{0},"connected subset excludes isolated resource-free site and remaps links");
    auto budget=regionalHabitatCandidate(1,1,bounds,flat,{16,2,.6,1});
    require(budget.status==RegionalHabitatStatus::WorkLimit && budget.groundQueries==1 && budget.sites.empty(),"query cap fails closed");
    std::size_t calls=0;
    HabitatGroundQuery counting=[&](double,double)->std::optional<HabitatGround> {++calls;return HabitatGround{0,0,true,false,true,true};};
    auto huge=regionalHabitatCandidate(1,1,{0,0,1e8,1e8},counting);
    require(huge.status==RegionalHabitatStatus::WorkLimit && calls==0,"candidate cap checked before querying");
    for (auto bad: {HabitatRegionBounds{0,0,0,1}, HabitatRegionBounds{1,0,0,1}, HabitatRegionBounds{0,0,std::numeric_limits<double>::infinity(),1}})
        require(regionalHabitatCandidate(1,1,bad,counting).status==RegionalHabitatStatus::InvalidInput,"invalid bounds");
    require(regionalHabitatCandidate(1,1,bounds,counting,{16,0,.6,100}).status==RegionalHabitatStatus::InvalidInput,"invalid interval");
    require(regionalHabitatCandidate(1,1,bounds,{}).status==RegionalHabitatStatus::InvalidInput,"missing query");
    require(calls==0,"invalid requests do not query");
    require(regionalHabitatCandidate(1,1,{1,0,2,1e9},counting).status==RegionalHabitatStatus::NoViableHabitat && calls==0,"empty lattice axis returns without huge loop");
    std::cout<<"REGIONAL_HABITAT_CANDIDATE_OK\n";
    return 0;
} catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
