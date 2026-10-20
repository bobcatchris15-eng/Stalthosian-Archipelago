#include "domain/hero_destination.hpp"
#include <iostream>
using namespace stalthos;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{HeroSim h;domain::HeroDrinkAutonomy routine;needs::NeedsCore n(HeroSpecies::Smalltrotter);n.restoreState(0,.8,0,0,false);
 check(!domain::HeroDestination::submit(h,{0,0,0},{40,0,0},true),"body local bound");check(!domain::HeroDestination::submit(h,{0,0,0},{4,0,0},false),"unsupported rejected");
 check(domain::HeroDestination::submit(h,{0,0,0},{4,0,0},true),"destination accepted");check(h.authority()==Authority::DestinationOrContext,"destination authority");
 auto a=domain::HeroDestination::update(h,{0,0,0},1.57079632679,.1,true,false,true);check(a.forward&&!a.contact,"ordinary motion without contact");
 domain::HeroDestination::update(h,{0,0,0},0,.1,true,true,true);check(h.goal()->status==GoalStatus::PlayerCancelled,"direct cancels immediately");h.setDirect(false);
 domain::HeroDestination::submit(h,{0,0,0},{4,0,0},true);domain::HeroDestination::update(h,{3.6,0,0},0,.1,true,false,true);check(h.goal()->status==GoalStatus::Completed&&h.authority()==Authority::Autonomy,"arrival yields");
 domain::HeroDestination::submit(h,{0,0,0},{4,0,0},true);domain::HeroDestination::update(h,{0,0,0},0,.1,true,false,false);check(h.goal()->status==GoalStatus::Abandoned,"support loss stops");
 domain::HeroDestination::submit(h,{0,0,0},{4,0,0},true);auto old=h.goal()->target;routine.update(h,n,{0,0,0},0,.1,false,Vec3{0,0,3},true,false);check(h.goal()->contextual&&h.goal()->target.x==old.x,"need routine preserves explicit goal");
 std::cout<<"HERO_DESTINATION_OK authority, arrival, cancellation, body-bound, support, routine priority\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
