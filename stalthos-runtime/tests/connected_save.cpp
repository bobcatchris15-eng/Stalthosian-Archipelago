#include "engine/connected_save.hpp"
#include "domain/weather_response.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::fauna;
namespace cs=engine::connected_save;
static void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool denied=false;try{f();}catch(const std::exception&){denied=true;}require(denied,m);}
int main(){try{
    std::vector<HabitatSite> sites{{{0,0,0},false,true,{1}},{{2,0,0},true,false,{0}}};
    ResourceLedgerDto ledger{{{100,ResourceKind::Creek,70,3},{200,ResourceKind::Shrub,9,1},{210,ResourceKind::Shrub,1,1},{211,ResourceKind::Shrub,.8,.8}},{{90,100,3},{91,200,1},{92,210,1},{93,211,.5},{94,211,.3}}};
    Population pop(42,1001,sites,2,{1});auto roster=pop.snapshot();std::uint64_t next=100;
    engine::NativePopulationRegistry registry(pop,ledger,{},{{0,100,sites[0].position},{1,200,sites[1].position}},next);
    SimCore clock;clock.advance(5,3);StalthosState world(42);world.setHero(1,1);
    Lifecycle life(HeroSpecies::Longstride);needs::NeedsCore needs(HeroSpecies::Longstride);needs.restoreState(.6,.4,.3,.1,true);
    auto h=persistence::captureSave(clock,world,persistence::HostPose{{1,0,0},.8,0});h.heroLifecycle=persistence::captureHeroLifecycle(life,1);h.heroNeeds=persistence::captureHeroNeeds(needs,1);h.cameraState=persistence::CameraStateSnapshot{1,1,{8,2,3},.5,.2,7};h.faunaPopulation=registry.snapshot();h.faunaResourceSessions=registry.sessions();h.resourceLedger=registry.ledger();
    cs::Candidate c{{h,registry.continuation(),cs::configuration(false),1},110,105,3,.6};
    const auto bytes=cs::encode(c);const auto decoded=cs::decode(bytes,c.simulation.configuration);require(cs::encode(decoded)==bytes,"exact envelope roundtrip");
    auto live=cs::restore(decoded,registry,c.simulation.configuration);require(live.clock.calendarSeconds()==15 && live.clock.physicalSeconds()==5,"shared clocks");
    require(live.fauna->sessions()==registry.sessions(),"all actor sessions");require(persistence::host_save::legacyBytes(decoded.simulation.host)==persistence::host_save::legacyBytes(h),"all host state and complete finite history");
    cs::validateAdmission(decoded,roster,ledger);
    auto wrong=c;wrong.simulation.host.resourceLedger->resources.pop_back();rejects([&]{cs::validateAdmission(wrong,roster,ledger);},"missing carrion refused");
    auto changed=roster;changed.actors[0].species=HeroSpecies::Reedcrest;rejects([&]{cs::validateAdmission(c,changed,ledger);},"changed independent roster refused");
    wrong=c;wrong.nextSession=100;rejects([&]{cs::encode(wrong);},"binding collision refuses");wrong=c;wrong.physicalSession=wrong.nextSession;rejects([&]{cs::encode(wrong);},"current session invalid");
    rejects([&]{cs::decode(bytes,c.simulation.configuration+1);},"configuration mismatch");auto corrupt=bytes;corrupt[4]=2;rejects([&]{cs::decode(corrupt,c.simulation.configuration);},"future version");corrupt=bytes;corrupt.back()^=1;rejects([&]{cs::decode(corrupt,c.simulation.configuration);},"checksum corruption");
    rejects([&]{cs::decode(persistence::host_save::encode(c.simulation),c.simulation.configuration);},"original STL9 cannot enter connected mode");
    environment::BodyWeatherResponse weather;weather.restoreWetness(decoded.wetness);require(weather.wetness()==.6,"weather integrates from exact restored wetness");rejects([&]{weather.restoreWetness(1.1);},"invalid wetness rejected");
    const auto dir=std::filesystem::temp_directory_path()/("stalthos-connected-probe-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(dir);const auto path=dir/"owned.stlc";
    cs::write(path,c,registry,c.simulation.configuration);require(cs::encode(cs::read(path,c.simulation.configuration))==bytes,"actual atomic disk roundtrip");
    for(auto fail:{persistence::host_save::Failure::Write,persistence::host_save::Failure::Close,persistence::host_save::Failure::Readback,persistence::host_save::Failure::Replace}){rejects([&]{cs::write(path,c,registry,c.simulation.configuration,fail);},"injected failure refused");require(cs::encode(cs::read(path,c.simulation.configuration))==bytes,"failure preserves exact destination");}
    const auto legacy=dir/"original.stl";persistence::host_save::write(legacy,c.simulation,registry.registry(),c.simulation.configuration);rejects([&]{cs::write(legacy,c,registry,c.simulation.configuration);},"never overwrite original mode save");
    require(persistence::host_save::encode(persistence::host_save::read(legacy,c.simulation.configuration))==persistence::host_save::encode(c.simulation),"original destination retained");
    std::filesystem::remove(path);std::filesystem::remove(legacy);std::filesystem::remove(dir);
    std::cout<<"CONNECTED_SAVE_OK: exact partial envelope/disk/weather/session/history; independent roster/resources; config/version/corruption and atomic failure preservation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
