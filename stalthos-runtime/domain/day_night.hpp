#pragma once
#include "foundation.hpp"
#include <algorithm>
namespace stalthos::environment {
// Proposed readable lighting, not astronomy. Calendar phase only; no simulation mutation.
struct DayNightTuning {double nightAmbient=.14,nightFill=1.2,nightSkyExposure=.18;};
struct DayNightLight {Vec3 sunDirection,sunColor,ambient;double sunScale,nightFill,skyExposure;};
inline DayNightLight dayNight(double fraction,double cloud,const DayNightTuning& tuning={}){
 if(!std::isfinite(fraction)||fraction<0||fraction>1||!std::isfinite(cloud)||cloud<0||cloud>1||!std::isfinite(tuning.nightAmbient)||tuning.nightAmbient<0||tuning.nightAmbient>.24||!std::isfinite(tuning.nightFill)||tuning.nightFill<0||!std::isfinite(tuning.nightSkyExposure)||tuning.nightSkyExposure<0||tuning.nightSkyExposure>1)throw std::invalid_argument("invalid day/night lighting input");
 const double a=(fraction-.25)*6.28318530717958647692,y=std::sin(a),c=std::cos(a);double day=std::clamp((y+.12)/.42,0.0,1.0);day=day*day*(3-2*day);const double warm=std::clamp(y/.5,0.0,1.0),ambient=tuning.nightAmbient+(.24-tuning.nightAmbient)*day;
 return {{.8*c,y,.6*c},{1,.58+.38*warm,.34+.56*warm},{ambient*(1-cloud/3),(ambient+.02*(1-day))*(1-cloud*7/24),(ambient+.07*(1-day))*(1-cloud/6)},std::pow(std::max(0.0,y),.65)*(1-.78*cloud),tuning.nightFill*(1-day)*(1-.5*cloud),(tuning.nightSkyExposure+(1-tuning.nightSkyExposure)*day)*(1-.55*cloud)};
}
}
