#pragma once
#include "fauna_population.hpp"
#include <functional>
namespace stalthos::domain {
struct HatchlingPath {std::vector<Vec3> points;std::size_t attach=0;};
// Host supplies rendered ground and dry eligibility, never camera residency.
// Half-meter segments retain terrain support in authoritative graph positions.
inline std::optional<HatchlingPath> hatchlingPath(Vec3 nest,const std::vector<fauna::HabitatSite>& sites,const std::function<std::optional<double>(double,double)>& dryGround){
 if(!finite(nest))return std::nullopt;
 std::vector<std::pair<double,std::size_t>> candidates;
 for(std::size_t i=0;i<sites.size();++i){const double d=std::hypot(nest.x-sites[i].position.x,nest.z-sites[i].position.z);if(d>=.1 && d<=96)candidates.push_back({d,i});}
 std::sort(candidates.begin(),candidates.end());
 for(const auto& c:candidates){const auto end=sites[c.second].position;const auto steps=std::size_t(std::ceil(c.first/.5));HatchlingPath path{{},c.second};bool valid=true;Vec3 previous=nest;
  for(std::size_t i=0;i<=steps;++i){const double f=double(i)/steps,x=nest.x+(end.x-nest.x)*f,z=nest.z+(end.z-nest.z)*f;const auto ground=dryGround(x,z);if(!ground || !std::isfinite(*ground)){valid=false;break;}const Vec3 point{x,*ground,z};
   if(i==0 && std::abs(*ground-nest.y)>.1){valid=false;break;}
   if(i && std::abs(point.y-previous.y)>.6*std::hypot(point.x-previous.x,point.z-previous.z)){valid=false;break;}
   if(i==steps){if(std::abs(point.y-end.y)>.01)valid=false;}else path.points.push_back(point);previous=point;
  }
  if(valid)return path;
 }
 return std::nullopt;
}
}
