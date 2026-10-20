#pragma once
#include "peer_companionship.hpp"
namespace stalthos::fauna {
// One retained pair for this native session. No serialized identity or population copy.
class NpcPeerAssociation {
    PeerCompanionship link_;
    Identity leader_=0;
    double time_=0;
public:
    explicit NpcPeerAssociation(PeerTuning tuning={}):link_(tuning){}
    void update(const std::vector<ActorView>& actors,double seconds) {
        if(!std::isfinite(seconds)||seconds<0)throw std::invalid_argument("invalid NPC peer time");
        time_+=seconds;
        if(!leader_) {
            for(const auto& a:actors) if(PeerCompanionship::eligible(a,a.species))
                for(const auto& b:actors) if(a.id<b.id && PeerCompanionship::eligible(b,a.species) &&
                    std::hypot(a.position.x-b.position.x,a.position.z-b.position.z)<link_.tuning().encounterMeters &&
                    (!leader_ || a.id<leader_))leader_=a.id;
        }
        const auto at=std::find_if(actors.begin(),actors.end(),[&](const ActorView& a){return a.id==leader_;});
        if(at==actors.end() || !at->alive)return;
        link_.update(leader_,at->species,at->position,PeerCompanionship::eligible(*at,at->species),seconds,time_,actors);
    }
    std::optional<NpcPeerMotion> motion(const std::vector<ActorView>& actors,Identity heroPeer=0) const {
        const auto at=std::find_if(actors.begin(),actors.end(),[&](const ActorView& a){return a.id==leader_;});
        if(at==actors.end() || !PeerCompanionship::eligible(*at,at->species) || !link_.motionPeer() ||
           heroPeer==leader_ || heroPeer==link_.peer())return std::nullopt;
        return NpcPeerMotion{leader_,link_.peer(),at->position,link_.tuning().waitMeters,.7};
    }
    Identity waitingFollower(const std::vector<ActorView>& actors,Identity heroPeer=0,const std::optional<BodyAwareness>& body=std::nullopt)const {
        const auto command=motion(actors,heroPeer);if(!command)return 0;
        for(const auto& a:actors)if(a.id==command->follower && (!body || !awareness(a.position,*body).avoid) && std::hypot(a.position.x-command->leaderBody.x,a.position.z-command->leaderBody.z)<=command->waitMeters+1e-6)return a.id;
        return 0;
    }
    Identity leader()const{return leader_;}
    const PeerCompanionship& link()const{return link_;}
};
}
