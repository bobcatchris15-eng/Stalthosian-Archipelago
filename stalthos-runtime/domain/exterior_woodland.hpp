#pragma once
#include "woodland_stands.hpp"
#include "foundation.hpp"
#include <array>
#include <functional>
namespace stalthos::domain {
// Proposed woodland edge in metres. Broad central lane leaves existing meadow resources south.
struct ExteriorWoodlandLayout {
 double entranceX=146, exitX=184, laneZ=47;
 std::array<Vec3,10> trees{{{154,0,38},{166,0,38},{178,0,38},{154,0,56},{166,0,56},{178,0,56},{148,0,40},{184,0,40},{148,0,54},{184,0,54}}};
};
inline std::vector<TreePlacement> exteriorWoodland(const std::function<double(double,double)>& ground,const ExteriorWoodlandLayout& layout={}) {
 std::vector<TreePlacement> result;
 for(std::size_t i=0;i<layout.trees.size();++i){const auto p=layout.trees[i];const double y=ground(p.x,p.z);if(!std::isfinite(y))throw std::runtime_error("exterior woodland needs resident rendered ground");result.push_back({9000+i,p.x,y,p.z,i*.7,1,i>=6,{}});}
 return result;
}
}
