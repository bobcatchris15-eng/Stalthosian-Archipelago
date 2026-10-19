#include "engine/native_save.hpp"
#include <iostream>
using namespace stalthos;
using namespace stalthos::fauna;
namespace hs=stalthos::persistence::host_save;
static void check(bool v,const char* m){if(!v)throw std::runtime_error(m);std::cout<<m<<"=1\n";}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
static std::string state(const engine::NativeSimulationOwner& owner){auto s=persistence::captureSave(owner.clock,owner.world);s.faunaPopulation=owner.fauna->snapshot();s.resourceLedger=owner.fauna->ledger();s.faunaResourceSessions=owner.fauna->sessions();return hs::legacyBytes(s);}
int main(){try{
 std::vector<HabitatSite> sites{{{0,0,0},false,true,{1}},{{2,0,0},true,false,{0}}};
 Population pop(42,1001,sites,2);auto imported=pop.snapshot();for(auto& a:imported.actors){a.id+=100000;a.species=HeroSpecies::Longstride;a.calendarAge=0;a.stage=LifeStage::Hatchling;}pop.restore(imported);
 ResourceLedgerDto ledger{{{100,ResourceKind::Creek,7.5,.3},{200,ResourceKind::Shrub,3.25,.2},{900,ResourceKind::Shrub,42,1}},{{91,100,.3},{92,200,.2},{93,900,1}}};
 std::vector<RegionalResourceAnchor> anchors{{0,100,sites[0].position},{1,200,sites[1].position}};
 engine::validateNativeResources(ledger,anchors);
 std::uint64_t next=100;engine::NativePopulationRegistry admission(pop,ledger,{{imported.actors[0].id,500}},anchors,next);
 SimCore clock;StalthosState world(42);world.setHero(1,1);auto host=persistence::captureSave(clock,world);host.faunaPopulation=admission.snapshot();host.resourceLedger=admission.ledger();host.faunaResourceSessions=admission.sessions();
 hs::Candidate c{host,admission.continuation(),engine::nativeSaveConfiguration(true),1};
 auto owner=engine::restoreNativeSave(hs::decode(hs::encode(c),5),admission,true);const auto before=state(owner);
 check(before==hs::legacyBytes(host),"custom_species_ids_resources_history_sessions_exact");
 auto bad=c;bad.regional->regions[0].anchors[0].position.x+=.1;
 check(rejects([&]{owner=engine::restoreNativeSave(bad,admission,true);}) && state(owner)==before,"anchor_rejection_owner_unchanged");
 bad=c;bad.host.faunaPopulation->habitatSignature++;bad.regional->regions[0].population.habitatSignature++;
 check(rejects([&]{owner=engine::restoreNativeSave(bad,admission,true);}) && state(owner)==before,"habitat_rejection_owner_unchanged");
 bad=c;bad.regional->regions[0].reservation.historyKeys--;
 check(rejects([&]{owner=engine::restoreNativeSave(bad,admission,true);}) && state(owner)==before,"reservation_rejection_owner_unchanged");
 check(rejects([&]{owner=engine::restoreNativeSave(c,admission,false);}) && state(owner)==before,"nonpopulated_rejection_owner_unchanged");
 auto wrong=ledger;wrong.resources[0].kind=ResourceKind::Shrub;check(rejects([&]{engine::validateNativeResources(wrong,anchors);}),"required_anchor_kind_rejected");
 auto empty=pop.snapshot();empty.actors.clear();pop.restore(empty);next=100;engine::NativePopulationRegistry emptyAdmission(pop,ledger,{},anchors,next);host.faunaPopulation=empty;host.faunaResourceSessions.clear();
 hs::Candidate ec{host,emptyAdmission.continuation(),5,1};auto eo=engine::restoreNativeSave(hs::decode(hs::encode(ec),5),emptyAdmission,true);
 check(eo.fauna->snapshot().actors.empty() && eo.fauna->ledger().sessions.size()==3,"empty_import_preserves_unbound_hero_history");
 std::cout<<"INDEPENDENT_NATIVE_SAVE_ADMISSION_OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
