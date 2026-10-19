#include "domain/connected_route.hpp"
#include <iostream>
int main(){try {
 stalthos::world::ConnectedRoute route(42);stalthos::world::WatershedTerrain old(42,{64,2,1});
 bool rejected=false;try {stalthos::world::ConnectedRouteLayout bad;bad.bluffBlendWidth=0;stalthos::world::ConnectedRoute invalid(42,bad);}catch(const std::invalid_argument&){rejected=true;}if(!rejected) throw std::runtime_error("invalid layout accepted");
 double grade=0;
 for(int z=-96;z<=96;z+=4) for(int x=-96;x<=96;x+=4) if(route.height(x,z)!=old.height(x,z)) throw std::runtime_error("original watershed changed");
 for(double x=24;x<=240;x+=.5) {const auto g=std::abs(route.triangleHeight(x+.5,24)-route.triangleHeight(x,24))/.5;grade=std::max(grade,g);if(g>.4) throw std::runtime_error("route too steep");}
 for(int z=-70;z<=96;z+=2) if(std::abs(route.height(96,z)-old.height(96,z))>1e-12) throw std::runtime_error("boundary gap");
 if(route.height(200,24)>15 || old.height(200,24)<60) throw std::runtime_error("wall not opened");
 std::cout<<"CONNECTED_ROUTE_OK max_grade="<<grade<<" meadow_y="<<route.height(200,24)<<" original_y="<<old.height(200,24)<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
