#pragma once
#include "fauna_population.hpp"
namespace stalthos::domain {
struct ProtectionObservation {Identity vulnerable=0,threat=0;Vec3 vulnerableBody{},threatBody{},interpose{};double distress=0;};
// Proposed body-local condition-distress + close adult predator approximation.
// Adult Longstride proximity is an actual potential threat, not inferred attack
// intent. There is no camera input, adoption, damage or persistent relationship.
inline std::optional<ProtectionObservation> brackenProtection(Identity defender,HeroSpecies species,Vec3 body,LifeStage stage,bool alive,const std::vector<fauna::ActorView>& actors){
 if(!defender || species!=HeroSpecies::Brackenback || !alive || stage!=LifeStage::Adult || !finite(body))return std::nullopt;
 std::optional<ProtectionObservation> result;double best=12;
 for(const auto& small:actors){if(!small.id || small.id==defender || !small.alive || !finite(small.position) || !std::isfinite(small.calendarAge) || small.calendarAge<0 || !std::isfinite(small.hunger) || !std::isfinite(small.thirst))continue;const auto& p=getHeroProfile(small.species);const bool vulnerable=small.species==HeroSpecies::Thistletail || small.calendarAge<p.hatchlingDuration+p.juvenileDuration;
  const double distress=std::max(small.hunger,small.thirst);const double distance=std::hypot(std::hypot(small.position.x-body.x,small.position.z-body.z),small.position.y-body.y);if(!vulnerable || distress<.85 || distance>best)continue;
  for(const auto& threat:actors){if(!threat.id || threat.id==small.id || threat.id==defender || !threat.alive || threat.species!=HeroSpecies::Longstride || !finite(threat.position) || !std::isfinite(threat.calendarAge))continue;const auto& adult=getHeroProfile(threat.species);if(threat.calendarAge<adult.hatchlingDuration+adult.juvenileDuration || threat.calendarAge>=adult.hatchlingDuration+adult.juvenileDuration+adult.adultDuration+adult.oldDuration)continue;
   const double dx=threat.position.x-small.position.x,dz=threat.position.z-small.position.z,d=std::hypot(dx,dz);if(d<3 || d>6 || std::abs(threat.position.y-small.position.y)>1 || std::hypot(std::hypot(threat.position.x-body.x,threat.position.z-body.z),threat.position.y-body.y)>12)continue;
   result=ProtectionObservation{small.id,threat.id,small.position,threat.position,{small.position.x+dx/d,small.position.y,small.position.z+dz/d},distress};best=distance;break;
  }
 }
 return result;
}
}
