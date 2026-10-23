#include "domain/hero_view_controller.hpp"
#include <iostream>
int main(){using namespace stalthos;using namespace stalthos::domain;HeroViewController c;c.toggleDetachedMode({0,0,0});c.setDetachedAnchor({-20,0,0});c.requestDangerRecall();for(int i=1;i<=1200;++i){const double x=i*.1*.16128;c.update({.1,.1},{x,0,0},0);}auto s=c.getState();std::cout<<"recall="<<c.dangerRecallActive()<<" detached="<<(s.mode==CameraMode::LocallyDetached)<<" body="<<120*.16128<<" anchor="<<s.detachedAnchor.x<<" residual="<<120*.16128-s.detachedAnchor.x<<'\n';}

