#include "domain/sapling_disturbance.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::domain;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
 StalthosState world(42);world.setHero(1,1);SaplingDisturbance tree;tree.position={148,2,40};
 require(!tree.apply(world,{128,2,40},true,true,3)&&world.regionSnapshot(1001).revision==0,"remote camera cannot apply");
 require(!tree.apply(world,{147,2,40},false,true,3)&&!tree.apply(world,{147,2,40},true,false,3),"dead and unsupported refuse");
 require(!tree.apply(world,{147,4,40},true,true,3),"vertical separation refuses");
 require(tree.apply(world,{146.5,2,40},true,true,3),"near body applies");
 const auto before=world.regionSnapshot(1001);require(before.revision==1&&before.trees.size()==1&&tree.exception(world)->condition==TreeCondition::StandingDamaged,"real exception recorded");
 require(!tree.apply(world,{147,2,40},true,true,5)&&world.regionSnapshot(1001).revision==1&&tree.exception(world)->changedAt==3,"repeat does not increment or rewrite history");
 world.heroDied();world.setHero(2,1);require(tree.exception(world)->tree==9006&&world.heroDeathKnown(1),"new individual keeps world change");
 require(!tree.eligible({128,2,40},true,true)&&tree.eligible({146.5,2,40},true,true),"departure and return retain eligibility truth");
 world.recordTree(1001,{9007,TreeCondition::StandingDamaged,9});require(tree.exception(world)->changedAt==3,"neighbor independent");
 std::cout<<"sapling body and world exception checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
