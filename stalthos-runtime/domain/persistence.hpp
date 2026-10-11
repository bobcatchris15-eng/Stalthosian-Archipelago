#pragma once

#include "foundation.hpp"
#include <cstdint>
#include <vector>
#include <optional>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <utility>

namespace stalthos {
namespace persistence {

struct ActiveEntity {
    Identity id;
    RegionId region;
};

struct RetainedEntity {
    Identity id;
    RegionId region;
    HeroSpecies species;
    RetentionReason reason;
    bool alive;
    bool active;
};

struct DeadHero {
    Identity id;
    RegionId region;
};

struct RegionTrees {
    RegionId region;
    std::vector<TreeException> trees;
};

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

// Size bounds to prevent OOM on malformed files
constexpr std::size_t MAX_ACTIVE_TRANSIENTS = 500'000;
constexpr std::size_t MAX_RETAINED = 10'000;
constexpr std::size_t MAX_DEAD_HEROES = 10'000;
constexpr std::size_t MAX_REGIONS = 500'000;
constexpr std::size_t MAX_TREES_PER_REGION = 50'000;

namespace detail {
    inline void write_u64(std::ostream& os, uint64_t v) {
        char buf[8];
        for (int i = 0; i < 8; ++i) buf[i] = static_cast<char>((v >> (i * 8)) & 0xFF);
        os.write(buf, 8);
    }
    inline uint64_t read_u64(std::istream& is) {
        char buf[8];
        if (!is.read(buf, 8)) throw std::runtime_error("truncated file");
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(static_cast<unsigned char>(buf[i])) << (i * 8);
        return v;
    }
    inline void write_u32(std::ostream& os, uint32_t v) {
        char buf[4];
        for (int i = 0; i < 4; ++i) buf[i] = static_cast<char>((v >> (i * 8)) & 0xFF);
        os.write(buf, 4);
    }
    inline uint32_t read_u32(std::istream& is) {
        char buf[4];
        if (!is.read(buf, 4)) throw std::runtime_error("truncated file");
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(static_cast<unsigned char>(buf[i])) << (i * 8);
        return v;
    }
    inline void write_double(std::ostream& os, double v) {
        uint64_t u;
        std::memcpy(&u, &v, sizeof(v));
        write_u64(os, u);
    }
    inline double read_double(std::istream& is) {
        uint64_t u = read_u64(is);
        double v;
        std::memcpy(&v, &u, sizeof(v));
        return v;
    }
    inline void write_u8(std::ostream& os, uint8_t v) {
        char c = static_cast<char>(v);
        os.write(&c, 1);
    }
    inline uint8_t read_u8(std::istream& is) {
        char c;
        if (!is.read(&c, 1)) throw std::runtime_error("truncated file");
        return static_cast<uint8_t>(c);
    }
}

inline void writeSave(const std::filesystem::path& path, const SaveData& data) {
    auto temp_path = path;
    temp_path += ".tmp";

    std::ofstream os(temp_path, std::ios::binary);
    if (!os) throw std::runtime_error("failed to open temp file for save");

    os.write("STL1", 4);
    detail::write_u32(os, 1);

    detail::write_double(os, data.calendarSeconds);
    detail::write_u64(os, data.seed);
    detail::write_u64(os, static_cast<uint64_t>(data.retentionLimit));

    if (data.hero) {
        detail::write_u8(os, 1);
        detail::write_u64(os, data.hero->id);
        detail::write_u64(os, data.hero->region);
    } else {
        detail::write_u8(os, 0);
    }

    if (data.activeTransients.size() > MAX_ACTIVE_TRANSIENTS) throw std::runtime_error("too many active transients");
    detail::write_u32(os, static_cast<uint32_t>(data.activeTransients.size()));
    for (const auto& a : data.activeTransients) {
        detail::write_u64(os, a.id);
        detail::write_u64(os, a.region);
    }

    if (data.retained.size() > MAX_RETAINED) throw std::runtime_error("too many retained entities");
    detail::write_u32(os, static_cast<uint32_t>(data.retained.size()));
    for (const auto& r : data.retained) {
        detail::write_u64(os, r.id);
        detail::write_u64(os, r.region);
        detail::write_u8(os, static_cast<uint8_t>(r.species));
        detail::write_u8(os, static_cast<uint8_t>(r.reason));
        detail::write_u8(os, r.alive ? 1 : 0);
        detail::write_u8(os, r.active ? 1 : 0);
    }

    if (data.deadHeroes.size() > MAX_DEAD_HEROES) throw std::runtime_error("too many dead heroes");
    detail::write_u32(os, static_cast<uint32_t>(data.deadHeroes.size()));
    for (const auto& dh : data.deadHeroes) {
        detail::write_u64(os, dh.id);
        detail::write_u64(os, dh.region);
    }

    if (data.regions.size() > MAX_REGIONS) throw std::runtime_error("too many regions");
    detail::write_u32(os, static_cast<uint32_t>(data.regions.size()));
    for (const auto& reg : data.regions) {
        detail::write_u64(os, reg.region);
        if (reg.trees.size() > MAX_TREES_PER_REGION) throw std::runtime_error("too many trees in region");
        detail::write_u32(os, static_cast<uint32_t>(reg.trees.size()));
        for (const auto& t : reg.trees) {
            detail::write_u64(os, t.tree);
            detail::write_u8(os, static_cast<uint8_t>(t.condition));
            detail::write_double(os, t.changedAt);
        }
    }

    os.flush();
    if (!os.good()) {
        os.close();
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        throw std::runtime_error("failed to write save data completely");
    }
    os.close();

    std::error_code ec;
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        throw std::runtime_error("failed to commit save atomically");
    }
}

inline SaveData readSave(const std::filesystem::path& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is) throw std::runtime_error("failed to open save file");

