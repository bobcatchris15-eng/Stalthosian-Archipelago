#include "domain/environmental_exposure.hpp"
#include <iostream>
#include <stdexcept>
using namespace stalthos;
void check(bool c,const char* m){if(!c)throw std::runtime_error(m);}
int main(){try{
 needs::NeedsCore exposed(HeroSpecies::Smalltrotter),lee(HeroSpecies::Smalltrotter);
 auto c=environment::previewWeather(environment::sample(1,1001,0,{}),1);
 environment::BodyWeatherResponse a,b;
 environment::LocalWeatherResponse wa,wb;
 for(int i=0;i<100;++i){wa=a.advance(c,.1,0);wb=b.advance(c,.1,.78);environment::applyExposure(exposed,wa,.1);environment::applyExposure(lee,wb,.1);}
 check(exposed.fatigue()>lee.fatigue() && exposed.fatigue()>0,"lee reduces wet wind fatigue");check(wa.wetness==wb.wetness,"lee is not waterproof");
 needs::NeedsCore benign(HeroSpecies::Smalltrotter);auto calm=environment::LocalWeatherResponse{};check(environment::applyExposure(benign,calm,100).fatigueAdded==0,"benign unchanged");
 exposed.restoreState(0,0,.64,0,false);environment::applyExposure(exposed,{1,20,0,0},100);check(exposed.fatigue()==.65 && exposed.injury()==0,"bounded nonlethal consequence");
 exposed.restoreState(0,0,.8,0,true);exposed.advance({1,500});check(environment::applyExposure(exposed,{1,20,0,0},1).fatigueAdded==0 && exposed.fatigue()==.75,"does not erase existing higher fatigue or double recovery");
 check(environment::applyExposure(benign,{1,20,0,0},1,false).fatigueAdded==0,"dead body untouched");
 bool rejected=false;try{environment::applyExposure(benign,calm,-1);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid time rejected");
 std::cout<<"EXPOSURE_OK lee, benign, physical time, cap, rest, terminal\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
