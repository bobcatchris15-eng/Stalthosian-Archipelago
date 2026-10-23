#pragma once
#include "foundation.hpp"
#include <string>
#include <algorithm>
namespace stalthos::domain {
inline HeroSpecies playableSpecies(const std::string& name){if(name=="Smalltrotter")return HeroSpecies::Smalltrotter;if(name=="Thistletail")return HeroSpecies::Thistletail;if(name=="Brackenback")return HeroSpecies::Brackenback;if(name=="Reedcrest")return HeroSpecies::Reedcrest;if(name=="Longstride")return HeroSpecies::Longstride;throw std::invalid_argument("implemented connected heroes: Smalltrotter, Thistletail, Brackenback, Reedcrest or Longstride");}
// Fictional litter-organism/seed cohort, not individual simulated insects.
struct MicroforagePatch {
 static constexpr Identity resource=210;
 double x=24,z=24,quantity=1,reach=.85,sense=12;
 bool sensed(Vec3 body,double ground,double remaining)const{return finite(body)&&std::isfinite(ground)&&remaining>0&&std::hypot(std::hypot(body.x-x,body.z-z),body.y-ground)<=sense;}
 bool eligible(Vec3 body,double ground,bool supported,bool alive)const{return alive&&supported&&finite(body)&&std::isfinite(ground)&&std::hypot(body.x-x,body.z-z)<=reach&&std::abs(body.y-ground)<=.4;}
 double fraction(double remaining)const{return std::clamp(remaining/quantity,0.0,1.0);}
};
}
