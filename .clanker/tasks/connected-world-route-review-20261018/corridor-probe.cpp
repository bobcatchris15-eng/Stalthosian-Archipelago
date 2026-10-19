#include "domain/connected_route.hpp"
#include "domain/rendered_ground_surface.hpp"
#include <iostream>
#include <cmath>
int main(){stalthos::world::ConnectedRoute r(42);double maxCore=0,maxBlend=0,maxJoin=0,maxJump=0;double coreX=0,coreZ=0,blendX=0,blendZ=0;
for(double x=96;x<=280;x+=2)for(double z=-200;z<=248;z+=2){const double h=.1,dx=(r.height(x+h,z)-r.height(x-h,z))/(2*h),dz=(r.height(x,z+h)-r.height(x,z-h))/(2*h),g=std::hypot(dx,dz);if(std::abs(z-24)<=12&&g>maxCore){maxCore=g;coreX=x;coreZ=z;}if(g>maxBlend){maxBlend=g;blendX=x;blendZ=z;}maxJump=std::max(maxJump,std::abs(r.height(x,z+2)-r.height(x,z)));}
for(double z=-200;z<=248;z+=.5)maxJoin=std::max(maxJoin,std::abs(r.height(96+1e-5,z)-r.height(96-1e-5,z)));
std::cout<<"core_max_gradient="<<maxCore<<" at="<<coreX<<','<<coreZ<<"\nfull_blend_max_gradient="<<maxBlend<<" at="<<blendX<<','<<blendZ<<"\njoin_epsilon_jump="<<maxJoin<<"\nmax_2m_lateral_height_change="<<maxJump<<'\n';
for(double x:{96.,112.,128.,160.,200.,240.,280.})std::cout<<"center_y "<<x<<' '<<r.height(x,24)<<'\n';
double mismatch=0;std::size_t tested=0;
for(double x=96;x<240;x+=8)for(double z=-40;z<80;z+=8){
std::vector<stalthos::domain::SurfaceVertex> v;for(auto p:std::vector<std::pair<double,double>>{{x,z},{x+2,z},{x,z+2},{x+2,z+2}})v.push_back({p.first,double(float(r.height(p.first,p.second))),p.second});
stalthos::domain::RenderedGroundSurface mesh;if(!mesh.build(v,std::vector<std::uint32_t>{0,1,2,1,3,2}))return 2;
for(auto f:std::vector<std::pair<double,double>>{{.13,.39},{.89,.74},{0,0},{1,1},{.5,.5}}){const auto px=x+f.first*2,pz=z+f.second*2;mismatch=std::max(mismatch,std::abs(mesh.sampleHeight(px,pz)-r.triangleHeight(px,pz)));++tested;}}
std::cout<<"independent_float_mesh_vs_triangle_helper_samples="<<tested<<" max_error_m="<<mismatch<<'\n';return mismatch<.00001?0:3;}
