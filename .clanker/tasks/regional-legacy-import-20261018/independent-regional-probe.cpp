#define main candidate_test_main
#include "../../../stalthos-runtime/tests/regional_archive.cpp"
#undef main
#include <random>
int main() { try {
    auto r=registry(); auto d=r.continuationSnapshot();
    std::string good; require(archive::encode(d,good),"base encode");
    // Deterministic malformed binary probe with exact output atomicity.
    std::mt19937_64 rng(607);
    for(int i=0;i<6000;++i) {
        auto bad=good; const auto n=1+rng()%8;
        for(std::uint64_t j=0;j<n;++j) bad[rng()%bad.size()]=static_cast<char>(rng());
        auto output=d; const auto prior=exact(output);
        if(!archive::decode(bad,output)) require(exact(output)==prior,"mutation decode output atomic");
        auto live=r; const auto before=exact(live.continuationSnapshot());
        if(!archive::restore(live,bad)) require(exact(live.continuationSnapshot())==before,"mutation restore atomic");
    }
    // Preserve every IEEE double bit, including all needs, injury and signed zero.
    auto exactDto=d; auto& a=exactDto.regions[0].population.actors[0];
    a.headingRadians=-0.0; a.injury=0.235; a.fatigue=0.486; a.thirst=0.119; a.distanceTravelledMeters=13.22;
    std::string binary; require(archive::encode(exactDto,binary),"all fields encode");
    RegionalContinuationDto loaded; require(archive::decode(binary,loaded),"all fields decode");
    std::string reencoded; require(archive::encode(loaded,reencoded)&&binary==reencoded,"all bits exact");
    require(std::signbit(loaded.regions[0].population.actors[0].headingRadians),"signed zero retained");
    // Imported floor and mapping survive fresh equivalent registry archive restore.
    auto history=ledger(100); history.resources[0].consumedQuantity=.5; history.sessions={{77,100,.5}};
    auto id=population(7).actors()[0].id;
    RegionalPopulationRegistry imported(population(7),history,{2,4},{{id,77}});
    require(archive::encode(imported.continuationSnapshot(),binary),"unconfigured provenance archive");
    RegionalPopulationRegistry equivalent(population(7),history,{2,4},{{id,77}});
    require(archive::restore(equivalent,binary),"unconfigured import restore");
    auto stolen=imported.continuationSnapshot(); stolen.regions[0].bindings[0].actor=population(7).actors()[1].id;
    require(archive::encode(stolen,binary),"stolen mapping structural encode");
    reject(equivalent,binary);
    std::cout<<"INDEPENDENT_REGIONAL_PROBE_OK mutations=6000 all_bits=true provenance=true\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }

