#include "domain/watershed_terrain.hpp"
#include <iostream>
int main(){stalthos::world::WatershedTerrain t(42,{64,2,1});for(int x=12;x<=320;x+=4) std::cout<<x<<","<<t.height(x,24)<<","<<(t.height(x+1,24)-t.height(x-1,24))/2<<"\n";}