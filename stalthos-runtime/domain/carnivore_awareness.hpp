#pragma once
#include "fauna_population.hpp"

namespace stalthos::fauna {
// Proposed conservative presence cue. No inferred intent, pursuit, damage or prey radar.
// Session-only observation; neither camera position nor dormant actors are inputs.
class CarnivoreAwareness {
    Identity actor_ = 0;
    double distance_ = 0;
    bool began_ = false;
public:
    static constexpr double noticeMeters = 12;
    static constexpr double releaseMeters = 16;
    Identity actor() const { return actor_; }
    double distance() const { return distance_; }
    bool began() const { return began_; }
    void update(Identity hero, Vec3 body, bool alive, const std::vector<ActorView>& actors) {
        if (!finite(body)) throw std::invalid_argument("invalid carnivore awareness body");
        const bool wasPresent = actor_ != 0;
        Identity nearest = 0; double best = releaseMeters;
        if (alive) for (const auto& a : actors) {
            if (!a.id || a.id == hero || !a.alive || a.species != HeroSpecies::Longstride || !finite(a.position)) continue;
            const auto& p = getHeroProfile(a.species);
            const double adult = p.hatchlingDuration + p.juvenileDuration;
            // Adult and old bodies qualify; a hatchling/juvenile does not.
            if (!std::isfinite(a.calendarAge) || a.calendarAge < adult || a.calendarAge >= adult+p.adultDuration+p.oldDuration) continue;
            const double d = std::hypot(std::hypot(a.position.x-body.x,a.position.z-body.z),a.position.y-body.y);
            const double limit = a.id == actor_ ? releaseMeters : noticeMeters;
            if (d < limit && (d < best || (d == best && a.id < nearest))) { nearest=a.id;best=d; }
        }
        actor_=nearest;distance_=nearest?best:0;
        // One recall per continuous encounter, even if the nearest actor changes.
        began_=!wasPresent && actor_!=0;
    }
};
}
