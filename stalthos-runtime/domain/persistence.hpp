#pragma once

#include "foundation.hpp"
#include "lifecycle.hpp"
#include "needs.hpp"
#include "resource_ledger.hpp"
#include "camera_framing.hpp"
#include "fauna_population.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace stalthos {
namespace persistence {

struct ActiveEntity { Identity id; RegionId region; };
struct RetainedEntity {
    Identity id; RegionId region; HeroSpecies species; RetentionReason reason;
    bool alive; bool active;
};
struct DeadHero { Identity id; RegionId region; };
struct RegionTrees { RegionId region; std::vector<TreeException> trees; std::uint64_t revision = 0; };
struct HostPose { Vec3 position; double headingRadians = 0; std::uint8_t cameraMode = 0; };
struct CameraStateSnapshot {
    std::uint8_t mode = 0;
    std::uint8_t focus = 0;
    Vec3 detachedAnchor{0.0, 0.0, 0.0};
    double orbitAzimuth = 0.0;
    double orbitElevation = 0.0;
    double orbitDistance = domain::CameraFraming::kDefaultDistance;
};
struct HeroLifecycleSnapshot {
    Identity heroId;
    HeroSpecies species;
    double calendarAge = 0.0;
    LifeStage stage = LifeStage::Hatchling;
};
struct HeroNeedsSnapshot {
    Identity heroId;
    HeroSpecies species;
    double hunger = 0.0;
    double thirst = 0.0;
    double fatigue = 0.0;
    double injury = 0.0;
    bool isResting = false;
};
struct SaveData {
    double physicalSeconds = 0.0;
    double calendarSeconds = 0.0;
    std::uint64_t seed = 0;
    std::size_t retentionLimit = 64;
    std::optional<ActiveEntity> hero;
    std::optional<HeroLifecycleSnapshot> heroLifecycle;
    std::optional<HeroNeedsSnapshot> heroNeeds;
    std::vector<ActiveEntity> activeTransients;
    std::vector<RetainedEntity> retained;
    std::vector<DeadHero> deadHeroes;
    std::vector<RegionTrees> regions;
    std::optional<HostPose> pose;
    std::optional<ResourceLedgerDto> resourceLedger;
    std::optional<CameraStateSnapshot> cameraState;
    std::optional<fauna::PopulationSnapshot> faunaPopulation;
    std::vector<std::pair<Identity,std::uint64_t>> faunaResourceSessions;
};

constexpr std::size_t MAX_ACTIVE_TRANSIENTS = 500'000;
constexpr std::size_t MAX_RETAINED = 10'000;
constexpr std::size_t MAX_DEAD_HEROES = 10'000;
constexpr std::size_t MAX_REGIONS = 100'000;
constexpr std::size_t MAX_TREES_PER_REGION = 50'000;
constexpr std::size_t MAX_TOTAL_TREES = 1'000'000;
constexpr std::size_t MAX_FAUNA_ACTORS = 16;
constexpr std::uint64_t MAX_SAVE_BYTES = 64ull * 1024ull * 1024ull;

namespace detail {
inline void write_u64(std::ostream& os, std::uint64_t v) {
    char buf[8];
    for (int i = 0; i < 8; ++i) buf[i] = static_cast<char>((v >> (i * 8)) & 0xff);
    os.write(buf, 8);
}
inline std::uint64_t read_u64(std::istream& is) {
    char buf[8];
    if (!is.read(buf, 8)) throw std::runtime_error("truncated file");
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(static_cast<unsigned char>(buf[i])) << (i * 8);
    return v;
}
inline void write_u32(std::ostream& os, std::uint32_t v) {
    char buf[4];
    for (int i = 0; i < 4; ++i) buf[i] = static_cast<char>((v >> (i * 8)) & 0xff);
    os.write(buf, 4);
}
inline std::uint32_t read_u32(std::istream& is) {
    char buf[4];
    if (!is.read(buf, 4)) throw std::runtime_error("truncated file");
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(static_cast<unsigned char>(buf[i])) << (i * 8);
    return v;
}
inline void write_double(std::ostream& os, double v) {
    static_assert(sizeof(double) == sizeof(std::uint64_t), "save format requires 64-bit doubles");
    std::uint64_t bits;
    std::memcpy(&bits, &v, sizeof(v));
    write_u64(os, bits);
}
inline double read_double(std::istream& is) {
    const auto bits = read_u64(is);
    double v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}
inline void write_u8(std::ostream& os, std::uint8_t v) { const char c = static_cast<char>(v); os.write(&c, 1); }
inline std::uint8_t read_u8(std::istream& is) {
    char c;
    if (!is.read(&c, 1)) throw std::runtime_error("truncated file");
    return static_cast<std::uint8_t>(c);
}

inline bool valid(HeroSpecies value) {
    return static_cast<unsigned>(value) <= static_cast<unsigned>(HeroSpecies::Longstride);
}
inline bool valid(RetentionReason value) {
    return static_cast<unsigned>(value) <= static_cast<unsigned>(RetentionReason::NotableEncounter);
}
inline bool valid(TreeCondition value) {
    return static_cast<unsigned>(value) <= static_cast<unsigned>(TreeCondition::Dead);
}

inline void claimIdentity(std::set<Identity>& ids, Identity id) {
    if (!ids.insert(id).second) throw std::runtime_error("duplicate or conflicting entity identity");
}

inline std::uint64_t checkedAdd(std::uint64_t total, std::uint64_t amount) {
    if (amount > std::numeric_limits<std::uint64_t>::max() - total) throw std::runtime_error("save size overflow");
    return total + amount;
}

inline std::uint64_t encodedSize(const SaveData& data) {
    // STL8 header, times, seed, retention limit, identities, optional pose,
    // optional lifecycle, optional needs, optional resource ledger, and optional camera state.
    std::uint64_t size = 8 + 16 + 8 + 8 + 1 + (data.hero ? 16 : 0) + 1 + (data.pose ? 33 : 0) +
        1 + (data.heroLifecycle ? 18 : 0) + 1 + (data.heroNeeds ? 42 : 0) + 1;
    if (data.resourceLedger) {
        size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.resourceLedger->resources.size()) * 25);
        size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.resourceLedger->sessions.size()) * 24);
    }
    size = checkedAdd(size, 1 + (data.cameraState ? 50 : 0));
    size = checkedAdd(size, 1 + (data.faunaPopulation ? 32 + static_cast<std::uint64_t>(data.faunaPopulation->actors.size()) * 108 + static_cast<std::uint64_t>(data.faunaResourceSessions.size()) * 16 : 0));
    size = checkedAdd(size, 4);
    size = checkedAdd(size, static_cast<std::uint64_t>(data.activeTransients.size()) * 16);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.retained.size()) * 20);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.deadHeroes.size()) * 16);
    size = checkedAdd(size, 4);
    for (const auto& region : data.regions)
        size = checkedAdd(size, 20 + static_cast<std::uint64_t>(region.trees.size()) * 17);
    if (size > MAX_SAVE_BYTES) throw std::runtime_error("save exceeds byte size bound");
    return size;
}

