#pragma once
#include "hero_autonomy.hpp"
namespace stalthos::domain {
class HeroDestination {
public:
 static constexpr double radius=12,arrival=.5;
 static bool active(const HeroSim& hero){return hero.goal() && hero.goal()->contextual && hero.goal()->status==GoalStatus::Active;}
 static bool submit(HeroSim& hero,Vec3 body,Vec3 target,bool supported){
  if(!finite(body)||!finite(target)||!supported || std::hypot(target.x-body.x,target.z-body.z)>radius || std::abs(target.y-body.y)>6 || hero.authority()==Authority::Direct)return false;
  hero.suggest(target,arrival,true);return true;
 }
 static DrinkAction update(HeroSim& hero,Vec3 body,double heading,double dt,bool alive,bool direct,bool supported){
  if(!finite(body)||!std::isfinite(heading)||!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid destination motion");
  hero.setDirect(direct);
  if(!active(hero))return {};
  if(!alive||!supported){hero.setGoalStatus(GoalStatus::Abandoned);return {};}
  const auto target=hero.goal()->target;const double dx=target.x-body.x,dz=target.z-body.z;
  if(std::hypot(dx,dz)<=arrival){hero.setGoalStatus(GoalStatus::Completed);return {};}
  const double difference=std::remainder(std::atan2(dx,dz)-heading,6.28318530717958647692);
  return {std::abs(difference)<.2,false,true,std::clamp(difference,-1.5*dt,1.5*dt)};
 }
};
}
