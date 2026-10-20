#pragma once
#include "domain/regional_population.hpp"
#include <limits>
#include "domain/npc_peer_association.hpp"
namespace stalthos::engine {
class NativePopulationRegistry {
    fauna::RegionalPopulationRegistry registry_;
    RegionId region_;
    fauna::NpcPeerAssociation npcPeers_;
    std::vector<fauna::ActorView> actors_;
    std::vector<fauna::RegionalContactEvent> events_;
    double food_=0,water_=0;
    std::uint64_t rejected_=0;
    static constexpr auto physical=fauna::RegionalAdmissionOrigin::PhysicalArrival;
    void refreshViews() {
        actors_.clear();
        const auto state=snapshot();
        for(const auto& a:state.actors) actors_.push_back({a.id,a.species,a.position,a.headingRadians,a.calendarAge,a.distanceTravelledMeters,a.hunger,a.thirst,a.fatigue,a.stage!=LifeStage::Dead,a.activity});
    }
public:
    NativePopulationRegistry(fauna::Population population,const ResourceLedgerDto& ledger,
        const std::vector<std::pair<Identity,std::uint64_t>>& saved,
        const std::vector<fauna::RegionalResourceAnchor>& anchors,std::uint64_t& nextSession)
        :registry_(std::move(population),ledger,{4096,256},saved),region_(registry_.legacyRegion()) {
        if(!registry_.configureContacts(region_,anchors) || !registry_.setActive(region_,true,physical)) throw std::runtime_error("native registry contact configuration rejected");
        const auto bindings=sessions();
        const auto state=snapshot();
        for(const auto& actor:state.actors) {
            if(actor.stage==LifeStage::Dead || std::any_of(bindings.begin(),bindings.end(),[&](const auto& b){return b.first==actor.id;})) continue;
            if(!nextSession || nextSession==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("native registry interaction session overflow");
            if(!registry_.bindContactSession(region_,actor.id,nextSession,physical)) throw std::runtime_error("native registry session admission rejected");
            ++nextSession;
        }
        refreshViews();
    }
    explicit NativePopulationRegistry(fauna::RegionalPopulationRegistry restored)
        :registry_(std::move(restored)),region_(registry_.legacyRegion()) {refreshViews();}
    const fauna::NpcPeerAssociation& npcPeers()const{return npcPeers_;}
    const fauna::RegionalPopulationRegistry& registry() const {return registry_;}
    fauna::RegionalContinuationDto continuation() const {return registry_.continuationSnapshot();}
    fauna::PopulationSnapshot snapshot() const {return registry_.snapshot(region_)->population;}
    ResourceLedgerDto ledger() const {return registry_.snapshot(region_)->ledger;}
    fauna::RegionalCapacity capacity() const {return registry_.capacity();}
    const std::vector<fauna::ActorView>& actors() const {return actors_;}
    std::vector<std::pair<Identity,std::uint64_t>> sessions() const {
        std::vector<std::pair<Identity,std::uint64_t>> result;
        const auto state=registry_.snapshot(region_);
        for(const auto& b:state->bindings) result.emplace_back(b.actor,b.session);
        return result;
    }
    void advance(const TimeStep& dt,bool active,const std::optional<fauna::BodyAwareness>& bodyAwareness=std::nullopt,bool allowNpcPeers=false) {
        if(allowNpcPeers && active && dt.physicalSeconds>0)npcPeers_.update(actors_,dt.physicalSeconds);
        const auto npcMotion=allowNpcPeers && active?npcPeers_.motion(actors_,bodyAwareness?bodyAwareness->peer:0):std::nullopt;
        registry_.setActive(region_,active,physical);registry_.advance(region_,dt,bodyAwareness,npcMotion);events_.clear();
        if(active && dt.physicalSeconds>0) {
            const auto state=*registry_.snapshot(region_);
            for(const auto& actor:state.population.actors) {
                if(bodyAwareness && fauna::awareness(actor.position,*bodyAwareness).avoid) continue;
                if(actor.stage==LifeStage::Dead || (actor.activity!=fauna::Activity::Browse && actor.activity!=fauna::Activity::Drink)) continue;
                const auto kind=actor.activity==fauna::Activity::Browse?ResourceKind::Shrub:ResourceKind::Creek;
                const auto binding=std::find_if(state.bindings.begin(),state.bindings.end(),[&](const auto& b){return b.actor==actor.id;});
                if(binding==state.bindings.end()) continue;
                for(const auto& anchor:state.anchors) {
                    if(anchor.site!=actor.site) continue;
                    const auto current=ledger();
                    const auto resource=std::find_if(current.resources.begin(),current.resources.end(),[&](const auto& r){return r.id==anchor.resource && r.kind==kind;});
                    if(resource==current.resources.end()) continue;
                    double prior=0;
                    for(const auto& key:current.sessions) if(key.sessionId==binding->session && key.resourceId==anchor.resource) prior=key.amount;
                    const double delta=std::min((kind==ResourceKind::Shrub?.015:.01)*dt.physicalSeconds,resource->initialQuantity-resource->consumedQuantity);
                    if(!(delta>0)) continue;
                    const auto result=registry_.contact(region_,actor.id,binding->session,actor.site,anchor.resource,prior+delta,physical);
                    if(result.admission==ResourceAdmission::HistoryFull) ++rejected_;
                    if(result.admission==ResourceAdmission::StorageFailure) throw std::bad_alloc();
                    if(result.event) {events_.push_back(*result.event);if(kind==ResourceKind::Shrub) food_+=result.event->amount;else water_+=result.event->amount;}
                }
            }
        }
        refreshViews();
    }
    void advanceDormant(const TimeStep& dt) {advance(dt,false);}
    // Hero contact eligibility and needs belong to the host; this grants no NPC relief.
    bool consume(std::uint64_t session,std::uint64_t resource,double amount) {
        const auto state=*registry_.snapshot(region_);
        for(const auto& b:state.bindings) if(b.session==session) throw std::runtime_error("hero session aliases NPC ownership");
        for(const auto& key:state.ledger.sessions) if(key.sessionId==session && key.resourceId==resource) {
            if(key.amount==amount) return false;
            throw std::runtime_error("conflicting hero interaction replay");
        }
        const auto result=registry_.consumeCumulative(region_,session,resource,amount);
        if(result==ResourceAdmission::NewKey) return true;
        if(result==ResourceAdmission::Replay) return false;
        throw std::runtime_error("hero resource admission denied");
    }
    const std::vector<fauna::RegionalContactEvent>& events() const {return events_;}
    double consumedFood() const {return food_;}
    double consumedWater() const {return water_;}
    std::uint64_t historyRejections() const {return rejected_;}
};
}
