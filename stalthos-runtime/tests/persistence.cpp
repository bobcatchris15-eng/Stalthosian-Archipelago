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
    // STL7 lifecycle presence adds one byte, needs presence adds one byte, resource ledger presence adds one byte, camera presence adds one byte before the active list.
    overwriteU64(path, 67, 20);
    rejects([&] { (void)readSave(path); }, "duplicate or conflicting entity identity");

    data = SaveData{};
    data.retained.push_back({3, 4, HeroSpecies::Smalltrotter, RetentionReason::Caregiver, true, true});
    writeSave(path, data);
    // The retained active flag follows the STL7 header and record fields at byte 73.
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(74); detail::write_u8(io, 2); io.flush();
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
    detail::write_u64(out, 900); detail::write_u64(out, 4); detail::write_u8(out, 1);
    detail::write_u64(out, 8); detail::write_u64(out, 2);
    detail::write_u32(out, 0); detail::write_u32(out, 0); detail::write_u32(out, 0);
    detail::write_u32(out, 2);
    detail::write_u64(out, 8); detail::write_u32(out, 0);
    detail::write_u64(out, 9); detail::write_u32(out, 1); detail::write_u64(out, 70);
    detail::write_u8(out, static_cast<std::uint8_t>(TreeCondition::Fallen)); detail::write_double(out, 44);
    out.close(); check(!out.fail(), "failed to write STL1 fixture");
    auto loaded = readSave(path);
    check(!loaded.cameraState.has_value(), "STL1 save must lack camera state");
    check(loaded.physicalSeconds == 0 && loaded.calendarSeconds == 55 && loaded.regions.size() == 2,
        "STL1 physical default or fields changed");
    check(loaded.regions[0].revision == 0 && loaded.regions[1].revision == 1,
        "STL1 deterministic revision reconstruction changed");
    const auto legacyNeeds = restoreHeroNeeds(loaded, HeroSpecies::Thistletail);
    check(legacyNeeds.species() == HeroSpecies::Thistletail && legacyNeeds.hunger() == 0.0 &&
        legacyNeeds.thirst() == 0.0 && legacyNeeds.fatigue() == 0.0 && legacyNeeds.injury() == 0.0 &&
        !legacyNeeds.isResting(), "STL1 needs fallback was not explicit zero baseline");
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
    check(encoded.size() == detail::encodedSize(data), "STL4 exact encoded size mismatch");
    check(std::string(encoded.data(), encoded.data() + 4) == "STL7", "new save did not use STL7");
    const auto loaded = readSave(path);
    check(loaded.heroLifecycle && loaded.heroLifecycle->heroId == 44 &&
        loaded.heroLifecycle->species == HeroSpecies::Reedcrest &&
        near(loaded.heroLifecycle->calendarAge, 4000.0) && loaded.heroLifecycle->stage == LifeStage::Adult,
        "hero lifecycle fields did not roundtrip");
    const auto restored = restoreHeroLifecycle(loaded, HeroSpecies::Smalltrotter);
    check(restored.species() == HeroSpecies::Reedcrest && near(restored.calendarAge(), 4000.0) && restored.stage() == LifeStage::Adult,
        "captured lifecycle did not restore");
    const auto legacyNeeds = restoreHeroNeeds(loaded, HeroSpecies::Smalltrotter);
    check(legacyNeeds.species() == HeroSpecies::Reedcrest && legacyNeeds.hunger() == 0.0 &&
        legacyNeeds.thirst() == 0.0 && legacyNeeds.fatigue() == 0.0 && legacyNeeds.injury() == 0.0,
        "STL3 needs fallback did not use lifecycle species and zero baseline");

    writeSave(path, data);
    const auto previous = bytes(path);
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
    check(old.hero && old.hero->id == 8 && !old.heroLifecycle && !old.cameraState, "STL2 migration invented lifecycle or camera data");
    const auto migrated = restoreHeroLifecycle(old, HeroSpecies::Thistletail);
    check(migrated.species() == HeroSpecies::Thistletail && migrated.calendarAge() == 0.0 && migrated.stage() == LifeStage::Hatchling,
        "STL2 fallback did not use supplied species and age zero");
    const auto legacyNeeds = restoreHeroNeeds(old, HeroSpecies::Thistletail);
    check(legacyNeeds.species() == HeroSpecies::Thistletail && legacyNeeds.hunger() == 0.0 &&
        legacyNeeds.thirst() == 0.0 && legacyNeeds.fatigue() == 0.0 && legacyNeeds.injury() == 0.0,
        "STL2 needs fallback was not explicit zero baseline");
    old.retained.push_back({8, 2, HeroSpecies::Smalltrotter, RetentionReason::BondedPeer, true, true});
    const auto knownSpecies = restoreHeroLifecycle(old, HeroSpecies::Reedcrest);
    check(knownSpecies.species() == HeroSpecies::Smalltrotter && knownSpecies.calendarAge() == 0.0,
        "legacy fallback overrode the retained hero's known species");
    writeSave(path, old);
    check(bytes(path).size() == detail::encodedSize(old) && bytes(path)[0] == 'S' && bytes(path)[3] == '7',
        "STL2 read/write migration did not emit STL7");

    SaveData data; data.hero = ActiveEntity{2, 3};
    data.heroLifecycle = HeroLifecycleSnapshot{2, HeroSpecies::Smalltrotter, 0.0, LifeStage::Hatchling};
    writeSave(path, data);
    auto file = bytes(path);
    file.resize(76); // Truncate the lifecycle stage byte itself.
    { std::ofstream out(path, std::ios::binary | std::ios::trunc); out.write(file.data(), static_cast<std::streamsize>(file.size())); }
    rejects([&] { (void)readSave(path); }, "truncated");

    writeSave(path, data);
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(58); detail::write_u8(io, 2); io.flush();
    }
    rejects([&] { (void)readSave(path); }, "corrupt lifecycle presence flag");
    writeSave(path, data);
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(4); detail::write_u32(io, 8); io.flush();
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


