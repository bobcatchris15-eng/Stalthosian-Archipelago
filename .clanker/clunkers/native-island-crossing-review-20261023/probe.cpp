#include "domain/native_archipelago.hpp"
#include "domain/water_locomotion.hpp"
#include <iostream>
#include <limits>
using namespace stalthos;using namespace stalthos::world;
void require(bool value,const char* label){if(!value)throw std::runtime_error(label);}
int main(){try{
 NativeArchipelago a(42);ConnectedRoute old(42);unsigned exact=0,margin=0,sea=0;
 for(int x=-96;x<=300;x+=3)for(int z=-96;z<=96;z+=3){require(a.height(x,z)==old.height(x,z),"origin changed");++exact;}
 for(double scale:{.05,.1,.2}){NativeArchipelago n(42,{scale,.1});double edge=n.bounds()-100;
  for(double x:{-edge,edge})for(double z:{-edge,edge})for(int ix=-1;ix<=1;++ix)for(int iz=-1;iz<=1;++iz){double tx=std::floor(x/128)*128+ix*128,tz=std::floor(z/128)*128+iz*128;for(double ox:{0.,128.})for(double oz:{0.,128.}){require(std::isfinite(n.sample(tx+ox,tz+oz).position.y),"playable corner streaming normal bounds");++margin;}}
 }
 require(a.landRegion(0,0,a.height(0,0))==1001,"freshwater origin wrongly sea");require(a.landRegion(250,0,-1)==0,"extended cove wrongly land");
 auto c=a.firstCrossing();require(bool(c),"no crossing");require(a.landRegion(c->start.x,c->start.z,c->start.y)==1001,"start land");
 needs::NeedsCore needs(HeroSpecies::Smalltrotter);needs.restoreState(0,0,0,0,true);double distance=0,seconds=0;bool landed=false;
 while(seconds<160){double x=c->start.x+std::sin(c->yaw)*distance,z=c->start.z+std::cos(c->yaw)*distance,bed=a.triangleHeight(x,z);auto w=domain::waterMotion(bed,true,needs.fatigue());
 if(w.mode==domain::WaterMode::Swimming){++sea;require(w.bodyDatum==-.55&&a.landRegion(x,z,bed)==0,"seabed walking/island authority");}
 if(sea&&a.landRegion(x,z,bed)==1003&&bed>.4&&std::hypot(x-c->end.x,z-c->end.z)<8){landed=true;break;}
 domain::applySwimming(needs,w,.02,true);needs.advance({.02,.02});double speed=w.mode==domain::WaterMode::Swimming?2.2*w.speedFactor:2*w.speedFactor;if(seconds>=2)distance+=speed*.02;seconds+=.02;
 }
 require(landed&&sea>1000&&!needs.isResting()&&needs.fatigue()>.2,"physical integration/fatigue/landing");
 double before=needs.fatigue();domain::applySwimming(needs,domain::waterMotion(-5,true,before),10,false);require(needs.fatigue()==before,"dead swimming needs mutated");
 require(domain::waterMotion(-10,false,0).bodyDatum==-10,"original fresh datum overridden");
 bool invalid=false;try{domain::waterMotion(std::numeric_limits<double>::quiet_NaN(),true,0);}catch(const std::invalid_argument&){invalid=true;}require(invalid,"nonfinite water accepted");
 std::cout<<"INDEPENDENT_ISLAND_OK exact_origin="<<exact<<" streaming_corners="<<margin<<" swim_steps="<<sea<<" landing_seconds="<<seconds<<" fatigue="<<needs.fatigue()<<" distance="<<distance<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
