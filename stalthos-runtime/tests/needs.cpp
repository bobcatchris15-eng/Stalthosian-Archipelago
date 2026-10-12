#include "../domain/needs.hpp"
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

using namespace stalthos;
using namespace stalthos::needs;

int testsPassed = 0;
int testsFailed = 0;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        testsFailed++;
        // Use explicit exceptions in Release instead of disabled assert
        throw std::runtime_error("Test failed: " + message);
    }
    testsPassed++;
}

bool almostEqual(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) < epsilon;
}

void testInitialization() {
    NeedsCore needs(HeroSpecies::Smalltrotter);
    require(needs.species() == HeroSpecies::Smalltrotter, "Species is correct");
    require(almostEqual(needs.hunger(), 0.0), "Initial hunger is 0");
    require(almostEqual(needs.thirst(), 0.0), "Initial thirst is 0");
    require(almostEqual(needs.fatigue(), 0.0), "Initial fatigue is 0");
    require(almostEqual(needs.injury(), 0.0), "Initial injury is 0");
    require(!needs.isResting(), "Not resting initially");

    bool caughtInvalidSpecies = false;
    try {
        NeedsCore bad(static_cast<HeroSpecies>(999));
    } catch (const std::invalid_argument&) {
        caughtInvalidSpecies = true;
    }
    require(caughtInvalidSpecies, "Invalid species throws at construction");
}

void testAdvanceOrdinary() {
    NeedsCore needs(HeroSpecies::Brackenback);
    // Profile: hunger 0.0001/cal, thirst 0.0002/cal, fatigue 0.005/phys
    TimeStep step = { 10.0, 100.0 }; // physical, calendar

    needs.advance(step);

    require(almostEqual(needs.hunger(), 0.01), "Hunger advanced by calendar time");
    require(almostEqual(needs.thirst(), 0.02), "Thirst advanced by calendar time");
    require(almostEqual(needs.fatigue(), 0.05), "Fatigue advanced by physical time");
}

void testDeltasAndSaturation() {
    NeedsCore needs(HeroSpecies::Reedcrest);

    // Saturation test
    TimeStep bigStep = { 1000.0, 100000.0 };
    needs.advance(bigStep);

    require(almostEqual(needs.hunger(), 1.0), "Hunger saturates at 1.0");
    require(almostEqual(needs.thirst(), 1.0), "Thirst saturates at 1.0");
    require(almostEqual(needs.fatigue(), 1.0), "Fatigue saturates at 1.0");
}

void testInterventions() {
    NeedsCore needs(HeroSpecies::Thistletail);
    TimeStep bigStep = { 1000.0, 100000.0 };
    needs.advance(bigStep); // saturate

    needs.feed(0.3);
    require(almostEqual(needs.hunger(), 0.7), "Feed reduced hunger");

    needs.drink(0.5);
    require(almostEqual(needs.thirst(), 0.5), "Drink reduced thirst");

    needs.sufferInjury(0.4);
    require(almostEqual(needs.injury(), 0.4), "Suffer injury increased injury");
    needs.sufferInjury(0.8);
    require(almostEqual(needs.injury(), 1.0), "Injury saturates at 1.0");

    needs.healInjury(0.6);
    require(almostEqual(needs.injury(), 0.4), "Heal injury reduced injury");

    needs.setResting(true);
    // Fatigue is 1.0. Recovery rate is 0.05/phys. Apply 10 phys seconds = 0.5 recovery.
    TimeStep restStep = { 10.0, 100.0 };
    needs.advance(restStep);

    // Hunger 0.7 + (0.0001*100) = 0.71
    require(almostEqual(needs.hunger(), 0.71), "Hunger advanced during rest due to calendar time");
    require(almostEqual(needs.fatigue(), 0.5), "Fatigue decreased during rest");

    // Test bounding on lower end
    needs.feed(1.0);
    needs.drink(1.0);
    needs.healInjury(1.0);
    require(almostEqual(needs.hunger(), 0.0), "Hunger clamped at 0.0");
    require(almostEqual(needs.thirst(), 0.0), "Thirst clamped at 0.0");
    require(almostEqual(needs.injury(), 0.0), "Injury clamped at 0.0");
}