void testStl5RoundtripResources() {
    const auto path = std::filesystem::path("test_resources_stl5.sav");
    SaveData data;
    ResourceLedger ledger;
    ledger.load({{100, ResourceKind::Creek, 50.0, 0.0},
                 {200, ResourceKind::Shrub, 5.0, 0.0}});
    ledger.consume(44, 100, 10.0);
    ledger.consume(45, 200, 2.0);
    data.resourceLedger = captureResourceLedger(ledger);

    writeSave(path, data);
    const auto fileBytes = bytes(path);
    check(std::string(fileBytes.data(), fileBytes.data() + 4) == "STL7", "new save did not use STL7");
    check(fileBytes.size() == detail::encodedSize(data), "STL7 size mismatch");

    auto loaded = readSave(path);
    ResourceLedger restored;
    restoreResourceLedger(loaded, restored);
    auto snapshot = restored.snapshot();
    check(snapshot.size() == 2, "restored resource count mismatch");
    bool creekFound = false, shrubFound = false;
    for (const auto& res : snapshot) {
        if (res.id == 100) { creekFound = true; check(res.consumedQuantity == 10.0, "restored creek amount mismatch"); }
        if (res.id == 200) { shrubFound = true; check(res.consumedQuantity == 2.0, "restored shrub amount mismatch"); }
    }
    check(creekFound && shrubFound, "missing restored resource");

    // test replay
    check(restored.consume(44, 100, 10.0), "failed to replay exact past consume");
    check(!restored.consume(44, 100, 5.0), "allowed modified replay");

    // test that next session ID is strictly greater than persisted max
    uint64_t maxSession = 0;
    for (const auto& s : loaded.resourceLedger->sessions) {
        if (s.sessionId > maxSession) maxSession = s.sessionId;
    }
    check(maxSession == 45, "max session ID mismatch");
    uint64_t nextSession = maxSession + 1;
    check(restored.consume(nextSession, 100, 5.0), "failed to consume with fresh session ID");

    // test history cap graceful exhaustion
    for (uint64_t s = nextSession + 1; s < nextSession + 300; ++s) {
        try {
            if (!restored.consume(s, 100, 0.01)) break;
        } catch (const std::runtime_error& e) {
            check(std::string(e.what()).find("capacity exceeded") != std::string::npos, "unexpected exception on cap");
            break;
        }
    }

    std::filesystem::remove(path);
}

void testLegacyStl4Compatibility() {
    const auto path = std::filesystem::path("test_legacy_stl4.sav");
    SaveData data;
    data.hero = ActiveEntity{33, 5};
    data.heroNeeds = HeroNeedsSnapshot{33, HeroSpecies::Smalltrotter, 0.4, 0.5, 0.1, 0.0, false};
    std::ofstream os(path, std::ios::binary | std::ios::trunc);
    os.write("STL4", 4);
    detail::write_u32(os, 4);
    detail::write_double(os, data.physicalSeconds);
    detail::write_double(os, data.calendarSeconds);
    detail::write_u64(os, data.seed);
    detail::write_u64(os, static_cast<std::uint64_t>(data.retentionLimit));
    detail::write_u8(os, 1); // hero
    detail::write_u64(os, data.hero->id); detail::write_u64(os, data.hero->region);
    detail::write_u8(os, 0); // pose
    detail::write_u8(os, 0); // lifecycle
    detail::write_u8(os, 1); // needs
    detail::write_u64(os, data.heroNeeds->heroId);
    detail::write_u8(os, static_cast<std::uint8_t>(data.heroNeeds->species));
    detail::write_double(os, data.heroNeeds->hunger);
    detail::write_double(os, data.heroNeeds->thirst);
    detail::write_double(os, data.heroNeeds->fatigue);
    detail::write_double(os, data.heroNeeds->injury);
    detail::write_u8(os, 0); // isResting
    detail::write_u32(os, 0); // activeTransients
    detail::write_u32(os, 0); // retained
    detail::write_u32(os, 0); // deadHeroes
    detail::write_u32(os, 0); // regions
    os.close();
    check(!os.fail(), "failed to write STL4 fixture");

    const auto loaded = readSave(path);
    check(!loaded.resourceLedger.has_value(), "STL4 save must lack resource ledger");
    check(!loaded.cameraState.has_value(), "STL4 save must lack camera state");
    check(loaded.hero && loaded.hero->id == 33, "STL4 hero identity preserved");
    check(loaded.heroNeeds && near(loaded.heroNeeds->hunger, 0.4), "STL4 hero needs preserved");

    // Old save defaults pools only if lacks ledger
    ResourceLedger ledger;
    restoreResourceLedger(loaded, ledger);
    check(ledger.snapshot().empty(), "restoreResourceLedger on save without ledger must reset to empty");

    // Default pools can be loaded cleanly via load()
    check(ledger.load({{100, ResourceKind::Creek, 1000000.0, 0.0},
                       {200, ResourceKind::Shrub, 2.0, 0.0}}), "failed to load default pools for legacy save");
    check(ledger.snapshot().size() == 2, "default pools size mismatch");

    std::filesystem::remove(path);
}

