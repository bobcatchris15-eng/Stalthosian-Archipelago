#pragma once
#include "foundation.hpp"
#include <algorithm>
namespace stalthos::domain {
// Authored fictional carrion cohort, not a killed retained NPC. Existing food
// ledger semantics remain authoritative; there is no new save/resource enum.
struct CarrionPatch {
 static constexpr Identity resource=211;
 double x=136,z=27,quantity=1,sense=14,reach=1.1;
 bool sensed(Vec3 body,double ground,double remaining)const{return finite(body)&&std::isfinite(ground)&&std::isfinite(remaining)&&remaining>0&&std::hypot(std::hypot(body.x-x,body.z-z),body.y-ground)<=sense;}
 bool eligible(Vec3 body,double ground,bool supported,bool alive)const{return alive&&supported&&finite(body)&&std::isfinite(ground)&&std::hypot(body.x-x,body.z-z)<=reach&&std::abs(body.y-ground)<=.5;}
 double fraction(double remaining)const{return std::clamp(remaining/quantity,0.0,1.0);}
};
}
