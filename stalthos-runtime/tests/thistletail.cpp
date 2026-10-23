#include "domain/thistletail.hpp"
#include "domain/hero_autonomy.hpp"
#include "domain/lifecycle.hpp"
#include "domain/resource_ledger.hpp"
#include <iostream>
using namespace stalthos;
static void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{
 require(domain::playableSpecies("Smalltrotter")==HeroSpecies::Smalltrotter,"default remains selectable");require(domain::playableSpecies("Thistletail")==HeroSpecies::Thistletail,"real contrasting enum selected");bool rejected=false;try{domain::playableSpecies("Unimplemented");}catch(const std::invalid_argument&){rejected=true;}require(rejected,"unimplemented choices refused");
 const auto species=domain::playableSpecies("Thistletail");needs::NeedsCore need(species);Lifecycle life(species);need.restoreState(.8,0,0,0,false);life.advance({1,800});require(life.stage()==LifeStage::Juvenile && need.species()==species,"selected lifecycle and needs own life");
 domain::MicroforagePatch patch;const Vec3 groundFood{patch.x,2,patch.z};HeroSim hero;domain::HeroDrinkAutonomy forage;
 auto update=[&](Vec3 body,bool direct,bool eligible,double remain){const bool sensed=patch.sensed(body,2,remain);return forage.update(hero,need,body,1.5707963267948966,.1,direct,sensed?std::optional<Vec3>(groundFood):std::nullopt,sensed,eligible,needs::SuggestedActivity::Eat,patch.resource);};
 auto action=update({patch.x-6,2,patch.z},false,false,1);require(action.forward && hero.perception.attention()->source==patch.resource,"ordinary body-local approach through existing single controller");
 action=update({patch.x-6,2,patch.z},true,false,1);require(!action.forward && !action.contact,"direct input overrides food goal");
 action=update({patch.x-36,2,patch.z},false,false,1);require(!action.active && !hero.perception.attention(),"camera-side patch cannot grant distant body knowledge");
 require(!patch.eligible(groundFood,2,false,true) && !patch.eligible(groundFood,2,true,false),"supported living contact required");require(!patch.eligible({patch.x,3,patch.z},2,true,true),"vertical reach bounded");
 ResourceLedger ledger(4,8);require(ledger.load({{patch.resource,ResourceKind::Shrub,patch.quantity,0}}),"finite existing food semantics");
 action=update(groundFood,false,patch.eligible(groundFood,2,true,true),1);require(action.contact,"real eligible contact intent");
 const auto result=ledger.tryConsumeCumulative(40,patch.resource,.5);require(result==ResourceAdmission::NewKey,"existing shared ledger contact");need.feed(.5);forage.completedContact(hero);require(std::abs(need.hunger()-.3)<1e-12,"actual selected needs relief");require(ledger.tryConsumeCumulative(40,patch.resource,.5)==ResourceAdmission::Replay,"held session idempotence");require(patch.fraction(.5)==.5 && patch.fraction(0)==0,"visible fraction reflects actual food");
 require(ledger.tryConsumeCumulative(41,patch.resource,.5)==ResourceAdmission::NewKey,"second finite contact");require(ledger.tryConsumeCumulative(42,patch.resource,.5)!=ResourceAdmission::NewKey,"no food invented after depletion");
 std::cout<<"THISTLETAIL_CONTRACT_OK selection needs/lifecycle body-local sensing single-goal override finite shared food idempotence\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
