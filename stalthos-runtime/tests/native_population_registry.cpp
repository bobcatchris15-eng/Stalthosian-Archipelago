#include "engine/native_population_registry.hpp"
#include "domain/regional_archive.hpp"
#include <iostream>
using namespace stalthos;
using namespace stalthos::fauna;
static void require(bool c,const char* m) {if(!c) throw std::runtime_error(m);}
int main() {
try {
    std::vector<HabitatSite> sites{{{0,2,0},false,true,{1,2}},{{4,2,0},false,true,{0,2}},{{2,2,0},true,false,{0,1}}};
    Population population(42,1001,sites,8);
    auto initial=population.snapshot();
    for(std::size_t i=0;i<initial.actors.size();++i) {auto& a=initial.actors[i];a.site=i%2;a.target=a.site;a.position=sites[a.site].position;a.activity=Activity::Drink;a.thirst=.8;}
    population.restore(initial);
    ResourceLedgerDto ledger{{{100,ResourceKind::Creek,100,3},{200,ResourceKind::Shrub,100,1}},{{90,100,1},{91,100,2},{92,200,1}}};
    std::vector<std::pair<Identity,std::uint64_t>> bindings{{initial.actors[0].id,90},{initial.actors[1].id,93}};
    std::vector<RegionalResourceAnchor> anchors{{0,100,sites[0].position},{1,100,sites[1].position},{2,200,sites[2].position}};
    std::uint64_t next=100;
    engine::NativePopulationRegistry host(population,ledger,bindings,anchors,next);
    require(host.snapshot().habitatSignature==initial.habitatSignature,"signature preserved");
    require(host.actors().size()==8 && host.sessions().size()==8,"eight retained and bound");
    require(host.ledger().sessions.size()==3 && host.ledger().resources[0].consumedQuantity==3,"full imported ledger retained");
    host.advance({1,1},true);
    require(host.events().size()==8,"both banks consume shared creek");
    require(host.consumedWater()>.079 && host.consumedWater()<.081,"accepted deltas");
    require(host.ledger().sessions[0].amount==1.01,"saved cumulative hydrated");
    const auto thirst=host.snapshot().actors[0].thirst;
    require(host.consume(next++,100,.5),"hero consumes same authority");
    require(host.snapshot().actors[0].thirst==thirst,"hero grants no NPC relief");
    bool denied=false;try {host.consume(90,100,.5);}catch(const std::exception&){denied=true;}
    require(denied,"hero cannot alias NPC ownership");
    const auto before=host.ledger();host.advanceDormant({10,20});
    require(host.events().empty() && host.ledger().sessions.size()==before.sessions.size() && host.ledger().resources[0].consumedQuantity==before.resources[0].consumedQuantity,"dormant preserves resources");
    Population equivalent(42,1001,sites,8);equivalent.restore(host.snapshot());
    engine::NativePopulationRegistry restored(std::move(equivalent),host.ledger(),host.sessions(),anchors,next);
    require(restored.snapshot().actors[0].thirst==host.snapshot().actors[0].thirst && restored.sessions()==host.sessions(),"exact state and binding restore");
    require(restored.ledger().sessions[0].amount==host.ledger().sessions[0].amount,"exact cumulative restore");
    RegionalPopulationRegistry registry(population,ledger,{3,256},bindings);
    require(registry.configureContacts(1001,anchors),"multi-bank configuration");
    std::string bytes;RegionalContinuationDto decoded;
    require(regional_archive::encode(registry.continuationSnapshot(),bytes) && regional_archive::decode(bytes,decoded) && registry.restoreContinuation(decoded),"multi-bank archive closure");
    RegionalPopulationRegistry duplicate(population,ledger,{3,256},bindings);
    require(!duplicate.configureContacts(1001,{{0,100,sites[0].position},{0,100,sites[0].position}}),"duplicate site kind rejected");
    std::cout<<"NATIVE_POPULATION_REGISTRY_ADAPTER_OK actors=8 shared_ledger=true multi_bank=true native_rendering=false\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
