#include "domain/brackenback.hpp"
#include "domain/thistletail.hpp"
#include "domain/hero_destination.hpp"
#include <iostream>
using namespace stalthos;
static void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{
 require(domain::playableSpecies("Brackenback")==HeroSpecies::Brackenback,"third selectable real species");needs::NeedsCore needs(HeroSpecies::Brackenback);Lifecycle life(HeroSpecies::Brackenback);life.advance({0,4500});require(life.stage()==LifeStage::Adult && needs.species()==HeroSpecies::Brackenback,"real species components");
 const auto& p=getHeroProfile(HeroSpecies::Longstride);
 fauna::ActorView small{2,HeroSpecies::Thistletail,{0,2,0},0,5000,0,.99,0,0,true,fauna::Activity::Rest};
 fauna::ActorView threat{3,HeroSpecies::Longstride,{4,2,0},0,p.hatchlingDuration+p.juvenileDuration,0,0,0,0,true,fauna::Activity::Rest};
 std::vector<fauna::ActorView> actors{small,threat};const Vec3 body{1,2,3};
 auto observe=[&](const std::vector<fauna::ActorView>& current,Vec3 at=Vec3{1,2,3},LifeStage stage=LifeStage::Adult,bool alive=true){return domain::brackenProtection(1,HeroSpecies::Brackenback,at,stage,alive,current);};
 auto seen=observe(actors);require(bool(seen) && seen->vulnerable==2 && seen->threat==3 && seen->interpose.x==1,"adult unrelated Thistletail qualifies and bounded interpose between real actors");
 require(!observe({small}) && !observe({threat}),"both distress and real threat required");auto changed=actors;changed[0].hunger=.2;require(!observe(changed),"calm small refuses");changed=actors;changed[1].alive=false;require(!observe(changed),"dead threat refuses");changed=actors;changed[0].alive=false;require(!observe(changed),"dead small refuses");changed=actors;changed[1].calendarAge=0;require(!observe(changed),"young predator not adult threat");
 changed=actors;changed[1].position.y=4;require(!observe(changed),"vertical plausibility");changed=actors;changed[1].position.x=2;require(!observe(changed),"avoid unsafe close interposition");require(!observe(actors,{36,2,3}) && !observe({}),"remote camera and inactive projection grant no observation");require(!observe(actors,body,LifeStage::Hatchling) && !observe(actors,body,LifeStage::Adult,false),"mature living defender required");
 changed=actors;changed[0].species=HeroSpecies::Smalltrotter;changed[0].calendarAge=0;require(bool(observe(changed)),"unrelated juvenile also qualifies");changed[0].calendarAge=4000;require(!observe(changed),"large adult not presumed vulnerable");
 HeroSim hero;require(domain::HeroDestination::submit(hero,body,seen->interpose,true),"existing contextual owner accepts supported interposition");auto action=domain::HeroDestination::update(hero,body,-1.5707963267948966,.1,true,false,true);require(action.active,"existing goal owns movement");action=domain::HeroDestination::update(hero,body,0,.1,true,true,true);require(!action.forward && !domain::HeroDestination::active(hero),"ordinary direct input immediately cancels defense goal");
 std::cout<<"BRACKENBACK_CONTRACT_OK real selection/profile local distress+adult threat no camera safe interpose direct override\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
