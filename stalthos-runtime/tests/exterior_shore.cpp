#include "domain/connected_route.hpp"
#include <iostream>
int main(){try{
 stalthos::world::ConnectedRoute route(42);double maxGrade=0;
 for(double x=96;x<=200;x+=1)for(double z=-30;z<=64;z+=2)if(route.height(x,z)!=route.inlandHeight(x,z))throw std::runtime_error("accepted meadow changed");
 for(double x=184;x<258;x+=.25){const double a=route.triangleHeight(x,24),b=route.triangleHeight(x+.25,24);maxGrade=std::max(maxGrade,std::abs(b-a)/.25);if(a<=0 || maxGrade>.4)throw std::runtime_error("dry supported descent blocked");}
 if(route.height(300,24)>=0 || !route.sample(300,24).sea || route.height(258,24)<.1)throw std::runtime_error("missing sea-level shore distinction");
 auto before=route.shore().layout();before.endX=300;stalthos::world::ConnectedRoute tuned(42,{},before);if(tuned.height(250,24)==route.height(250,24))throw std::runtime_error("shore tuning ignored");
 std::cout<<"EXTERIOR_SHORE_OK preserved meadow dry184to258 sea300 max_grade="<<maxGrade<<" dry_edge_y="<<route.triangleHeight(258,24)<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what();return 1;}}
