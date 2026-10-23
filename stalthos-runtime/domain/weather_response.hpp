#pragma once
#include "world_environment.hpp"
#include "connected_route.hpp"
#include <algorithm>
#include <array>
namespace stalthos::environment {
struct WeatherResponseTuning {double wettingPerSecond=.12,dryingPerSecond=.015,windReduction=.8;};
struct LocalWeatherResponse {double wetness=0,wind=0,comfort=1,shelter=0;};
// Coarse lee-of-outcrop field, not roof/ray occlusion or hydrological shelter.
inline double routeWindShelter(double x,double z,const world::ConnectedRouteLayout& route={}) {
    if(!std::isfinite(x)||!std::isfinite(z)) throw std::invalid_argument("invalid shelter position");
    const std::array<std::pair<double,double>,5> rocks{{{route.joinX+16,route.centerZ-16},{route.joinX+30,route.centerZ+20},{route.meadowX-24,route.centerZ-28},{route.meadowX+4,route.centerZ+29},{route.meadowX+24,route.centerZ-18}}};
    double shelter=0;
    for(const auto& rock:rocks) {
        const double dx=x-rock.first,dz=z-rock.second,along=dx*.8+dz*.6,cross=-dx*.6+dz*.8;
        if(along>=2 && along<18 && std::abs(cross)<8) shelter=std::max(shelter,.9*(1-along/18)*(1-std::abs(cross)/8));
    }
    return shelter;
}
class BodyWeatherResponse {
    WeatherResponseTuning tuning_;
    double wetness_=0;
public:
    explicit BodyWeatherResponse(WeatherResponseTuning tuning={}):tuning_(tuning) {
        if(!std::isfinite(tuning.wettingPerSecond)||tuning.wettingPerSecond<0||!std::isfinite(tuning.dryingPerSecond)||tuning.dryingPerSecond<0||!std::isfinite(tuning.windReduction)||tuning.windReduction<0||tuning.windReduction>1) throw std::invalid_argument("invalid weather response tuning");
    }
    double wetness()const{return wetness_;}
    void restoreWetness(double value){if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("invalid restored wetness");wetness_=value;}
    LocalWeatherResponse advance(const Conditions& c,double physicalSeconds,double shelter) {
        if(!std::isfinite(physicalSeconds)||physicalSeconds<0||!std::isfinite(shelter)||shelter<0||shelter>1||!std::isfinite(c.rainfall)||c.rainfall<0||c.rainfall>1||!std::isfinite(c.cloud)||c.cloud<0||c.cloud>1||!std::isfinite(c.windMetersPerSecond)||c.windMetersPerSecond<0) throw std::invalid_argument("invalid local weather response");
        // Wind shelter never intercepts rainfall. Wetness integrates physical time.
        wetness_=std::clamp(wetness_+(c.rainfall*tuning_.wettingPerSecond-(1-c.rainfall)*(1-c.cloud)*tuning_.dryingPerSecond)*physicalSeconds,0.0,1.0);
        const double wind=c.windMetersPerSecond*(1-tuning_.windReduction*shelter);
        return {wetness_,wind,1-std::clamp(wetness_*.35+wind/20*.65,0.0,1.0),shelter};
    }
};
inline Conditions previewWeather(Conditions baseline,unsigned phase) {
    if(phase>2) throw std::invalid_argument("invalid diagnostic weather phase");
    baseline.cloud=phase==0?.1:.95;baseline.rainfall=phase==0?0:.85;baseline.windMetersPerSecond=phase==0?2:12;
    baseline.weather=phase==0?Weather::Clear:Weather::Rain;return baseline;
}
}
