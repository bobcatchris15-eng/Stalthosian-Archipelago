#pragma once
#include "foundation.hpp"
#include "needs.hpp"
#include <algorithm>
namespace stalthos::domain {
struct DrinkAction {bool forward=false,contact=false,active=false;double turn=0;};
class HeroDrinkAutonomy {
    bool active_=false;
    double cooldown_=0;
    Vec3 target_{};
    Identity targetResource_=0;
    void stop(HeroSim& hero,GoalStatus status) {
        if(active_ && hero.goal() && (hero.goal()->status==GoalStatus::Active || hero.goal()->status==GoalStatus::Paused || hero.goal()->status==GoalStatus::Replanning)) hero.setGoalStatus(status);
        active_=false;
    }
public:
    static constexpr double senseRadius=24;
    DrinkAction update(HeroSim& hero,const needs::NeedsCore& needs,Vec3 body,double heading,double physicalDt,bool direct,
        std::optional<Vec3> localWater,bool available,bool eligible,needs::SuggestedActivity activity=needs::SuggestedActivity::Drink,Identity resourceId=100) {
        if(!finite(body)||!std::isfinite(heading)||!std::isfinite(physicalDt)||physicalDt<0) throw std::invalid_argument("invalid hero autonomy input");
        hero.setDirect(direct);
        if(localWater && finite(*localWater)) {
            const double dx=localWater->x-body.x,dy=localWater->y-body.y,dz=localWater->z-body.z;
            if(dx*dx+dy*dy+dz*dz<=senseRadius*senseRadius && available) hero.perception.observe({resourceId,*localWater,activity==needs::SuggestedActivity::Eat?needs.hunger():needs.thirst()});else hero.perception.clear();
        } else hero.perception.clear();
        cooldown_=std::max(0.0,cooldown_-physicalDt);
        if(direct) {stop(hero,GoalStatus::PlayerCancelled);return {};}
        if(!available || !hero.perception.attention() || needs.suggestActivity()!=activity) {stop(hero,GoalStatus::Completed);return {};}
        if(active_ && targetResource_!=resourceId) stop(hero,GoalStatus::Completed);
        if(cooldown_>0) return {};
        if(!active_) {
            if(!hero.perception.attention()) return {};
            targetResource_=resourceId;target_=hero.perception.attention()->point;hero.suggest(target_,.8);active_=true;
        }
        if(eligible) return {false,true,true,0};
        const double dx=target_.x-body.x,dz=target_.z-body.z;
        const double difference=std::remainder(std::atan2(dx,dz)-heading,6.28318530717958647692);
        const double turn=std::clamp(difference,-1.5*physicalDt,1.5*physicalDt);
        return {std::abs(difference)<.3 && dx*dx+dz*dz>.4,false,true,turn};
    }
    void completedContact(HeroSim& hero) {stop(hero,GoalStatus::Completed);cooldown_=3;}
};
}