inline std::uint64_t encodedSizeStl5(const SaveData& data) {
    // STL5 header, times, seed, retention limit, identities, optional pose,
    // optional lifecycle, optional needs, and optional resource ledger.
    std::uint64_t size = 8 + 16 + 8 + 8 + 1 + (data.hero ? 16 : 0) + 1 + (data.pose ? 33 : 0) +
        1 + (data.heroLifecycle ? 18 : 0) + 1 + (data.heroNeeds ? 42 : 0) + 1;
    if (data.resourceLedger) {
        size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.resourceLedger->resources.size()) * 25);
        size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.resourceLedger->sessions.size()) * 24);
    }
    size = checkedAdd(size, 4);
    size = checkedAdd(size, static_cast<std::uint64_t>(data.activeTransients.size()) * 16);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.retained.size()) * 20);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.deadHeroes.size()) * 16);
    size = checkedAdd(size, 4);
    for (const auto& region : data.regions)
        size = checkedAdd(size, 20 + static_cast<std::uint64_t>(region.trees.size()) * 17);
    if (size > MAX_SAVE_BYTES) throw std::runtime_error("save exceeds byte size bound");
    return size;
}

inline std::uint64_t encodedSizeStl4(const SaveData& data) {
    // STL4 header, times, seed, retention limit, identities, optional pose,
    // the lifecycle presence byte plus its fixed-width payload,
    // and the needs presence byte plus its fixed-width payload.
    std::uint64_t size = 8 + 16 + 8 + 8 + 1 + (data.hero ? 16 : 0) + 1 + (data.pose ? 33 : 0) +
        1 + (data.heroLifecycle ? 18 : 0) + 1 + (data.heroNeeds ? 42 : 0) + 4;
    size = checkedAdd(size, static_cast<std::uint64_t>(data.activeTransients.size()) * 16);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.retained.size()) * 20);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.deadHeroes.size()) * 16);
    size = checkedAdd(size, 4);
    for (const auto& region : data.regions)
        size = checkedAdd(size, 20 + static_cast<std::uint64_t>(region.trees.size()) * 17);
    if (size > MAX_SAVE_BYTES) throw std::runtime_error("save exceeds byte size bound");
    return size;
}

inline std::uint64_t encodedSizeStl3(const SaveData& data) {
    // STL3 header, times, seed, retention limit, identities, optional pose,
    // and the lifecycle presence byte plus its fixed-width payload.
    std::uint64_t size = 8 + 16 + 8 + 8 + 1 + (data.hero ? 16 : 0) + 1 + (data.pose ? 33 : 0) +
        1 + (data.heroLifecycle ? 18 : 0) + 4;
    size = checkedAdd(size, static_cast<std::uint64_t>(data.activeTransients.size()) * 16);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.retained.size()) * 20);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.deadHeroes.size()) * 16);
    size = checkedAdd(size, 4);
    for (const auto& region : data.regions)
        size = checkedAdd(size, 20 + static_cast<std::uint64_t>(region.trees.size()) * 17);
    return size;
}

inline std::uint64_t encodedSizeStl2(const SaveData& data) {
    std::uint64_t size = 8 + 16 + 8 + 8 + 1 + (data.hero ? 16 : 0) + 1 + (data.pose ? 33 : 0) + 4;
    size = checkedAdd(size, static_cast<std::uint64_t>(data.activeTransients.size()) * 16);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.retained.size()) * 20);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.deadHeroes.size()) * 16);
    size = checkedAdd(size, 4);
    for (const auto& region : data.regions)
        size = checkedAdd(size, 20 + static_cast<std::uint64_t>(region.trees.size()) * 17);
    return size;
}

inline std::uint64_t serializedFileSize(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) throw std::runtime_error("failed to inspect save file size");
    if (size > MAX_SAVE_BYTES) throw std::runtime_error("save exceeds byte size bound");
    return size;
}

inline void validate(const CameraStateSnapshot& s) {
    if (s.mode > 1) throw std::runtime_error("invalid camera mode");
    if (s.focus > 1) throw std::runtime_error("invalid input focus");
    if (!finite(s.detachedAnchor) ||
        std::abs(s.detachedAnchor.x) > 1e6 ||
        std::abs(s.detachedAnchor.y) > 1e6 ||
        std::abs(s.detachedAnchor.z) > 1e6)
        throw std::runtime_error("invalid camera detached anchor");
    if (!std::isfinite(s.orbitAzimuth) || !std::isfinite(s.orbitElevation) || !std::isfinite(s.orbitDistance) ||
        s.orbitDistance < domain::CameraFraming::kMinDistance || s.orbitDistance > domain::CameraFraming::kMaxDistance ||
        s.orbitElevation < domain::CameraFraming::kMinElevation || s.orbitElevation > domain::CameraFraming::kMaxElevation)
        throw std::runtime_error("invalid camera orbit parameters");
}

