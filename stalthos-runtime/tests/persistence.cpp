#include "../domain/persistence.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stalthos;
using namespace stalthos::persistence;

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-10; }

template<class F> void rejects(F action, const char* fragment = nullptr) {
    try { action(); }
    catch (const std::exception& e) {
        if (fragment && std::string(e.what()).find(fragment) == std::string::npos)
            throw std::runtime_error(std::string("unexpected failure: ") + e.what());
        return;
    }
    throw std::runtime_error("invalid operation accepted");
}

std::vector<char> bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void overwriteU64(const std::filesystem::path& path, std::streamoff offset, std::uint64_t value) {
    std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
    io.seekp(offset);
    detail::write_u64(io, value);
    io.flush();
    check(io.good(), "failed to mutate fixture");
}

void testRoundtripAndDeterminism() {
    const auto path = std::filesystem::path("test_roundtrip.sav");
    const auto path2 = std::filesystem::path("test_roundtrip_2.sav");
    SaveData data;
    data.calendarSeconds = 42.5; data.seed = 999; data.retentionLimit = 128;
    data.hero = ActiveEntity{1, 10};
    data.activeTransients = {{3, 11}, {2, 10}};
    data.retained = {
        {5, 11, HeroSpecies::Reedcrest, RetentionReason::DependentYoung, false, false},
        {4, 10, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true}};
    data.deadHeroes.push_back({6, 12});
    data.regions.push_back({10, {{101, TreeCondition::StandingDamaged, 20.0}, {100, TreeCondition::Fallen, 10.0}}});
    writeSave(path, data);
    writeSave(path2, data);
    check(bytes(path) == bytes(path2), "same DTO did not serialize deterministically");

    auto loaded = readSave(path);
    check(near(loaded.calendarSeconds, 42.5) && loaded.seed == 999 && loaded.retentionLimit == 128, "header fields mismatch");
    check(loaded.hero && loaded.hero->id == 1 && loaded.hero->region == 10, "hero mismatch");
    check(loaded.activeTransients.size() == 2 && loaded.activeTransients[0].id == 2 && loaded.activeTransients[1].id == 3, "active order/fields mismatch");
    check(loaded.retained.size() == 2 && loaded.retained[0].id == 4 && loaded.retained[1].id == 5, "retained order/fields mismatch");
    check(loaded.retained[0].species == HeroSpecies::Smalltrotter && loaded.retained[0].reason == RetentionReason::Caregiver && loaded.retained[0].alive && loaded.retained[0].active, "retained state mismatch");
    check(!loaded.retained[1].alive && !loaded.retained[1].active, "dead retained state mismatch");
    check(loaded.deadHeroes.size() == 1 && loaded.deadHeroes[0].id == 6, "dead hero mismatch");
    check(loaded.regions.size() == 1 && loaded.regions[0].trees.size() == 2 && loaded.regions[0].trees[0].tree == 100, "tree order/fields mismatch");
    std::filesystem::remove(path); std::filesystem::remove(path2);
}

void testWriteAndRestoreValidation() {
    const auto path = std::filesystem::path("test_validation.sav");
    SaveData baseline; baseline.calendarSeconds = 10.0;
    writeSave(path, baseline);
    const auto original = bytes(path);
    const auto preserved = [&](SaveData invalid, const char* expected) {
        rejects([&] { writeSave(path, invalid); }, expected);
        check(bytes(path) == original, "invalid write changed previous save");
        check(!std::filesystem::exists(path.string() + ".tmp"), "invalid write left a temp file");
        rejects([&] { (void)restoreSave(invalid); }, expected);
    };

    SaveData invalid = baseline; invalid.retentionLimit = 1;
    invalid.retained = {{2, 10, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true},
                        {3, 10, HeroSpecies::Reedcrest, RetentionReason::BondedPeer, true, true}};
    preserved(invalid, "retained records exceed retention limit");
    invalid = baseline; invalid.hero = ActiveEntity{9, 10}; invalid.activeTransients.push_back({9, 11});
    preserved(invalid, "duplicate or conflicting entity identity");
    invalid = baseline; invalid.retained.push_back({9, 10, static_cast<HeroSpecies>(200), RetentionReason::Caregiver, true, true});
    preserved(invalid, "invalid species");
    invalid = baseline; invalid.regions.push_back({2, {{99, TreeCondition::Fallen, 1.0}}});
    invalid.regions.push_back({2, {{100, TreeCondition::Dead, 2.0}}});
    preserved(invalid, "duplicate region record");
    invalid = baseline; invalid.regions.push_back({2, {{99, TreeCondition::Fallen, 1.0},
                                                      {99, TreeCondition::Dead, 2.0}}});
    preserved(invalid, "duplicate or conflicting tree reference");
    invalid = baseline; invalid.regions.push_back({2, {{99, TreeCondition::Fallen, -1.0}}});
    preserved(invalid, "invalid tree timestamp");
    invalid = baseline; invalid.regions.push_back({2, {{99, static_cast<TreeCondition>(99), 1.0}}});
    preserved(invalid, "invalid tree condition");
    invalid = baseline; invalid.retained.push_back({9, 10, HeroSpecies::Smalltrotter, static_cast<RetentionReason>(99), true, true});
    preserved(invalid, "invalid retention reason");
    invalid = baseline; invalid.calendarSeconds = std::numeric_limits<double>::infinity();
    preserved(invalid, "invalid calendar");
    invalid = baseline; invalid.calendarSeconds = -1.0;
    preserved(invalid, "invalid calendar");
    invalid = baseline; invalid.retained.push_back({9, 10, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, false, true});
    preserved(invalid, "dead retained entity cannot be active");
    std::filesystem::remove(path);
}

