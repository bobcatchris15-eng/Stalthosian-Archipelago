#pragma once
#include "foundation.hpp"
namespace stalthos::domain {
struct SaplingDisturbance {
 Identity tree=9006;RegionId region=1001;Vec3 position{};double reach=1.8,verticalReach=.9;
 std::optional<TreeException> exception(const StalthosState& world)const{for(const auto& t:world.regionSnapshot(region).trees)if(t.tree==tree)return t;return std::nullopt;}
 bool eligible(Vec3 body,bool alive,bool supported)const{return alive&&supported&&finite(body)&&finite(position)&&std::hypot(body.x-position.x,body.z-position.z)<=reach&&std::abs(body.y-position.y)<=verticalReach;}
 bool apply(StalthosState& world,Vec3 body,bool alive,bool supported,double calendarSeconds)const{
  if(!eligible(body,alive,supported)||exception(world))return false;
  world.recordTree(region,{tree,TreeCondition::StandingDamaged,calendarSeconds});return true;
 }
};
}