inline void validate(const SaveData& data) {
    if (!std::isfinite(data.calendarSeconds) || data.calendarSeconds < 0) throw std::runtime_error("invalid calendar");
    if (!std::isfinite(data.physicalSeconds) || data.physicalSeconds < 0) throw std::runtime_error("invalid physical time");
    if (data.pose && (!finite(data.pose->position) || !std::isfinite(data.pose->headingRadians) || data.pose->cameraMode > 2))
        throw std::runtime_error("invalid host pose");
    if (data.heroLifecycle) {
        const auto& life = *data.heroLifecycle;
        if (!data.hero || life.heroId != data.hero->id)
            throw std::runtime_error("lifecycle identity does not match current hero");
        if (!valid(life.species)) throw std::runtime_error("invalid lifecycle species");
        if (static_cast<unsigned>(life.stage) > static_cast<unsigned>(LifeStage::Dead))
            throw std::runtime_error("invalid lifecycle stage");
        if (life.stage == LifeStage::Dead)
            throw std::runtime_error("dead lifecycle cannot belong to current live hero");
        try {
            Lifecycle checked(life.species);
            checked.restoreState(life.species, life.calendarAge, life.stage);
        } catch (const std::exception&) {
            throw std::runtime_error("lifecycle profile, age, and stage are inconsistent");
        }
    }
    if (data.heroNeeds) {
        const auto& needs = *data.heroNeeds;
        if (!data.hero || needs.heroId != data.hero->id)
            throw std::runtime_error("needs identity does not match current hero");
        if (!valid(needs.species)) throw std::runtime_error("invalid needs species");
        // All available records for the current hero must agree on species.
        if (data.heroLifecycle && needs.species != data.heroLifecycle->species)
            throw std::runtime_error("needs species does not match lifecycle species");
        const auto retainedHero = std::find_if(data.retained.begin(), data.retained.end(),
            [&](const auto& retained) { return retained.id == data.hero->id; });
        if (retainedHero != data.retained.end() && needs.species != retainedHero->species)
            throw std::runtime_error("needs species does not match retained hero species");

        if (!std::isfinite(needs.hunger) || needs.hunger < 0.0 || needs.hunger > 1.0 ||
            !std::isfinite(needs.thirst) || needs.thirst < 0.0 || needs.thirst > 1.0 ||
            !std::isfinite(needs.fatigue) || needs.fatigue < 0.0 || needs.fatigue > 1.0 ||
            !std::isfinite(needs.injury) || needs.injury < 0.0 || needs.injury > 1.0) {
            throw std::runtime_error("invalid needs values");
        }
    }
    if (data.cameraState) {
        validate(*data.cameraState);
    }
    if (data.retentionLimit > MAX_RETAINED) throw std::runtime_error("retention limit exceeds size bound");
    if (data.retained.size() > data.retentionLimit) throw std::runtime_error("retained records exceed retention limit");
    if (data.activeTransients.size() > MAX_ACTIVE_TRANSIENTS) throw std::runtime_error("too many active transients");
    if (data.faunaPopulation && data.faunaPopulation->actors.size() > MAX_FAUNA_ACTORS) throw std::runtime_error("too many active fauna actors");
    if (data.retained.size() > MAX_RETAINED) throw std::runtime_error("too many retained entities");
    if (data.deadHeroes.size() > MAX_DEAD_HEROES) throw std::runtime_error("too many dead heroes");
    if (data.regions.size() > MAX_REGIONS) throw std::runtime_error("too many regions");

    std::set<Identity> activeIds, retainedIds, deadIds;
    if (data.hero) activeIds.insert(data.hero->id);
    for (const auto& value : data.activeTransients) claimIdentity(activeIds, value.id);
    for (const auto& value : data.retained) {
        claimIdentity(retainedIds, value.id);
        if (!valid(value.species)) throw std::runtime_error("invalid species");
        if (!valid(value.reason)) throw std::runtime_error("invalid retention reason");
        if (value.active && !value.alive) throw std::runtime_error("dead retained entity cannot be active");
    }
    for (const auto& value : data.deadHeroes) claimIdentity(deadIds, value.id);
    if(data.faunaResourceSessions.size()>MAX_FAUNA_ACTORS || (!data.faunaPopulation && !data.faunaResourceSessions.empty()))
        throw std::runtime_error("invalid fauna resource session count");
    if (data.faunaPopulation) {
        if (data.faunaPopulation->seed != data.seed || data.faunaPopulation->region == 0) throw std::runtime_error("fauna population world identity mismatch");
        std::set<Identity> faunaIds;
        for (const auto& actor : data.faunaPopulation->actors) {
            if (!actor.id || actor.id == 1 || !faunaIds.insert(actor.id).second || activeIds.count(actor.id) || retainedIds.count(actor.id) || deadIds.count(actor.id))
                throw std::runtime_error("duplicate or conflicting fauna identity");
            if (!valid(actor.species) || static_cast<unsigned>(actor.stage) > static_cast<unsigned>(LifeStage::Dead) || static_cast<unsigned>(actor.activity) > static_cast<unsigned>(fauna::Activity::Drink)) throw std::runtime_error("invalid fauna enum");
            if (actor.site >= fauna::Population::maxSites || actor.target >= fauna::Population::maxSites) throw std::runtime_error("fauna route index out of range");
            if (!finite(actor.position) || !std::isfinite(actor.headingRadians) || !std::isfinite(actor.distanceTravelledMeters) || actor.distanceTravelledMeters < 0.0) throw std::runtime_error("invalid fauna motion");
            if (!std::isfinite(actor.hunger) || actor.hunger < 0 || actor.hunger > 1 || !std::isfinite(actor.thirst) || actor.thirst < 0 || actor.thirst > 1 || !std::isfinite(actor.fatigue) || actor.fatigue < 0 || actor.fatigue > 1 || !std::isfinite(actor.injury) || actor.injury < 0 || actor.injury > 1) throw std::runtime_error("invalid fauna needs");
            try { Lifecycle life(actor.species); life.restoreState(actor.species,actor.calendarAge,actor.stage); } catch (...) { throw std::runtime_error("invalid fauna lifecycle"); }
        }
        std::set<Identity> mappedActors;
        std::set<std::uint64_t> mappedSessions;
        for(const auto& entry:data.faunaResourceSessions) {
            if(!faunaIds.count(entry.first) || !entry.second || !mappedActors.insert(entry.first).second || !mappedSessions.insert(entry.second).second)
                throw std::runtime_error("invalid fauna resource session mapping");
        }
    }
    for (const auto& value : data.retained) {
        const bool isHero = data.hero && data.hero->id == value.id;
        if (isHero && (!value.alive || !value.active || data.hero->region != value.region)) throw std::runtime_error("inconsistent retained hero");
        if (isHero && data.heroLifecycle && value.species != data.heroLifecycle->species)
            throw std::runtime_error("retained hero species contradicts lifecycle species");
        if (activeIds.count(value.id) && !isHero) throw std::runtime_error("active retained overlap requires host representation");
        if (deadIds.count(value.id) && (value.alive || value.active)) throw std::runtime_error("inconsistent retained dead hero");
    }
    if (data.hero && deadIds.count(data.hero->id)) throw std::runtime_error("current hero is dead");
    for (const auto& value : data.deadHeroes)
        if (activeIds.count(value.id) || (retainedIds.count(value.id) &&
            (std::find_if(data.retained.begin(), data.retained.end(), [&](const auto& r) { return r.id == value.id && r.region == value.region && !r.alive && !r.active; }) == data.retained.end())))
            throw std::runtime_error("conflicting dead hero identity");

    std::set<RegionId> regions;
    std::size_t totalTrees = 0;
    for (const auto& region : data.regions) {
        if (!regions.insert(region.region).second) throw std::runtime_error("duplicate region record");
        if (region.trees.size() > MAX_TREES_PER_REGION) throw std::runtime_error("too many trees in region");
        if (region.trees.size() > MAX_TOTAL_TREES - totalTrees) throw std::runtime_error("too many tree records in save");
        totalTrees += region.trees.size();
        std::set<Identity> localTrees;
        for (const auto& tree : region.trees) {
            if (!localTrees.insert(tree.tree).second)
                throw std::runtime_error("duplicate or conflicting tree reference");
            if (!valid(tree.condition)) throw std::runtime_error("invalid tree condition");
            if (!std::isfinite(tree.changedAt) || tree.changedAt < 0) throw std::runtime_error("invalid tree timestamp");
        }
    }
    for (const auto& region : data.regions)
        if (region.revision < region.trees.size()) throw std::runtime_error("region revision below tree record count");
    if (data.resourceLedger) {
        ResourceLedger ledger;
        if (!ledger.restore(*data.resourceLedger)) throw std::runtime_error("invalid resource ledger records");
    }
    (void)encodedSize(data);
}

