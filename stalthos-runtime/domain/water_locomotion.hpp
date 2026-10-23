#pragma once
#include "needs.hpp"
namespace stalthos::domain {
enum class WaterMode {Land,Wading,Swimming};
struct WaterMotion {WaterMode mode;double depth=0,bodyDatum=0,speedFactor=1;};
inline WaterMotion waterMotion(double bed,bool marine,double fatigue){
    if(!std::isfinite(bed)||!std::isfinite(fatigue)||fatigue<0||fatigue>1)throw std::invalid_argument("invalid water body state");
    const double depth=marine?std::max(0.0,-bed):0;
    return {depth>=.55?WaterMode::Swimming:depth>.05?WaterMode::Wading:WaterMode::Land,depth,marine?std::max(bed,-.55):bed,depth>=.55?(fatigue>.9?.4:1):depth>.05?.7:1};
}
inline void applySwimming(needs::NeedsCore& n,const WaterMotion& water,double dt,bool alive){if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid swim timestep");if(alive&&water.mode!=WaterMode::Land){n.setResting(false);n.addFatigue(dt*(water.mode==WaterMode::Swimming?.002:.0006));}}
}
