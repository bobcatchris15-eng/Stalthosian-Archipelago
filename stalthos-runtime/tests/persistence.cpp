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
    data.physicalSeconds = 2.5; data.calendarSeconds = 42.5; data.seed = 999; data.retentionLimit = 128;
    data.hero = ActiveEntity{1, 10};
    data.activeTransients = {{3, 11}, {2, 10}};
    data.retained = {
        {5, 11, HeroSpecies::Reedcrest, RetentionReason::DependentYoung, false, false},
        {4, 10, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true}};
    data.deadHeroes.push_back({6, 12});
    data.regions.push_back({10, {{101, TreeCondition::StandingDamaged, 20.0}, {100, TreeCondition::Fallen, 10.0}}, 5});
    data.pose = HostPose{{1, 2, 3}, .5, 2};
    writeSave(path, data);
    writeSave(path2, data);
    check(bytes(path) == bytes(path2), "same DTO did not serialize deterministically");

    auto loaded = readSave(path);
    check(near(loaded.physicalSeconds, 2.5) && near(loaded.calendarSeconds, 42.5) && loaded.seed == 999 && loaded.retentionLimit == 128, "header fields mismatch");
    check(loaded.hero && loaded.hero->id == 1 && loaded.hero->region == 10, "hero mismatch");
    check(loaded.activeTransients.size() == 2 && loaded.activeTransients[0].id == 2 && loaded.activeTransients[1].id == 3, "active order/fields mismatch");
    check(loaded.retained.size() == 2 && loaded.retained[0].id == 4 && loaded.retained[1].id == 5, "retained order/fields mismatch");
    check(loaded.retained[0].species == HeroSpecies::Smalltrotter && loaded.retained[0].reason == RetentionReason::Caregiver && loaded.retained[0].alive && loaded.retained[0].active, "retained state mismatch");
    check(!loaded.retained[1].alive && !loaded.retained[1].active, "dead retained state mismatch");
    check(loaded.deadHeroes.size() == 1 && loaded.deadHeroes[0].id == 6, "dead hero mismatch");
    check(loaded.regions[0].revision == 5 && loaded.pose && loaded.pose->cameraMode == 2 && near(loaded.pose->position.z, 3), "revision or pose mismatch");
    check(loaded.regions.size() == 1 && loaded.regions[0].trees.size() == 2 && loaded.regions[0].trees[0].tree == 100, "tree order/fields mismatch");
    std::filesystem::remove(path); std::filesystem::remove(path2);
}

