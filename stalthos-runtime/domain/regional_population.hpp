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
    InvalidReservation, EmptyPopulation, SeedConflict, StorageFailure
};
struct RegionalReservation {
    std::size_t resources = 0;
    std::size_t historyKeys = 0;
};
struct RegionalCapacity {
    std::size_t regions = 0, individuals = 0, resources = 0, historyKeys = 0;
};
struct RetainedRegionSnapshot {
    PopulationSnapshot population;
    ResourceLedgerDto ledger;
    RegionalReservation reservation;
    bool active = false;
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
        Entry(Population p, ResourceLedger l, RegionalReservation r)
            : population(std::move(p)), ledger(std::move(l)), reservation(r) {}
    };
    std::map<RegionId, Entry> regions_;
    std::uint64_t seed_;
    RegionId legacyRegion_;
    RegionalCapacity used_{};

    RegionalAdmission insert(Population population, const ResourceLedgerDto& history,
                             RegionalReservation reservation) {
        const auto candidate = population.snapshot();
        if (regions_.count(candidate.region)) return RegionalAdmission::DuplicateRegion;
        if (candidate.seed != seed_) return RegionalAdmission::SeedConflict;
        if (candidate.actors.empty()) return RegionalAdmission::EmptyPopulation;
        if (regions_.size() >= maxRegions) return RegionalAdmission::RegionLimit;
        if (candidate.actors.size() > maxIndividuals - used_.individuals)
            return RegionalAdmission::IndividualLimit;
        if (reservation.resources > maxResources || reservation.historyKeys > maxHistoryKeys ||
            reservation.resources < history.resources.size() || reservation.historyKeys < history.sessions.size())
            return RegionalAdmission::InvalidReservation;
        if (reservation.resources > maxResources - used_.resources ||
            reservation.historyKeys > maxHistoryKeys - used_.historyKeys)
            return RegionalAdmission::ReservationLimit;
        ResourceLedger ledger(reservation.resources, reservation.historyKeys);
        if (!ledger.restore(history)) return RegionalAdmission::InvalidReservation;
        for (const auto& entry : regions_) {
            for (const auto& actor : entry.second.population.actors())
                for (const auto& other : candidate.actors)
                    if (actor.id == other.id) return RegionalAdmission::IdentityConflict;
            for (const auto& resource : entry.second.ledger.snapshot())
                for (const auto& other : history.resources)
                    if (resource.id == other.id) return RegionalAdmission::ResourceConflict;
        }
        // All validation and allocations happen before usage counters commit.
        // No erasure or replacement API exists: retained identities and keys stay.
        regions_.emplace(candidate.region, Entry(std::move(population), std::move(ledger), reservation));
        ++used_.regions;
        used_.individuals += candidate.actors.size();
        used_.resources += reservation.resources;
        used_.historyKeys += reservation.historyKeys;
        return RegionalAdmission::Accepted;
    }
public:
    // Bootstrap is mandatory: the original population and ALL retained legacy
    // history are imported unchanged. Used history counts inside its reservation,
    // never once as used keys and again as reserved keys.
    RegionalPopulationRegistry(Population legacy, const ResourceLedgerDto& history,
                               RegionalReservation reservation)
        : seed_(legacy.snapshot().seed), legacyRegion_(legacy.snapshot().region) {
        if (insert(std::move(legacy), history, reservation) != RegionalAdmission::Accepted)
            throw std::invalid_argument("invalid retained legacy population/reservation");
    }
    RegionalAdmission admit(RegionalAdmissionOrigin origin, Population population,
                            const ResourceLedgerDto& history, RegionalReservation reservation) {
        if (origin == RegionalAdmissionOrigin::CameraPrewarm) return RegionalAdmission::CameraOnly;
        if (origin != RegionalAdmissionOrigin::PhysicalArrival) return RegionalAdmission::InvalidOrigin;
        try { return insert(std::move(population), history, reservation); }
        catch (const std::bad_alloc&) { return RegionalAdmission::StorageFailure; }
    }
    RegionalCapacity capacity() const noexcept { return used_; }
    RegionId legacyRegion() const noexcept { return legacyRegion_; }
    std::optional<RetainedRegionSnapshot> snapshot(RegionId region) const {
        const auto found = regions_.find(region);
        if (found == regions_.end()) return std::nullopt;
        const auto& entry = found->second;
        return RetainedRegionSnapshot{entry.population.snapshot(), entry.ledger.continuationSnapshot(),
                                      entry.reservation, entry.active};
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
        auto& entry = found->second;
        if (entry.active) entry.population.advance(dt, false);
        else entry.population.advanceDormant(dt);
        return true;
    }
    // Ledger bookkeeping only: contact validation, actor binding, and accepted-delta
    // needs relief remain host integration work. This method grants no relief.
    ResourceAdmission consumeCumulative(RegionId region, std::uint64_t session,
                                         std::uint64_t resource, double total) noexcept {
        const auto found = regions_.find(region);
        if (found == regions_.end()) return ResourceAdmission::MissingResource;
        if (!found->second.active) return ResourceAdmission::InvalidRequest;
        return found->second.ledger.tryConsumeCumulative(session, resource, total);
    }
};

} // namespace stalthos::fauna