void testCorruptResourceLedgerRollbackAndCaps() {
    const auto path = std::filesystem::path("test_corrupt_resources.sav");
    SaveData validData;
    ResourceLedger validLedger;
    validLedger.load({{100, ResourceKind::Creek, 50.0, 0.0}});
    validData.resourceLedger = captureResourceLedger(validLedger);
    writeSave(path, validData);
    const auto baselineBytes = bytes(path);

    auto preserved = [&](const SaveData& badData, const char* fragment) {
        rejects([&] { writeSave(path, badData); }, fragment);
        check(bytes(path) == baselineBytes, "failed writeSave modified prior valid save file");
    };

    // Invalid resource kind
    {
        SaveData bad = validData;
        bad.resourceLedger->resources[0].kind = static_cast<ResourceKind>(99);
        preserved(bad, "invalid resource ledger records");
    }
    // Negative initial quantity
    {
        SaveData bad = validData;
        bad.resourceLedger->resources[0].initialQuantity = -1.0;
        preserved(bad, "invalid resource ledger records");
    }
    // Non-finite initial quantity
    {
        SaveData bad = validData;
        bad.resourceLedger->resources[0].initialQuantity = std::numeric_limits<double>::infinity();
        preserved(bad, "invalid resource ledger records");
    }
    // Consumed > Initial
    {
        SaveData bad = validData;
        bad.resourceLedger->resources[0].consumedQuantity = 100.0;
        preserved(bad, "invalid resource ledger records");
    }
    // Negative consumed quantity
    {
        SaveData bad = validData;
        bad.resourceLedger->resources[0].consumedQuantity = -5.0;
        preserved(bad, "invalid resource ledger records");
    }
    // Duplicate resource ID
    {
        SaveData bad = validData;
        bad.resourceLedger->resources.push_back({100, ResourceKind::Creek, 50.0, 0.0});
        preserved(bad, "invalid resource ledger records");
    }
    // Session ID 0
    {
        SaveData bad = validData;
        bad.resourceLedger->sessions.push_back({0, 100, 1.0});
        preserved(bad, "invalid resource ledger records");
    }
    // Session unknown resource ID
    {
        SaveData bad = validData;
        bad.resourceLedger->sessions.push_back({1, 999, 1.0});
        preserved(bad, "invalid resource ledger records");
    }
    // Session non-finite amount
    {
        SaveData bad = validData;
        bad.resourceLedger->sessions.push_back({1, 100, std::numeric_limits<double>::quiet_NaN()});
        preserved(bad, "invalid resource ledger records");
    }
    // Session negative amount
    {
        SaveData bad = validData;
        bad.resourceLedger->sessions.push_back({1, 100, -2.0});
        preserved(bad, "invalid resource ledger records");
    }
    // Duplicate session key (sessionId, resourceId)
    {
        SaveData bad = validData;
        bad.resourceLedger->resources[0].consumedQuantity = 10.0;
        bad.resourceLedger->sessions.push_back({1, 100, 5.0});
        bad.resourceLedger->sessions.push_back({1, 100, 5.0});
        preserved(bad, "invalid resource ledger records");
    }
    // Session total exceeding resource consumed quantity
    {
        SaveData bad = validData;
        bad.resourceLedger->resources[0].consumedQuantity = 2.0;
        bad.resourceLedger->sessions.push_back({1, 100, 5.0});
        preserved(bad, "invalid resource ledger records");
    }
    // Resource count exceeding 4096 cap
    {
        SaveData bad = validData;
        bad.resourceLedger->resources.clear();
        for (std::uint64_t i = 1; i <= 4097; ++i) {
            bad.resourceLedger->resources.push_back({i, ResourceKind::Shrub, 1.0, 0.0});
        }
        preserved(bad, "invalid resource ledger records");
    }
    // Session count exceeding 256 cap
    {
        SaveData bad = validData;
        bad.resourceLedger->resources[0].initialQuantity = 1000.0;
        bad.resourceLedger->resources[0].consumedQuantity = 257.0;
        for (std::uint64_t s = 1; s <= 257; ++s) {
            bad.resourceLedger->sessions.push_back({s, 100, 1.0});
        }
        preserved(bad, "invalid resource ledger records");
    }

    // Corrupt presence byte (> 1) via readSave
    {
        auto file = bytes(path);
        file[44] = 2;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(file.data(), static_cast<std::streamsize>(file.size()));
        out.close();
        rejects([&] { (void)readSave(path); }, "corrupt resource ledger presence flag");
    }

    // Trailing bytes at end of file
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(baselineBytes.data(), static_cast<std::streamsize>(baselineBytes.size()));
        out.put('X');
        out.close();
        rejects([&] { (void)readSave(path); }, "trailing bytes at end of file");
    }

    std::filesystem::remove(path);
}

