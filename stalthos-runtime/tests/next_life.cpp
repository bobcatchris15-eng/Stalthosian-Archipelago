#include "domain/next_life.hpp"
#include <iostream>
#include <stdexcept>
using namespace stalthos;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 StalthosState world(42);world.setHero(1,7);world.heroDied();
 require(!world.retained(1),"fixture must cover nonretained dead hero");
 world.instantiate(2,8);uint64_t next=14;
 const auto selected=domain::beginNextLife(world,1,{3,4},next);
 require(selected.hero==5 && selected.session==14 && next==15,"fresh ownership");
 require(world.heroDeathKnown(1)&&world.excluded(1),"old death preserved");
 require(world.snapshot().hero->second==7&&world.worldSeed()==42,"region and world preserved");
 bool refused=false;try{domain::beginNextLife(world,1,{},next);}catch(const std::logic_error&){refused=true;}
 require(refused&&next==15,"live replacement refused without session mutation");
 world.heroDied();const auto again=domain::beginNextLife(world,5,{},next);
 require(again.hero==6&&again.session==15&&world.heroDeathKnown(5)&&world.heroDeathKnown(1),"multiple deaths kept");
 world.heroDied();uint64_t exhausted=0;refused=false;try{domain::beginNextLife(world,6,{},exhausted);}catch(const std::overflow_error&){refused=true;}
 require(refused&&!world.snapshot().hero&&world.heroDeathKnown(6),"exhaustion leaves death intact");
 std::cout<<"next-life ownership checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
