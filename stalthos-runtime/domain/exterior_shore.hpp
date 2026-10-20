#pragma once
#include "foundation.hpp"
#include <algorithm>
#include <cmath>
namespace stalthos::world {
// Authored connected-preview cove, not an inferred global island coastline.
struct ExteriorShoreLayout {double startX=200,endX=280,centerZ=24,seaLevel=0,depth=2,halfWidth=32,flare=.4,bankBlend=64;};
class ExteriorShore {
 ExteriorShoreLayout layout_;
 static double smooth(double t){t=std::clamp(t,0.0,1.0);return t*t*(3-2*t);}
public:
 explicit ExteriorShore(ExteriorShoreLayout layout={}):layout_(layout){
  for(double v:{layout.startX,layout.endX,layout.centerZ,layout.seaLevel,layout.depth,layout.halfWidth,layout.flare,layout.bankBlend})if(!std::isfinite(v))throw std::invalid_argument("nonfinite shore layout");
  if(layout.seaLevel!=0 || layout.startX<200 || layout.endX-layout.startX<64 || layout.endX>600 || layout.depth<=0 || layout.depth>5 || layout.halfWidth<12 || layout.bankBlend<32 || layout.flare<0 || layout.flare>1)throw std::invalid_argument("invalid bounded shore layout");
 }
 const ExteriorShoreLayout& layout()const{return layout_;}
 double height(double inland,double anchor,double x,double z)const {
  if(x<=layout_.startX)return inland;
  const double along=smooth((x-layout_.startX)/(layout_.endX-layout_.startX));
  const double half=layout_.halfWidth+std::min(200.0,(x-layout_.startX)*layout_.flare);
  const double across=1-smooth((std::abs(z-layout_.centerZ)-half)/layout_.bankBlend);
  const double bed=anchor+(layout_.seaLevel-layout_.depth-anchor)*along;
  return inland+(bed-inland)*across;
 }
};
}