struct TempFileCleanup {
    std::filesystem::path path;
    bool keep = false;
    ~TempFileCleanup() {
        if (!keep) { std::error_code ec; std::filesystem::remove(path, ec); }
    }
};
} // namespace detail

inline void writeSave(const std::filesystem::path& path, const SaveData& data) {
    detail::validate(data); // Reject before touching the previous save or a temp file.
    auto tempPath = path;
    tempPath += ".tmp";
    detail::TempFileCleanup cleanup{tempPath};
    std::ofstream os(tempPath, std::ios::binary | std::ios::trunc);
    if (!os) throw std::runtime_error("failed to open temp file for save");

    os.write("STL8", 4);
    detail::write_u32(os, 8);
    detail::write_double(os, data.physicalSeconds);
    detail::write_double(os, data.calendarSeconds);
    detail::write_u64(os, data.seed);
    detail::write_u64(os, static_cast<std::uint64_t>(data.retentionLimit));
    detail::write_u8(os, data.hero ? 1 : 0);
    if (data.hero) { detail::write_u64(os, data.hero->id); detail::write_u64(os, data.hero->region); }
    detail::write_u8(os, data.pose ? 1 : 0);
    if (data.pose) {
        detail::write_double(os, data.pose->position.x); detail::write_double(os, data.pose->position.y);
        detail::write_double(os, data.pose->position.z); detail::write_double(os, data.pose->headingRadians);
        detail::write_u8(os, data.pose->cameraMode);
    }
    detail::write_u8(os, data.heroLifecycle ? 1 : 0);
    if (data.heroLifecycle) {
        detail::write_u64(os, data.heroLifecycle->heroId);
        detail::write_u8(os, static_cast<std::uint8_t>(data.heroLifecycle->species));
        detail::write_double(os, data.heroLifecycle->calendarAge);
        detail::write_u8(os, static_cast<std::uint8_t>(data.heroLifecycle->stage));
    }

    detail::write_u8(os, data.heroNeeds ? 1 : 0);
    if (data.heroNeeds) {
        detail::write_u64(os, data.heroNeeds->heroId);
        detail::write_u8(os, static_cast<std::uint8_t>(data.heroNeeds->species));
        detail::write_double(os, data.heroNeeds->hunger);
        detail::write_double(os, data.heroNeeds->thirst);
        detail::write_double(os, data.heroNeeds->fatigue);
        detail::write_double(os, data.heroNeeds->injury);
        detail::write_u8(os, data.heroNeeds->isResting ? 1 : 0);
    }

    detail::write_u8(os, data.resourceLedger ? 1 : 0);
    if (data.resourceLedger) {
        auto ledgerDto = *data.resourceLedger;
        std::sort(ledgerDto.resources.begin(), ledgerDto.resources.end(),
            [](const auto& a, const auto& b) { return a.id < b.id; });
        std::sort(ledgerDto.sessions.begin(), ledgerDto.sessions.end(),
            [](const auto& a, const auto& b) {
                if (a.sessionId != b.sessionId) return a.sessionId < b.sessionId;
                return a.resourceId < b.resourceId;
            });
        detail::write_u32(os, static_cast<std::uint32_t>(ledgerDto.resources.size()));
        for (const auto& res : ledgerDto.resources) {
            detail::write_u64(os, res.id);
            detail::write_u8(os, static_cast<std::uint8_t>(res.kind));
            detail::write_double(os, res.initialQuantity);
            detail::write_double(os, res.consumedQuantity);
        }
        detail::write_u32(os, static_cast<std::uint32_t>(ledgerDto.sessions.size()));
        for (const auto& ses : ledgerDto.sessions) {
            detail::write_u64(os, ses.sessionId);
            detail::write_u64(os, ses.resourceId);
            detail::write_double(os, ses.amount);
        }
    }

    detail::write_u8(os, data.cameraState ? 1 : 0);
    if (data.cameraState) {
        detail::write_u8(os, data.cameraState->mode);
        detail::write_u8(os, data.cameraState->focus);
        detail::write_double(os, data.cameraState->detachedAnchor.x);
        detail::write_double(os, data.cameraState->detachedAnchor.y);
        detail::write_double(os, data.cameraState->detachedAnchor.z);
        detail::write_double(os, data.cameraState->orbitAzimuth);
        detail::write_double(os, data.cameraState->orbitElevation);
        detail::write_double(os, data.cameraState->orbitDistance);
    }
    detail::write_u8(os, data.faunaPopulation ? 1 : 0);
    if (data.faunaPopulation) {
        const auto& pop=*data.faunaPopulation;
        detail::write_u64(os,pop.seed); detail::write_u64(os,pop.region); detail::write_u64(os,pop.habitatSignature);
        detail::write_u32(os,static_cast<std::uint32_t>(pop.actors.size()));
        for (const auto& a:pop.actors) {
            detail::write_u64(os,a.id); detail::write_u8(os,static_cast<std::uint8_t>(a.species));
            detail::write_double(os,a.position.x); detail::write_double(os,a.position.y); detail::write_double(os,a.position.z);
            detail::write_double(os,a.headingRadians); detail::write_double(os,a.distanceTravelledMeters);
            detail::write_u64(os,a.site); detail::write_u64(os,a.target); detail::write_double(os,a.calendarAge);
            detail::write_u8(os,static_cast<std::uint8_t>(a.stage)); detail::write_double(os,a.hunger); detail::write_double(os,a.thirst);
            detail::write_double(os,a.fatigue); detail::write_double(os,a.injury); detail::write_u8(os,a.isResting?1:0);
            detail::write_u8(os,static_cast<std::uint8_t>(a.activity));
        }
        detail::write_u32(os,static_cast<std::uint32_t>(data.faunaResourceSessions.size()));
        for(const auto& entry:data.faunaResourceSessions) {
            detail::write_u64(os,entry.first); detail::write_u64(os,entry.second);
        }
    }

    auto active = data.activeTransients;
    std::sort(active.begin(), active.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    detail::write_u32(os, static_cast<std::uint32_t>(active.size()));
    for (const auto& value : active) { detail::write_u64(os, value.id); detail::write_u64(os, value.region); }

    auto retained = data.retained;
    std::sort(retained.begin(), retained.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    detail::write_u32(os, static_cast<std::uint32_t>(retained.size()));
    for (const auto& value : retained) {
        detail::write_u64(os, value.id); detail::write_u64(os, value.region);
        detail::write_u8(os, static_cast<std::uint8_t>(value.species));
        detail::write_u8(os, static_cast<std::uint8_t>(value.reason));
        detail::write_u8(os, value.alive ? 1 : 0); detail::write_u8(os, value.active ? 1 : 0);
    }

    auto dead = data.deadHeroes;
    std::sort(dead.begin(), dead.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    detail::write_u32(os, static_cast<std::uint32_t>(dead.size()));
    for (const auto& value : dead) { detail::write_u64(os, value.id); detail::write_u64(os, value.region); }

    auto regions = data.regions;
    std::sort(regions.begin(), regions.end(), [](const auto& a, const auto& b) { return a.region < b.region; });
    detail::write_u32(os, static_cast<std::uint32_t>(regions.size()));
    for (auto& region : regions) {
        std::sort(region.trees.begin(), region.trees.end(), [](const auto& a, const auto& b) { return a.tree < b.tree; });
        detail::write_u64(os, region.region);
        detail::write_u64(os, region.revision);
        detail::write_u32(os, static_cast<std::uint32_t>(region.trees.size()));
        for (const auto& tree : region.trees) {
            detail::write_u64(os, tree.tree);
            detail::write_u8(os, static_cast<std::uint8_t>(tree.condition));
            detail::write_double(os, tree.changedAt);
        }
    }
    os.flush();
    if (!os.good()) throw std::runtime_error("failed to write save data completely");
    os.close();
    if (os.fail()) throw std::runtime_error("failed to close save data completely");

    std::error_code ec;
    std::filesystem::rename(tempPath, path, ec);
    if (ec) throw std::runtime_error("failed to commit save atomically");
    cleanup.keep = true;
}

inline SaveData readSave(const std::filesystem::path& path) {
    const auto fileSize = detail::serializedFileSize(path);
    std::ifstream is(path, std::ios::binary);
    if (!is) throw std::runtime_error("failed to open save file");
    char magic[4];
    if (!is.read(magic, 4))
        throw std::runtime_error("invalid magic or corrupt file");
    const bool legacy = std::strncmp(magic, "STL1", 4) == 0;
    const bool stl2 = std::strncmp(magic, "STL2", 4) == 0;
    const bool stl3 = std::strncmp(magic, "STL3", 4) == 0;
    const bool stl4 = std::strncmp(magic, "STL4", 4) == 0;
    const bool stl5 = std::strncmp(magic, "STL5", 4) == 0;
    const bool stl6 = std::strncmp(magic, "STL6", 4) == 0;
    const bool stl7 = std::strncmp(magic, "STL7", 4) == 0;
    const bool stl8 = std::strncmp(magic, "STL8", 4) == 0;
    if (!legacy && !stl2 && !stl3 && !stl4 && !stl5 && !stl6 && !stl7 && !stl8) throw std::runtime_error("invalid magic or corrupt file");
    const auto version = detail::read_u32(is);
    if ((legacy && version != 1) || (stl2 && version != 2) || (stl3 && version != 3) || (stl4 && version != 4) || (stl5 && version != 5) || (stl6 && version != 6) || (stl7 && version != 7) || (stl8 && version != 8)) throw std::runtime_error("unsupported save version");

    SaveData data;
    if (!legacy) data.physicalSeconds = detail::read_double(is);
    data.calendarSeconds = detail::read_double(is);
    data.seed = detail::read_u64(is);
    const auto limit = detail::read_u64(is);
    if (limit > MAX_RETAINED || limit > std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("retention limit exceeds size bound");
    data.retentionLimit = static_cast<std::size_t>(limit);
    const auto hasHero = detail::read_u8(is);
    if (hasHero > 1) throw std::runtime_error("corrupt hero presence flag");
    if (hasHero) data.hero = ActiveEntity{detail::read_u64(is), detail::read_u64(is)};
    if (!legacy) {
        const auto hasPose = detail::read_u8(is);
        if (hasPose > 1) throw std::runtime_error("corrupt pose presence flag");
        if (hasPose) data.pose = HostPose{{detail::read_double(is), detail::read_double(is), detail::read_double(is)}, detail::read_double(is), detail::read_u8(is)};
    }
    if (stl3 || stl4 || stl5 || stl6 || stl7 || stl8) {
        const auto hasLifecycle = detail::read_u8(is);
        if (hasLifecycle > 1) throw std::runtime_error("corrupt lifecycle presence flag");
        if (hasLifecycle) {
            HeroLifecycleSnapshot life;
            life.heroId = detail::read_u64(is);
            const auto species = detail::read_u8(is);
            if (species > static_cast<std::uint8_t>(HeroSpecies::Longstride)) throw std::runtime_error("invalid lifecycle species");
            life.species = static_cast<HeroSpecies>(species);
            life.calendarAge = detail::read_double(is);
            const auto stage = detail::read_u8(is);
            if (stage > static_cast<std::uint8_t>(LifeStage::Dead)) throw std::runtime_error("invalid lifecycle stage");
            life.stage = static_cast<LifeStage>(stage);
            data.heroLifecycle = life;
        }
    }
    if (stl4 || stl5 || stl6 || stl7 || stl8) {
        const auto hasNeeds = detail::read_u8(is);
        if (hasNeeds > 1) throw std::runtime_error("corrupt needs presence flag");
        if (hasNeeds) {
            HeroNeedsSnapshot needs;
            needs.heroId = detail::read_u64(is);
            const auto species = detail::read_u8(is);
            if (species > static_cast<std::uint8_t>(HeroSpecies::Longstride)) throw std::runtime_error("invalid needs species");
            needs.species = static_cast<HeroSpecies>(species);
            needs.hunger = detail::read_double(is);
            needs.thirst = detail::read_double(is);
            needs.fatigue = detail::read_double(is);
            needs.injury = detail::read_double(is);
            const auto resting = detail::read_u8(is);
            if (resting > 1) throw std::runtime_error("invalid resting flag");
            needs.isResting = (resting == 1);
            data.heroNeeds = needs;
        }
    }
    if (stl5 || stl6 || stl7 || stl8) {
        const auto hasResourceLedger = detail::read_u8(is);
        if (hasResourceLedger > 1) throw std::runtime_error("corrupt resource ledger presence flag");
        if (hasResourceLedger) {
            ResourceLedgerDto ledger;
            auto count = detail::read_u32(is);
            if (count > 4096 || static_cast<std::uint64_t>(count) * 25 > fileSize) throw std::runtime_error("resources exceed size bound");
            ledger.resources.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                ResourceDto res;
                res.id = detail::read_u64(is);
                const auto kind = detail::read_u8(is);
                if (kind > static_cast<std::uint8_t>(ResourceKind::Creek)) throw std::runtime_error("invalid resource kind");
                res.kind = static_cast<ResourceKind>(kind);
                res.initialQuantity = detail::read_double(is);
                res.consumedQuantity = detail::read_double(is);
                ledger.resources.push_back(res);
            }
            count = detail::read_u32(is);
            if (count > 256 || static_cast<std::uint64_t>(count) * 24 > fileSize) throw std::runtime_error("resource sessions exceed size bound");
            ledger.sessions.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                ResourceSessionDto ses;
                ses.sessionId = detail::read_u64(is);
                ses.resourceId = detail::read_u64(is);
                ses.amount = detail::read_double(is);
                ledger.sessions.push_back(ses);
            }
            data.resourceLedger = ledger;
        }
    }

    if (stl6 || stl7 || stl8) {
        const auto hasCameraState = detail::read_u8(is);
        if (hasCameraState > 1) throw std::runtime_error("corrupt camera state presence flag");
        if (hasCameraState) {
            CameraStateSnapshot cam;
            cam.mode = detail::read_u8(is);
            cam.focus = detail::read_u8(is);
            cam.detachedAnchor.x = detail::read_double(is);
            cam.detachedAnchor.y = detail::read_double(is);
            cam.detachedAnchor.z = detail::read_double(is);
            cam.orbitAzimuth = detail::read_double(is);
            cam.orbitElevation = detail::read_double(is);
            cam.orbitDistance = detail::read_double(is);
            detail::validate(cam);
            data.cameraState = cam;
        }
    }

    if (stl7 || stl8) {
        const auto present=detail::read_u8(is); if (present>1) throw std::runtime_error("corrupt fauna population presence flag");
        if (present) {
            fauna::PopulationSnapshot pop; pop.seed=detail::read_u64(is); pop.region=detail::read_u64(is); pop.habitatSignature=detail::read_u64(is);
            const auto actors=detail::read_u32(is); if (actors>MAX_FAUNA_ACTORS || static_cast<std::uint64_t>(actors)*108>fileSize) throw std::runtime_error("fauna actors exceed size bound");
            pop.actors.reserve(actors);
            for (std::uint32_t i=0;i<actors;++i) {
                fauna::ActorSnapshot a; a.id=detail::read_u64(is); const auto species=detail::read_u8(is);
                if (species>static_cast<std::uint8_t>(HeroSpecies::Longstride)) throw std::runtime_error("invalid fauna species"); a.species=static_cast<HeroSpecies>(species);
                a.position={detail::read_double(is),detail::read_double(is),detail::read_double(is)}; a.headingRadians=detail::read_double(is); a.distanceTravelledMeters=detail::read_double(is);
                const auto site=detail::read_u64(is),target=detail::read_u64(is); if(site>std::numeric_limits<std::size_t>::max()||target>std::numeric_limits<std::size_t>::max()) throw std::runtime_error("fauna route index out of range"); a.site=static_cast<std::size_t>(site); a.target=static_cast<std::size_t>(target);
                a.calendarAge=detail::read_double(is); const auto stage=detail::read_u8(is); if(stage>static_cast<std::uint8_t>(LifeStage::Dead)) throw std::runtime_error("invalid fauna stage"); a.stage=static_cast<LifeStage>(stage);
                a.hunger=detail::read_double(is); a.thirst=detail::read_double(is); a.fatigue=detail::read_double(is); a.injury=detail::read_double(is);
                const auto resting=detail::read_u8(is), activity=detail::read_u8(is); if(resting>1) throw std::runtime_error("invalid fauna resting flag"); if(activity>static_cast<std::uint8_t>(fauna::Activity::Drink)) throw std::runtime_error("invalid fauna activity"); a.isResting=resting==1; a.activity=static_cast<fauna::Activity>(activity); pop.actors.push_back(a);
            }
            if (stl8) {
            const auto sessionCount=detail::read_u32(is);
            if(sessionCount>MAX_FAUNA_ACTORS) throw std::runtime_error("invalid fauna resource session count");
            for(std::uint32_t i=0;i<sessionCount;++i) {
                const auto actor=detail::read_u64(is), session=detail::read_u64(is);
                data.faunaResourceSessions.emplace_back(actor,session);
            }
            }
            data.faunaPopulation=std::move(pop);
        }
    }

    auto count = detail::read_u32(is);
    if (count > MAX_ACTIVE_TRANSIENTS || static_cast<std::uint64_t>(count) * 16 > fileSize)
        throw std::runtime_error("active transients exceed size bound");
    data.activeTransients.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) data.activeTransients.push_back({detail::read_u64(is), detail::read_u64(is)});

    count = detail::read_u32(is);
    if (count > MAX_RETAINED || count > data.retentionLimit || static_cast<std::uint64_t>(count) * 20 > fileSize)
        throw std::runtime_error("retained records exceed retention or size bound");
    data.retained.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        RetainedEntity value;
        value.id = detail::read_u64(is); value.region = detail::read_u64(is);
        const auto species = detail::read_u8(is), reason = detail::read_u8(is);
        const auto alive = detail::read_u8(is), active = detail::read_u8(is);
        if (species > static_cast<std::uint8_t>(HeroSpecies::Longstride)) throw std::runtime_error("invalid species");
        if (reason > static_cast<std::uint8_t>(RetentionReason::NotableEncounter)) throw std::runtime_error("invalid retention reason");
        if (alive > 1 || active > 1) throw std::runtime_error("invalid retained boolean flag");
        value.species = static_cast<HeroSpecies>(species); value.reason = static_cast<RetentionReason>(reason);
        value.alive = alive == 1; value.active = active == 1;
        data.retained.push_back(value);
    }

    count = detail::read_u32(is);
    if (count > MAX_DEAD_HEROES || static_cast<std::uint64_t>(count) * 16 > fileSize)
        throw std::runtime_error("dead heroes exceed size bound");
    data.deadHeroes.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) data.deadHeroes.push_back({detail::read_u64(is), detail::read_u64(is)});

    count = detail::read_u32(is);
    if (count > MAX_REGIONS || static_cast<std::uint64_t>(count) * 12 > fileSize)
        throw std::runtime_error("regions exceed size bound");
    data.regions.reserve(count);
    std::size_t totalTrees = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        RegionTrees region;
        region.region = detail::read_u64(is);
        if (!legacy) region.revision = detail::read_u64(is);
        const auto trees = detail::read_u32(is);
        if (trees > MAX_TREES_PER_REGION || trees > MAX_TOTAL_TREES - totalTrees ||
            static_cast<std::uint64_t>(trees) * 17 > fileSize)
            throw std::runtime_error("tree records exceed aggregate size bound");
        totalTrees += trees;
        region.trees.reserve(trees);
        for (std::uint32_t j = 0; j < trees; ++j) {
            TreeException tree;
            tree.tree = detail::read_u64(is);
            const auto condition = detail::read_u8(is);
            if (condition > static_cast<std::uint8_t>(TreeCondition::Dead)) throw std::runtime_error("invalid tree condition");
            tree.condition = static_cast<TreeCondition>(condition);
            tree.changedAt = detail::read_double(is);
            if (!std::isfinite(tree.changedAt) || tree.changedAt < 0) throw std::runtime_error("invalid tree timestamp");
            region.trees.push_back(tree);
        }
        // STL1 stored only final tree overrides, so exact historical revision counts
        // were unavailable. Use the deterministic lower bound: one per saved override.
        if (legacy) region.revision = static_cast<std::uint64_t>(region.trees.size());
        data.regions.push_back(std::move(region));
    }
    char trailing;
    if (is.read(&trailing, 1)) throw std::runtime_error("trailing bytes at end of file");
    if (is.bad()) throw std::runtime_error("failed while reading save file");
    detail::validate(data);
    if (legacy) {
        // Compare against the legacy layout, whose records omit physical time, pose and revisions.
        std::uint64_t legacySize = 8 + 8 + 8 + 8 + 1 + (data.hero ? 16 : 0) + 4;
        legacySize = detail::checkedAdd(legacySize, static_cast<std::uint64_t>(data.activeTransients.size()) * 16);
        legacySize = detail::checkedAdd(legacySize, 4 + static_cast<std::uint64_t>(data.retained.size()) * 20);
        legacySize = detail::checkedAdd(legacySize, 4 + static_cast<std::uint64_t>(data.deadHeroes.size()) * 16);
        legacySize = detail::checkedAdd(legacySize, 4);
        for (const auto& region : data.regions) legacySize = detail::checkedAdd(legacySize, 12 + static_cast<std::uint64_t>(region.trees.size()) * 17);
        if (legacySize != fileSize) throw std::runtime_error("save size does not match encoded records");
    } else if ((stl8 ? detail::encodedSize(data) : (stl7 ? detail::encodedSize(data)-(data.faunaPopulation ? 4 : 0) : (stl6 ? detail::encodedSize(data)-1 : (stl5 ? detail::encodedSizeStl5(data) : (stl4 ? detail::encodedSizeStl4(data) : (stl3 ? detail::encodedSizeStl3(data) : detail::encodedSizeStl2(data))))))) != fileSize)
        throw std::runtime_error("save size does not match encoded records");
    return data;
}

