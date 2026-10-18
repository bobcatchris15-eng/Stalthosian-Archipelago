#pragma once

#include "fauna_population.hpp"
#include "resource_ledger.hpp"
#include <map>
#include <optional>
#include <utility>

namespace stalthos::fauna {

// This registry owns its ledgers. It does not enforce budgets on an unrelated
// host ledger; integration must transfer ownership of every retained key.
enum class RegionalAdmissionOrigin { PhysicalArrival, CameraPrewarm };
enum class RegionalAdmission {
    Accepted, CameraOnly, InvalidOrigin, DuplicateRegion, RegionLimit,
    IndividualLimit, IdentityConflict, ResourceConflict, ReservationLimit,
    InvalidReservation, EmptyPopulation, SeedConflict, StorageFailure, SessionConflict
};
struct RegionalReservation {
    std::size_t resources = 0;
    std::size_t historyKeys = 0;
};
struct RegionalCapacity {
    std::size_t regions = 0, individuals = 0, resources = 0, historyKeys = 0;
};
// An anchor associates one existing ledger resource with a real habitat site.
// Positions are supplied by world construction, never by a contact request.
struct RegionalResourceAnchor {
    std::size_t site = 0;
    std::uint64_t resource = 0;
    Vec3 position{};
};
struct RegionalContactBinding {
    Identity actor = 0;
    std::uint64_t session = 0;
};
struct RegionalContactEvent {
    Identity actor = 0;
    std::uint64_t resource = 0;
    ResourceKind kind = ResourceKind::Shrub;
    double amount = 0;
};
struct RegionalContactResult {
    ResourceAdmission admission = ResourceAdmission::InvalidRequest;
    std::optional<RegionalContactEvent> event;
};
struct RetainedRegionSnapshot {
    PopulationSnapshot population;
    ResourceLedgerDto ledger;
    RegionalReservation reservation;
    bool active = false;
    bool contactsConfigured = false;
    std::vector<RegionalResourceAnchor> anchors;
    std::vector<RegionalContactBinding> bindings;
};
// Detached in-memory continuation, deliberately not a disk save representation.
// Restore requires the same already-admitted immutable regional world layout.
struct RegionalContinuationDto {
    std::uint64_t seed = 0;
    RegionId legacyRegion = 0;
    std::vector<RetainedRegionSnapshot> regions;
};

class RegionalPopulationRegistry {
public:
    static constexpr std::size_t maxRegions = 8;
    static constexpr std::size_t maxIndividuals = 64;
    static constexpr std::size_t maxResources = 4096;
    static constexpr std::size_t maxHistoryKeys = 256;
private:
    struct Entry {
        Population population;
        ResourceLedger ledger;
        RegionalReservation reservation;
        bool active = false;
        bool contactsConfigured = false;
        std::vector<RegionalResourceAnchor> anchors;
        std::map<Identity, std::uint64_t> sessions;
        std::set<std::uint64_t> importedSessions;
        std::map<Identity, std::uint64_t> importedBindings;
        ResourceLedgerDto importedBaseline;
        Entry(Population p, ResourceLedger l, RegionalReservation r)
            : population(std::move(p)), ledger(std::move(l)), reservation(r), importedBaseline(ledger.continuationSnapshot()) {
            for (const auto& key:importedBaseline.sessions) importedSessions.insert(key.sessionId);
        }
    };
    std::map<RegionId, Entry> regions_;
    std::uint64_t seed_;
    RegionId legacyRegion_;
    RegionalCapacity used_{};

