#include "engine/native_population_registry.hpp"
#include "domain/hatchling_path.hpp"
#include "domain/nesting.hpp"
#include "domain/next_life.hpp"
#include <iostream>
using namespace stalthos;
using namespace stalthos::fauna;
static void require(bool x,const char* why){if(!x)throw std::runtime_error(why);}
int main(){try{
 std::vector<HabitatSite> sites{{{0,2,0},false,true,{1}},{{4,2,0},true,false,{0}}};Population population(42,1001,sites,8);
 ResourceLedgerDto history{{{100,ResourceKind::Creek,100,3},{200,ResourceKind::Shrub,100,1}},{{90,100,3},{91,200,1}}};
 std::vector<RegionalResourceAnchor> anchors{{0,100,sites[0].position},{1,200,sites[1].position}};std::uint64_t next=100;
 engine::NativePopulationRegistry host(population,history,{},anchors,next);const auto old=host.snapshot();const auto oldSessions=host.sessions();const auto oldNext=next;
 const Vec3 nest{2,2,2};const auto dry=[](double,double)->std::optional<double>{return 2;};
 const auto path=domain::hatchlingPath(nest,host.habitat(),dry);require(bool(path),"local dry path exists");
 require(!domain::hatchlingPath(nest,sites,[](double,double)->std::optional<double>{return std::nullopt;}),"wet path denied");
 require(!domain::hatchlingPath(nest,sites,[](double x,double)->std::optional<double>{return 2+10*x;}),"steep/unsupported path denied");
 auto attempt=[&](Identity id,RegionalAdmissionOrigin origin=RegionalAdmissionOrigin::PhysicalArrival){return host.admitHatchling(id,path->points,path->attach,next,{2,3},origin);};
 require(attempt(2)==RegionalAdmission::IdentityConflict,"hero/dead hero exclusions");
 require(attempt(4,RegionalAdmissionOrigin::CameraPrewarm)==RegionalAdmission::CameraOnly,"camera cannot admit");require(next==oldNext && host.actors().size()==8,"camera leaves allocator and population");
 auto bad=path->points;bad[0].x+=20;require(host.admitHatchling(4,bad,path->attach,next,{})!=RegionalAdmission::Accepted,"invalid route denied");require(next==oldNext,"failed route leaves session allocator");
 auto collision=next;next=90;require(attempt(4)==RegionalAdmission::SessionConflict,"historical ledger session cannot be claimed");require(next==90,"rejected collision no allocator mutation");next=collision;
 require(attempt(4)==RegionalAdmission::Accepted,"newborn admitted");require(next==oldNext+1 && host.actors().size()==9 && host.sessions().size()==9,"one fresh actor session");
 const auto state=host.snapshot();const auto& child=state.actors.back();require(child.id==4 && child.calendarAge==0 && child.stage==LifeStage::Hatchling,"actual newborn life");require(child.position.x==nest.x && child.position.z==nest.z,"birth at real nest not nearest unrelated node");
 for(std::size_t i=0;i<old.actors.size();++i){const auto& a=old.actors[i];const auto& b=state.actors[i];require(a.id==b.id && a.position.x==b.position.x && a.position.z==b.position.z && a.calendarAge==b.calendarAge && a.hunger==b.hunger && a.thirst==b.thirst && a.site==b.site && a.target==b.target,"old actors exact at commit");}
 const auto admittedSessions=host.sessions();for(const auto& b:oldSessions)require(std::find(admittedSessions.begin(),admittedSessions.end(),b)!=admittedSessions.end(),"old sessions retained");
 require(host.ledger().sessions.size()==2 && host.ledger().sessions[0].sessionId==90 && host.ledger().sessions[0].amount==3 && host.ledger().resources[0].consumedQuantity==3,"full history unchanged at birth");
 require(attempt(4)==RegionalAdmission::IdentityConflict && host.actors().size()==9 && next==oldNext+1,"no duplicate/revisit birth");
 auto beforeDormant=host.snapshot().actors.back();host.advanceDormant({2,10});require(host.admitHatchling(9,path->points,path->attach,next,{})!=RegionalAdmission::Accepted,"inactive region cannot hatch");auto dormant=host.snapshot().actors.back();require(dormant.calendarAge==10 && dormant.position.x==beforeDormant.position.x && dormant.distanceTravelledMeters==0,"inactive retention no movement");
 host.advance({2,7},true);auto moved=host.snapshot().actors.back();require(moved.calendarAge==17 && moved.distanceTravelledMeters>0 && moved.thirst>dormant.thirst,"independent normal movement needs and once-only calendar");require(std::abs(moved.position.y-2)<1e-9,"supported newborn route");
 const auto continuation=host.continuation();require(host.registry().capacity().individuals==9,"capacity updated");auto copy=host.registry();require(copy.restoreContinuation(continuation),"continuation against admitted graph works");
 StalthosState world(42);world.setHero(2,1);world.heroDied();std::vector<Identity> npc;for(const auto& a:host.actors())npc.push_back(a.id);const auto replacement=domain::beginNextLife(world,2,npc,next);require(replacement.hero!=4,"new hero cannot alias newborn");require(host.snapshot().actors.back().id==4,"new life retains child");
 domain::Nesting clutch;require(clutch.lay(2,HeroSpecies::Smalltrotter,LifeStage::Adult,.1,.1,.1,{nest,true,true,true,0},0,42),"lays");clutch.advance({1,900});require(clutch.recordAdmittedOffspring(4) && !clutch.recordAdmittedOffspring(5) && clutch.offspring()==4,"one-shot closure after commit");
 // Exhaust local actor cap without replacing anyone; failed birth keeps all state.
 while(host.actors().size()<Population::maxActors){Identity id=1000+host.actors().size();require(host.admitHatchling(id,path->points,path->attach,next,{})==RegionalAdmission::Accepted,"fill bounded capacity");}
 const auto atCap=next;require(host.admitHatchling(9999,path->points,path->attach,next,{})==RegionalAdmission::IndividualLimit && next==atCap && host.actors().size()==16,"capacity failure leaves eggs/allocator/population intact");
 std::cout<<"HATCHLING_ADMISSION_OK transactional identity/session/history, local support, independent needs/movement, camera refusal, continuation, next-life retention, capacity rollback\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