inline HeroNeedsSnapshot captureHeroNeeds(const needs::NeedsCore& n, Identity heroId) {
    return {heroId, n.species(), n.hunger(), n.thirst(), n.fatigue(), n.injury(), n.isResting()};
}

inline needs::NeedsCore restoreHeroNeeds(const SaveData& data, HeroSpecies legacySpecies) {
    detail::validate(data);
    if (!data.hero) throw std::runtime_error("cannot restore needs without current hero");
    // Old saves may already know this hero's species through retention.
    auto species = legacySpecies;
    for (const auto& retained : data.retained)
        if (retained.id == data.hero->id) species = retained.species;
    // Prefer lifecycle species if present
    if (data.heroLifecycle) species = data.heroLifecycle->species;

    needs::NeedsCore core(data.heroNeeds ? data.heroNeeds->species : species);
    if (data.heroNeeds) {
        core.restoreState(data.heroNeeds->hunger, data.heroNeeds->thirst, data.heroNeeds->fatigue, data.heroNeeds->injury, data.heroNeeds->isResting);
    } // Without needs evidence: known species, defaults to zero.
    return core;
}

inline HeroLifecycleSnapshot captureHeroLifecycle(const Lifecycle& lifecycle, Identity heroId) {
    return {heroId, lifecycle.species(), lifecycle.calendarAge(), lifecycle.stage()};
}

