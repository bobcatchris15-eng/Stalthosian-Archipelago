#include "engine/native_population_registry.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::fauna;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
 std::vector<HabitatSite> sites{{{0,0,0},false,true,{1}},{{4,0,0},true,false,{0}}};Population pop(42,1001,sites,2,{1});const auto old=pop.snapshot();
 check(pop.foodSiteAllowed(HeroSpecies::Longstride,1)&&!pop.connectedDiet(),"default original policy unchanged");
 check(pop.appendCarrionBranch({{2,0,2},{1.5,0,1.5},{1,0,1},{.5,0,.5}},0),"supported caller-sampled leaf append");const auto food=*pop.carrionSite();
 check(pop.snapshot().actors.size()==old.actors.size()&&pop.snapshot().actors[0].id==old.actors[0].id&&pop.snapshot().actors[0].position.x==old.actors[0].position.x,"append preserves original actor state");
 auto state=pop.snapshot();for(std::size_t i=0;i<2;++i){auto& a=state.actors[i];a.id=42+i;a.species=i?HeroSpecies::Smalltrotter:HeroSpecies::Longstride;const auto& p=getHeroProfile(a.species);a.calendarAge=p.hatchlingDuration+p.juvenileDuration+1;a.stage=LifeStage::Adult;a.site=food;a.target=food;a.position={2,0,2};a.activity=Activity::Browse;a.hunger=.8;a.thirst=.1;a.fatigue=.1;}pop.restore(state,{1});
 ResourceLedgerDto history{{{100,ResourceKind::Creek,100,3},{200,ResourceKind::Shrub,10,1},{211,ResourceKind::Shrub,1,0}},{{90,100,3},{91,200,1}}};std::uint64_t next=100;
 const std::vector<RegionalResourceAnchor> anchors{{0,100,sites[0].position},{1,200,sites[1].position},{food,211,{2,0,2}}};
 engine::NativePopulationRegistry native(pop,history,{},anchors,next);auto registry=native.registry();auto bindings=native.sessions();auto session=[&](Identity id){for(auto b:bindings)if(b.first==id)return b.second;throw std::runtime_error("missing binding");};
 const auto before=registry.snapshot(1001)->ledger;const auto denied=registry.contact(1001,43,session(43),food,211,.2,RegionalAdmissionOrigin::PhysicalArrival);check(!denied.event&&registry.snapshot(1001)->ledger.sessions.size()==before.sessions.size(),"herbivore direct carrion refuses without history/relief");
 const auto accepted=registry.contact(1001,42,session(42),food,211,.2,RegionalAdmissionOrigin::PhysicalArrival);check(accepted.event&&accepted.event->amount==.2,"real Longstride food contact");const auto fed=registry.snapshot(1001)->population.actors[0].hunger;
 const auto replay=registry.contact(1001,42,session(42),food,211,.2,RegionalAdmissionOrigin::PhysicalArrival);check(!replay.event&&registry.snapshot(1001)->population.actors[0].hunger==fed,"replay never grants second relief");
 engine::NativePopulationRegistry host(std::move(registry));check(host.consume(next++ ,211,.5),"hero competes over same finite resource");const auto age=host.actors()[0].calendarAge;host.advance({0,0},true);check(host.actors()[0].calendarAge==age,"zero tick no duplicated aging");
 host.advance({20,20},true);check(std::abs(host.ledger().resources.back().consumedQuantity-1)<1e-9,"NPC consumes exact remaining .3 after hero .5 and NPC .2");check(host.actors()[0].hunger<fed,"accepted shared food grants actual NPC hunger relief");check(host.sessions()==bindings&&host.actors().size()==2,"no duplicate actor/session");
 check(host.ledger().sessions[0].sessionId==90&&host.ledger().sessions[0].amount==3&&host.ledger().resources[1].consumedQuantity==1,"old plant/water histories unchanged");
 auto continued=host.registry();check(continued.restoreContinuation(host.continuation()),"depleted availability preserves immutable diet signature and continuation");
 const auto eaten=host.ledger().resources.back().consumedQuantity;host.advanceDormant({10,10});check(host.ledger().resources.back().consumedQuantity==eaten,"dormant camera cannot eat");
 // Wrong-diet explicit plant contact, despite a syntactically legal Browse pose.
 state=pop.snapshot();state.actors[0].site=1;state.actors[0].target=1;state.actors[0].position={4,0,0};state.actors[0].activity=Activity::Browse;pop.restore(state,{1});std::uint64_t allocator=200;engine::NativePopulationRegistry plant(pop,history,{},anchors,allocator);auto raw=plant.registry();const auto plantBinding=plant.sessions()[0].second;
 check(!raw.contact(1001,42,plantBinding,1,200,.2,RegionalAdmissionOrigin::PhysicalArrival).event,"Longstride direct plant food refuses");const auto hunger=plant.actors()[0].hunger;plant.advance({1,1},true);check(plant.actors()[0].hunger>=hunger,"no local/native plant relief");check(plant.snapshot().actors[0].target==food,"hungry Longstride chooses carrion supported graph route");
 std::cout<<"NPC_CARRION_OK diet route/direct contact/local relief/finite hero competition/replay/depletion continuation/dormant/legacy/default/history\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