void testSavedDepletedQuantitiesRestoreAcrossLives() {
    const auto path = std::filesystem::path("test_lives_depletion.sav");
    SimCore sim;
    StalthosState world(12345);
    world.setHero(1, 1);

    ResourceLedger ledger;
    ledger.load({{100, ResourceKind::Creek, 1000000.0, 0.0},
                 {200, ResourceKind::Shrub, 2.0, 0.0}});

    // Hero 1 drinks from creek and browses from shrub in session 1
    check(ledger.consume(1, 100, 0.5), "hero 1 failed to drink");
    check(ledger.consume(1, 200, 1.5), "hero 1 failed to browse");

    // Save state while Hero 1 is alive
    auto data1 = captureSave(sim, world);
    data1.resourceLedger = captureResourceLedger(ledger);
    writeSave(path, data1);

    // Verify encoded size and magic
    const auto file1 = bytes(path);
    check(std::string(file1.data(), file1.data() + 4) == "STL7", "save was not STL7");
    check(file1.size() == detail::encodedSize(data1), "STL7 size mismatch");

    // Hero 1 dies!
    world.heroDied();
    check(world.heroDeathKnown(1), "world did not record hero 1 death");
    check(!world.snapshot().hero.has_value(), "dead hero still marked as active hero");

    // Save world with dead hero (successor selection requires save with no current hero)
    auto dataDead = captureSave(sim, world);
    dataDead.resourceLedger = captureResourceLedger(ledger);
    writeSave(path, dataDead);

    // Successor selection: load save and assign successor ID 2
    auto loadedDead = readSave(path);
    check(!loadedDead.hero.has_value(), "loaded save should have no live hero");
    check(loadedDead.deadHeroes.size() == 1 && loadedDead.deadHeroes[0].id == 1, "dead hero record missing");

    auto restoredPair = restoreSave(loadedDead);
    SimCore clock2 = std::move(restoredPair.first);
    StalthosState world2 = std::move(restoredPair.second);
    world2.setHero(2, 1);

    ResourceLedger ledger2;
    restoreResourceLedger(loadedDead, ledger2);

    // Check depleted quantities persisted across hero death and successor instantiation
    auto snap = ledger2.snapshot();
    double creekRem = 0, shrubRem = 0;
    for (const auto& r : snap) {
        if (r.id == 100) creekRem = r.initialQuantity - r.consumedQuantity;
        if (r.id == 200) shrubRem = r.initialQuantity - r.consumedQuantity;
    }
    check(near(creekRem, 999999.5), "creek depleted quantity did not restore across lives");
    check(near(shrubRem, 0.5), "shrub depleted quantity did not restore across lives");

    // Determine next session ID: strictly greater than persisted max
    uint64_t maxPersisted = 0;
    for (const auto& s : loadedDead.resourceLedger->sessions) {
        if (s.sessionId > maxPersisted) maxPersisted = s.sessionId;
    }
    check(maxPersisted == 1, "persisted max session ID mismatch");
    uint64_t successorSession = maxPersisted + 1; // session 2

    // Same live hold cannot double consume
    check(ledger2.consume(successorSession, 200, 0.5), "successor failed to consume remaining shrub");
    check(ledger2.consume(successorSession, 200, 0.5), "same session replay failed"); // replay succeeds
    // But cannot consume extra: shrub is now depleted (2.0 consumed out of 2.0)
    uint64_t freshSession = successorSession + 1;
    check(!ledger2.consume(freshSession, 200, 0.1), "allowed consuming from depleted shrub");

    // Save successor state
    auto dataSuccessor = captureSave(clock2, world2);
    dataSuccessor.resourceLedger = captureResourceLedger(ledger2);
    writeSave(path, dataSuccessor);

    // Read back and confirm
    auto finalLoaded = readSave(path);
    ResourceLedger finalLedger;
    restoreResourceLedger(finalLoaded, finalLedger);
    auto finalSnap = finalLedger.snapshot();
    for (const auto& r : finalSnap) {
        if (r.id == 200) {
            check(near(r.consumedQuantity, 2.0), "final shrub did not persist full depletion");
        }
    }

    std::filesystem::remove(path);
}

