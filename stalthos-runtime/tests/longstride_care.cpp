#include "domain/longstride_care.hpp"
#include "domain/hero_destination.hpp"
#include "engine/native_population_registry.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::fauna;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
 const auto& profile=getHeroProfile(HeroSpecies::Longstride);ActorView parent{42,HeroSpecies::Longstride,{0,0,0},0,profile.hatchlingDuration+profile.juvenileDuration+1,0,.2,.6,.2,true,Activity::Travel};
 domain::LongstrideCare care;check(care.bindBirth(1,HeroSpecies::Longstride,LifeStage::Hatchling,{3.5,0,0},parent),"actual adult birth caregiver");check(!care.bindBirth(1,HeroSpecies::Longstride,LifeStage::Hatchling,{3.5,0,0},parent),"never rebind same life");
 auto decide=[&](Vec3 body,bool direct=false,bool goal=false,bool need=false,bool physical=true){return care.observe(1,LifeStage::Hatchling,body,true,physical,direct,goal,need,{parent});};
 check(decide({7,0,0}).target.has_value(),"asymmetrical young follows actual parent");check(!decide({7,0,0},true).target,"direct overrides");check(!decide({7,0,0},false,true).target,"player goal overrides");check(!decide({7,0,0},false,false,true).target,"own needs priority");check(!decide({7,0,0},false,false,false,false).target,"inactive body refusal");check(!decide({40,0,0}).present,"remote camera does not locate parent");check(!decide({3,0,0}).target,"wait spacing");check(!decide({7,4,0}).present,"vertical refusal");check(!care.observe(2,LifeStage::Hatchling,{7,0,0},true,true,false,false,false,{parent}).target,"different hero never adopts");
 HeroSim hero;auto target=*decide({7,0,0}).target;check(domain::HeroDestination::submit(hero,{7,0,0},target,true),"one existing destination owner");auto move=domain::HeroDestination::update(hero,{7,0,0},-1.57079632679,1,true,false,true);check(move.forward,"ordinary forward controller");hero.setDirect(true);check(!domain::HeroDestination::active(hero),"direct cancels owned goal");
 parent.alive=false;check(!decide({7,0,0}).target&&care.terminated(),"actual caregiver death terminates");parent.alive=true;parent.id=43;check(!decide({7,0,0}).target,"other adult never replaces dead caregiver");
 std::vector<HabitatSite> sites{{{0,0,0},true,false,{1}},{{10,0,0},false,true,{0}}};Population pop(42,1001,sites,1,{1});auto snap=pop.snapshot();auto& a=snap.actors[0];a.id=42;a.species=HeroSpecies::Longstride;a.calendarAge=profile.hatchlingDuration+profile.juvenileDuration+1;a.stage=LifeStage::Adult;a.position={0,0,0};a.site=0;a.target=1;a.thirst=.6;a.hunger=.2;a.fatigue=.2;a.activity=Activity::Travel;pop.restore(snap,{1});
 ResourceLedgerDto history{{{100,ResourceKind::Creek,100,3}},{{90,100,3}}};std::uint64_t next=100;engine::NativePopulationRegistry registry(pop,history,{},{{1,100,sites[1].position}},next);auto binding=registry.sessions();
 const auto age=registry.actors()[0].calendarAge;registry.advance({1,2},true,BodyAwareness{{3.5,0,0},{},42,3,true,true},true);check(registry.actors()[0].position.x>0,"actual adult continues own route rather than flee/follow young");check(registry.actors()[0].calendarAge==age+2,"one registry lifecycle tick");check(registry.sessions()==binding&&registry.actors().size()==1,"no added actor/session");
 for(int i=0;i<80;++i)registry.advance({1,1},true,BodyAwareness{{7,0,0},{},42,3,true,true},true);
 check(registry.ledger().resources[0].consumedQuantity>3,"urgent parent actually consumes shared water near own young");check(registry.ledger().sessions[0].sessionId==90&&registry.ledger().sessions[0].amount==3,"old ledger history preserved");
 std::cout<<"LONGSTRIDE_CARE_OK actual caregiver/once binding/direct-needs-player override/body sense/death-N refusal/real parent single tick and shared water\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

