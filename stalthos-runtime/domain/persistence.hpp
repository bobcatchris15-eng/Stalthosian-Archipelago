#pragma once

#include "foundation.hpp"
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
struct RegionTrees { RegionId region; std::vector<TreeException> trees; };
struct SaveData {
    double calendarSeconds = 0.0;
    std::uint64_t seed = 0;
    std::size_t retentionLimit = 64;
    std::optional<ActiveEntity> hero;
    std::vector<ActiveEntity> activeTransients;
    std::vector<RetainedEntity> retained;
    std::vector<DeadHero> deadHeroes;
    std::vector<RegionTrees> regions;
};

constexpr std::size_t MAX_ACTIVE_TRANSIENTS = 500'000;
constexpr std::size_t MAX_RETAINED = 10'000;
constexpr std::size_t MAX_DEAD_HEROES = 10'000;
constexpr std::size_t MAX_REGIONS = 100'000;
constexpr std::size_t MAX_TREES_PER_REGION = 50'000;
constexpr std::size_t MAX_TOTAL_TREES = 1'000'000;
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
    // Header plus fixed fields and collection counts; tree records are 17 bytes.
    std::uint64_t size = 8 + 8 + 8 + 8 + 1 + (data.hero ? 16 : 0) + 4;
    size = checkedAdd(size, static_cast<std::uint64_t>(data.activeTransients.size()) * 16);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.retained.size()) * 20);
    size = checkedAdd(size, 4 + static_cast<std::uint64_t>(data.deadHeroes.size()) * 16);
    size = checkedAdd(size, 4);
    for (const auto& region : data.regions)
        size = checkedAdd(size, 12 + static_cast<std::uint64_t>(region.trees.size()) * 17);
    if (size > MAX_SAVE_BYTES) throw std::runtime_error("save exceeds byte size bound");
    return size;
}

inline std::uint64_t serializedFileSize(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) throw std::runtime_error("failed to inspect save file size");
    if (size > MAX_SAVE_BYTES) throw std::runtime_error("save exceeds byte size bound");
    return size;
}

inline void validate(const SaveData& data) {
    if (!std::isfinite(data.calendarSeconds) || data.calendarSeconds < 0)
        throw std::runtime_error("invalid calendar");
    if (data.retentionLimit > MAX_RETAINED) throw std::runtime_error("retention limit exceeds size bound");
    if (data.retained.size() > data.retentionLimit) throw std::runtime_error("retained records exceed retention limit");
    if (data.activeTransients.size() > MAX_ACTIVE_TRANSIENTS) throw std::runtime_error("too many active transients");
    if (data.retained.size() > MAX_RETAINED) throw std::runtime_error("too many retained entities");
    if (data.deadHeroes.size() > MAX_DEAD_HEROES) throw std::runtime_error("too many dead heroes");
    if (data.regions.size() > MAX_REGIONS) throw std::runtime_error("too many regions");

    std::set<Identity> entityIds;
    if (data.hero) claimIdentity(entityIds, data.hero->id);
    for (const auto& value : data.activeTransients) claimIdentity(entityIds, value.id);
    for (const auto& value : data.retained) {
        claimIdentity(entityIds, value.id);
        if (!valid(value.species)) throw std::runtime_error("invalid species");
        if (!valid(value.reason)) throw std::runtime_error("invalid retention reason");
        if (value.active && !value.alive) throw std::runtime_error("dead retained entity cannot be active");
    }
    for (const auto& value : data.deadHeroes) claimIdentity(entityIds, value.id);

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

    os.write("STL1", 4);
    detail::write_u32(os, 1);
    detail::write_double(os, data.calendarSeconds);
    detail::write_u64(os, data.seed);
    detail::write_u64(os, static_cast<std::uint64_t>(data.retentionLimit));
    detail::write_u8(os, data.hero ? 1 : 0);
    if (data.hero) { detail::write_u64(os, data.hero->id); detail::write_u64(os, data.hero->region); }

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
    if (!is.read(magic, 4) || std::strncmp(magic, "STL1", 4) != 0)
        throw std::runtime_error("invalid magic or corrupt file");
    if (detail::read_u32(is) != 1) throw std::runtime_error("unsupported save version");

    SaveData data;
    data.calendarSeconds = detail::read_double(is);
    data.seed = detail::read_u64(is);
    const auto limit = detail::read_u64(is);
    if (limit > MAX_RETAINED || limit > std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("retention limit exceeds size bound");
    data.retentionLimit = static_cast<std::size_t>(limit);
    const auto hasHero = detail::read_u8(is);
    if (hasHero > 1) throw std::runtime_error("corrupt hero presence flag");
    if (hasHero) data.hero = ActiveEntity{detail::read_u64(is), detail::read_u64(is)};

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
        data.regions.push_back(std::move(region));
    }
    char trailing;
    if (is.read(&trailing, 1)) throw std::runtime_error("trailing bytes at end of file");
    if (is.bad()) throw std::runtime_error("failed while reading save file");
    detail::validate(data);
    if (detail::encodedSize(data) != fileSize) throw std::runtime_error("save size does not match encoded records");
    return data;
}

inline std::pair<SimCore, StalthosState> restoreSave(const SaveData& data) {
    detail::validate(data); // Validate the whole snapshot before constructing any state.
    SimCore sim;
    if (data.calendarSeconds > 0) sim.advance(data.calendarSeconds, 1.0);
    StalthosState state(data.seed, data.retentionLimit);
    for (const auto& value : data.deadHeroes) {
        state.setHero(value.id, value.region);
        state.heroDied();
    }
    for (const auto& value : data.retained) {
        if (!state.instantiate(value.id, value.region)) throw std::runtime_error("failed to instantiate retained entity");
        if (!state.retain(value.id, value.species, value.reason)) throw std::runtime_error("failed to restore retained entity");
        if (!value.alive) {
            if (!state.markRetainedDead(value.id)) throw std::runtime_error("failed to restore retained death state");
        } else if (!value.active) {
            state.retire(value.id);
        }
    }
    for (const auto& value : data.activeTransients)
        if (!state.instantiate(value.id, value.region)) throw std::runtime_error("failed to instantiate active transient");
    for (const auto& region : data.regions)
        for (const auto& tree : region.trees) state.recordTree(region.region, tree);
    if (data.hero) state.setHero(data.hero->id, data.hero->region);
    return {std::move(sim), std::move(state)};
}

} // namespace persistence
} // namespace stalthos
