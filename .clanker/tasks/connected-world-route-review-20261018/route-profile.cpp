#include "domain/watershed_terrain.hpp"
#include <iostream>
#include <iomanip>
int main(){stalthos::world::WatershedTerrain terrain(42,{64,2,1});std::cout<<"x,z,y,grade_to_next\n";for(double z:{0.,12.,24.,40.,60.})for(double x=12;x<=240;x+=2){const auto y=terrain.height(x,z),next=terrain.height(x+2,z);std::cout<<std::setprecision(12)<<x<<','<<z<<','<<y<<','<<(next-y)/2<<'\n';}}
