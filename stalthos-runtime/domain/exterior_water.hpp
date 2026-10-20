#pragma once
#include "exterior_browse.hpp"
namespace stalthos::fauna {
struct ExteriorWaterSource {
 double bankX=132,bankZ=30,centerZ=33.5,radius=2.6,quantity=2,reach=1.1,verticalReach=.85;
 static constexpr Identity resource=209;
 double surfaceHeight(const std::function<double(double,double)>& ground)const{double y=ground(bankX,centerZ);for(int i=0;i<48;++i){const double a=i*6.283185307179586/48;y=std::max(y,ground(bankX+radius*std::cos(a),centerZ+radius*std::sin(a)));}return y+.04;}
 bool eligible(Vec3 body,double bankHeight,double surface,bool supported)const{return supported&&finite(body)&&std::isfinite(bankHeight)&&std::isfinite(surface)&&std::hypot(body.x-bankX,body.z-bankZ)<=reach&&std::abs(body.y-bankHeight)<=.85&&std::abs(surface-bankHeight)<=verticalReach;}
};
inline Population withExteriorWater(const Population& original,ExteriorHabitat& habitat,const std::set<Identity>& excluded,bool browse,const ExteriorWaterSource& source={}){
 const auto site=habitat.residents[2];if(site>=habitat.sites.size()||std::hypot(habitat.sites[site].position.x-source.bankX,habitat.sites[site].position.z-source.bankZ)>.01)throw std::invalid_argument("water bank must match exterior resident site");
 habitat.sites[site].freshwater=true;auto result=browse?withExteriorBrowse(original,habitat,excluded):withExteriorResidents(original,habitat,excluded);auto state=result.snapshot();auto& drinker=state.actors.at(state.actors.size()-2);drinker.site=site;drinker.target=site;drinker.position=habitat.sites[site].position;drinker.thirst=.6;drinker.hunger=.22;drinker.fatigue=.28;drinker.activity=Activity::Drink;result.restore(state,excluded);return result;
}
}