inline Lifecycle restoreHeroLifecycle(const SaveData& data, HeroSpecies legacySpecies) {
    detail::validate(data);
    if (!data.hero) throw std::runtime_error("cannot restore lifecycle without current hero");
    // Old saves may already know this hero's species through retention. Preserve
    // that evidence before using the caller's explicit provisional fallback.
    auto species = legacySpecies;
    for (const auto& retained : data.retained)
        if (retained.id == data.hero->id) species = retained.species;
    Lifecycle lifecycle(data.heroLifecycle ? data.heroLifecycle->species : species);
    if (data.heroLifecycle) {
        lifecycle.restoreState(data.heroLifecycle->species, data.heroLifecycle->calendarAge, data.heroLifecycle->stage);
    } // Without lifecycle evidence: known retained species or supplied fallback, age zero.
    return lifecycle;
}

inline std::optional<ResourceLedgerDto> captureResourceLedger(const ResourceLedger& ledger) {
    return ledger.continuationSnapshot();
}

inline void restoreResourceLedger(const SaveData& data, ResourceLedger& ledger) {
    if (data.resourceLedger) {
        if (!ledger.restore(*data.resourceLedger)) {
            throw std::runtime_error("failed to restore resource ledger from save data");
        }
    } else {
        // Fallback for older saves without resource ledger.
        // It relies on main.cpp to populate initial pools via ledger.load.
        ledger.reset();
    }
}

