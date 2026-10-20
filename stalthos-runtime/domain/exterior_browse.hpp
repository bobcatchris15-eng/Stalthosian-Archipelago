#pragma once
#include "exterior_residents.hpp"
#include "resource_ledger.hpp"
namespace stalthos::fauna {
struct ExteriorBrowsePatch {double x=144,z=20,quantity=1,scale=.7;static constexpr Identity resource=208;static constexpr std::size_t shrubIndex=8;};
// Creation-only habitat content; original eight actors/history remain unchanged.
inline Population withExteriorBrowse(const Population& original,ExteriorHabitat& habitat,const std::set<Identity>& excluded,const ExteriorBrowsePatch& patch={}){
 const auto site=habitat.residents.back();
 if(site>=habitat.sites.size() || std::hypot(habitat.sites[site].position.x-patch.x,habitat.sites[site].position.z-patch.z)>.01 || !std::isfinite(patch.quantity)||patch.quantity<=0)throw std::invalid_argument("exterior browse patch must match resident site");
 habitat.sites[site].browse=true;auto result=withExteriorResidents(original,habitat,excluded);auto state=result.snapshot();auto& browser=state.actors.back();browser.site=site;browser.target=site;browser.position=habitat.sites[site].position;browser.hunger=.55;browser.thirst=.25;browser.fatigue=.28;browser.activity=Activity::Browse;result.restore(state,excluded);return result;
}
}
