#include "domain/nesting.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace stalthos;
using namespace stalthos::domain;
void require(bool b){if(!b)throw std::runtime_error("nesting contract failed");}
int main(){
 Nesting nest;NestSite site{{20,2,24},true,true,true,.1};
 auto lay=[&](LifeStage stage,const NestSite& s){return nest.lay(17,HeroSpecies::Smalltrotter,stage,.2,.2,.2,s,40);};
 require(!lay(LifeStage::Hatchling,site));require(!lay(LifeStage::Dead,site));
 require(!nest.lay(17,HeroSpecies::Reedcrest,LifeStage::Adult,.2,.2,.2,site,40));
 for(int i=0;i<5;++i){auto bad=site;if(i==0)bad.supported=false;if(i==1)bad.dry=false;if(i==2)bad.clear=false;if(i==3)bad.relief=.19;if(i==4)bad.position.x=std::numeric_limits<double>::quiet_NaN();require(!lay(LifeStage::Adult,bad));}
 require(!nest.lay(17,HeroSpecies::Smalltrotter,LifeStage::Adult,.8,.2,.2,site,40));
 require(lay(LifeStage::Adult,site));require(!lay(LifeStage::Adult,site));
 nest.advance({500,0});require(nest.progress()==0); // physical time cannot incubate
 nest.advance({1,450});require(nest.progress()==.5);require(!nest.incubated());
 nest.advance({1,10000});require(nest.incubated());require(nest.progress()==1);
 require(nest.clutch()->parent==17 && nest.clutch()->position.x==20 && nest.clutch()->laidAt==40);
 require(!nest.lay(18,HeroSpecies::Smalltrotter,LifeStage::Adult,.2,.2,.2,site,1000)); // new life cannot replace retained nest
 bool threw=false;try{nest.advance({0,-1});}catch(const std::invalid_argument&){threw=true;}require(threw);
 std::cout<<"NESTING_CONTRACT_OK eligibility, retained location, calendar-only progress, bounded clutch, no offspring admission\n";
}