void testNeedsStl4RoundtripAndValidation() {
    const auto path = std::filesystem::path("test_needs_stl4.sav");
    SaveData data; data.hero = ActiveEntity{55, 8}; data.calendarSeconds = 12345;
    needs::NeedsCore source(HeroSpecies::Brackenback);
    source.restoreState(0.1, 0.2, 0.3, 0.4, true);
    data.heroNeeds = captureHeroNeeds(source, data.hero->id);
    writeSave(path, data);
    const auto encoded = bytes(path);
    check(encoded.size() == detail::encodedSize(data), "STL7 needs exact encoded size mismatch");
    check(std::string(encoded.data(), encoded.data() + 4) == "STL7", "new save did not use STL7");

    const auto loaded = readSave(path);
    check(loaded.heroNeeds && loaded.heroNeeds->heroId == 55 &&
        loaded.heroNeeds->species == HeroSpecies::Brackenback &&
        near(loaded.heroNeeds->hunger, 0.1) && near(loaded.heroNeeds->thirst, 0.2) &&
        near(loaded.heroNeeds->fatigue, 0.3) && near(loaded.heroNeeds->injury, 0.4) &&
        loaded.heroNeeds->isResting == true,
        "hero needs fields did not roundtrip");

    const auto restored = restoreHeroNeeds(loaded, HeroSpecies::Smalltrotter);
    check(restored.species() == HeroSpecies::Brackenback &&
        near(restored.hunger(), 0.1) && near(restored.thirst(), 0.2) &&
        near(restored.fatigue(), 0.3) && near(restored.injury(), 0.4) &&
        restored.isResting() == true,
        "captured needs did not restore");

    // Needs and lifecycle records are independent optional fields in STL4.
    for (const bool hasLifecycle : {false, true}) {
        for (const bool hasNeeds : {false, true}) {
            SaveData combination; combination.hero = data.hero;
            if (hasLifecycle)
                combination.heroLifecycle = HeroLifecycleSnapshot{55, HeroSpecies::Brackenback, 0.0, LifeStage::Hatchling};
            if (hasNeeds) combination.heroNeeds = data.heroNeeds;
            writeSave(path, combination);
            const auto combinationBytes = bytes(path);
            check(combinationBytes.size() == detail::encodedSize(combination),
                "STL4 optional lifecycle/needs encoded size mismatch");
            const auto roundtrip = readSave(path);
            check(roundtrip.heroLifecycle.has_value() == hasLifecycle && roundtrip.heroNeeds.has_value() == hasNeeds,
                "STL4 optional lifecycle/needs presence changed on roundtrip");
        }
    }

    writeSave(path, data);
    const auto previous = bytes(path);
    auto rejectWrite = [&](SaveData invalid, const char* reason) {
        rejects([&] { writeSave(path, invalid); }, reason);
        check(bytes(path) == previous, "invalid needs write changed previous save");
        check(!std::filesystem::exists(path.string() + ".tmp"), "invalid needs write left a temp file");
    };

    auto invalid = data; invalid.heroNeeds->heroId = 56;
    rejectWrite(invalid, "needs identity");
    invalid = data; invalid.hero.reset();
    rejectWrite(invalid, "needs identity");
    invalid = data; invalid.heroNeeds->hunger = 1.1;
    rejectWrite(invalid, "invalid needs values");
    invalid = data; invalid.heroNeeds->thirst = std::numeric_limits<double>::infinity();
    rejectWrite(invalid, "invalid needs values");

    // Conflicting species between lifecycle and needs
    invalid = data;
    invalid.heroLifecycle = HeroLifecycleSnapshot{invalid.hero->id, HeroSpecies::Smalltrotter, 0.0, LifeStage::Hatchling};
    rejectWrite(invalid, "needs species does not match lifecycle species");
    invalid = data;
    invalid.retained.push_back({55, 8, HeroSpecies::Smalltrotter, RetentionReason::BondedPeer, true, true});
    rejectWrite(invalid, "needs species does not match retained hero species");

    // Corrupt presence flag
    writeSave(path, data);
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(58); detail::write_u8(io, 2); io.flush();
    }
    rejects([&] { (void)readSave(path); }, "lifecycle presence flag");

    writeSave(path, data);
    {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(59); detail::write_u8(io, 2); io.flush();
    }
    rejects([&] { (void)readSave(path); }, "needs presence flag");

    auto corruptNeeds = [&](const std::function<void(std::fstream&)>& mutate, const char* message) {
        writeSave(path, data);
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        mutate(io); io.flush(); check(io.good(), "failed to corrupt needs fixture"); io.close();
        rejects([&] { (void)readSave(path); }, message);
    };
    corruptNeeds([](std::fstream& io) { io.seekp(68); detail::write_u8(io, 255); }, "invalid needs species");
    corruptNeeds([](std::fstream& io) { io.seekp(69); detail::write_double(io, std::numeric_limits<double>::quiet_NaN()); }, "invalid needs values");
    corruptNeeds([](std::fstream& io) { io.seekp(77); detail::write_double(io, 1.01); }, "invalid needs values");
    corruptNeeds([](std::fstream& io) { io.seekp(101); detail::write_u8(io, 2); }, "invalid resting flag");
    writeSave(path, data);
    { auto truncated = bytes(path); truncated.resize(101); std::ofstream io(path, std::ios::binary | std::ios::trunc); io.write(truncated.data(), static_cast<std::streamsize>(truncated.size())); }
    rejects([&] { (void)readSave(path); }, "truncated");
    writeSave(path, data);
    { auto trailing = bytes(path); trailing.push_back('\0'); std::ofstream io(path, std::ios::binary | std::ios::trunc); io.write(trailing.data(), static_cast<std::streamsize>(trailing.size())); }
    rejects([&] { (void)readSave(path); }, "trailing bytes");

    std::filesystem::remove(path);
}