void testWriteAndRestoreValidation() {
    const auto path = std::filesystem::path("test_validation.sav");
    SaveData baseline; baseline.calendarSeconds = 10.0;
    writeSave(path, baseline);
    const auto original = bytes(path);
    const auto preserved = [&](SaveData invalid, const char* expected) {
        try { rejects([&] { writeSave(path, invalid); }, expected); }
        catch (const std::exception& e) { throw std::runtime_error(std::string("expected [") + expected + "] but " + e.what()); }
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
    invalid = baseline; invalid.retained = {
        {9, 10, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true},
        {9, 10, HeroSpecies::Reedcrest, RetentionReason::BondedPeer, true, true}};
    preserved(invalid, "duplicate or conflicting entity identity");
    invalid = baseline; invalid.hero = ActiveEntity{9, 10};
    invalid.retained.push_back({9, 11, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true});
    preserved(invalid, "inconsistent retained hero");
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
    // STL2 retentionLimit begins at byte 32. Lower it below the stored count.
    overwriteU64(path, 32, 0);
    rejects([&] { (void)readSave(path); }, "retained records exceed retention");

    data = SaveData{}; data.activeTransients = {{20, 1}, {21, 2}};
    writeSave(path, data);
    // STL3 lifecycle presence adds one byte before the active list.
    overwriteU64(path, 63, 20);
    rejects([&] { (void)readSave(path); }, "duplicate or conflicting entity identity");

    data = SaveData{};
    data.retained.push_back({3, 4, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true});
    writeSave(path, data);
    // The retained active flag follows the STL3 header and record fields at byte 70.
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(70); detail::write_u8(io, 2); io.flush();
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
    data.regions.push_back({10, {{100, TreeCondition::Fallen, 50.0}}, 7});
    data.regions.push_back({11, {{100, TreeCondition::Dead, 51.0}}, 9});
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
    check(state.regionSnapshot(10).revision == 7 && state.regionSnapshot(11).revision == 9, "region revisions not restored exactly");

    // Every status-bearing mutation is checked during restore. Capacity mismatch is rejected before reconstruction.
    data.retentionLimit = 0;
    rejects([&] { (void)restoreSave(data); }, "retained records exceed retention limit");
}

void testAuthoritativeCaptureRoundtrip() {
    SimCore sim; sim.advance(1.25, 3600.0);
    StalthosState source(1234, 8);
    source.recordTree(10, {70, TreeCondition::Fallen, 12});
    source.recordTree(10, {70, TreeCondition::Dead, 13});
    source.recordTree(11, {70, TreeCondition::StandingDamaged, 14});
    source.setHero(1, 10);
    check(source.retain(1, HeroSpecies::Smalltrotter, RetentionReason::BondedPeer), "retained current hero setup failed");
    source.heroDied();
    source.setHero(2, 11);
    check(source.retain(2, HeroSpecies::Reedcrest, RetentionReason::Caregiver), "retained successor setup failed");
    check(source.instantiate(3, 12), "transient setup failed");
    auto snapshot = source.snapshot(); snapshot.regions.push_back({99, 1000, {}});
    auto world = StalthosState::fromSnapshot(snapshot);
    const HostPose pose{{-1, 2.5, 8}, -0.25, 1};
    auto captured = captureSave(sim, world, pose);
    check(captured.physicalSeconds == 1.25 && captured.calendarSeconds == 4500, "time capture did not preserve independent clocks");
    check(captured.retained.size() == 2 && captured.retained[0].id == 1 && !captured.retained[0].alive && captured.retained[1].id == 2 && captured.retained[1].active,
        "capture did not preserve former/current retained heroes");
    check(captured.regions.size() == 3 && captured.regions[0].revision == 2 && captured.regions[1].revision == 1 &&
        captured.regions[2].region == 99 && captured.regions[2].revision == 1000 && captured.regions[2].trees.empty(), "capture did not preserve exact and empty region revisions");

    const auto p1 = std::filesystem::path("test_capture_1.sav");
    const auto p2 = std::filesystem::path("test_capture_2.sav");
    writeSave(p1, captured);
    auto loaded = readSave(p1);
    auto restored = restoreSave(loaded);
    auto recaptured = captureSave(restored.first, restored.second, loaded.pose);
    writeSave(p2, recaptured);
    check(bytes(p1) == bytes(p2), "state/capture/write/read/restore/capture changed serialized state");
    check(restored.second.heroDeathKnown(1) && restored.second.retained(1) && !restored.second.retained(1)->alive,
        "dead retained former hero was not restored");
    check(restored.second.retained(2) && restored.second.retained(2)->active && restored.second.regionSnapshot(99).revision == 1000,
        "current retained hero or high empty revision was not restored");
    std::filesystem::remove(p1); std::filesystem::remove(p2);
}

void testLegacyStl1Compatibility() {
    const auto path = std::filesystem::path("test_legacy_stl1.sav");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write("STL1", 4); detail::write_u32(out, 1); detail::write_double(out, 55);
    detail::write_u64(out, 900); detail::write_u64(out, 4); detail::write_u8(out, 0);
    detail::write_u32(out, 0); detail::write_u32(out, 0); detail::write_u32(out, 0);
    detail::write_u32(out, 2);
    detail::write_u64(out, 8); detail::write_u32(out, 0);
    detail::write_u64(out, 9); detail::write_u32(out, 1); detail::write_u64(out, 70);
    detail::write_u8(out, static_cast<std::uint8_t>(TreeCondition::Fallen)); detail::write_double(out, 44);
    out.close(); check(!out.fail(), "failed to write STL1 fixture");
    auto loaded = readSave(path);
    check(loaded.physicalSeconds == 0 && loaded.calendarSeconds == 55 && loaded.regions.size() == 2,
        "STL1 physical default or fields changed");
    check(loaded.regions[0].revision == 0 && loaded.regions[1].revision == 1,
        "STL1 deterministic revision reconstruction changed");
    std::filesystem::remove(path);
}

void testLifecycleStl3RoundtripAndValidation() {
    const auto path = std::filesystem::path("test_lifecycle_stl3.sav");
    SaveData data; data.hero = ActiveEntity{44, 7}; data.calendarSeconds = 12345;
    Lifecycle source(HeroSpecies::Reedcrest);
    source.restoreState(HeroSpecies::Reedcrest, 4000.0, LifeStage::Adult);
    data.heroLifecycle = captureHeroLifecycle(source, data.hero->id);
    writeSave(path, data);
    const auto encoded = bytes(path);
    check(encoded.size() == detail::encodedSize(data), "STL3 exact encoded size mismatch");
    check(std::string(encoded.data(), encoded.data() + 4) == "STL3", "new save did not use STL3");
    const auto loaded = readSave(path);
    check(loaded.heroLifecycle && loaded.heroLifecycle->heroId == 44 &&
        loaded.heroLifecycle->species == HeroSpecies::Reedcrest &&
        near(loaded.heroLifecycle->calendarAge, 4000.0) && loaded.heroLifecycle->stage == LifeStage::Adult,
        "hero lifecycle fields did not roundtrip");
    const auto restored = restoreHeroLifecycle(loaded, HeroSpecies::Smalltrotter);
    check(restored.species() == HeroSpecies::Reedcrest && near(restored.calendarAge(), 4000.0) && restored.stage() == LifeStage::Adult,
        "captured lifecycle did not restore");

    const auto previous = encoded;
    auto rejectWrite = [&](SaveData invalid, const char* reason) {
        rejects([&] { writeSave(path, invalid); }, reason);
        check(bytes(path) == previous, "invalid lifecycle write changed previous save");
        check(!std::filesystem::exists(path.string() + ".tmp"), "invalid lifecycle write left a temp file");
    };
    auto invalid = data; invalid.heroLifecycle->heroId = 45;
    rejectWrite(invalid, "lifecycle identity");
    invalid = data; invalid.hero.reset();
    rejectWrite(invalid, "lifecycle identity");
    invalid = data; invalid.heroLifecycle->calendarAge = 1000; // Age is below Reedcrest's adult threshold.
    rejectWrite(invalid, "inconsistent");
    invalid = data; invalid.heroLifecycle->stage = LifeStage::Dead;
    rejectWrite(invalid, "dead lifecycle");
    invalid = data; invalid.heroLifecycle->calendarAge = std::numeric_limits<double>::quiet_NaN();
    rejectWrite(invalid, "inconsistent");
    invalid = data; invalid.heroLifecycle->stage = static_cast<LifeStage>(200);
    rejectWrite(invalid, "invalid lifecycle stage");
    invalid = data;
    invalid.retained.push_back({44, 7, HeroSpecies::Smalltrotter, RetentionReason::BondedPeer, true, true});
    rejectWrite(invalid, "species contradicts lifecycle");
    rejects([&] { (void)restoreSave(invalid); }, "species contradicts lifecycle");
    auto consistent = data;
    consistent.retained.push_back({44, 7, HeroSpecies::Reedcrest, RetentionReason::BondedPeer, true, true});
    writeSave(path, consistent);
    check(readSave(path).retained[0].species == HeroSpecies::Reedcrest,
        "consistent retained hero lifecycle failed to roundtrip");
    std::filesystem::remove(path);
}

void testStl2MigrationAndStl3Corruption() {
    const auto path = std::filesystem::path("test_lifecycle_compat.sav");
    // Construct a valid pre-lifecycle STL2 fixture with a current hero and pose.
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write("STL2", 4); detail::write_u32(out, 2);
        detail::write_double(out, 3.0); detail::write_double(out, 9.0);
        detail::write_u64(out, 91); detail::write_u64(out, 64);
        detail::write_u8(out, 1); detail::write_u64(out, 8); detail::write_u64(out, 2);
        detail::write_u8(out, 0);
        detail::write_u32(out, 0); detail::write_u32(out, 0); detail::write_u32(out, 0); detail::write_u32(out, 0);
    }
    auto old = readSave(path);
    check(old.hero && old.hero->id == 8 && !old.heroLifecycle, "STL2 migration invented lifecycle data");
    const auto migrated = restoreHeroLifecycle(old, HeroSpecies::Thistletail);
    check(migrated.species() == HeroSpecies::Thistletail && migrated.calendarAge() == 0.0 && migrated.stage() == LifeStage::Hatchling,
        "STL2 fallback did not use supplied species and age zero");
    old.retained.push_back({8, 2, HeroSpecies::Smalltrotter, RetentionReason::BondedPeer, true, true});
    const auto knownSpecies = restoreHeroLifecycle(old, HeroSpecies::Reedcrest);
    check(knownSpecies.species() == HeroSpecies::Smalltrotter && knownSpecies.calendarAge() == 0.0,
        "legacy fallback overrode the retained hero's known species");
    writeSave(path, old);
    check(bytes(path).size() == detail::encodedSize(old) && bytes(path)[0] == 'S' && bytes(path)[3] == '3',
        "STL2 read/write migration did not emit STL3");

    SaveData data; data.hero = ActiveEntity{2, 3};
    data.heroLifecycle = HeroLifecycleSnapshot{2, HeroSpecies::Smalltrotter, 0.0, LifeStage::Hatchling};
    writeSave(path, data);
    auto file = bytes(path);
    file.resize(59 + 8 + 1 + 8); // Truncate the lifecycle stage byte itself.
    { std::ofstream out(path, std::ios::binary | std::ios::trunc); out.write(file.data(), static_cast<std::streamsize>(file.size())); }
    rejects([&] { (void)readSave(path); }, "truncated");

    writeSave(path, data);
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(58); detail::write_u8(io, 2); io.flush();
    }
    rejects([&] { (void)readSave(path); }, "lifecycle presence");
    writeSave(path, data);
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(4); detail::write_u32(io, 4); io.flush();
    }
    rejects([&] { (void)readSave(path); }, "unsupported save version");
    std::filesystem::remove(path);
}

