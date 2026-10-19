#include "domain/weather_response.hpp"
#include <iostream>
#include <cmath>
using namespace stalthos;using namespace stalthos::environment;
static void check(bool p,const char* label){if(!p)throw std::runtime_error(label);std::cout<<label<<"=1\n";}
int main(){try{
 auto rain=previewWeather(sample(42,1001,0),1),clear=previewWeather(rain,0);
 BodyWeatherResponse one,many,sheltered;auto single=one.advance(rain,4,0);LocalWeatherResponse split;
 for(int i=0;i<240;++i)split=many.advance(rain,4./240,0);
 check(std::abs(single.wetness-split.wetness)<1e-12,"constant_rain_dt_partition_independent");
 auto lee=sheltered.advance(rain,4,.85);
 check(lee.wetness==single.wetness && lee.wind<single.wind && lee.comfort>single.comfort,"lee_reduces_wind_without_waterproofing");
 auto hold=one.advance(clear,0,.85);check(hold.wetness==single.wetness,"zero_physical_time_no_wetness_change");
 auto dryOne=one.advance(clear,2,0);for(int i=0;i<120;++i)split=many.advance(clear,2./120,0);
 check(std::abs(dryOne.wetness-split.wetness)<1e-12 && dryOne.wetness<single.wetness,"drying_dt_partition_independent");
 SimCore paused,fast;BodyWeatherResponse pausedBody,fastBody;LocalWeatherResponse p,f;
 for(int i=0;i<120;++i){auto pt=paused.advance(1./60,0),ft=fast.advance(1./60,100);p=pausedBody.advance(previewWeather(sample(42,1001,paused.calendarSeconds()),1),pt.physicalSeconds,0);f=fastBody.advance(previewWeather(sample(42,1001,fast.calendarSeconds()),1),ft.physicalSeconds,0);}
 check(std::abs(p.wetness-f.wetness)<1e-12,"calendar_scale_does_not_change_physical_wetting");
 check(std::abs(paused.physicalSeconds()-2)<1e-12 && paused.calendarSeconds()==0 && std::abs(fast.physicalSeconds()-2)<1e-12 && std::abs(fast.calendarSeconds()-200)<1e-9,"weather_preserves_existing_clock_semantics");
 check(routeWindShelter(118,12)>0 && routeWindShelter(100,0)==0,"actual_body_rock_lee_field_has_pocket_and_exposure");
 std::cout<<"INDEPENDENT_WEATHER_RESPONSE_OK wetness="<<single.wetness<<" wind_exposed="<<single.wind<<" wind_lee="<<lee.wind<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