void testLegacyStl5Compatibility() {
    const auto path = std::filesystem::path("test_legacy_stl5.sav");
    SaveData data;
    data.hero = ActiveEntity{42, 6};
    data.pose = HostPose{{10.0, 20.0, 30.0}, 1.2, 1}; // legacy mode 1 = broadside
    data.heroNeeds = HeroNeedsSnapshot{42, HeroSpecies::Smalltrotter, 0.2, 0.3, 0.4, 0.1, false};
    ResourceLedger ledger;
    ledger.load({{100, ResourceKind::Creek, 500.0, 50.0}});
    ledger.consume(1, 100, 10.0);
    data.resourceLedger = captureResourceLedger(ledger);

    std::ofstream os(path, std::ios::binary | std::ios::trunc);
    os.write("STL5", 4);
    detail::write_u32(os, 5);
    detail::write_double(os, data.physicalSeconds);
    detail::write_double(os, data.calendarSeconds);
    detail::write_u64(os, data.seed);
    detail::write_u64(os, static_cast<std::uint64_t>(data.retentionLimit));
    detail::write_u8(os, 1); // hero
    detail::write_u64(os, data.hero->id); detail::write_u64(os, data.hero->region);
    detail::write_u8(os, 1); // pose
    detail::write_double(os, data.pose->position.x);
    detail::write_double(os, data.pose->position.y);
    detail::write_double(os, data.pose->position.z);
    detail::write_double(os, data.pose->headingRadians);
    detail::write_u8(os, data.pose->cameraMode); // mode 1
    detail::write_u8(os, 0); // lifecycle
    detail::write_u8(os, 1); // needs
    detail::write_u64(os, data.heroNeeds->heroId);
    detail::write_u8(os, static_cast<std::uint8_t>(data.heroNeeds->species));
    detail::write_double(os, data.heroNeeds->hunger);
    detail::write_double(os, data.heroNeeds->thirst);
    detail::write_double(os, data.heroNeeds->fatigue);
    detail::write_double(os, data.heroNeeds->injury);
    detail::write_u8(os, data.heroNeeds->isResting ? 1 : 0);
    detail::write_u8(os, 1); // resourceLedger
    detail::write_u32(os, static_cast<std::uint32_t>(data.resourceLedger->resources.size()));
    for (const auto& res : data.resourceLedger->resources) {
        detail::write_u64(os, res.id);
        detail::write_u8(os, static_cast<std::uint8_t>(res.kind));
        detail::write_double(os, res.initialQuantity);
        detail::write_double(os, res.consumedQuantity);
    }
    detail::write_u32(os, static_cast<std::uint32_t>(data.resourceLedger->sessions.size()));
    for (const auto& ses : data.resourceLedger->sessions) {
        detail::write_u64(os, ses.sessionId);
        detail::write_u64(os, ses.resourceId);
        detail::write_double(os, ses.amount);
    }
    detail::write_u32(os, 0); // activeTransients
    detail::write_u32(os, 0); // retained
    detail::write_u32(os, 0); // deadHeroes
    detail::write_u32(os, 0); // regions
    os.close();
    check(!os.fail(), "failed to write STL5 fixture");

    const auto loaded = readSave(path);
    check(!loaded.cameraState.has_value(), "STL5 save must lack camera state");
    check(loaded.hero && loaded.hero->id == 42, "STL5 hero identity preserved");
    check(loaded.pose && loaded.pose->cameraMode == 1, "STL5 legacy pose cameraMode 1 preserved");
    check(loaded.resourceLedger.has_value() && loaded.resourceLedger->resources.size() == 1, "STL5 resource ledger preserved");

    // Write migrated save and verify it upgrades to STL7
    writeSave(path, loaded);
    const auto migratedBytes = bytes(path);
    check(std::string(migratedBytes.data(), migratedBytes.data() + 4) == "STL7", "migrated save was not STL7");
    check(migratedBytes.size() == detail::encodedSize(loaded), "STL7 migrated size mismatch");

    const auto reloaded = readSave(path);
    check(!reloaded.cameraState.has_value(), "migrated save camera state remains optional nullopt");
    check(reloaded.pose && reloaded.pose->cameraMode == 1, "migrated save preserves legacy cameraMode 1");

    std::filesystem::remove(path);
}

