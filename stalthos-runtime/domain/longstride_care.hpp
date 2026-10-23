#pragma once
#include "fauna_population.hpp"
namespace stalthos::domain {
struct CareDecision {std::optional<Vec3> target;bool present=false;const char* status="No wild caregiver for this life";};
// One actual wild caregiver, not asserted biological lineage. No camera input,
// parent movement command, feeding dependency or replacement-parent search.
class LongstrideCare {
    Identity young_=0,parent_=0;bool terminated_=false;
public:
    Identity young()const{return young_;}Identity parent()const{return parent_;}bool terminated()const{return terminated_;}
    static bool adult(const fauna::ActorView& a){const auto& p=getHeroProfile(HeroSpecies::Longstride);return a.id&&a.alive&&a.species==HeroSpecies::Longstride&&finite(a.position)&&std::isfinite(a.calendarAge)&&a.calendarAge>=p.hatchlingDuration+p.juvenileDuration&&a.calendarAge<p.hatchlingDuration+p.juvenileDuration+p.adultDuration+p.oldDuration;}
    bool bindBirth(Identity young,HeroSpecies species,LifeStage stage,Vec3 body,const fauna::ActorView& parent){
        if(young_||!young||young==parent.id||species!=HeroSpecies::Longstride||stage!=LifeStage::Hatchling||!finite(body)||!adult(parent)||std::abs(body.y-parent.position.y)>2||std::hypot(body.x-parent.position.x,body.z-parent.position.z)>6)return false;
        young_=young;parent_=parent.id;return true;
    }
    CareDecision observe(Identity hero,LifeStage stage,Vec3 body,bool alive,bool physical,bool direct,bool playerGoal,bool needsPriority,const std::vector<fauna::ActorView>& actors){
        if(!young_||hero!=young_)return {};
        if(terminated_)return {{},false,"Wild caregiver lost - ordinary independent activity"};
        if(!alive||stage==LifeStage::Adult||stage==LifeStage::Old||stage==LifeStage::Dead){terminated_=true;return {{},false,"Young life ended or matured - care ended"};}
        if(!physical||!finite(body))return {{},false,"Wild caregiver separated - body-local response only"};
        const auto at=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.id==parent_;});
        if(at==actors.end()||!adult(*at)){terminated_=true;return {{},false,"Wild caregiver dead or missing - no replacement"};}
        const double dx=body.x-at->position.x,dz=body.z-at->position.z,d=std::hypot(dx,dz);
        if(d>=18||std::abs(body.y-at->position.y)>2)return {{},false,"Wild caregiver separated - return by body"};
        if(direct||playerGoal)return {{},true,"Wild caregiver nearby - your movement takes priority"};
        if(needsPriority)return {{},true,"Tending own needs before following caregiver"};
        if(d<=3.5)return {{},true,"Waiting near wild caregiver"};
        const double step=std::min(d-3.0,8.0);
        return {Vec3{body.x-dx/d*step,body.y+(at->position.y-body.y)*step/d,body.z-dz/d*step},true,"Following actual wild caregiver - WASD overrides"};
    }
};
}
