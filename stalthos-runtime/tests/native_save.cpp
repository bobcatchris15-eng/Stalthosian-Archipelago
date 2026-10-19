#include "engine/native_save.hpp"
#include <iostream>
using namespace stalthos;
using namespace stalthos::fauna;
namespace hs=persistence::host_save;
static void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> static void rejects(F action,const char* message) {bool caught=false;try {action();} catch(const std::exception&) {caught=true;}require(caught,message);}
int main() {try {
    std::vector<HabitatSite> sites{{{0,0,0},false,true,{1}},{{2,0,0},true,false,{0}}};
    std::vector<RegionalResourceAnchor> anchors{{0,100,sites[0].position},{1,200,sites[1].position}};
    ResourceLedgerDto ledger{{{100,ResourceKind::Creek,70,3},{200,ResourceKind::Shrub,9,1},{777,ResourceKind::Shrub,4,0}},{{90,100,3},{91,200,1}}};
    engine::validateNativeResources(ledger,anchors);
    Population pop(42,1001,sites,2);auto declared=pop.snapshot();
    declared.actors[0].species=HeroSpecies::Smalltrotter;
    declared.actors[1].species=HeroSpecies::Longstride;
    declared.actors[0].id=123;declared.actors[1].id=124;
    for(auto& actor:declared.actors) {const auto& profile=getHeroProfile(actor.species);actor.calendarAge=profile.hatchlingDuration+profile.juvenileDuration+1;actor.stage=LifeStage::Adult;}
    pop.restore(declared); // Validated save declaration, deliberately not current generated admission.
    std::uint64_t next=100;
    engine::NativePopulationRegistry admitted(pop,ledger,{{123,90}},anchors,next);
    SimCore clock;StalthosState world(42);world.setHero(1,1);world.heroDied();
    auto save=persistence::captureSave(clock,world);
    save.faunaPopulation=admitted.snapshot();save.resourceLedger=admitted.ledger();save.faunaResourceSessions=admitted.sessions();
    hs::Candidate candidate{save,admitted.continuation(),engine::nativeSaveConfiguration(true),1};
    auto detached=engine::restoreNativeSave(hs::decode(hs::encode(candidate),candidate.configuration),admitted,true);
    require(detached.world.heroDeathKnown(1) && !detached.world.snapshot().hero,"terminal death preserved");
    detached.world.setHero(2,1);require(detached.world.snapshot().hero->first==2 && detached.world.heroDeathKnown(1),"explicit successor after full restore survives");
    require(detached.fauna->snapshot().actors[0].id==123 && detached.fauna->snapshot().actors[1].species==HeroSpecies::Longstride,"arbitrary legal saved individuals preserved");
    require(detached.fauna->ledger().resources.size()==3 && detached.fauna->ledger().resources[0].initialQuantity==70,"extra resources and quantities preserved");
    require(detached.fauna->sessions()==admitted.sessions(),"all actor ownership preserved");
    rejects([&]{engine::restoreNativeSave(candidate,admitted,false);},"configuration mismatch rejected");
    auto invalid=candidate;invalid.regional->regions[0].anchors[0].position.x+=1;
    rejects([&]{engine::restoreNativeSave(invalid,admitted,true);},"external anchors reject coherent candidate");
    invalid=candidate;invalid.regional->legacyRegion=2002;
    rejects([&]{engine::restoreNativeSave(invalid,admitted,true);},"alien region rejected");
    auto wrong=ledger;wrong.resources[0].kind=ResourceKind::Shrub;
    rejects([&]{engine::validateNativeResources(wrong,anchors);},"external required kind enforced");
    Population empty(42,1001,sites,0);next=100;
    engine::NativePopulationRegistry emptyAdmission(empty,ledger,{},anchors,next);
    auto emptySave=save;emptySave.faunaPopulation=emptyAdmission.snapshot();emptySave.faunaResourceSessions={};
    hs::Candidate emptyCandidate{emptySave,emptyAdmission.continuation(),engine::nativeSaveConfiguration(true),1};
    require(engine::restoreNativeSave(emptyCandidate,emptyAdmission,true).fauna->actors().empty(),"legal empty legacy remains empty");
    std::cout<<"NATIVE_SAVE_OK: save-declared arbitrary species/IDs, quantities/extra resources, empty, detached terminal/successor, external anchors/kinds/config\n";return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