    RegionalAdmission insert(Population population, const ResourceLedgerDto& history,
                             RegionalReservation reservation,
                             const std::vector<std::pair<Identity,std::uint64_t>>& savedSessions = {},
                             bool allowEmptyLegacy = false) {
        const auto candidate = population.snapshot();
        if (regions_.count(candidate.region)) return RegionalAdmission::DuplicateRegion;
        if (candidate.seed != seed_) return RegionalAdmission::SeedConflict;
        if (candidate.actors.empty() && !allowEmptyLegacy) return RegionalAdmission::EmptyPopulation;
        if (regions_.size() >= maxRegions) return RegionalAdmission::RegionLimit;
        if (candidate.actors.size() > maxIndividuals - used_.individuals)
            return RegionalAdmission::IndividualLimit;
        if (reservation.resources > maxResources || reservation.historyKeys > maxHistoryKeys ||
            reservation.resources < history.resources.size() || reservation.historyKeys < history.sessions.size())
            return RegionalAdmission::InvalidReservation;
        if (reservation.resources > maxResources - used_.resources ||
            reservation.historyKeys > maxHistoryKeys - used_.historyKeys)
            return RegionalAdmission::ReservationLimit;
        // Explicit trusted saved provenance is accepted only during admission.
        // Empty-history sessions are valid: the host allocates before first contact.
        if (savedSessions.size()>candidate.actors.size()) return RegionalAdmission::SessionConflict;
        std::map<Identity,std::uint64_t> bindings;
        std::set<std::uint64_t> uniqueSessions;
        for (const auto& binding:savedSessions) {
            if (!binding.first || !binding.second ||
                !std::any_of(candidate.actors.begin(),candidate.actors.end(),
                    [&](const ActorSnapshot& a) { return a.id==binding.first; }) ||
                !bindings.emplace(binding).second || !uniqueSessions.insert(binding.second).second)
                return RegionalAdmission::SessionConflict;
        }
        ResourceLedger ledger(reservation.resources, reservation.historyKeys);
        if (!ledger.restore(history)) return RegionalAdmission::InvalidReservation;
        for (const auto& entry : regions_) {
            for (const auto session:uniqueSessions) {
                for (const auto& binding:entry.second.sessions)
                    if (binding.second==session) return RegionalAdmission::SessionConflict;
                for (const auto& key:entry.second.ledger.continuationSnapshot().sessions)
                    if (key.sessionId==session) return RegionalAdmission::SessionConflict;
            }
            for (const auto& binding:entry.second.sessions)
                for (const auto& key:history.sessions)
                    if (binding.second==key.sessionId) return RegionalAdmission::SessionConflict;
            for (const auto& actor : entry.second.population.actors())
                for (const auto& other : candidate.actors)
                    if (actor.id == other.id) return RegionalAdmission::IdentityConflict;
            for (const auto& resource : entry.second.ledger.snapshot())
                for (const auto& other : history.resources)
                    if (resource.id == other.id) return RegionalAdmission::ResourceConflict;
        }
        // All validation and allocations happen before usage counters commit.
        // No erasure or replacement API exists: retained identities and keys stay.
        Entry next(std::move(population), std::move(ledger), reservation);
        next.sessions=bindings; next.importedBindings=std::move(bindings);
        regions_.emplace(candidate.region, std::move(next));
        ++used_.regions;
        used_.individuals += candidate.actors.size();
        used_.resources += reservation.resources;
        used_.historyKeys += reservation.historyKeys;
        return RegionalAdmission::Accepted;
    }
    static double distanceSquared(Vec3 a, Vec3 b) noexcept {
        const double x=a.x-b.x, y=a.y-b.y, z=a.z-b.z;
        return x*x+y*y+z*z;
    }
    static void refreshAvailability(Entry& entry) {
        if (!entry.contactsConfigured) return;
        std::map<std::uint64_t,ResourceDto> resources;
        for (const auto& resource:entry.ledger.snapshot()) resources.emplace(resource.id,resource);
        std::vector<std::pair<bool,bool>> available(entry.population.sites().size(),{false,false});
        for (const auto& anchor:entry.anchors) {
            const auto found=resources.find(anchor.resource);
            if (found!=resources.end() && found->second.consumedQuantity<found->second.initialQuantity) {
                if (found->second.kind==ResourceKind::Shrub) available[anchor.site].first=true;
                else available[anchor.site].second=true;
            }
        }
        for (std::size_t site=0; site<available.size(); ++site)
            entry.population.setSiteResources(site,available[site].first,available[site].second);
    }
public:
    static constexpr double contactRadiusMeters = 0.8;
    // Bootstrap is mandatory: the original population and ALL retained legacy
    // history are imported unchanged. Used history counts inside its reservation,
    // never once as used keys and again as reserved keys.
    RegionalPopulationRegistry(Population legacy, const ResourceLedgerDto& history,
                               RegionalReservation reservation,
                               const std::vector<std::pair<Identity,std::uint64_t>>& savedSessions = {})
        : seed_(legacy.snapshot().seed), legacyRegion_(legacy.snapshot().region) {
        if (insert(std::move(legacy), history, reservation, savedSessions, true) != RegionalAdmission::Accepted)
            throw std::invalid_argument("invalid retained legacy population/reservation");
    }
    RegionalAdmission admit(RegionalAdmissionOrigin origin, Population population,
                            const ResourceLedgerDto& history, RegionalReservation reservation,
                            const std::vector<std::pair<Identity,std::uint64_t>>& savedSessions = {}) {
        if (origin == RegionalAdmissionOrigin::CameraPrewarm) return RegionalAdmission::CameraOnly;
        if (origin != RegionalAdmissionOrigin::PhysicalArrival) return RegionalAdmission::InvalidOrigin;
        try { return insert(std::move(population), history, reservation, savedSessions); }
        catch (const std::bad_alloc&) { return RegionalAdmission::StorageFailure; }
    }
    RegionalCapacity capacity() const noexcept { return used_; }
    RegionId legacyRegion() const noexcept { return legacyRegion_; }
    std::optional<RetainedRegionSnapshot> snapshot(RegionId region) const {
        const auto found = regions_.find(region);
        if (found == regions_.end()) return std::nullopt;
        const auto& entry = found->second;
        RetainedRegionSnapshot out;
        out.population=entry.population.snapshot(); out.ledger=entry.ledger.continuationSnapshot();
        out.reservation=entry.reservation; out.active=entry.active;
        out.contactsConfigured=entry.contactsConfigured; out.anchors=entry.anchors;
        for (const auto& session:entry.sessions) out.bindings.push_back({session.first,session.second});
        return out;
    }
    // Physical activation preserves routes/needs. Camera queries only read snapshots.
    // Activation grants no perception or resource relief by itself.
    bool setActive(RegionId region, bool active, RegionalAdmissionOrigin origin) noexcept {
        if (origin != RegionalAdmissionOrigin::PhysicalArrival) return false;
        const auto found = regions_.find(region);
        if (found == regions_.end()) return false;
        found->second.active = active;
        return true;
    }
    bool advance(RegionId region, const TimeStep& dt) {
        const auto found = regions_.find(region);
        if (found == regions_.end()) return false;
        if (!std::isfinite(dt.physicalSeconds) || dt.physicalSeconds<0 ||
            !std::isfinite(dt.calendarSeconds) || dt.calendarSeconds<0)
            throw std::invalid_argument("invalid regional time step");
        auto& entry = found->second;
        if (entry.active) {
            refreshAvailability(entry);
            entry.population.advance(dt, false);
        }
        else entry.population.advanceDormant(dt);
        return true;
    }
    // Multiple contact sites may share one resource (for example creek banks).
    // Each site still has at most one anchor per kind; global anchor count is
    // bounded by the same resource reservation. Configuration is immutable and validates anchors against
    // the population's actual habitat and the registry's existing resource kinds.
    bool configureContacts(RegionId region, const std::vector<RegionalResourceAnchor>& anchors) {
        const auto found=regions_.find(region);
        if (found==regions_.end() || found->second.contactsConfigured) return false;
        auto& entry=found->second;
        if (anchors.size()>entry.reservation.resources) return false;
        const auto resources=entry.ledger.snapshot();
        std::set<std::pair<std::size_t,ResourceKind>> slots;
        for (const auto& anchor:anchors) {
            if (anchor.site>=entry.population.sites().size() || !finite(anchor.position) ||
                distanceSquared(anchor.position,entry.population.sites()[anchor.site].position)>
                    contactRadiusMeters*contactRadiusMeters) return false;
            const auto resource=std::find_if(resources.begin(),resources.end(),[&](const ResourceDto& r) { return r.id==anchor.resource; });
            if (resource==resources.end()) return false;
            const auto& site=entry.population.sites()[anchor.site];
            if ((resource->kind==ResourceKind::Shrub ? !site.browse : !site.freshwater) ||
                !slots.emplace(anchor.site,resource->kind).second) return false;
        }
        auto copy=anchors; // allocate before committing configuration
        entry.anchors.swap(copy); entry.contactsConfigured=true;
        return true;
    }
    bool bindContactSession(RegionId region, Identity actor, std::uint64_t session,
                            RegionalAdmissionOrigin origin) {
        if (origin!=RegionalAdmissionOrigin::PhysicalArrival || !session) return false;
        const auto found=regions_.find(region);
        if (found==regions_.end() || !found->second.active || !found->second.contactsConfigured) return false;
        auto& entry=found->second;
        const auto actual=std::find_if(entry.population.actors().begin(),entry.population.actors().end(),
            [&](const ActorView& a) { return a.id==actor && a.alive; });
        if (actual==entry.population.actors().end()) return false;
        const auto prior=entry.sessions.find(actor);
        if (prior!=entry.sessions.end()) return prior->second==session;
        // Never claim an imported legacy key, another actor, or another region's
        // session. Old unbound keys remain complete, bookkeeping-only history.
        for (const auto& regionEntry:regions_) {
            if (regionEntry.second.importedSessions.count(session)) return false;
            for (const auto& bound:regionEntry.second.sessions) if (bound.second==session) return false;
            for (const auto& key:regionEntry.second.ledger.continuationSnapshot().sessions)
                if (key.sessionId==session) return false;
        }
        entry.sessions.emplace(actor,session);
        return true;
    }
    RegionalContactResult contact(RegionId region, Identity actor, std::uint64_t session,
                                  std::size_t site, std::uint64_t resource, double cumulativeTotal,
                                  RegionalAdmissionOrigin origin) {
        RegionalContactResult out;
        if (origin!=RegionalAdmissionOrigin::PhysicalArrival) return out;
        const auto found=regions_.find(region);
        if (found==regions_.end()) return out;
        auto& entry=found->second;
        if (!entry.active || !entry.contactsConfigured) return out;
        const auto binding=entry.sessions.find(actor);
        if (binding==entry.sessions.end() || binding->second!=session) return out;
        const auto actors=entry.population.snapshot();
        const auto actual=std::find_if(actors.actors.begin(),actors.actors.end(),
            [&](const ActorSnapshot& a) { return a.id==actor; });
        if (actual==actors.actors.end() || actual->stage==LifeStage::Dead || actual->site!=site) return out;
        const auto anchor=std::find_if(entry.anchors.begin(),entry.anchors.end(),
            [&](const RegionalResourceAnchor& a) { return a.site==site && a.resource==resource; });
        if (anchor==entry.anchors.end() || distanceSquared(actual->position,anchor->position)>
            contactRadiusMeters*contactRadiusMeters) return out;
        const auto history=entry.ledger.continuationSnapshot();
        const auto record=std::find_if(history.resources.begin(),history.resources.end(),
            [&](const ResourceDto& r) { return r.id==resource; });
        if (record==history.resources.end() ||
            actual->activity!=(record->kind==ResourceKind::Shrub ? Activity::Browse : Activity::Drink)) return out;
        double prior=0;
        for (const auto& key:history.sessions) if (key.sessionId==session && key.resourceId==resource) prior=key.amount;
        out.admission=entry.ledger.tryConsumeCumulative(session,resource,cumulativeTotal);
        if (out.admission==ResourceAdmission::NewKey || out.admission==ResourceAdmission::Update) {
            const double delta=cumulativeTotal-prior;
            entry.population.applyAcceptedResourceContact(actor,record->kind==ResourceKind::Shrub,
                delta*(record->kind==ResourceKind::Shrub ? 0.1 : 0.2));
            out.event=RegionalContactEvent{actor,resource,record->kind,delta};
        }
        return out;
    }
    RegionalContinuationDto continuationSnapshot() const {
        RegionalContinuationDto out{seed_,legacyRegion_,{}};
        for (const auto& entry:regions_) out.regions.push_back(*snapshot(entry.first));
        return out;
    }
    // Validate on a detached copy; even a late invalid binding leaves all live
    // populations, ledger histories, availability and bindings unchanged.
    bool restoreContinuation(const RegionalContinuationDto& dto) {
        if (dto.seed!=seed_ || dto.legacyRegion!=legacyRegion_ || dto.regions.size()!=regions_.size()) return false;
        try {
            auto next=*this;
            std::set<RegionId> regions;
            std::set<Identity> actors;
            std::map<std::uint64_t, std::pair<RegionId,Identity>> sessions;
            for (const auto& saved:dto.regions) {
                const auto found=next.regions_.find(saved.population.region);
                if (found==next.regions_.end() || !regions.insert(saved.population.region).second) return false;
                auto& entry=found->second;
                if (saved.reservation.resources!=entry.reservation.resources ||
                    saved.reservation.historyKeys!=entry.reservation.historyKeys ||
                    saved.contactsConfigured!=entry.contactsConfigured || saved.anchors.size()!=entry.anchors.size() ||
                    saved.bindings.size()>entry.population.actors().size()) return false;
                for (std::size_t i=0;i<saved.anchors.size();++i) {
                    const auto& a=saved.anchors[i]; const auto& b=entry.anchors[i];
                    if (a.site!=b.site || a.resource!=b.resource || a.position.x!=b.position.x ||
                        a.position.y!=b.position.y || a.position.z!=b.position.z) return false;
                }
                const auto original=entry.population.snapshot();
                if (saved.population.actors.size()!=original.actors.size()) return false;
                std::set<Identity> identities;
                for (const auto& a:original.actors) identities.insert(a.id);
                for (const auto& a:saved.population.actors) {
                    const auto admitted=std::find_if(original.actors.begin(),original.actors.end(),
                        [&](const ActorSnapshot& existing) { return existing.id==a.id; });
                    if (!identities.erase(a.id) || !actors.insert(a.id).second ||
                        admitted==original.actors.end() || admitted->species!=a.species) return false;
                }
                const auto resources=entry.ledger.snapshot();
                if (saved.ledger.resources.size()!=resources.size()) return false;
                for (const auto& r:saved.ledger.resources) {
                    const auto old=std::find_if(resources.begin(),resources.end(),[&](const ResourceDto& v) { return v.id==r.id; });
                    if (old==resources.end() || old->kind!=r.kind || old->initialQuantity!=r.initialQuantity) return false;
                }
                // Admission history is a permanent floor. Rewinding later adapter
                // state is allowed, but cannot erase or refund imported world history.
                for (const auto& baseline:entry.importedBaseline.resources) {
                    const auto restored=std::find_if(saved.ledger.resources.begin(),saved.ledger.resources.end(),
                        [&](const ResourceDto& resource) { return resource.id==baseline.id; });
                    if (restored==saved.ledger.resources.end() || restored->consumedQuantity<baseline.consumedQuantity) return false;
                }
                for (const auto& baseline:entry.importedBaseline.sessions) {
                    const auto restored=std::find_if(saved.ledger.sessions.begin(),saved.ledger.sessions.end(),
                        [&](const ResourceSessionDto& key) { return key.sessionId==baseline.sessionId && key.resourceId==baseline.resourceId; });
                    if (restored==saved.ledger.sessions.end() || restored->amount<baseline.amount) return false;
                }
                entry.population.restore(saved.population);
                if (!entry.ledger.restore(saved.ledger)) return false;
                const auto priorBindings=entry.sessions;
                entry.sessions.clear();
                for (const auto& binding:saved.bindings) {
                    // Only immutable admission provenance can own imported keys.
                    for (const auto& region:regions_) {
                        if (region.second.importedSessions.count(binding.session)) {
                            const auto owner=region.second.importedBindings.find(binding.actor);
                            if (region.first!=saved.population.region || owner==region.second.importedBindings.end() ||
                                owner->second!=binding.session) return false;
                        }
                        for (const auto& key:region.second.ledger.continuationSnapshot().sessions)
                            if (key.sessionId==binding.session && !std::any_of(region.second.sessions.begin(),region.second.sessions.end(),
                                [&](const auto& owner) { return owner.first==binding.actor && owner.second==binding.session; })) return false;
                        const auto known=region.second.sessions.find(binding.actor);
                        if (known!=region.second.sessions.end() &&
                            (region.first!=saved.population.region || known->second!=binding.session)) return false;
                        for (const auto& knownSession:region.second.sessions)
                            if (knownSession.second==binding.session && knownSession.first!=binding.actor) return false;
                    }
                    const auto prior=priorBindings.find(binding.actor);
                    if (prior!=priorBindings.end() && prior->second!=binding.session) return false;
                    for (const auto& oldBinding:priorBindings)
                        if (oldBinding.second==binding.session && oldBinding.first!=binding.actor) return false;
                    if ((!saved.contactsConfigured && !entry.importedBindings.count(binding.actor)) || !binding.session || !std::any_of(saved.population.actors.begin(),saved.population.actors.end(),
                        [&](const ActorSnapshot& a) { return a.id==binding.actor; }) ||
                        !entry.sessions.emplace(binding.actor,binding.session).second ||
                        !sessions.emplace(binding.session,std::make_pair(saved.population.region,binding.actor)).second) return false;
                }
                for (const auto& imported:entry.importedBindings) {
                    const auto owner=entry.sessions.find(imported.first);
                    if (owner==entry.sessions.end() || owner->second!=imported.second) return false;
                }
                for (const auto& oldBinding:priorBindings) {
                    const bool retainedHistory=std::any_of(saved.ledger.sessions.begin(),saved.ledger.sessions.end(),
                        [&](const ResourceSessionDto& key) { return key.sessionId==oldBinding.second; });
                    const auto retainedBinding=entry.sessions.find(oldBinding.first);
                    if (retainedHistory && (retainedBinding==entry.sessions.end() || retainedBinding->second!=oldBinding.second)) return false;
                }
                entry.active=saved.active;
                refreshAvailability(entry);
            }
            // A bound session cannot alias any other region's imported history.
            for (const auto& region:next.regions_) for (const auto& key:region.second.ledger.continuationSnapshot().sessions) {
                const auto bound=sessions.find(key.sessionId);
                if (bound!=sessions.end() && bound->second.first!=region.first) return false;
            }
            regions_.swap(next.regions_);
            return true;
        } catch (const std::exception&) { return false; }
    }
    // Explicit bookkeeping-only compatibility API. It bypasses contact validation
    // and grants no relief/events; new physical consumers should use contact().
    ResourceAdmission consumeCumulative(RegionId region, std::uint64_t session,
                                         std::uint64_t resource, double total) noexcept {
        const auto found = regions_.find(region);
        if (found == regions_.end()) return ResourceAdmission::MissingResource;
        if (!found->second.active) return ResourceAdmission::InvalidRequest;
        for (const auto& other:regions_) if (other.first!=region)
            for (const auto& binding:other.second.sessions)
                if (binding.second==session) return ResourceAdmission::Conflict;
        return found->second.ledger.tryConsumeCumulative(session, resource, total);
    }
};

} // namespace stalthos::fauna
