#include "domain/host_save.hpp"
#include "engine/native_population_registry.hpp"
#include <iostream>
using namespace stalthos;
using namespace stalthos::fauna;
namespace hs=stalthos::persistence::host_save;
static void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> static void rejected(F action,const char* message) {bool caught=false;try {action();} catch(const std::exception&) {caught=true;} require(caught,message);}
static std::string disk(const std::filesystem::path& path) {std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
static void rehash(std::string& bytes) {const auto h=hs::checksum(std::string_view(bytes).substr(0,bytes.size()-8));for(unsigned i=0;i<8;++i) bytes[bytes.size()-8+i]=static_cast<char>(h>>(8*i));}
int main() {try {
    std::vector<HabitatSite> sites{{{0,0,0},false,true,{1}},{{2,0,0},true,false,{0}}};
    Population pop(42,1001,sites,8);
    ResourceLedgerDto ledger{{{100,ResourceKind::Creek,100,3},{200,ResourceKind::Shrub,10,1}},{{90,100,3},{91,200,1}}};
    const auto actor=pop.snapshot().actors.front().id;
    std::uint64_t next=100;
    engine::NativePopulationRegistry registry(pop,ledger,{{actor,90}},{{0,100,sites[0].position},{1,200,sites[1].position}},next);
    RegionalPopulationRegistry admitted(pop,ledger,{4096,256},registry.sessions());
    require(admitted.configureContacts(1001,{{0,100,sites[0].position},{1,200,sites[1].position}}),"external configuration");
    require(admitted.setActive(1001,true,RegionalAdmissionOrigin::PhysicalArrival),"external activation");
    SimCore clock;StalthosState world(42);world.setHero(1,1);
    auto save=persistence::captureSave(clock,world);
    save.faunaPopulation=registry.snapshot();save.resourceLedger=registry.ledger();save.faunaResourceSessions=registry.sessions();
    hs::Candidate candidate{save,admitted.continuationSnapshot(),2,1};
    const auto bytes=hs::encode(candidate);const auto decoded=hs::decode(bytes,2);
    require(hs::encode(decoded)==bytes,"bit exact STL9 roundtrip");
    require(admitted.restoreContinuation(*decoded.regional),"detached registry restore");
    const auto restored=hs::restore(decoded,admitted,2);
    require(restored.host.worldSeed()==42,"whole detached owner restored");
    auto changed=decoded;
    auto& changedActor=changed.regional->regions[0].population.actors[0];
    changedActor.species=changedActor.species==HeroSpecies::Smalltrotter?HeroSpecies::Brackenback:HeroSpecies::Smalltrotter;
    changed.host.faunaPopulation=changed.regional->regions[0].population;
    rejected([&]{(void)hs::restore(changed,admitted,2);},"coherent payload species tampering rejected by external world");
    changed=decoded;changed.regional->regions[0].ledger.resources[0].initialQuantity+=1;
    changed.host.resourceLedger=changed.regional->regions[0].ledger;
    rejected([&]{(void)hs::restore(changed,admitted,2);},"coherent resource tampering rejected by external world");
    rejected([&]{hs::decode(bytes,3);},"configuration rejected");
    auto bad=bytes;bad[4]=10;rehash(bad);rejected([&]{hs::decode(bad,2);},"unknown container version rejected");
    bad=bytes;bad[75]='7';rehash(bad);rejected([&]{hs::decode(bad,2);},"non-STL8 embedded host rejected");
    bad=bytes;bad[24]^=1;rehash(bad);rejected([&]{hs::decode(bad,2);},"seed binding rejected");
    bad=bytes;bad[32]^=1;rehash(bad);rejected([&]{hs::decode(bad,2);},"clock binding rejected");
    bad=bytes;bad[48]^=1;rehash(bad);rejected([&]{hs::decode(bad,2);},"hero region binding rejected");
    bad=bytes;bad[56]=static_cast<char>(255);rehash(bad);rejected([&]{hs::decode(bad,2);},"length rejected");
    bad=bytes;bad.back()^=1;rejected([&]{hs::decode(bad,2);},"checksum rejected");
    rejected([&]{hs::decode(bytes.substr(0,bytes.size()-1),2);},"truncation rejected");
    rejected([&]{hs::decode(bytes+"x",2);},"trailing bytes rejected");
    auto mismatched=candidate;mismatched.host.faunaPopulation->actors.front().thirst=.7;
    rejected([&]{hs::encode(mismatched);},"host registry mismatch rejected");
    mismatched=candidate;mismatched.generation=0;rejected([&]{hs::encode(mismatched);},"zero generation rejected");
    auto immutable=*decoded.regional;immutable.regions[0].anchors[0].position.x+=1;
    const auto before=admitted.continuationSnapshot();
    auto invalid=decoded;invalid.regional=immutable;
    rejected([&]{(void)hs::restore(invalid,admitted,2);},"external anchors rejected");
    std::string prior,after;regional_archive::encode(before,prior);regional_archive::encode(admitted.continuationSnapshot(),after);require(prior==after,"failed detached restore unchanged");
    const auto directory=std::filesystem::temp_directory_path()/("stalthos-host-save-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);const auto path=directory/"world.stl";
    hs::write(path,candidate,admitted,2);require(disk(path)==bytes,"new destination transaction");
    candidate.generation=2;hs::write(path,candidate,admitted,2);require(hs::read(path,2).generation==2,"existing Windows destination replaced");
    const auto original=disk(path);
    for(auto failure:{hs::Failure::Write,hs::Failure::Close,hs::Failure::Readback,hs::Failure::Replace}) {
        candidate.generation++;rejected([&]{hs::write(path,candidate,admitted,2,failure);},"injected failure rejected");
        require(disk(path)==original && hs::read(path,2).generation==2,"prior save retained and readable");
    }
#ifdef _WIN32
    const auto handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    require(handle!=INVALID_HANDLE_VALUE,"lock destination");
    rejected([&]{hs::write(path,candidate,admitted,2);},"real Windows replacement lock rejected");CloseHandle(handle);
    require(disk(path)==original,"locked destination unchanged");
#endif
    hs::exportLegacy(directory/"legacy.stl",candidate);
    const auto imported=hs::read(directory/"legacy.stl",999);
    require(!imported.regional && hs::legacyBytes(imported.host)==hs::legacyBytes(candidate.host),"legacy exact import");
    // Valid two-region DTO; resource IDs and actor IDs are globally unique.
    Population exterior(42,1002,sites,1);
    auto multi=candidate;multi.regional->regions[0].reservation={4094,254};
    auto region=multi.regional->regions[0];region.population=exterior.snapshot();region.reservation={2,2};
    region.ledger={{{300,ResourceKind::Creek,5,0}}, {}};region.bindings.clear();region.anchors={{0,300,sites[0].position}};
    multi.regional->regions.push_back(region);
    (void)hs::encode(multi);
    rejected([&]{hs::exportLegacy(directory/"lossy.stl",multi);},"lossy multiregion STL8 refused");
    require(!std::filesystem::exists(directory/"lossy.stl"),"lossy export creates no output");
    for(const auto& entry:std::filesystem::directory_iterator(directory)) require(entry.path().filename().string().find("candidate-")==std::string::npos,"failed candidate cleaned");
    std::filesystem::remove_all(directory);
    std::cout<<"HOST_SAVE_OK: STL9 binding, detached restore, Windows overwrite, four injected failures, locked destination, legacy import, multiregion refusal\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
