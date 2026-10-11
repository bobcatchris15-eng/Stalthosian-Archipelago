#include "../domain/persistence.hpp"
#include <iostream>
#include <functional>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <cmath>

using namespace stalthos;
using namespace stalthos::persistence;

namespace {

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

bool near(double a, double b) {
    return std::abs(a - b) < 1e-10;
}

template<class F> void rejects(F action, const char* expected_message = nullptr) {
    bool thrown = false;
    try {
        action();
    } catch (const std::exception& e) {
        thrown = true;
        if (expected_message) {
            std::string msg(e.what());
            if (msg.find(expected_message) == std::string::npos) {
                std::cerr << "Expected exception message to contain: '" << expected_message
                          << "', but got: '" << msg << "'\n";
                throw;
            }
        }
    }
    check(thrown, "invalid operation accepted");
}

void testRoundtrip() {
    SaveData data;
    data.calendarSeconds = 42.5;
    data.seed = 999;
    data.retentionLimit = 128;

    data.hero = ActiveEntity{1, 10};

    data.activeTransients.push_back({2, 10});
    data.activeTransients.push_back({3, 11});

    data.retained.push_back({4, 10, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true});
    data.retained.push_back({5, 11, HeroSpecies::Reedcrest, RetentionReason::DependentYoung, false, false});

    data.deadHeroes.push_back({6, 12});

    RegionTrees rt;
    rt.region = 10;
    rt.trees.push_back({100, TreeCondition::Fallen, 10.0});
    rt.trees.push_back({101, TreeCondition::StandingDamaged, 20.0});
    data.regions.push_back(rt);

    auto path = std::filesystem::path("test_roundtrip.sav");
    writeSave(path, data);

    auto loaded = readSave(path);

    check(near(loaded.calendarSeconds, data.calendarSeconds), "calendar mismatch");
    check(loaded.seed == data.seed, "seed mismatch");
    check(loaded.retentionLimit == data.retentionLimit, "retentionLimit mismatch");

    check(loaded.hero.has_value() && loaded.hero->id == 1 && loaded.hero->region == 10, "hero mismatch");

    check(loaded.activeTransients.size() == 2, "active transients count mismatch");
    check(loaded.activeTransients[0].id == 2 && loaded.activeTransients[0].region == 10, "transient 0 mismatch");
    check(loaded.activeTransients[1].id == 3 && loaded.activeTransients[1].region == 11, "transient 1 mismatch");

    check(loaded.retained.size() == 2, "retained count mismatch");
    check(loaded.retained[0].id == 4 && loaded.retained[0].species == HeroSpecies::Smalltrotter &&
          loaded.retained[0].reason == RetentionReason::Caregiver && loaded.retained[0].alive && loaded.retained[0].active, "retained 0 mismatch");
    check(loaded.retained[1].id == 5 && loaded.retained[1].species == HeroSpecies::Reedcrest &&
          loaded.retained[1].reason == RetentionReason::DependentYoung && !loaded.retained[1].alive && !loaded.retained[1].active, "retained 1 mismatch");

    check(loaded.deadHeroes.size() == 1, "dead heroes count mismatch");
    check(loaded.deadHeroes[0].id == 6 && loaded.deadHeroes[0].region == 12, "dead hero 0 mismatch");

    check(loaded.regions.size() == 1, "regions count mismatch");
    check(loaded.regions[0].region == 10, "region id mismatch");
    check(loaded.regions[0].trees.size() == 2, "trees count mismatch");
    check(loaded.regions[0].trees[0].tree == 100 && loaded.regions[0].trees[0].condition == TreeCondition::Fallen && near(loaded.regions[0].trees[0].changedAt, 10.0), "tree 0 mismatch");
    check(loaded.regions[0].trees[1].tree == 101 && loaded.regions[0].trees[1].condition == TreeCondition::StandingDamaged && near(loaded.regions[0].trees[1].changedAt, 20.0), "tree 1 mismatch");

    std::filesystem::remove(path);
}

void testValidation() {
    auto path = std::filesystem::path("test_validation.sav");

    // Corrupt magic
    {
        std::ofstream os(path, std::ios::binary);
        os.write("BAD1", 4);
    }
    rejects([&] { readSave(path); }, "invalid magic or corrupt file");

    // Unknown version
    {
        std::ofstream os(path, std::ios::binary);
        os.write("STL1", 4);
        detail::write_u32(os, 2);
    }
    rejects([&] { readSave(path); }, "unsupported save version");

    // Truncated file
    {
        std::ofstream os(path, std::ios::binary);
        os.write("STL1", 4);
        detail::write_u32(os, 1);
        detail::write_double(os, 42.0); // Only part of the file
    }
    rejects([&] { readSave(path); }, "truncated file");

    // Invalid species
    {
        SaveData data;
        data.retained.push_back({1, 10, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true});
        writeSave(path, data);

        // Corrupt the species byte directly in file
        std::fstream fs(path, std::ios::in | std::ios::out | std::ios::binary);
        fs.seekp(4 + 4 + 8 + 8 + 8 + 1 + 4 + 4 + 8 + 8, std::ios::beg); // offset to species
        detail::write_u8(fs, 99);
    }
    rejects([&] { readSave(path); }, "invalid species");

    std::filesystem::remove(path);
}

void testSizeBounds() {
    auto path = std::filesystem::path("test_bounds.sav");

    // Test writing beyond bound
    SaveData data;
    data.activeTransients.resize(MAX_ACTIVE_TRANSIENTS + 1);
    rejects([&] { writeSave(path, data); }, "too many active transients");

    // Test reading beyond bound (falsify count in file to prevent OOM)
    data.activeTransients.clear();
    writeSave(path, data);
    {
        std::fstream fs(path, std::ios::in | std::ios::out | std::ios::binary);
        fs.seekp(4 + 4 + 8 + 8 + 8 + 1, std::ios::beg); // offset to activeTransients count
        detail::write_u32(fs, static_cast<uint32_t>(MAX_ACTIVE_TRANSIENTS + 1));
    }
    rejects([&] { readSave(path); }, "active transients exceed size bound");

    std::filesystem::remove(path);
}

void testFailurePreservation() {
    auto path = std::filesystem::path("test_preservation.sav");

    SaveData data1;
    data1.calendarSeconds = 10.0;
    writeSave(path, data1);

    // We can simulate failure by making temp directory path or locking it,
    // but the logic relies on exceptions thrown before atomic rename.
    // Let's test that if we fail to write, the original is intact.
    SaveData data2;
    data2.activeTransients.resize(MAX_ACTIVE_TRANSIENTS + 1);
    rejects([&] { writeSave(path, data2); }, "too many active transients");

    // Original should still be valid
    auto loaded = readSave(path);
    check(near(loaded.calendarSeconds, 10.0), "original file corrupted");

    // Clean up
    std::filesystem::remove(path);
}

void testRebuildState() {
    SaveData data;
    data.calendarSeconds = 100.0;
    data.seed = 42;
    data.retentionLimit = 64;

    data.activeTransients.push_back({1, 10});
    data.retained.push_back({2, 10, HeroSpecies::Reedcrest, RetentionReason::BondedPeer, true, false}); // inactive retained
    data.deadHeroes.push_back({3, 11});
    data.hero = ActiveEntity{4, 12};

    RegionTrees rt;
    rt.region = 10;
    rt.trees.push_back({100, TreeCondition::Fallen, 50.0});
    data.regions.push_back(rt);

    auto [sim, state] = restoreSave(data);

    check(near(sim.calendarSeconds(), 100.0), "sim calendar not restored");
    check(state.worldSeed() == 42, "state seed not restored");

    check(state.excluded(1), "transient not instantiated");
    check(state.excluded(2), "retained not instantiated");
    check(state.excluded(3), "dead hero not excluded");
    check(state.excluded(4), "hero not instantiated");

    auto ret = state.retained(2);
    check(ret != nullptr && ret->species == HeroSpecies::Reedcrest && !ret->active, "retained state incorrect");

    check(state.heroDeathKnown(3), "dead hero death truth lost");

    auto snap = state.regionSnapshot(10);
    check(snap.trees.size() == 1 && snap.trees[0].condition == TreeCondition::Fallen, "tree exception not restored");
}

}

int main() {
    const std::pair<const char*, std::function<void()>> scenarios[] = {
        {"roundtrip serialization", testRoundtrip},
        {"malformed save validation", testValidation},
        {"size bounds protection", testSizeBounds},
        {"failure preservation", testFailurePreservation},
        {"rebuild domain state", testRebuildState}
    };
    int failed = 0;
    for (auto& scenario : scenarios) {
        try {
            scenario.second();
            std::cout << "PASS " << scenario.first << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "FAIL " << scenario.first << ": " << e.what() << '\n';
        }
    }
    std::cout << (5 - failed) << "/5 tests passed\n";
    return failed ? 1 : 0;
}