#pragma once
#include "archipelago_world.hpp"
#include "connected_route.hpp"
#include <optional>
namespace stalthos::world {
// Editable first-draft geography compression. Same shared drowned geological field,
// not four separately generated disks. Origin's existing habitats stay verbatim.
struct NativeIslandLayout {double horizontalScale=.1,verticalScale=.1;};
class NativeArchipelago {
    Archipelago field_;ConnectedRoute origin_;NativeIslandLayout layout_;
public:
    explicit NativeArchipelago(std::uint64_t seed,NativeIslandLayout layout={}):field_(seed),origin_(seed),layout_(layout){if(!std::isfinite(layout.horizontalScale)||layout.horizontalScale<.05||layout.horizontalScale>.2||!std::isfinite(layout.verticalScale)||layout.verticalScale<.05||layout.verticalScale>.2)throw std::invalid_argument("invalid draft island scale");}
    double bounds()const{return Archipelago::worldBoundsMeters*layout_.horizontalScale;}
    const NativeIslandLayout& layout()const{return layout_;}const Archipelago& field()const{return field_;}
    bool protectedOrigin(double x,double z)const{return x>=-96&&x<=300&&z>=-96&&z<=96;}
    double height(double x,double z)const{
        if(!std::isfinite(x)||!std::isfinite(z)||std::abs(x)>bounds()+384||std::abs(z)>bounds()+384)throw std::out_of_range("native draft archipelago bounds");
        const double outside=std::max({0.0,-96-x,x-300,std::abs(z)-96});if(outside==0)return origin_.height(x,z);
        double t=std::clamp(outside/96,0.0,1.0);t=t*t*(3-2*t);
        return origin_.height(x,z)*(1-t)+field_.elevation(std::clamp(x/layout_.horizontalScale,-13999.0,13999.0),std::clamp(z/layout_.horizontalScale,-13999.0,13999.0))*layout_.verticalScale*t;
    }
    double triangleHeight(double x,double z)const{const double a=std::floor(x/2)*2,b=std::floor(z/2)*2,u=(x-a)/2,v=(z-b)/2,p=height(a,b),q=height(a+2,b),r=height(a,b+2),s=height(a+2,b+2);return u+v<=1?p+(q-p)*u+(r-p)*v:s+(r-s)*(1-u)+(q-s)*(1-v);}
    TerrainVertex sample(double x,double z)const{const double dx=(height(x+1,z)-height(x-1,z))/2,dz=(height(x,z+1)-height(x,z-1))/2,inv=1/std::sqrt(1+dx*dx+dz*dz),y=height(x,z);return {{x,y,z},{-dx*inv,inv,-dz*inv},.35,y<=0};}
    RegionId landRegion(double x,double z,double actualBed)const{if(std::abs(x)<=96&&std::abs(z)<=96)return 1001;if(actualBed<=0)return 0;if(protectedOrigin(x,z))return 1001;RegionId id=0;double best=1e30;for(const auto& r:field_.regions()){const double d=std::hypot(x-r.centerX*layout_.horizontalScale,z-r.centerZ*layout_.horizontalScale)/r.referenceSpanMeters;if(d<best){best=d;id=r.id;}}return id;}
    // Deterministic diagnostic start on real Main shore; ordinary movement covers
    // the actual sea interval and lands on Stone. No intermediate relocation.
    struct Crossing {Vec3 start,end;double yaw=0,seaMeters=0,distance=0;};
    std::optional<Crossing> firstCrossing()const{
        const auto& stone=field_.regions()[2];const double ex=stone.centerX*layout_.horizontalScale,ez=stone.centerZ*layout_.horizontalScale,total=std::hypot(ex,ez),dx=ex/total,dz=ez/total;double enter=-1,exit=-1;
        for(double d=30;d<total;d+=.5){const double h=triangleHeight(dx*d,dz*d);if(enter<0&&h<-.05)enter=d;if(enter>0&&d>enter+10&&h>.4){exit=d;break;}}
        if(enter<0||exit<0)return std::nullopt;const double begin=std::max(0.0,enter-5),end=exit+6;return Crossing{{dx*begin,triangleHeight(dx*begin,dz*begin),dz*begin},{dx*end,triangleHeight(dx*end,dz*end),dz*end},std::atan2(dx,dz),exit-enter,end-begin};
    }
};
}