void testMalformedFilesAndBounds() {
    const auto path = std::filesystem::path("test_malformed.sav");
    SaveData data; data.retentionLimit = 2;
    data.retained.push_back({3, 4, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true});
    writeSave(path, data);
    // retentionLimit is the u64 beginning at byte 24. Lower it below the stored count.
    overwriteU64(path, 24, 0);
    rejects([&] { (void)readSave(path); }, "retained records exceed retention");

    data = SaveData{}; data.activeTransients = {{20, 1}, {21, 2}};
    writeSave(path, data);
    // With no hero, the active count begins at byte 33; replace the second ID with the first ID.
    overwriteU64(path, 53, 20);
    rejects([&] { (void)readSave(path); }, "duplicate or conflicting entity identity");

    data = SaveData{};
    data.retained.push_back({3, 4, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true});
    writeSave(path, data);
    // The retained active flag follows ID, region, species, reason, and alive at byte 60.
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(60); detail::write_u8(io, 2); io.flush();
        check(io.good(), "failed to mutate retained boolean fixture");
    }
    rejects([&] { (void)readSave(path); }, "invalid retained boolean flag");

    // A sparse over-limit file must be rejected from file metadata before parsing or allocation.
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.seekp(static_cast<std::streamoff>(MAX_SAVE_BYTES)); out.put('\0');
    }
    rejects([&] { (void)readSave(path); }, "byte size bound");

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write("BAD1", 4);
    }
    rejects([&] { (void)readSave(path); }, "invalid magic");
    std::filesystem::remove(path);
}

void testWriteFailurePreservesPriorSave() {
    const auto path = std::filesystem::path("test_preservation.sav");
    SaveData prior; prior.calendarSeconds = 10.0; writeSave(path, prior);
    const auto original = bytes(path);
    SaveData replacement; replacement.calendarSeconds = 20.0;
    // On Windows, a directory at the temporary-file path makes opening the temp fail.
    const auto temp = std::filesystem::path(path.string() + ".tmp");
    std::filesystem::create_directory(temp);
    rejects([&] { writeSave(path, replacement); }, "open temp file");
    check(bytes(path) == original, "failed write damaged prior save");
    check(!std::filesystem::exists(temp), "failed write left temp path behind");
    check(near(readSave(path).calendarSeconds, 10.0), "prior save no longer reads");
    std::filesystem::remove(path);
}

void testRestoreStatusesAndDomainState() {
    SaveData data;
    data.calendarSeconds = 100.0; data.seed = 42; data.retentionLimit = 64;
    data.activeTransients.push_back({1, 10});
    data.retained.push_back({2, 10, HeroSpecies::Reedcrest, RetentionReason::BondedPeer, true, false});
    data.deadHeroes.push_back({3, 11}); data.hero = ActiveEntity{4, 12};
    data.regions.push_back({10, {{100, TreeCondition::Fallen, 50.0}}});
    data.regions.push_back({11, {{100, TreeCondition::Dead, 51.0}}});
    auto restored = restoreSave(data);
    auto& sim = restored.first; auto& state = restored.second;
    check(near(sim.calendarSeconds(), 100.0) && state.worldSeed() == 42, "calendar or seed not restored");
    check(state.excluded(1) && state.excluded(2) && state.excluded(3) && state.excluded(4), "entity identities not restored");
    const auto* retained = state.retained(2);
    check(retained && retained->species == HeroSpecies::Reedcrest && !retained->active, "retained record not restored");
    check(state.heroDeathKnown(3), "dead hero truth not restored");
    const auto snapshot = state.regionSnapshot(10);
    check(snapshot.trees.size() == 1 && snapshot.trees[0].condition == TreeCondition::Fallen, "tree exception not restored");

    check(state.regionSnapshot(11).trees.size() == 1, "region-local tree identity was lost");

    // Every status-bearing mutation is checked during restore. Capacity mismatch is rejected before reconstruction.
    data.retentionLimit = 0;
    rejects([&] { (void)restoreSave(data); }, "retained records exceed retention limit");
}
} // namespace

int main() {
    const std::pair<const char*, std::function<void()>> scenarios[] = {
        {"roundtrip and deterministic ordering", testRoundtripAndDeterminism},
        {"write and restore validation", testWriteAndRestoreValidation},
        {"malformed files and aggregate bounds", testMalformedFilesAndBounds},
        {"write failure preserves prior save", testWriteFailurePreservesPriorSave},
        {"restore checks mutation statuses", testRestoreStatusesAndDomainState}
    };
    int failed = 0;
    for (const auto& scenario : scenarios) {
        try { scenario.second(); std::cout << "PASS " << scenario.first << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << scenario.first << ": " << e.what() << '\n'; }
    }
    std::cout << (5 - failed) << "/5 tests passed\n";
    return failed ? 1 : 0;
}
