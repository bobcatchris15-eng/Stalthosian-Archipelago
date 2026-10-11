#include "../domain/lifecycle.hpp"
#include <iostream>
#include <functional>
#include <cmath>

using namespace stalthos;

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

template<class F>
void rejects(F action) {
    bool thrown = false;
    try {
        action();
    } catch (const std::exception&) {
        thrown = true;
    }
    check(thrown, "invalid operation accepted");
}

void testAllSpeciesProfiles() {
    const double expected[5][4] = {
        {1000.0, 2000.0, 10000.0, 3000.0},
        {1500.0, 3000.0, 15000.0, 4000.0},
        {1200.0, 2500.0, 12000.0, 3500.0},
        {800.0, 1500.0, 8000.0, 2000.0},
        {2000.0, 4000.0, 20000.0, 5000.0}
    };
    for (int i = 0; i < 5; ++i) {
        HeroSpecies species = static_cast<HeroSpecies>(i);
        const HeroProfile& profile = getHeroProfile(species);
        Lifecycle life(species);
        check(profile.species == species, "Profile species key incorrect");
        check(profile.hatchlingDuration == expected[i][0] &&
              profile.juvenileDuration == expected[i][1] &&
              profile.adultDuration == expected[i][2] &&
              profile.oldDuration == expected[i][3], "Draft profile durations incorrect");
        check(life.species() == species, "Species initialized incorrectly");
        check(life.stage() == LifeStage::Hatchling, "Initial stage is not Hatchling");
        check(life.calendarAge() == 0.0, "Initial age is not 0");
    }
    rejects([&]() { Lifecycle invalid(static_cast<HeroSpecies>(99)); });
}

void testAcceleratedCalendar() {
    Lifecycle life(HeroSpecies::Smalltrotter);
    TimeStep step{0.016, 500.0};
    auto transitions = life.advance(step);
    check(life.calendarAge() == 500.0, "Age did not advance");
    check(step.physicalSeconds == 0.016, "Lifecycle changed physical elapsed time");
    check(life.stage() == LifeStage::Hatchling, "Stage changed prematurely");
    check(transitions.empty(), "Transitions reported when none occurred");

    // Push past juvenile threshold (1000 for Smalltrotter)
    TimeStep step2{0.016, 600.0};
    transitions = life.advance(step2);
    check(life.calendarAge() == 1100.0, "Age did not advance correctly");
    check(life.stage() == LifeStage::Juvenile, "Did not transition to Juvenile");
    check(transitions.size() == 1 && transitions[0] == LifeStage::Juvenile, "Transition to Juvenile not reported correctly");
}

void testBigJumps() {
    Lifecycle life(HeroSpecies::Smalltrotter);
    // Profile: H=1000, J=2000, A=10000, O=3000
    // Dead threshold is 1000 + 2000 + 10000 + 3000 = 16000
    TimeStep bigStep{1.0, 17000.0};
    auto transitions = life.advance(bigStep);

    check(life.calendarAge() == 17000.0, "Age did not advance correctly on big jump");
    check(life.stage() == LifeStage::Dead, "Did not transition to Dead");
    check(transitions.size() == 4, "Did not report exactly 4 transitions");
    check(transitions[0] == LifeStage::Juvenile, "Transition 1 incorrect");
    check(transitions[1] == LifeStage::Adult, "Transition 2 incorrect");
    check(transitions[2] == LifeStage::Old, "Transition 3 incorrect");
    check(transitions[3] == LifeStage::Dead, "Transition 4 incorrect");
}

void testRepeatedDeath() {
    Lifecycle life(HeroSpecies::Smalltrotter);
    TimeStep bigStep{1.0, 20000.0};
    life.advance(bigStep);
    check(life.stage() == LifeStage::Dead, "Should be dead");

    TimeStep smallStep{1.0, 10.0};
    auto transitions = life.advance(smallStep);
    check(life.calendarAge() == 20000.0, "Age should not advance after death");
    check(life.stage() == LifeStage::Dead, "Should still be dead");
    check(transitions.empty(), "Should not report transitions after death");
}

void testRestoredState() {
    Lifecycle life(HeroSpecies::Brackenback);
    // Brackenback Profile: H=1500, J=3000, A=15000, O=4000
    // Juvenile: 1500, Adult: 4500, Old: 19500, Dead: 23500

    life.restoreState(HeroSpecies::Brackenback, 5000.0, LifeStage::Adult);
    check(life.calendarAge() == 5000.0, "Restored age incorrect");
    check(life.stage() == LifeStage::Adult, "Restored stage incorrect");

    // Restore consistency validation
    rejects([&]() { life.restoreState(HeroSpecies::Brackenback, 5000.0, LifeStage::Juvenile); });
    rejects([&]() { life.restoreState(HeroSpecies::Brackenback, 24000.0, LifeStage::Old); });

    // State preservation on rejection
    check(life.calendarAge() == 5000.0, "Age corrupted after failed restore");
    check(life.stage() == LifeStage::Adult, "Stage corrupted after failed restore");
}

void testMalformedTimeAndOverflow() {
    Lifecycle life(HeroSpecies::Reedcrest);

    rejects([&]() { life.advance({1.0, -10.0}); });
    rejects([&]() { life.advance({1.0, std::numeric_limits<double>::infinity()}); });
    rejects([&]() { life.advance({1.0, std::numeric_limits<double>::quiet_NaN()}); });
    rejects([&]() { life.advance({-1.0, 10.0}); });
    rejects([&]() { life.advance({std::numeric_limits<double>::infinity(), 10.0}); });
    rejects([&]() { life.advance({0.0, -std::numeric_limits<double>::infinity()}); });

    double initialAge = life.calendarAge();
    LifeStage initialStage = life.stage();

    // Because adding max to a small number might just equal max in floating point,
    // let's create a scenario that is guaranteed to generate an infinity or overflow.
    life.advance({0.25, std::numeric_limits<double>::max()});
    const double ageBeforeOverflow = life.calendarAge();
    const LifeStage stageBeforeOverflow = life.stage();
    rejects([&]() { life.advance({0.25, std::numeric_limits<double>::max()}); });

    check(life.calendarAge() == ageBeforeOverflow, "Age corrupted after overflow rejection");
    check(life.stage() == stageBeforeOverflow, "Stage corrupted after overflow rejection");
    check(initialAge == 0.0 && initialStage == LifeStage::Hatchling,
          "Initial state setup unexpectedly changed");
}

} // namespace

int main() {
    const std::pair<const char*, std::function<void()>> scenarios[] = {
        {"All Species Profiles Initialized", testAllSpeciesProfiles},
        {"Accelerated Calendar with physical seconds unchanged", testAcceleratedCalendar},
        {"Big Jumps give all transitions exactly once", testBigJumps},
        {"Repeated Death is terminal", testRepeatedDeath},
        {"Restored State validates age/stage", testRestoredState},
        {"Malformed Time and Overflow are rejected cleanly", testMalformedTimeAndOverflow}
    };
    int failed = 0;
    for (const auto& scenario : scenarios) {
        try {
            scenario.second();
            std::cout << "PASS " << scenario.first << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "FAIL " << scenario.first << ": " << e.what() << '\n';
        }
    }
    std::cout << (6 - failed) << "/6 tests passed\n";
    return failed ? 1 : 0;
}
