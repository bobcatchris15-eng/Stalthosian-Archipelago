#pragma once
#include "weather_response.hpp"
#include "needs.hpp"
namespace stalthos::environment {
// Proposed comfort proxy, not thermodynamics. No injury or lifecycle effect.
struct ExposureTuning {double comfortThreshold=.5,fatiguePerSecond=.012,fatigueCeiling=.65;};
struct ExposureResponse {double severity=0,fatigueAdded=0;};
inline ExposureResponse applyExposure(needs::NeedsCore& needs,const LocalWeatherResponse& local,double physicalSeconds,bool alive=true,const ExposureTuning& tuning={}) {
    if(!std::isfinite(physicalSeconds)||physicalSeconds<0 || !std::isfinite(local.comfort)||local.comfort<0||local.comfort>1 || !std::isfinite(tuning.comfortThreshold)||tuning.comfortThreshold<=0||tuning.comfortThreshold>1 || !std::isfinite(tuning.fatiguePerSecond)||tuning.fatiguePerSecond<0 || !std::isfinite(tuning.fatigueCeiling)||tuning.fatigueCeiling<0||tuning.fatigueCeiling>1) throw std::invalid_argument("invalid exposure input");
    ExposureResponse result;
    if(!alive)return result;
    result.severity=std::clamp((tuning.comfortThreshold-local.comfort)/tuning.comfortThreshold,0.0,1.0);
    result.fatigueAdded=std::min(std::max(0.0,tuning.fatigueCeiling-needs.fatigue()),result.severity*tuning.fatiguePerSecond*physicalSeconds);
    needs.addFatigue(result.fatigueAdded);
    return result;
}
}
