#pragma once
#include "hero_destination.hpp"
#include <functional>
namespace stalthos::domain {
struct ExplorationTuning {double stepMeters=1.5,arrivalMeters=.25,waitSeconds=3,pauseSeconds=3,legSeconds=30;};
enum class ExplorationPhase {Waiting,Moving,Looking};
class HeroExploration {
 ExplorationTuning tuning_;ExplorationPhase phase_=ExplorationPhase::Waiting;
 double remaining_,leg_=0,bearing_=0;unsigned choice_=0;bool bearingKnown_=false;Vec3 target_{};
 bool owns(const HeroSim& hero)const{return phase_==ExplorationPhase::Moving&&hero.goal()&&!hero.goal()->contextual&&std::hypot(hero.goal()->target.x-target_.x,hero.goal()->target.z-target_.z)<1e-8&&hero.goal()->status==GoalStatus::Active;}
 void stop(HeroSim& hero){if(owns(hero))hero.setGoalStatus(GoalStatus::Abandoned);phase_=ExplorationPhase::Waiting;remaining_=tuning_.waitSeconds;}
public:
 explicit HeroExploration(ExplorationTuning tuning={}):tuning_(tuning),remaining_(tuning.waitSeconds){
  if(!std::isfinite(tuning.stepMeters)||tuning.stepMeters<=0||tuning.stepMeters>HeroDestination::radius||!std::isfinite(tuning.arrivalMeters)||tuning.arrivalMeters<=0||tuning.arrivalMeters>=tuning.stepMeters||!std::isfinite(tuning.waitSeconds)||tuning.waitSeconds<=0||!std::isfinite(tuning.pauseSeconds)||tuning.pauseSeconds<=0||!std::isfinite(tuning.legSeconds)||tuning.legSeconds<=0)throw std::invalid_argument("invalid exploration tuning");
 }
 ExplorationPhase phase()const{return phase_;}const Vec3& target()const{return target_;}const ExplorationTuning& tuning()const{return tuning_;}
 DrinkAction update(HeroSim& hero,Vec3 body,double heading,double dt,bool permitted,
  const std::function<std::optional<Vec3>(Vec3,double,double,unsigned)>& pick,
  const std::function<bool(Vec3,Vec3)>& supported){
  if(!finite(body)||!std::isfinite(heading)||!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid exploration context");
  if(!permitted||hero.authority()==Authority::Direct||HeroDestination::active(hero)){stop(hero);bearingKnown_=false;choice_=0;return {};}
  if(phase_==ExplorationPhase::Waiting||phase_==ExplorationPhase::Looking){
   remaining_=std::max(0.0,remaining_-dt);if(remaining_>0)return {};
   if(!bearingKnown_){bearing_=heading;bearingKnown_=true;}
   const auto candidate=pick(body,bearing_,tuning_.stepMeters,choice_++%4);
   if(!candidate||!finite(*candidate)||std::hypot(candidate->x-body.x,candidate->z-body.z)>tuning_.stepMeters+.001||!supported(body,*candidate)){phase_=ExplorationPhase::Waiting;remaining_=tuning_.waitSeconds;return {};}
   target_=*candidate;hero.suggest(target_,tuning_.arrivalMeters);phase_=ExplorationPhase::Moving;leg_=0;
  }
  if(!owns(hero)||!supported(body,target_)){stop(hero);return {};}
  const double dx=target_.x-body.x,dz=target_.z-body.z;
  if(std::hypot(dx,dz)<=tuning_.arrivalMeters){hero.setGoalStatus(GoalStatus::Completed);phase_=ExplorationPhase::Looking;remaining_=tuning_.pauseSeconds;return {};}
  leg_+=dt;if(leg_>tuning_.legSeconds){stop(hero);return {};}
  const double difference=std::remainder(std::atan2(dx,dz)-heading,6.283185307179586);
  return {std::abs(difference)<.2,false,true,std::clamp(difference,-1.5*dt,1.5*dt)};
 }
};
}
