#pragma once
#include "watershed_terrain.hpp"
namespace stalthos::world {
// Editable layout in meters; connected preview uses existing world-space mesh datum.
struct ConnectedRouteLayout {
    double centerZ=24, dryBankX=24, joinX=96, meadowX=200;
    double openHalfWidth=72, bluffBlendWidth=144, gentleRise=.012;
};
class ConnectedRoute {
    WatershedTerrain base_;
    ConnectedRouteLayout layout_;
    static double smooth(double a,double b,double v) {const auto t=std::clamp((v-a)/(b-a),0.0,1.0);return t*t*(3-2*t);}
public:
    explicit ConnectedRoute(std::uint64_t seed,ConnectedRouteLayout layout={}):base_(seed,{64,2,1}),layout_(layout) {
        if(!std::isfinite(layout.centerZ) || !std::isfinite(layout.dryBankX) || !std::isfinite(layout.joinX) || !std::isfinite(layout.meadowX) || !std::isfinite(layout.openHalfWidth) || !std::isfinite(layout.bluffBlendWidth) || !std::isfinite(layout.gentleRise) || layout.joinX!=96 || layout.dryBankX<16 || layout.dryBankX>=layout.joinX || layout.meadowX<=layout.joinX || layout.openHalfWidth<=0 || layout.bluffBlendWidth<=0 || std::abs(layout.gentleRise)>.1) throw std::invalid_argument("invalid connected route layout");
    }
    const ConnectedRouteLayout& layout() const {return layout_;}
    double height(double x,double z) const {
        const auto existing=base_.height(x,z);
        if(x<=layout_.joinX) return existing;
        const double d=x-layout_.joinX;
        const double edge=base_.height(layout_.joinX,z);
        const double slope=(base_.height(layout_.joinX+1,z)-base_.height(layout_.joinX-1,z))/2;
        const double meadow=edge+slope*24*(1-std::exp(-d/24))+layout_.gentleRise*d*smooth(0,24,d)
            +.35*std::sin(d/55)*std::sin((z-layout_.centerZ)/37)*smooth(0,32,d);
        const double cross=std::abs(z-layout_.centerZ);
        const double weight=1-smooth(layout_.openHalfWidth,layout_.openHalfWidth+layout_.bluffBlendWidth,cross);
        return existing+(meadow-existing)*weight;
    }
    // Same two-meter triangles and diagonal as the emitted exterior mesh.
    double triangleHeight(double x,double z) const {
        const double left=std::floor(x/2)*2,back=std::floor(z/2)*2,fx=(x-left)/2,fz=(z-back)/2;
        const double a=height(left,back),b=height(left+2,back),c=height(left,back+2),d=height(left+2,back+2);
        return fx+fz<=1?a+(b-a)*fx+(c-a)*fz:d+(c-d)*(1-fx)+(b-d)*(1-fz);
    }
    TerrainVertex sample(double x,double z) const {
        const double h=1,dx=(height(x+h,z)-height(x-h,z))/(2*h),dz=(height(x,z+h)-height(x,z-h))/(2*h);
        const double inv=1/std::sqrt(1+dx*dx+dz*dz);return {{x,height(x,z),z},{-dx*inv,inv,-dz*inv},.35,false};
    }
};
}
