#pragma once

#include "foundation.hpp"
#include <stdexcept>
#include <limits>
#include <cmath>
#include <algorithm>

namespace stalthos {
namespace needs {

enum class SuggestedActivity { None, Eat, Drink, Rest };

struct NeedsProfile {
    HeroSpecies species;
    // Base rates of increase per calendar second
    double hungerRate;
    double thirstRate;
    // Base rate of increase per physical second
    double fatigueRate;

    // Rest rate of decrease per physical second
    double restRecoveryRate;
};

// Proposed draft defaults. Fiction-first, not real physiology.
inline const NeedsProfile& getNeedsProfile(HeroSpecies species) {
    // All 5 species share provisional rates for now. Marked as proposed tuning.

    static const NeedsProfile smalltrotter = { HeroSpecies::Smalltrotter, 0.0001, 0.0002, 0.005, 0.05 };
    static const NeedsProfile brackenback  = { HeroSpecies::Brackenback,  0.0001, 0.0002, 0.005, 0.05 };
    static const NeedsProfile reedcrest    = { HeroSpecies::Reedcrest,    0.0001, 0.0002, 0.005, 0.05 };
    static const NeedsProfile thistletail  = { HeroSpecies::Thistletail,  0.0001, 0.0002, 0.005, 0.05 };
    static const NeedsProfile longstride   = { HeroSpecies::Longstride,   0.0001, 0.0002, 0.005, 0.05 };

    switch (species) {
        case HeroSpecies::Smalltrotter: return smalltrotter;
        case HeroSpecies::Brackenback: return brackenback;
        case HeroSpecies::Reedcrest: return reedcrest;
        case HeroSpecies::Thistletail: return thistletail;
        case HeroSpecies::Longstride: return longstride;
    }
    throw std::invalid_argument("Unknown species");
}

class NeedsCore {
    HeroSpecies species_;
    // Normalized 0.0 to 1.0, where 0.0 is optimal and 1.0 is critical.
    double hunger_ = 0.0;
    double thirst_ = 0.0;
    double fatigue_ = 0.0;
    double injury_ = 0.0;
    bool isResting_ = false;

    double clampNormalized(double val) const {
        if (!std::isfinite(val)) throw std::overflow_error("Non-finite need value calculated");
        return std::max(0.0, std::min(1.0, val));
    }

public:
    explicit NeedsCore(HeroSpecies species) : species_(species) {
        // Reject invalid enum early
        (void)getNeedsProfile(species_);
    }

    HeroSpecies species() const { return species_; }
    double hunger() const { return hunger_; }
    double thirst() const { return thirst_; }
    double fatigue() const { return fatigue_; }
    double injury() const { return injury_; }
    bool isResting() const { return isResting_; }

    void advance(const TimeStep& timeStep) {
        if (!std::isfinite(timeStep.physicalSeconds) || timeStep.physicalSeconds < 0.0 ||
            !std::isfinite(timeStep.calendarSeconds) || timeStep.calendarSeconds < 0.0) {
            throw std::invalid_argument("Invalid time step");
        }

        const NeedsProfile& profile = getNeedsProfile(species_);

        // Calculate potential new values without modifying state
        double newHunger = hunger_ + (profile.hungerRate * timeStep.calendarSeconds);
        double newThirst = thirst_ + (profile.thirstRate * timeStep.calendarSeconds);

        double newFatigue = fatigue_;
        if (isResting_) {
            // Resting decreases fatigue, but only based on physical time, so calendar acceleration
            // does not multiply physical rest effect doubly if it was already handled by host,
            // but we follow explicit rule: calendar accelerates hunger/thirst, physical advances fatigue/reflexes.
            newFatigue -= (profile.restRecoveryRate * timeStep.physicalSeconds);
        } else {
            newFatigue += (profile.fatigueRate * timeStep.physicalSeconds);
        }

        // Validate first
        if (!std::isfinite(newHunger) || !std::isfinite(newThirst) || !std::isfinite(newFatigue)) {
            throw std::overflow_error("Overflow in needs calculation");
        }

        // Apply with bounding clamp
        hunger_ = clampNormalized(newHunger);
        thirst_ = clampNormalized(newThirst);
        fatigue_ = clampNormalized(newFatigue);
    }

    // Bounded interventions
    void feed(double amount) {
        if (!std::isfinite(amount) || amount < 0.0) throw std::invalid_argument("Invalid feed amount");
        double newHunger = hunger_ - amount;
        hunger_ = clampNormalized(newHunger);
    }

    void drink(double amount) {
        if (!std::isfinite(amount) || amount < 0.0) throw std::invalid_argument("Invalid drink amount");
        double newThirst = thirst_ - amount;
        thirst_ = clampNormalized(newThirst);
    }

    void setResting(bool resting) {
        isResting_ = resting;
    }

    void sufferInjury(double amount) {
        if (!std::isfinite(amount) || amount < 0.0) throw std::invalid_argument("Invalid injury amount");
        double newInjury = injury_ + amount;
        injury_ = clampNormalized(newInjury);
    }

    void healInjury(double amount) {
        if (!std::isfinite(amount) || amount < 0.0) throw std::invalid_argument("Invalid healing amount");
        double newInjury = injury_ - amount;
        injury_ = clampNormalized(newInjury);
    }

    // Derived suggestion - explicitly not an authority
    SuggestedActivity suggestActivity() const {
        // Return most urgent need, or None if all are below a threshold
        const double threshold = 0.5; // Arbitrary provisional threshold

        SuggestedActivity suggestion = SuggestedActivity::None;
        double maxUrgency = threshold;

        if (thirst_ > maxUrgency) {
            maxUrgency = thirst_;
            suggestion = SuggestedActivity::Drink;
        }
        if (hunger_ > maxUrgency) {
            maxUrgency = hunger_;
            suggestion = SuggestedActivity::Eat;
        }
        if (fatigue_ > maxUrgency) {
            maxUrgency = fatigue_;
            suggestion = SuggestedActivity::Rest;
        }

        // Strict comparison preserves the first equal urgency: Drink, Eat,
        // then Rest. Suggestions never replace player/controller authority.

        return suggestion;
    }

    // Explicit restore state for testing and potential future persistence
    void restoreState(double rHunger, double rThirst, double rFatigue, double rInjury, bool rResting) {
        if (!std::isfinite(rHunger) || rHunger < 0.0 || rHunger > 1.0 ||
            !std::isfinite(rThirst) || rThirst < 0.0 || rThirst > 1.0 ||
            !std::isfinite(rFatigue) || rFatigue < 0.0 || rFatigue > 1.0 ||
            !std::isfinite(rInjury) || rInjury < 0.0 || rInjury > 1.0) {
            throw std::invalid_argument("Invalid restore state values");
        }
        hunger_ = rHunger;
        thirst_ = rThirst;
        fatigue_ = rFatigue;
        injury_ = rInjury;
        isResting_ = rResting;
    }
};

} // namespace needs
} // namespace stalthos
