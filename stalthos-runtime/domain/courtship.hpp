#pragma once
#include "fauna_population.hpp"
namespace stalthos::domain {
// Speculative voluntary pairing rule, not a sex/genetics or fertility model.
class SmalltrotterCourtship {
 Identity hero_=0,mate_=0;bool active_=false,ready_=false;double quiet_=0,elapsed_=0;
 std::optional<Vec3> previousHero_,previousMate_;std::string status_="B - court a nearby calm adult Smalltrotter before nesting";
 static bool eligible(const fauna::ActorView& a){const auto& p=getHeroProfile(HeroSpecies::Smalltrotter);return a.id && a.alive && a.species==HeroSpecies::Smalltrotter && finite(a.position) && a.calendarAge>=p.hatchlingDuration+p.juvenileDuration && a.calendarAge<p.hatchlingDuration+p.juvenileDuration+p.adultDuration && a.hunger<.42 && a.thirst<.48 && a.fatigue<.62;}
public:
 Identity mate()const{return mate_;}bool active()const{return active_;}bool ready()const{return active_&&ready_;}double quiet()const{return quiet_;}const std::string& status()const{return status_;}
 void cancel(const std::string& why){active_=false;ready_=false;quiet_=0;status_=why;}
 bool request(Identity hero,HeroSpecies species,LifeStage stage,Vec3 body,bool needsPermit,const std::vector<fauna::ActorView>& actors){
  if(!hero || species!=HeroSpecies::Smalltrotter || stage!=LifeStage::Adult || !finite(body) || !needsPermit){cancel("Courtship requires a comfortable living adult Smalltrotter");return false;}
  Identity selected=0;double best=12;for(const auto& a:actors)if(a.id!=hero && eligible(a)){const double d=std::hypot(std::hypot(a.position.x-body.x,a.position.z-body.z),a.position.y-body.y);if(d<best && std::abs(a.position.y-body.y)<1.5){selected=a.id;best=d;}}
  if(!selected){cancel("No nearby calm living adult Smalltrotter is ready to respond");return false;}
  hero_=hero;mate_=selected;active_=true;ready_=false;quiet_=0;elapsed_=0;previousHero_=body;previousMate_.reset();status_="Courtship invitation - mate approaching; stay quiet, WASD cancels";return true;
 }
 void advance(Identity hero,Vec3 body,bool alive,bool needsPermit,bool interrupted,double dt,const std::vector<fauna::ActorView>& actors){
  if(!active_)return;if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("invalid courtship time");
  if(hero!=hero_ || !alive || !needsPermit || interrupted || !finite(body)){cancel("Courtship interrupted - direct input, destination or ordinary needs");return;}
  elapsed_+=dt;const fauna::ActorView* partner=nullptr;for(const auto& a:actors)if(a.id==mate_)partner=&a;
  if(!partner || !eligible(*partner) || std::hypot(std::hypot(partner->position.x-body.x,partner->position.z-body.z),partner->position.y-body.y)>14 || elapsed_>40){cancel("Mate unavailable, departed or no longer calm - courtship ended");return;}
  const double distance=std::hypot(partner->position.x-body.x,partner->position.z-body.z);
  bool still=distance<=3.5 && std::abs(partner->position.y-body.y)<1;
  if(dt>0 && previousHero_ && std::hypot(body.x-previousHero_->x,body.z-previousHero_->z)/dt>.15)still=false;
  if(dt>0 && previousMate_ && std::hypot(partner->position.x-previousMate_->x,partner->position.z-previousMate_->z)/dt>.15)still=false;
  quiet_=still?quiet_+dt:0;ready_=quiet_>=2;previousHero_=body;previousMate_=partner->position;
  status_=ready_?"Mate responded and pair ready - choosing clear nest ground":still?"Mate nearby - quiet readiness response":"Courtship invitation - mate approaching; stay quiet";
 }
 bool currentMateReady(Vec3 body,const std::vector<fauna::ActorView>& actors)const{if(!ready() || !finite(body))return false;for(const auto& a:actors)if(a.id==mate_)return eligible(a) && std::hypot(a.position.x-body.x,a.position.z-body.z)<=3.5 && std::abs(a.position.y-body.y)<1;return false;}
 Identity acceptedMate(Identity hero)const{return hero==hero_ && ready()?mate_:0;}
 void complete(){active_=false;ready_=false;status_="Pair response accepted - clutch laid";}
};
}