void testStl7CameraStateRoundtripAndValidation() {
    const auto path = std::filesystem::path("test_camera_stl7.sav");
    SaveData data;
    data.pose = HostPose{{1.0, 2.0, 3.0}, 0.5, 2}; // Legacy mode 2 (frontal)
    data.cameraState = CameraStateSnapshot{
        1, // mode (LocallyDetached)
        1, // focus (FreeCamera)
        {10.0, 20.0, 30.0}, // anchor
        1.5, // azimuth
        0.5, // elevation
        5.0 // distance
    };

    writeSave(path, data);
    const auto fileBytes = bytes(path);
    check(std::string(fileBytes.data(), fileBytes.data() + 4) == "STL7", "new save did not use STL7");
    check(fileBytes.size() == detail::encodedSize(data), "STL7 size mismatch");

    auto loaded = readSave(path);
    check(loaded.cameraState.has_value(), "camera state missing");
    check(loaded.cameraState->mode == 1, "mode mismatch");
    check(loaded.cameraState->focus == 1, "focus mismatch");
    check(near(loaded.cameraState->detachedAnchor.x, 10.0) &&
          near(loaded.cameraState->detachedAnchor.y, 20.0) &&
          near(loaded.cameraState->detachedAnchor.z, 30.0), "anchor mismatch");
    check(near(loaded.cameraState->orbitAzimuth, 1.5), "azimuth mismatch");
    check(near(loaded.cameraState->orbitElevation, 0.5), "elevation mismatch");
    check(near(loaded.cameraState->orbitDistance, 5.0), "distance mismatch");
    // Verify legacy host pose cameraMode semantics are intact and distinct from cameraState
    check(loaded.pose.has_value() && loaded.pose->cameraMode == 2, "legacy camera mode 2 mismatch");

    // Test optional omission in STL7
    SaveData dataWithoutCam = data;
    dataWithoutCam.cameraState.reset();
    writeSave(path, dataWithoutCam);
    check(bytes(path).size() == detail::encodedSize(dataWithoutCam), "STL7 without camera size mismatch");
    auto loadedWithoutCam = readSave(path);
    check(!loadedWithoutCam.cameraState.has_value(), "camera state should be omitted");

    // In-memory validation rejections on writeSave
    auto invalid = data;
    invalid.cameraState->mode = 2;
    rejects([&] { writeSave(path, invalid); }, "invalid camera mode");
    invalid = data;
    invalid.cameraState->mode = 255;
    rejects([&] { writeSave(path, invalid); }, "invalid camera mode");

    invalid = data;
    invalid.cameraState->focus = 2;
    rejects([&] { writeSave(path, invalid); }, "invalid input focus");
    invalid = data;
    invalid.cameraState->focus = 255;
    rejects([&] { writeSave(path, invalid); }, "invalid input focus");

    invalid = data;
    invalid.cameraState->detachedAnchor.x = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { writeSave(path, invalid); }, "invalid camera detached anchor");
    invalid = data;
    invalid.cameraState->detachedAnchor.y = std::numeric_limits<double>::infinity();
    rejects([&] { writeSave(path, invalid); }, "invalid camera detached anchor");
    invalid = data;
    invalid.cameraState->detachedAnchor.z = 1000001.0;
    rejects([&] { writeSave(path, invalid); }, "invalid camera detached anchor");
    invalid = data;
    invalid.cameraState->detachedAnchor.x = -1000001.0;
    rejects([&] { writeSave(path, invalid); }, "invalid camera detached anchor");

    invalid = data;
    invalid.cameraState->orbitAzimuth = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");
    invalid = data;
    invalid.cameraState->orbitElevation = std::numeric_limits<double>::infinity();
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");
    invalid = data;
    invalid.cameraState->orbitDistance = std::numeric_limits<double>::infinity();
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");
    invalid = data;
    invalid.cameraState->orbitDistance = -0.5;
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");
    invalid = data;
    invalid.cameraState->orbitDistance = 1000001.0;
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");

    invalid = data;
    invalid.cameraState->orbitDistance = domain::CameraFraming::kMinDistance - 0.01;
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");
    invalid = data;
    invalid.cameraState->orbitDistance = domain::CameraFraming::kMaxDistance + 0.01;
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");
    invalid = data;
    invalid.cameraState->orbitElevation = domain::CameraFraming::kMinElevation - 0.01;
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");
    invalid = data;
    invalid.cameraState->orbitElevation = domain::CameraFraming::kMaxElevation + 0.01;
    rejects([&] { writeSave(path, invalid); }, "invalid camera orbit parameters");
    // Corrupted binary payload and presence tests on readSave
    // Offset calculation:
    // Header: STL7 (4) + version(4) + phys(8) + cal(8) + seed(8) + retLimit(8) = 40
    // hero: 0 (presence) = 41
    // pose: 1 (presence) + 24 (pos) + 8 (heading) + 1 (cameraMode) = 34 => 75
    // lifecycle: 0 (presence) => 76
    // needs: 0 (presence) => 77
    // resourceLedger: 0 (presence) => 78
    // cameraState presence flag: offset 78
    const std::streamoff camPresenceOffset = 78;
    auto corruptAt = [&](std::streamoff offset, const std::function<void(std::fstream&)>& mutateFn, const char* expected) {
        writeSave(path, data);
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        io.seekp(offset);
        mutateFn(io);
        io.flush();
        check(io.good(), "failed to corrupt fixture");
        io.close();
        rejects([&] { (void)readSave(path); }, expected);
    };

    corruptAt(camPresenceOffset, [](std::fstream& io) { detail::write_u8(io, 2); }, "corrupt camera state presence flag");
    corruptAt(camPresenceOffset + 1, [](std::fstream& io) { detail::write_u8(io, 2); }, "invalid camera mode");
    corruptAt(camPresenceOffset + 2, [](std::fstream& io) { detail::write_u8(io, 2); }, "invalid input focus");
    corruptAt(camPresenceOffset + 3, [](std::fstream& io) { detail::write_double(io, std::numeric_limits<double>::quiet_NaN()); }, "invalid camera detached anchor");
    corruptAt(camPresenceOffset + 27, [](std::fstream& io) { detail::write_double(io, std::numeric_limits<double>::infinity()); }, "invalid camera orbit parameters");
    corruptAt(camPresenceOffset + 43, [](std::fstream& io) { detail::write_double(io, -1.0); }, "invalid camera orbit parameters");

    // Truncated payload tests
    writeSave(path, data);
    {
        auto truncated = bytes(path);
        truncated.resize(camPresenceOffset + 10);
        std::ofstream io(path, std::ios::binary | std::ios::trunc);
        io.write(truncated.data(), static_cast<std::streamsize>(truncated.size()));
    }
    rejects([&] { (void)readSave(path); }, "truncated");

    // Trailing bytes test
    writeSave(path, data);
    {
        auto trailing = bytes(path);
        trailing.push_back('\0');
        std::ofstream io(path, std::ios::binary | std::ios::trunc);
        io.write(trailing.data(), static_cast<std::streamsize>(trailing.size()));
    }
    rejects([&] { (void)readSave(path); }, "trailing bytes");

    std::filesystem::remove(path);
}
void testStl7FaunaPopulationRoundtripAndValidation() {
    const auto path=std::filesystem::path("test_fauna_stl7.sav"); SaveData data; data.seed=42;
    fauna::PopulationSnapshot pop; pop.seed=42; pop.region=7; pop.habitatSignature=99;
    pop.actors.push_back({123,HeroSpecies::Smalltrotter,{1,2,3},0.5,4.0,0,1,500.0,LifeStage::Hatchling,0.2,0.3,0.4,0.1,true,fauna::Activity::Travel}); data.faunaPopulation=pop;
    writeSave(path,data); const auto raw=bytes(path); check(std::string(raw.data(),raw.data()+4)=="STL7","fauna save did not use STL7");
    const auto loaded=readSave(path); check(loaded.faunaPopulation.has_value(),"fauna payload missing"); const auto& a=loaded.faunaPopulation->actors.at(0);
    check(a.id==123 && a.site==0 && a.target==1 && a.stage==LifeStage::Hatchling && a.isResting && a.activity==fauna::Activity::Travel,"fauna discrete state mismatch");
    check(a.position.x==1 && a.position.y==2 && a.position.z==3 && a.injury==0.1,"fauna scalar state mismatch");
    auto invalid=data; invalid.faunaPopulation->actors.push_back(pop.actors[0]); rejects([&]{writeSave(path,invalid);},"duplicate or conflicting fauna identity");
    invalid=data; invalid.hero=ActiveEntity{123,7}; rejects([&]{writeSave(path,invalid);},"duplicate or conflicting fauna identity");
    invalid=data; invalid.faunaPopulation->seed=43; rejects([&]{writeSave(path,invalid);},"fauna population world identity mismatch");
    invalid=data; invalid.faunaPopulation->actors[0].hunger=std::numeric_limits<double>::quiet_NaN(); rejects([&]{writeSave(path,invalid);},"invalid fauna needs");
    invalid=data; invalid.faunaPopulation->actors[0].site=fauna::Population::maxSites; rejects([&]{writeSave(path,invalid);},"fauna route index out of range");
    writeSave(path,data);
    { std::fstream io(path,std::ios::binary|std::ios::in|std::ios::out); io.seekp(46); detail::write_u8(io,2); io.flush(); }
    rejects([&]{(void)readSave(path);},"corrupt fauna population presence flag");
    writeSave(path,data);
    { std::fstream io(path,std::ios::binary|std::ios::in|std::ios::out); io.seekp(71); detail::write_u32(io,17); io.flush(); }
    rejects([&]{(void)readSave(path);},"fauna actors exceed size bound");
    writeSave(path,data); { auto truncated=bytes(path); truncated.resize(150); std::ofstream io(path,std::ios::binary|std::ios::trunc); io.write(truncated.data(),static_cast<std::streamsize>(truncated.size())); }
    rejects([&]{(void)readSave(path);},"truncated");
    writeSave(path,data);
    // Presence header follows the prior STL6 fields; first actor species follows id.
    { std::fstream io(path,std::ios::binary|std::ios::in|std::ios::out); io.seekp(83); detail::write_u8(io,255); io.flush(); }
    rejects([&]{(void)readSave(path);},"invalid fauna species"); std::filesystem::remove(path);
}
void testLegacyStl6Compatibility() {
    const auto path=std::filesystem::path("test_legacy_stl6.sav"); std::ofstream os(path,std::ios::binary|std::ios::trunc);
    os.write("STL6",4); detail::write_u32(os,6); detail::write_double(os,0); detail::write_double(os,0); detail::write_u64(os,0); detail::write_u64(os,64);
    for(int i=0;i<6;++i) detail::write_u8(os,0); // hero, pose, lifecycle, needs, ledger, camera
    detail::write_u32(os,0); detail::write_u32(os,0); detail::write_u32(os,0); detail::write_u32(os,0); os.close();
    const auto loaded=readSave(path); check(!loaded.faunaPopulation,"STL6 invented fauna population");
    writeSave(path,loaded); const auto migrated=bytes(path); check(std::string(migrated.data(),migrated.data()+4)=="STL7","STL6 migration did not emit STL7");
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
        {"physical time and pose validation", testInvalidPhysicalTimeAndPose},
        {"STL4 needs roundtrip and validation", testNeedsStl4RoundtripAndValidation},
        {"STL4 legacy compatibility", testLegacyStl4Compatibility},
        {"STL5 resource ledger roundtrip", testStl5RoundtripResources},
        {"corrupt resource ledger rollback and size caps", testCorruptResourceLedgerRollbackAndCaps},
        {"saved depleted quantities restore across lives", testSavedDepletedQuantitiesRestoreAcrossLives},
        {"STL5 legacy compatibility", testLegacyStl5Compatibility},
        {"STL7 camera state roundtrip and validation", testStl7CameraStateRoundtripAndValidation},
        {"STL7 fauna population roundtrip and validation", testStl7FaunaPopulationRoundtripAndValidation},
        {"STL6 compatibility", testLegacyStl6Compatibility}
    };
    int failed = 0;
    for (const auto& scenario : scenarios) {
        try { scenario.second(); std::cout << "PASS " << scenario.first << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << scenario.first << ": " << e.what() << '\n'; }
    }
    std::cout << (19 - failed) << "/19 tests passed\n";
    return failed ? 1 : 0;
}
