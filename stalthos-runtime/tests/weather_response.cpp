#include "domain/weather_response.hpp"
#include <iostream>
int main(){try {using namespace stalthos::environment;
 auto conditions=previewWeather(sample(42,1001,0),1);BodyWeatherResponse exposed,sheltered;
 const auto a=exposed.advance(conditions,4,0),b=sheltered.advance(conditions,4,.85);
 if(a.wetness!=b.wetness || b.wind>=a.wind || b.comfort<=a.comfort || a.wetness<=0) throw std::runtime_error("wind shelter response invalid");
 if(routeWindShelter(118,12)<=0 || routeWindShelter(100,0)!=0) throw std::runtime_error("outcrop lee field invalid");
 const auto hold=exposed.advance(conditions,0,0);if(hold.wetness!=a.wetness) throw std::runtime_error("zero physical time advanced wetness");
 auto clear=previewWeather(conditions,0);if(exposed.advance(clear,100,0).wetness>=a.wetness) throw std::runtime_error("drying response absent");
 bool reject=false;try {exposed.advance(conditions,-1,0);}catch(const std::invalid_argument&){reject=true;}if(!reject) throw std::runtime_error("negative time accepted");
 std::cout<<"WEATHER_RESPONSE_OK wetness="<<a.wetness<<" exposed_wind="<<a.wind<<" sheltered_wind="<<b.wind<<" comfort_gain="<<b.comfort-a.comfort<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
