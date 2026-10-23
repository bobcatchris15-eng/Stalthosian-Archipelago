#pragma once
#include "peer_companionship.hpp"
namespace stalthos::domain {
enum class ReedLeader { None, Hero, Partner };
struct ReedPeerDecision {ReedLeader leader=ReedLeader::None;std::optional<Vec3> heroTarget;};
// One already-learned conspecific peer; no camera, teleport, new identity or AI tick.
inline ReedPeerDecision reedPeerDecision(Vec3 body,bool alive,bool direct,bool playerDestination,bool heroNeedsPriority,const fauna::ActorView* peer){
 if(!alive || !finite(body) || !peer || !peer->id || !fauna::PeerCompanionship::eligible(*peer,HeroSpecies::Reedcrest) || !finite(peer->position) || std::abs(peer->position.y-body.y)>2)return {};
 const double dx=body.x-peer->position.x,dz=body.z-peer->position.z,d=std::hypot(dx,dz);if(d>=18)return {};
 if(direct || playerDestination || heroNeedsPriority)return {ReedLeader::Hero,std::nullopt};
 if(d<=3.5)return {ReedLeader::Partner,std::nullopt};
 return {ReedLeader::Partner,Vec3{peer->position.x+dx/d*3,peer->position.y,peer->position.z+dz/d*3}};
}
}
