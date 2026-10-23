#pragma once
#include "lifecycle.hpp"
#include <algorithm>
#include <optional>
#include <string>

namespace stalthos::domain {
// Proposed fictional first-draft tuning. One bounded clutch per connected session.
// Birth identity is recorded only after authoritative live population admission.
struct NestSite { Vec3 position{}; bool supported=false, dry=false, clear=false; double relief=0; };
struct Clutch { Identity parent=0; Vec3 position{}; double laidAt=0, incubatedSeconds=0;Identity mate=0; };
class Nesting {
    std::optional<Clutch> clutch_;
    Identity offspring_=0;
public:
    static constexpr double incubationSeconds=900;
    const std::optional<Clutch>& clutch() const { return clutch_; }
    std::string refusal(Identity parent,HeroSpecies species,LifeStage stage,double hunger,double thirst,double fatigue,const NestSite& site) const {
        if(clutch_) return "One clutch already laid this session";
        if(!parent || species!=HeroSpecies::Smalltrotter || stage!=LifeStage::Adult) return "Nesting requires a living adult Smalltrotter";
        for(double v:{hunger,thirst,fatigue}) if(!std::isfinite(v) || v<0 || v>=.7) return "Feed, drink or rest before nesting";
        if(!finite(site.position) || !std::isfinite(site.relief) || !site.supported || site.relief<0 || site.relief>.18) return "Choose gently sloping supported ground";
        if(!site.dry) return "Choose dry ground away from water";
        if(!site.clear) return "Choose ground clear of plants, rocks and animals";
        return {};
    }
    bool lay(Identity parent,HeroSpecies species,LifeStage stage,double hunger,double thirst,double fatigue,const NestSite& site,double calendar,Identity acceptedMate=0) {
        if(!acceptedMate || acceptedMate==parent || !std::isfinite(calendar) || calendar<0 || !refusal(parent,species,stage,hunger,thirst,fatigue,site).empty()) return false;
        clutch_=Clutch{parent,site.position,calendar,0,acceptedMate}; return true;
    }
    void advance(const TimeStep& step) {
        if(!std::isfinite(step.calendarSeconds) || step.calendarSeconds<0) throw std::invalid_argument("invalid incubation calendar step");
        if(clutch_) clutch_->incubatedSeconds=std::min(incubationSeconds,clutch_->incubatedSeconds+step.calendarSeconds);
    }
    double progress() const { return clutch_?clutch_->incubatedSeconds/incubationSeconds:0; }
    bool incubated() const { return clutch_ && progress()>=1; }
    Identity offspring()const{return offspring_;}
    bool recordAdmittedOffspring(Identity id){if(!incubated() || offspring_ || !id)return false;offspring_=id;return true;}
};
}
