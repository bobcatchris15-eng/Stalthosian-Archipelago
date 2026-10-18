#pragma once

#include "domain/fauna_population.hpp"
#include "domain/resource_ledger.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace stalthos::engine {

// Bridges active fauna contacts to the world's existing resource ledger.
// Resource ids follow the host convention: creek 100 and shrubs 200 + input index.
class FaunaResources {
public:
    enum class Kind { Food, Water };
    struct Event {
        Identity actorId;
        std::uint64_t resourceId;
        Kind kind;
        double amount;
    };
    static constexpr double foodPerPhysicalSecond = 0.015;
    static constexpr double waterPerPhysicalSecond = 0.01;
    static constexpr double contactRadiusMeters = 0.8;

private:
    struct SiteResource {
        std::optional<std::uint64_t> food;
        bool water = false;
    };
    using ContactKey = std::pair<Identity, std::uint64_t>;

    std::vector<fauna::HabitatSite> sites_;
    std::vector<SiteResource> resourcesBySite_;
    std::uint64_t* nextSession_;
    std::map<Identity, std::uint64_t> sessions_;
    std::map<ContactKey, double> cumulative_;
    std::vector<Event> events_;
    double consumedFood_ = 0;
    double consumedWater_ = 0;
    bool historyRestored_ = false;

    static double distanceSquared(Vec3 a, Vec3 b) noexcept {
        const double x=a.x-b.x, y=a.y-b.y, z=a.z-b.z;
        return x*x+y*y+z*z;
    }
    static std::map<std::uint64_t, ResourceDto> resourceMap(const ResourceLedger& ledger) {
        std::map<std::uint64_t, ResourceDto> result;
        for (const auto& resource: ledger.snapshot()) result.emplace(resource.id,resource);
        return result;
    }
    void ensureSessions(const std::vector<fauna::ActorView>& actors) {
        for (const auto& actor: actors) {
            if (sessions_.count(actor.id)) continue;
            if (actor.id==0 || *nextSession_==0 || *nextSession_==std::numeric_limits<std::uint64_t>::max())
                throw std::overflow_error("fauna interaction session id unavailable");
            sessions_.emplace(actor.id,(*nextSession_)++);
        }
    }
    std::optional<std::pair<std::size_t,std::uint64_t>> contactAt(Vec3 position, Kind kind) const {
        if (!finite(position)) return std::nullopt;
        const double limit=contactRadiusMeters*contactRadiusMeters;
        double nearest=limit;
        std::optional<std::pair<std::size_t,std::uint64_t>> result;
        for (std::size_t i=0;i<sites_.size();++i) {
            std::optional<std::uint64_t> id;
            if (kind==Kind::Food) id=resourcesBySite_[i].food;
            else if (resourcesBySite_[i].water) id=100;
            if (!id) continue;
            const auto d=distanceSquared(position,sites_[i].position);
            if (d<=nearest) { nearest=d; result=std::make_pair(i,*id); }
        }
        return result;
    }

public:
    FaunaResources(const std::vector<fauna::HabitatSite>& sites,
                   const std::vector<Vec3>& shrubPositions,
                   std::uint64_t& nextInteractionSession,
                   const std::vector<std::pair<Identity,std::uint64_t>>& savedSessions = {})
        : sites_(sites), resourcesBySite_(sites.size()), nextSession_(&nextInteractionSession) {
        if(savedSessions.size()>fauna::Population::maxActors) throw std::invalid_argument("too many fauna resource sessions");
        std::set<std::uint64_t> used;
        for(const auto& item:savedSessions) {
            if(!item.first || !item.second || !used.insert(item.second).second || !sessions_.emplace(item).second)
                throw std::invalid_argument("invalid fauna resource session mapping");
        }
        if (shrubPositions.size()>3896) throw std::invalid_argument("fauna shrub resource ids exceed bounded range");
        for (const auto& p: shrubPositions) if (!finite(p)) throw std::invalid_argument("non-finite shrub resource position");
        for (std::size_t i=0;i<sites.size();++i) {
            if (!finite(sites[i].position)) throw std::invalid_argument("non-finite fauna resource site");
            if (sites[i].browse) {
                double nearest=contactRadiusMeters*contactRadiusMeters;
                for (std::size_t shrub=0;shrub<shrubPositions.size();++shrub) {
                    const auto d=distanceSquared(sites[i].position,shrubPositions[shrub]);
                    if (d<=nearest) {
                        nearest=d;
                        resourcesBySite_[i].food=200+static_cast<std::uint64_t>(shrub);
                    }
                }
                if (!resourcesBySite_[i].food) throw std::invalid_argument("browse site does not match a supplied shrub position");
            }
            resourcesBySite_[i].water=sites[i].freshwater;
        }
    }

