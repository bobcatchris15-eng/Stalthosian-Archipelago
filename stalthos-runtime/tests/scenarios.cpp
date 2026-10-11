#include "../domain/foundation.hpp"
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

using namespace stalthos;
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-10; }
template<class F> void rejects(F action) {
    bool thrown = false; try { action(); } catch (const std::exception&) { thrown = true; }
    check(thrown, "invalid operation accepted");
}
void timeScenario() {
    SimCore clock;
    auto normal = clock.advance(.016, 1);
    auto rest = clock.advance(.016, 3600);
    auto chooser = clock.advance(.016, .1);
    check(near(normal.physicalSeconds, rest.physicalSeconds), "acceleration changed contact delta");
    check(near(rest.calendarSeconds, 57.6), "rest calendar acceleration wrong");
    check(near(chooser.calendarSeconds, .0016), "context time dilation wrong");
    check(near(clock.calendarSeconds(), .016 + 57.6 + .0016), "domains did not share calendar");
    check(near(clock.advance(.25, 0).calendarSeconds, 0), "paused calendar advanced");
    double before = clock.calendarSeconds();
    rejects([&] { clock.advance(-1); }); rejects([&] { clock.advance(1, -1); });
    rejects([&] { clock.advance(std::numeric_limits<double>::infinity()); });
    rejects([&] { clock.advance(std::numeric_limits<double>::max(), 2); });
    check(clock.calendarSeconds() == before, "invalid clock request changed calendar");
}
void steeringScenario() {
    const double pi = std::acos(-1.0);
    for (double body : {0.0, pi / 2, pi, -pi / 2}) {
        auto expected = BeastCore::bodyForward(body);
        for (double camera : {0.0, pi / 2, pi, 3 * pi / 2}) {
            HeroView view; view.cameraHeadingRadians = camera;
            auto movement = BeastCore::bodyForward(body);
            check(near(movement.x, expected.x) && near(movement.z, expected.z), "camera rotated WASD");
        }
    }
    auto east = BeastCore::bodyForward(pi / 2);
    check(near(east.x, 1) && near(east.z, 0), "body-relative forward wrong");
    rejects([] { BeastCore::bodyForward(std::numeric_limits<double>::quiet_NaN()); });
}
void intentScenario() {
    HeroSim hero;
    check(hero.authority() == Authority::Autonomy, "uncommanded animal waits for player");
    hero.suggest({10, 0, 20}, .5);
    for (int i = 0; i < 100; ++i) hero.routineHunger();
    check(hero.authority() == Authority::DestinationOrContext && hero.goal()->target.x == 10, "hunger erased destination");
    hero.setGoalStatus(GoalStatus::Paused); hero.routineHunger();
    check(hero.authority() == Authority::DestinationOrContext, "startle pause dropped goal");
    hero.setGoalStatus(GoalStatus::Replanning); hero.setGoalStatus(GoalStatus::Active);
    hero.setDirect(true); hero.routineHunger();
    check(hero.authority() == Authority::Direct && hero.goal()->status == GoalStatus::PlayerCancelled, "direct failed to cancel destination");
    hero.setDirect(false);
    check(hero.authority() == Authority::Autonomy, "release resurrected cancelled travel");
    hero.suggest({5, 0, 5}, 1, true); hero.setDirect(true, true); hero.setDirect(false);
    check(hero.authority() == Authority::DestinationOrContext, "explicit preservation ignored");
    hero.setGoalStatus(GoalStatus::Completed);
    check(hero.authority() == Authority::Autonomy, "goal completion froze autonomy");
    hero.setDirect(true); hero.suggest({1, 0, 1}, 0);
    check(hero.authority() == Authority::Direct && hero.goal()->status == GoalStatus::PlayerCancelled, "destination seized held direct input");
    hero.setDirect(false); hero.suggest({1, 0, 1}, 0); hero.setGoalStatus(GoalStatus::Abandoned);
    check(hero.authority() == Authority::Autonomy, "failed route retained movement authority");
    rejects([&] { hero.suggest({0, 0, 0}, -1); });
}
void perceptionScenario() {
    static_assert(!std::is_convertible<CameraStimulus, BodyStimulus>::value, "camera stimuli leak into body API");
    static_assert(!std::is_invocable<decltype(&BodyPerception::observe), BodyPerception&, CameraStimulus>::value,
        "body perception accepts camera stimulus");
    HeroSim hero; HeroView view;
    view.cameraAttention = CameraStimulus{90, {100, 0, 100}, 1};
    check(!hero.perception.attention(), "camera-only predator alerted body");
    hero.perception.observe({91, {2, 0, 3}, .8});
    check(hero.perception.attention()->source == 91, "body did not perceive its own alarm");
    view.cameraAttention = CameraStimulus{92, {-100, 0, 0}, 5};
    check(hero.perception.attention()->source == 91, "camera observation overwrote body attention");
}
void contactScenario() {
    InteractionsCore contacts(2); double leaves = 8;
    contacts.beginSession(1);
    check(!contacts.applyContact(1, 1, false, 2, leaves) && leaves == 8, "unreachable feeding applied");
    check(contacts.applyContact(1, 1, true, 2, leaves), "valid feeding failed");
    check(!contacts.applyContact(1, 1, true, 2, leaves) && leaves == 6, "duplicate contact charged twice");
    check(!contacts.applyContact(1, 2, true, 9, leaves), "insufficient resources applied");
    check(contacts.applyContact(1, 2, true, 1, leaves), "invalid attempt consumed token");
    check(!contacts.applyContact(1, 3, true, 1, leaves) && leaves == 5, "capacity failed open");
    contacts.beginSession(2);
    check(!contacts.applyContact(1, 1, true, 2, leaves), "stale contact applied after session change");
    check(contacts.applyContact(2, 1, true, 1, leaves) && leaves == 4, "new session contact denied");
    rejects([&] { contacts.beginSession(2); });
}
void continuityScenario() {
    StalthosState world(42, 2), identical(42), other(43);
    check(world.regionSeed(7) == identical.regionSeed(7) && world.regionSeed(7) != other.regionSeed(7), "seed reconstruction unstable");
    check(world.regionSeed(7) != world.regionSeed(8), "regions share baseline");
    auto oldSnapshot = world.regionSnapshot(7);
    world.recordTree(7, {100, TreeCondition::Fallen, 60});
    check(!world.snapshotCurrent(7, oldSnapshot.revision), "stale async region accepted");
    check(world.instantiate(200, 7), "companion staging failed");
    check(world.retain(200, HeroSpecies::Reedcrest, RetentionReason::BondedPeer), "bonded peer retention failed");
    world.retire(200);
    check(!world.instantiate(200, 7), "retained companion duplicated on region activation");
    check(world.reactivate(200) && !world.reactivate(200), "reactivation duplicated companion");
    check(world.markRetainedDead(200), "companion death failed");
    world.setHero(300, 7); world.heroDied(); world.setHero(301, 8); world.heroDied(); world.setHero(302, 7);
    auto revisit = world.regionSnapshot(7);
    check(revisit.trees.size() == 1 && revisit.trees[0].condition == TreeCondition::Fallen, "hero replacement reset fallen tree");
    check(world.heroDeathKnown(300) && world.heroDeathKnown(301), "hero death truth lost");
    check(!world.instantiate(300, 7) && world.excluded(300), "dead hero resurrected");
    check(!world.retained(200)->alive && !world.reactivate(200) && !world.instantiate(200, 7), "dead companion resurrected");
    check(world.instantiate(201, 8) && world.retain(201, HeroSpecies::Longstride, RetentionReason::DependentYoung), "second eligible identity failed");
    check(world.instantiate(202, 8) && !world.retain(202, HeroSpecies::Brackenback, RetentionReason::NotableEncounter), "retention exceeded bound");
    world.retire(202); check(world.instantiate(202, 8), "transient retirement kept biography");
    check(world.retainedCount() == 2, "retention budget changed");
    rejects([&] { world.setHero(303, 8); });
    rejects([&] { world.retire(302); });
    check(!world.instantiate(302, 8), "hero relocated through generic retirement");
    StalthosState retainedHero(42); retainedHero.setHero(500, 1);
    check(retainedHero.retain(500, HeroSpecies::Reedcrest, RetentionReason::BondedPeer), "hero retention setup failed");
    rejects([&] { retainedHero.markRetainedDead(500); });
    retainedHero.heroDied();
    check(retainedHero.heroDeathKnown(500) && !retainedHero.retained(500)->alive, "retained hero death transition failed");
}
}
int main() {
    const std::pair<const char*, std::function<void()>> scenarios[] = {
        {"shared calendar with responsive physical time", timeScenario},
        {"body-relative forward at all camera headings", steeringScenario},
        {"destination persistence and direct cancellation", intentScenario},
        {"camera-only stimulus excluded from bodily perception", perceptionScenario},
        {"contact idempotence and fail-closed bounds", contactScenario},
        {"region and death continuity across hero replacement", continuityScenario}
    };
    int failed = 0;
    for (auto& scenario : scenarios) {
        try { scenario.second(); std::cout << "PASS " << scenario.first << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << scenario.first << ": " << e.what() << '\n'; }
    }
    std::cout << (6 - failed) << "/6 scenarios passed\n";
    return failed ? 1 : 0;
}
