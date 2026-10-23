#include "domain/fauna_population.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::fauna;
int main(){try{std::vector<HabitatSite>s{{{0,0,0},true,false,{1}},{{10,0,0},false,true,{2}},{{10,0,10},false,false,{0}}};Population p(42,1001,s,1);auto v=p.snapshot();auto&a=v.actors[0];a.species=HeroSpecies::Reedcrest;a.calendarAge=5000;a.stage=LifeStage::Adult;a.site=0;a.target=1;a.position={3,0,0};a.activity=Activity::Travel;a.hunger=.2;a.thirst=.2;a.fatigue=.2;p.restore(v);BodyAwareness context{{-4,0,0},{},a.id,3,false,true};p.advance({.1,.1},false,context);p.restore(p.snapshot());std::cout<<"DIRECTED_EDGE_RESTORE_OK\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
