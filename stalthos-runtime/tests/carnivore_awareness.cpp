#include "domain/carnivore_awareness.hpp"
#include "domain/hero_view_controller.hpp"
#include <iostream>
using namespace stalthos;
using namespace stalthos::fauna;
using namespace stalthos::domain;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    ActorView actor{42,HeroSpecies::Longstride,{8,0,0},0,7000,0,.2,.2,.2,true,Activity::Rest};
    std::vector<ActorView> actors{actor};CarnivoreAwareness cue;
    cue.update(1,{0,0,0},true,actors);check(cue.actor()==42 && cue.began(),"body detects retained adult identity");
    cue.update(1,{-6,0,0},true,actors);check(cue.actor()==42 && !cue.began(),"hysteresis retains encounter");
    cue.update(1,{-10,0,0},true,actors);check(!cue.actor(),"real separation clears cue");
    cue.update(1,{0,0,0},true,actors);check(cue.began(),"new encounter rearms recall");
    cue.update(1,{0,30,0},true,actors);check(!cue.actor(),"vertical separation is body local");
    cue.update(1,{40,0,0},true,actors);check(!cue.actor(),"remote body never learns from camera");
    actors[0].calendarAge=3000;cue.update(1,{0,0,0},true,actors);check(!cue.actor(),"juvenile excluded");
    actors[0]=actor;actors[0].alive=false;cue.update(1,{0,0,0},true,actors);check(!cue.actor(),"dead actor excluded");
    actors[0]=actor;cue.update(42,{0,0,0},true,actors);check(!cue.actor(),"self excluded");
    cue.update(1,{0,0,0},false,actors);check(!cue.actor(),"dead hero has no awareness");
    actors[0].species=HeroSpecies::Smalltrotter;cue.update(1,{0,0,0},true,actors);check(!cue.actor(),"herbivore excluded");
    actors[0]=actor;cue.update(1,{0,0,0},true,actors);cue.update(1,{0,0,0},true,{});check(!cue.actor(),"inactive population clears awareness");
    HeroViewController camera;camera.toggleDetachedMode({0,0,0});camera.setDetachedAnchor({20,0,0});camera.setTravelDestination({4,0,0});camera.requestDangerRecall();
    auto out=camera.update({.1,.1},{0,0,0},0);check(out.cameraState.target.x<20 && camera.dangerRecallActive(),"recall converges through sole controller");
    check(camera.getState().pendingDestination.has_value(),"recall leaves body intention responsive");
    camera.applyOrbitInput(1,0);check(!camera.dangerRecallActive(),"orbit immediately cancels");
    camera.requestDangerRecall();camera.applyZoomInput(1);check(!camera.dangerRecallActive(),"zoom immediately cancels");
    camera.requestDangerRecall();camera.applyDetachedPan(1,0,.1,{0,0,0});check(!camera.dangerRecallActive(),"pan immediately cancels");
    camera.requestDangerRecall();camera.update({0,0},{0,0,0},0);check(camera.dangerRecallActive(),"zero dt cannot complete recall");
    for(int i=0;i<100;++i)camera.update({.1,.1},{0,0,0},0);
    check(!camera.dangerRecallActive() && camera.getState().mode==CameraMode::TetheredFollow,"recall completes");
    camera.requestDangerRecall();check(!camera.dangerRecallActive(),"follow mode ignores recall request");
    camera.toggleDetachedMode({0,0,0});camera.setDetachedAnchor({20,0,0});camera.requestDangerRecall();
    try{camera.update({.1,.1},{0,0,0},0,[](double,double)->double{throw std::runtime_error("fixture terrain failure");});}catch(const std::runtime_error&){}
    check(camera.dangerRecallActive() && camera.getState().detachedAnchor.x==20,"failed camera update rolls recall back");
    camera.toggleDetachedMode({0,0,0});check(!camera.dangerRecallActive(),"mode action immediately cancels");
    std::cout<<"CARNIVORE_OK retained adult body-only awareness separation recall input cancellation rollback\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