    // Call before Population::advance so depleted contacts cannot be selected.
    void refreshAvailability(fauna::Population& population, const ResourceLedger& ledger) {
        if(!historyRestored_) {
            std::map<std::uint64_t,Identity> actorBySession;
            std::set<Identity> actorIds;
            for(const auto& actor:population.actors()) actorIds.insert(actor.id);
            for(const auto& entry:sessions_) {
                if(!actorIds.count(entry.first)) throw std::invalid_argument("resource session has no population actor");
                actorBySession.emplace(entry.second,entry.first);
            }
            for(const auto& entry:ledger.continuationSnapshot().sessions) {
                const auto actor=actorBySession.find(entry.sessionId);
                if(actor!=actorBySession.end()) cumulative_.emplace(ContactKey{actor->second,entry.resourceId},entry.amount);
            }
            historyRestored_=true;
        }
        ensureSessions(population.actors());
        const auto resources=resourceMap(ledger);
        for (std::size_t i=0;i<resourcesBySite_.size();++i) {
            bool food=false, water=false;
            if (resourcesBySite_[i].food) {
                const auto it=resources.find(*resourcesBySite_[i].food);
                food=it!=resources.end() && it->second.kind==ResourceKind::Shrub &&
                    it->second.consumedQuantity<it->second.initialQuantity;
            }
            if (resourcesBySite_[i].water) {
                const auto it=resources.find(100);
                water=it!=resources.end() && it->second.kind==ResourceKind::Creek &&
                    it->second.consumedQuantity<it->second.initialQuantity;
            }
            population.setSiteResources(i,food,water);
        }
    }

    // Call after Population::advance. Consumption requires an alive actor whose
    // current activity and position both match the corresponding real resource.
    void applyContacts(const std::vector<fauna::ActorView>& actors, double physicalSeconds,
                       ResourceLedger& ledger) {
        if (!std::isfinite(physicalSeconds)||physicalSeconds<0)
            throw std::invalid_argument("invalid fauna resource elapsed time");
        ensureSessions(actors);
        events_.clear();
        events_.reserve(actors.size());
        if (physicalSeconds==0) return;
        auto resources=resourceMap(ledger);
        for (const auto& actor: actors) {
            if (!actor.alive || (actor.activity!=fauna::Activity::Browse && actor.activity!=fauna::Activity::Drink)) continue;
            const auto kind=actor.activity==fauna::Activity::Browse ? Kind::Food : Kind::Water;
            const auto contact=contactAt(actor.position,kind);
            if (!contact) continue;
            const auto resourceId=contact->second;
            const auto resource=resources.find(resourceId);
            if (resource==resources.end()) continue;
            const auto expectedKind=kind==Kind::Food ? ResourceKind::Shrub : ResourceKind::Creek;
            if (resource->second.kind!=expectedKind) continue;
            const double remaining=resource->second.initialQuantity-resource->second.consumedQuantity;
            if (!(remaining>0)) continue;
            const double rate=kind==Kind::Food ? foodPerPhysicalSecond : waterPerPhysicalSecond;
            const double requested=rate*physicalSeconds;
            if (!std::isfinite(requested)) throw std::overflow_error("fauna resource amount overflow");
            const ContactKey key{actor.id,resourceId};
            const auto prior=cumulative_.find(key);
            const double oldTotal=prior==cumulative_.end()?0:prior->second;
            const double delta=std::min(requested,remaining);
            const double total=oldTotal+delta;
            if (!std::isfinite(total)) throw std::overflow_error("fauna cumulative resource amount overflow");
            const double totalFood=kind==Kind::Food?consumedFood_+delta:consumedFood_;
            const double totalWater=kind==Kind::Water?consumedWater_+delta:consumedWater_;
            if (!std::isfinite(totalFood)||!std::isfinite(totalWater)) throw std::overflow_error("fauna resource statistics overflow");
            const auto session=sessions_.at(actor.id);
            if (!ledger.consumeCumulative(session,resourceId,total)) continue;
            if (prior==cumulative_.end()) cumulative_.emplace(key,total);
            else prior->second=total;
            resource->second.consumedQuantity+=delta;
            Event event{actor.id,resourceId,kind,delta};
            events_.push_back(event);
            consumedFood_=totalFood; consumedWater_=totalWater;
        }
    }

    double consumedFood() const noexcept { return consumedFood_; }
    double consumedWater() const noexcept { return consumedWater_; }
    const std::vector<Event>& events() const noexcept { return events_; }
    std::vector<std::pair<Identity,std::uint64_t>> sessions() const {
        return {sessions_.begin(),sessions_.end()};
    }
};

} // namespace stalthos::engine
