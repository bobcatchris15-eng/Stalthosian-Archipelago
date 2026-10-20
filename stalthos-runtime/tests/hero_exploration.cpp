#include "domain/hero_exploration.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::domain;
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
int main(){try{
 HeroSim hero;HeroExploration roam;unsigned picks=0;
 auto pick=[&](Vec3 p,double,double step,unsigned)->std::optional<Vec3>{++picks;return Vec3{p.x,p.y,p.z+step};};auto supported=[](Vec3,Vec3){return true;};
 require(!roam.update(hero,{0,0,0},0,2.9,true,pick,supported).active&&picks==0,"quiet delay");
 require(roam.update(hero,{0,0,0},0,.2,true,pick,supported).forward&&!hero.goal()->contextual,"ordinary single goal motion");
 roam.update(hero,{0,0,1.3},0,.1,true,pick,supported);require(roam.phase()==ExplorationPhase::Looking&&hero.goal()->status==GoalStatus::Completed,"arrival pause");
 roam.update(hero,{0,0,1.3},0,2,true,pick,supported);require(picks==1,"pause has physical duration");
 roam.update(hero,{0,0,1.3},0,1.1,true,pick,supported);hero.setDirect(true);roam.update(hero,{0,0,1.3},0,.1,false,pick,supported);require(hero.goal()->status==GoalStatus::PlayerCancelled&&roam.phase()==ExplorationPhase::Waiting,"direct override");
 hero.setDirect(false);roam.update(hero,{0,0,1.3},0,4,true,pick,supported);roam.update(hero,{0,0,1.3},0,.1,false,pick,supported);require(hero.goal()->status==GoalStatus::Abandoned,"higher need cancels own goal");
 require(HeroDestination::submit(hero,{0,0,1.3},{1,0,1.3},true),"context command");roam.update(hero,{0,0,1.3},0,10,true,pick,supported);require(HeroDestination::active(hero)&&hero.goal()->target.x==1,"context destination preserved");
 HeroSim waiting;HeroExploration rejected;unsigned attempts=0;auto reject=[&](Vec3,Vec3){++attempts;return false;};rejected.update(waiting,{0,0,0},0,3,true,pick,reject);for(int i=0;i<20;++i)rejected.update(waiting,{0,0,0},0,.1,true,pick,reject);require(attempts==1&&!waiting.goal(),"unsupported point waits without jitter");
 std::cout<<"exploration priority and supported pause checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
