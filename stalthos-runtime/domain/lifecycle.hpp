#pragma once

#include "foundation.hpp"
#include <vector>
#include <stdexcept>
#include <limits>
#include <cmath>

namespace stalthos {

enum class LifeStage { Hatchling, Juvenile, Adult, Old, Dead };

struct HeroProfile {
    HeroSpecies species;
    double hatchlingDuration;
    double juvenileDuration;
    double adultDuration;
    double oldDuration;
};

// Proposed draft defaults. Fiction-first, not real paleontology.
inline const HeroProfile& getHeroProfile(HeroSpecies species) {
    static const HeroProfile smalltrotter { HeroSpecies::Smalltrotter, 1000.0, 2000.0, 10000.0, 3000.0 };
    static const HeroProfile brackenback { HeroSpecies::Brackenback, 1500.0, 3000.0, 15000.0, 4000.0 };
    static const HeroProfile reedcrest { HeroSpecies::Reedcrest, 1200.0, 2500.0, 12000.0, 3500.0 };
    static const HeroProfile thistletail { HeroSpecies::Thistletail, 800.0, 1500.0, 8000.0, 2000.0 };
    static const HeroProfile longstride { HeroSpecies::Longstride, 2000.0, 4000.0, 20000.0, 5000.0 };

    switch (species) {
        case HeroSpecies::Smalltrotter: return smalltrotter;
        case HeroSpecies::Brackenback: return brackenback;
        case HeroSpecies::Reedcrest: return reedcrest;
        case HeroSpecies::Thistletail: return thistletail;
        case HeroSpecies::Longstride: return longstride;
    }
    throw std::invalid_argument("Unknown species");
}

class Lifecycle {
    HeroSpecies species_;
    double calendarAge_ = 0.0;
    LifeStage stage_ = LifeStage::Hatchling;

public:
    explicit Lifecycle(HeroSpecies species) : species_(species) {
        // Reject invalid enum values at construction rather than retaining an
        // unusable profile until the first update.
        (void)getHeroProfile(species_);
    }

    HeroSpecies species() const { return species_; }
    double calendarAge() const { return calendarAge_; }
    LifeStage stage() const { return stage_; }

    std::vector<LifeStage> advance(const TimeStep& timeStep) {
        if (!std::isfinite(timeStep.physicalSeconds) || timeStep.physicalSeconds < 0.0 ||
            !std::isfinite(timeStep.calendarSeconds) || timeStep.calendarSeconds < 0.0) {
            throw std::invalid_argument("Invalid time step");
        }

        double newAge = calendarAge_ + timeStep.calendarSeconds;
        // In floating point math, adding a large value might not produce infinity if it fits within bounds,
        // but if it exceeds our sensible time limits or literal std::numeric_limits<double>::max(), it overflows.
        // We ensure that calendar age remains finite.
        if (!std::isfinite(newAge) || newAge < calendarAge_) {
            throw std::overflow_error("Calendar age overflow");
        }

        if (stage_ == LifeStage::Dead) {
            return {};
        }

        std::vector<LifeStage> transitions;
        const HeroProfile& profile = getHeroProfile(species_);

        double juvenileThreshold = profile.hatchlingDuration;
        double adultThreshold = juvenileThreshold + profile.juvenileDuration;
        double oldThreshold = adultThreshold + profile.adultDuration;
        double deadThreshold = oldThreshold + profile.oldDuration;

        // Build the complete result before changing the object. reserve() is
        // the only potentially throwing operation after validation; the four
        // enum appends then cannot allocate.
        transitions.reserve(4);
        LifeStage newStage = stage_;
        auto checkAndTransition = [&](LifeStage from, LifeStage to, double threshold) {
            if (newStage == from && newAge >= threshold) {
                newStage = to;
                transitions.push_back(to);
            }
        };

        checkAndTransition(LifeStage::Hatchling, LifeStage::Juvenile, juvenileThreshold);
        checkAndTransition(LifeStage::Juvenile, LifeStage::Adult, adultThreshold);
        checkAndTransition(LifeStage::Adult, LifeStage::Old, oldThreshold);
        checkAndTransition(LifeStage::Old, LifeStage::Dead, deadThreshold);

        calendarAge_ = newAge;
        stage_ = newStage;
        return transitions;
    }

    void restoreState(HeroSpecies species, double restoredAge, LifeStage restoredStage) {
        if (!std::isfinite(restoredAge) || restoredAge < 0.0) {
            throw std::invalid_argument("Invalid restored age");
        }

        const HeroProfile& profile = getHeroProfile(species);
        double juvenileThreshold = profile.hatchlingDuration;
        double adultThreshold = juvenileThreshold + profile.juvenileDuration;
        double oldThreshold = adultThreshold + profile.adultDuration;
        double deadThreshold = oldThreshold + profile.oldDuration;

        LifeStage expectedStage = LifeStage::Hatchling;
        if (restoredAge >= deadThreshold) {
            expectedStage = LifeStage::Dead;
        } else if (restoredAge >= oldThreshold) {
            expectedStage = LifeStage::Old;
        } else if (restoredAge >= adultThreshold) {
            expectedStage = LifeStage::Adult;
        } else if (restoredAge >= juvenileThreshold) {
            expectedStage = LifeStage::Juvenile;
        }

        if (expectedStage != restoredStage) {
            throw std::invalid_argument("Profile, age, and stage inconsistency during restore");
        }

        species_ = species;
        calendarAge_ = restoredAge;
        stage_ = restoredStage;
    }

    // Reproduction, parenting, and needs are explicitly unfinished for the draft.
    void reproduce() {
        throw std::logic_error("Reproduction explicitly unfinished for the full first draft.");
    }

    void parent() {
        throw std::logic_error("Parenting explicitly unfinished for the full first draft.");
    }

    void updateNeeds() {
        throw std::logic_error("Needs explicitly unfinished for the full first draft.");
    }
};

} // namespace stalthos