void testInvalidPhysicalTimeAndPose() {
    const auto path = std::filesystem::path("test_pose_validation.sav");
    SaveData baseline; writeSave(path, baseline); const auto original = bytes(path);
    auto noPose = readSave(path);
    check(!noPose.pose && noPose.physicalSeconds == 0, "default pose or physical time is not empty/zero");
    for (const auto& invalid : std::vector<SaveData>{
        [] { SaveData d; d.physicalSeconds = -1; return d; }(),
        [] { SaveData d; d.physicalSeconds = std::numeric_limits<double>::infinity(); return d; }(),
        [] { SaveData d; d.pose = HostPose{{NAN, 0, 0}, 0, 0}; return d; }(),
        [] { SaveData d; d.pose = HostPose{{0, 0, 0}, INFINITY, 0}; return d; }(),
        [] { SaveData d; d.pose = HostPose{{0, 0, 0}, 0, 3}; return d; }()}) {
        rejects([&] { writeSave(path, invalid); });
        check(bytes(path) == original, "invalid time or pose changed previous save");
        rejects([&] { (void)restoreSave(invalid); });
    }
    std::filesystem::remove(path);
}
} // namespace

int main() {
    const std::pair<const char*, std::function<void()>> scenarios[] = {
        {"roundtrip and deterministic ordering", testRoundtripAndDeterminism},
        {"write and restore validation", testWriteAndRestoreValidation},
        {"malformed files and aggregate bounds", testMalformedFilesAndBounds},
        {"write failure preserves prior save", testWriteFailurePreservesPriorSave},
        {"restore checks mutation statuses", testRestoreStatusesAndDomainState},
        {"authoritative capture through file roundtrip", testAuthoritativeCaptureRoundtrip},
        {"STL1 fixture compatibility", testLegacyStl1Compatibility},
        {"STL3 lifecycle roundtrip and validation", testLifecycleStl3RoundtripAndValidation},
        {"STL2 migration and STL3 corruption", testStl2MigrationAndStl3Corruption},
        {"physical time and pose validation", testInvalidPhysicalTimeAndPose}
    };
    int failed = 0;
    for (const auto& scenario : scenarios) {
        try { scenario.second(); std::cout << "PASS " << scenario.first << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << scenario.first << ": " << e.what() << '\n'; }
    }
    std::cout << (10 - failed) << "/10 tests passed\n";
    return failed ? 1 : 0;
}
