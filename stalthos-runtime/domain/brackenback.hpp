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
 for(const auto& distressedActor:actors){if(!distressedActor.id || distressedActor.id==defender || !distressedActor.alive || !finite(distressedActor.position) || !std::isfinite(distressedActor.calendarAge) || distressedActor.calendarAge<0 || !std::isfinite(distressedActor.hunger) || !std::isfinite(distressedActor.thirst))continue;const auto& p=getHeroProfile(distressedActor.species);const bool vulnerable=distressedActor.species==HeroSpecies::Thistletail || distressedActor.calendarAge<p.hatchlingDuration+p.juvenileDuration;
  const double distress=std::max(distressedActor.hunger,distressedActor.thirst);const double distance=std::hypot(std::hypot(distressedActor.position.x-body.x,distressedActor.position.z-body.z),distressedActor.position.y-body.y);if(!vulnerable || distress<.85 || distance>best)continue;
  for(const auto& threat:actors){if(!threat.id || threat.id==distressedActor.id || threat.id==defender || !threat.alive || threat.species!=HeroSpecies::Longstride || !finite(threat.position) || !std::isfinite(threat.calendarAge))continue;const auto& adult=getHeroProfile(threat.species);if(threat.calendarAge<adult.hatchlingDuration+adult.juvenileDuration || threat.calendarAge>=adult.hatchlingDuration+adult.juvenileDuration+adult.adultDuration+adult.oldDuration)continue;
   const double dx=threat.position.x-distressedActor.position.x,dz=threat.position.z-distressedActor.position.z,d=std::hypot(dx,dz);if(d<3 || d>6 || std::abs(threat.position.y-distressedActor.position.y)>1 || std::hypot(std::hypot(threat.position.x-body.x,threat.position.z-body.z),threat.position.y-body.y)>12)continue;
   result=ProtectionObservation{distressedActor.id,threat.id,distressedActor.position,threat.position,{distressedActor.position.x+dx/d,distressedActor.position.y,distressedActor.position.z+dz/d},distress};best=distance;break;
  }
 }
 return result;
}
}