void testInvalidInputs() {
    NeedsCore needs(HeroSpecies::Longstride);
    needs.restoreState(0.5, 0.5, 0.5, 0.5, false);

    // Negative time step
    bool caught = false;
    try { needs.advance({-1.0, 10.0}); } catch(const std::invalid_argument&) { caught = true; }
    require(caught, "Negative physical time throws");
    require(almostEqual(needs.hunger(), 0.5), "State preserved after bad physical time");

    caught = false;
    try { needs.advance({10.0, -1.0}); } catch(const std::invalid_argument&) { caught = true; }
    require(caught, "Negative calendar time throws");
    require(almostEqual(needs.hunger(), 0.5), "State preserved after bad calendar time");

    // NaN
    caught = false;
    try { needs.advance({std::numeric_limits<double>::quiet_NaN(), 10.0}); } catch(const std::invalid_argument&) { caught = true; }
    require(caught, "NaN physical time throws");

    // Invalid intervention amounts
    caught = false;
    try { needs.feed(-0.1); } catch(const std::invalid_argument&) { caught = true; }
    require(caught, "Negative feed throws");
    require(almostEqual(needs.hunger(), 0.5), "State preserved after bad feed");

    caught = false;
    try { needs.drink(std::numeric_limits<double>::quiet_NaN()); } catch(const std::invalid_argument&) { caught = true; }
    require(caught, "NaN drink throws");
    require(almostEqual(needs.thirst(), 0.5), "State preserved after bad drink");

    // Very large finite values remain valid when the scaled needs stay finite.
    caught = false;
    try { needs.advance({std::numeric_limits<double>::max(), std::numeric_limits<double>::max()}); } catch(...) { caught = true; }
    require(!caught, "Large finite time step is accepted when calculations remain finite");
    require(almostEqual(needs.hunger(), 1.0) && almostEqual(needs.thirst(), 1.0) && almostEqual(needs.fatigue(), 1.0), "Large finite time step saturates needs");
}

void testDerivedSuggestion() {
    NeedsCore needs(HeroSpecies::Smalltrotter);
    require(needs.suggestActivity() == SuggestedActivity::None, "Suggests none initially");

    needs.restoreState(0.6, 0.3, 0.3, 0.0, false);
    require(needs.suggestActivity() == SuggestedActivity::Eat, "Suggests Eat when hunger > 0.5");

    needs.restoreState(0.3, 0.8, 0.3, 0.0, false);
    require(needs.suggestActivity() == SuggestedActivity::Drink, "Suggests Drink when thirst > 0.5");

    needs.restoreState(0.3, 0.3, 0.9, 0.0, false);
    require(needs.suggestActivity() == SuggestedActivity::Rest, "Suggests Rest when fatigue > 0.5");

    // Tie breaking - deterministic priority based on code logic
    needs.restoreState(0.8, 0.8, 0.8, 0.0, false);
    // thirst is checked first (maxUrgency = 0.8)
    // hunger checked next (not > 0.8)
    // fatigue checked next (not > 0.8)
    require(needs.suggestActivity() == SuggestedActivity::Drink, "Deterministic priority resolves ties (Thirst wins)");

    needs.restoreState(0.9, 0.8, 0.8, 0.0, false);
    require(needs.suggestActivity() == SuggestedActivity::Eat, "Deterministic priority works (Hunger wins)");

    // Ensure suggestion doesn't mutate state
    needs.restoreState(0.9, 0.8, 0.8, 0.0, false);
    needs.suggestActivity();
    require(almostEqual(needs.hunger(), 0.9) && almostEqual(needs.thirst(), 0.8) && almostEqual(needs.fatigue(), 0.8), "Suggest activity does not mutate state");
}

int main() {
    try {
        testInitialization();
        testAdvanceOrdinary();
        testDeltasAndSaturation();
        testInterventions();
        testInvalidInputs();
        testDerivedSuggestion();

        std::cout << testsPassed << " tests passed. " << testsFailed << " tests failed." << std::endl;
        return testsFailed > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 2;
    }
}