inline SaveData captureSave(const SimCore& sim, const StalthosState& world,
                            std::optional<HostPose> pose = std::nullopt,
                            std::optional<CameraStateSnapshot> cameraState = std::nullopt) {
    const auto snapshot = world.snapshot();
    SaveData data; data.physicalSeconds = sim.physicalSeconds(); data.calendarSeconds = sim.calendarSeconds();
    data.seed = snapshot.seed; data.retentionLimit = snapshot.retentionLimit; data.pose = pose;
    data.cameraState = cameraState;
    if (snapshot.hero) data.hero = ActiveEntity{snapshot.hero->first, snapshot.hero->second};
    std::set<Identity> retainedIds;
    for (const auto& value : snapshot.retained) {
        retainedIds.insert(value.id);
        data.retained.push_back({value.id, value.region, value.species, value.reason, value.alive, value.active});
    }
    for (const auto& value : snapshot.active)
        if ((!snapshot.hero || snapshot.hero->first != value.first) && !retainedIds.count(value.first))
            data.activeTransients.push_back({value.first, value.second});
    for (const auto& value : snapshot.deadHeroes) data.deadHeroes.push_back({value.first, value.second});
    for (const auto& value : snapshot.regions) data.regions.push_back({value.region, value.trees, value.revision});
    detail::validate(data);
    return data;
}

