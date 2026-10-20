#pragma once
#include "fauna_population.hpp"
#include "relationships.hpp"
namespace stalthos::fauna {
struct PeerTuning {double encounterMeters=9,separationMeters=18,quietSeconds=1.5,waitMeters=3;};
class PeerCompanionship {
 relationships::RelationshipTracker links_{2,1};Identity peer_=0,candidate_=0;double quiet_=0;bool present_=false,available_=false;PeerTuning tuning_;std::optional<Vec3> previousBody_;
public:
 explicit PeerCompanionship(PeerTuning tuning={}):tuning_(tuning){}
 static bool eligible(const ActorView& a,HeroSpecies species){return a.alive && a.species==species && a.hunger<=.42 && a.thirst<=.48 && a.fatigue<=.62;}
 void update(Identity hero,HeroSpecies species,Vec3 body,bool calm,double dt,double time,const std::vector<ActorView>& actors){
  if(!hero||!finite(body)||!std::isfinite(dt)||dt<0||!std::isfinite(time)||time<0)throw std::invalid_argument("invalid peer body context");
  if(previousBody_ && dt>0 && std::hypot(body.x-previousBody_->x,body.z-previousBody_->z)/dt>.3)calm=false;previousBody_=body;
  if(!peer_){Identity selected=0;double best=tuning_.encounterMeters;if(calm)for(const auto& a:actors){double d=std::hypot(a.position.x-body.x,a.position.z-body.z);if(a.id!=hero && eligible(a,species) && d<best){selected=a.id;best=d;}}
   if(selected && selected==candidate_)quiet_+=dt;else{candidate_=selected;quiet_=selected?dt:0;}
   if(selected && quiet_>=tuning_.quietSeconds){links_.formPeerLink(hero,selected,time);peer_=selected;}
  }
  present_=false;available_=false;
  if(peer_)for(const auto& a:actors)if(a.id==peer_){present_=a.alive && std::hypot(a.position.x-body.x,a.position.z-body.z)<tuning_.separationMeters;available_=present_ && eligible(a,species);if(!a.alive)links_.observeDeath(hero,peer_,time);else if(present_)links_.observePresent(hero,peer_,time);else links_.observeSeparation(hero,peer_,time);break;}
 }
 Identity peer()const{return peer_;}Identity motionPeer()const{return available_?peer_:0;}bool present()const{return present_;}const relationships::RelationshipTracker& links()const{return links_;}const PeerTuning& tuning()const{return tuning_;}
};
}
