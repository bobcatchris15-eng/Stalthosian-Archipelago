#pragma once
#include "foundation.hpp"
namespace stalthos::domain {
struct BodyAudioTuning {double stepMeters=.06,intervalSeconds=.45,maxFrameStep=1;};
class BodyMotionAudio {
 BodyAudioTuning tuning_;std::optional<Vec3> previous_;double distance_=0,cooldown_=0;bool moving_=false;
public:
 explicit BodyMotionAudio(BodyAudioTuning tuning={}):tuning_(tuning){}
 bool moving()const{return moving_;}
 bool advance(Vec3 body,double dt,bool alive,bool supported){
  if(!finite(body)||!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid body audio context");
  moving_=false;cooldown_=std::max(0.0,cooldown_-dt);
  if(!alive||!supported){previous_=body;distance_=0;return false;}
  if(!previous_){previous_=body;return false;}
  const double d=std::hypot(body.x-previous_->x,body.z-previous_->z);previous_=body;
  if(d>tuning_.maxFrameStep){distance_=0;return false;}
  moving_=d>1e-5;if(!moving_){distance_=0;return false;}
  distance_+=d;if(distance_<tuning_.stepMeters||cooldown_>0)return false;
  distance_=0;cooldown_=tuning_.intervalSeconds;return true;
 }
};
}