inline std::pair<SimCore, StalthosState> restoreSave(const SaveData& data) {
    detail::validate(data); // Complete DTO validation precedes reconstruction.
    FoundationSnapshot foundation; foundation.seed = data.seed; foundation.retentionLimit = data.retentionLimit;
    if (data.hero) foundation.hero = std::make_pair(data.hero->id, data.hero->region);
    for (const auto& value : data.activeTransients) foundation.active.emplace_back(value.id, value.region);
    if (data.hero) foundation.active.emplace_back(data.hero->id, data.hero->region);
    for (const auto& value : data.retained)
        foundation.retained.push_back({value.id, value.region, value.species, value.reason, value.alive, value.active});
    for (const auto& value : data.retained)
        if (value.active && (!data.hero || value.id != data.hero->id)) foundation.active.emplace_back(value.id, value.region);
    for (const auto& value : data.deadHeroes) foundation.deadHeroes.emplace_back(value.id, value.region);
    for (const auto& value : data.regions) foundation.regions.push_back({value.region, value.revision, value.trees});
    auto sim = SimCore::fromTimes(data.physicalSeconds, data.calendarSeconds);
    auto state = StalthosState::fromSnapshot(foundation);
    return {std::move(sim), std::move(state)};
}

} // namespace persistence
} // namespace stalthos