    char magic[4];
    if (!is.read(magic, 4) || std::strncmp(magic, "STL1", 4) != 0) {
        throw std::runtime_error("invalid magic or corrupt file");
    }

    uint32_t version = detail::read_u32(is);
    if (version != 1) {
        throw std::runtime_error("unsupported save version");
    }

    SaveData data;
    data.calendarSeconds = detail::read_double(is);
    if (!std::isfinite(data.calendarSeconds) || data.calendarSeconds < 0) {
        throw std::runtime_error("invalid calendar");
    }

    data.seed = detail::read_u64(is);
    data.retentionLimit = static_cast<std::size_t>(detail::read_u64(is));

    uint8_t hasHero = detail::read_u8(is);
    if (hasHero > 1) throw std::runtime_error("corrupt hero presence flag");
    if (hasHero) {
        ActiveEntity h;
        h.id = detail::read_u64(is);
        h.region = detail::read_u64(is);
        data.hero = h;
    }

    uint32_t numActive = detail::read_u32(is);
    if (numActive > MAX_ACTIVE_TRANSIENTS) throw std::runtime_error("active transients exceed size bound");
    data.activeTransients.reserve(numActive);
    for (uint32_t i = 0; i < numActive; ++i) {
        ActiveEntity a;
        a.id = detail::read_u64(is);
        a.region = detail::read_u64(is);
        data.activeTransients.push_back(a);
    }

    uint32_t numRetained = detail::read_u32(is);
    if (numRetained > MAX_RETAINED) throw std::runtime_error("retained exceed size bound");
    data.retained.reserve(numRetained);
    for (uint32_t i = 0; i < numRetained; ++i) {
        RetainedEntity r;
        r.id = detail::read_u64(is);
        r.region = detail::read_u64(is);

        uint8_t sp = detail::read_u8(is);
        if (sp > static_cast<uint8_t>(HeroSpecies::Longstride)) throw std::runtime_error("invalid species");
        r.species = static_cast<HeroSpecies>(sp);

        uint8_t rs = detail::read_u8(is);
        if (rs > static_cast<uint8_t>(RetentionReason::NotableEncounter)) throw std::runtime_error("invalid retention reason");
        r.reason = static_cast<RetentionReason>(rs);

        uint8_t alive = detail::read_u8(is);
        if (alive > 1) throw std::runtime_error("invalid alive flag");
        r.alive = (alive == 1);

        uint8_t active = detail::read_u8(is);
        if (active > 1) throw std::runtime_error("invalid active flag");
        r.active = (active == 1);

        data.retained.push_back(r);
    }

    uint32_t numDeadHeroes = detail::read_u32(is);
    if (numDeadHeroes > MAX_DEAD_HEROES) throw std::runtime_error("dead heroes exceed size bound");
    data.deadHeroes.reserve(numDeadHeroes);
    for (uint32_t i = 0; i < numDeadHeroes; ++i) {
        DeadHero dh;
        dh.id = detail::read_u64(is);
        dh.region = detail::read_u64(is);
        data.deadHeroes.push_back(dh);
    }

    uint32_t numRegions = detail::read_u32(is);
    if (numRegions > MAX_REGIONS) throw std::runtime_error("regions exceed size bound");
    data.regions.reserve(numRegions);
    for (uint32_t i = 0; i < numRegions; ++i) {
        RegionTrees reg;
        reg.region = detail::read_u64(is);

        uint32_t numTrees = detail::read_u32(is);
        if (numTrees > MAX_TREES_PER_REGION) throw std::runtime_error("trees per region exceed size bound");
        reg.trees.reserve(numTrees);
        for (uint32_t j = 0; j < numTrees; ++j) {
            TreeException t;
            t.tree = detail::read_u64(is);

            uint8_t cond = detail::read_u8(is);
            if (cond > static_cast<uint8_t>(TreeCondition::Dead)) throw std::runtime_error("invalid tree condition");
            t.condition = static_cast<TreeCondition>(cond);

            t.changedAt = detail::read_double(is);
            if (!std::isfinite(t.changedAt) || t.changedAt < 0) throw std::runtime_error("invalid tree timestamp");

            reg.trees.push_back(t);
        }
        data.regions.push_back(reg);
    }

    // Check for trailing garbage
    char dummy;
    if (is.read(&dummy, 1)) {
        throw std::runtime_error("trailing bytes at end of file");
    }

    return data;
}

inline std::pair<SimCore, StalthosState> restoreSave(const SaveData& data) {
    SimCore sim;
    if (data.calendarSeconds > 0) {
        sim.advance(data.calendarSeconds, 1.0);
    }

    StalthosState state(data.seed, data.retentionLimit);

    for (const auto& dh : data.deadHeroes) {
        state.setHero(dh.id, dh.region);
        state.heroDied();
    }

    for (const auto& r : data.retained) {
        state.instantiate(r.id, r.region);
        state.retain(r.id, r.species, r.reason);
        if (!r.alive) {
            state.markRetainedDead(r.id);
        } else if (!r.active) {
            state.retire(r.id);
        }
    }

    for (const auto& a : data.activeTransients) {
        state.instantiate(a.id, a.region);
    }

    for (const auto& reg : data.regions) {
        for (const auto& t : reg.trees) {
            state.recordTree(reg.region, t);
        }
    }

    if (data.hero) {
        state.setHero(data.hero->id, data.hero->region);
    }

    return {std::move(sim), std::move(state)};
}

} // namespace persistence
} // namespace stalthos