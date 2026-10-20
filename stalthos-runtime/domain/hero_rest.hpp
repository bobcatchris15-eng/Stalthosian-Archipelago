#pragma once
#include "needs.hpp"
namespace stalthos::domain {
// Routine rest owns no movement, clock, resource or serialized state.
class HeroRestAutonomy {
    bool active_=false;
public:
    static constexpr double recoveredFatigue=.3;
    bool update(needs::NeedsCore& needs,bool alive,bool direct) {
        if(!alive || direct) active_=false;
        else if(active_) {
            const double other=std::max(needs.hunger(),needs.thirst());
            if(needs.fatigue()<=recoveredFatigue || (other>.5 && other>=needs.fatigue())) active_=false;
        } else if(needs.suggestActivity()==needs::SuggestedActivity::Rest) active_=true;
        needs.setResting(active_);
        return active_;
    }
};
}
