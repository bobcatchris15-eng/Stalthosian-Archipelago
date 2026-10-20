#pragma once
#include "foundation.hpp"
#include <algorithm>
#include <limits>
namespace stalthos::domain {
struct NextLife {Identity hero;uint64_t session;};
// Explicit session-local continuation. Recorded death is authoritative even when
// the old hero was never retained as an animal. No world or ledger reconstruction.
inline NextLife beginNextLife(StalthosState& world,Identity dead,const std::vector<Identity>& npcIds,uint64_t& nextSession){
 const auto state=world.snapshot();
 if(state.hero)throw std::logic_error("current hero must die before next life");
 const auto death=std::find_if(state.deadHeroes.begin(),state.deadHeroes.end(),[&](const auto& d){return d.first==dead;});
 if(death==state.deadHeroes.end())throw std::logic_error("next life requires recorded death");
 if(nextSession==0)throw std::overflow_error("contact session exhausted");
 Identity candidate=dead;
 do{if(candidate==std::numeric_limits<Identity>::max())throw std::overflow_error("hero identity exhausted");++candidate;}
 while(world.excluded(candidate)||std::find(npcIds.begin(),npcIds.end(),candidate)!=npcIds.end());
 world.setHero(candidate,death->second);
 return {candidate,nextSession++};
}
}
