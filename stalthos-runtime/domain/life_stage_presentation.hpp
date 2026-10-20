#pragma once
#include "lifecycle.hpp"
#include <array>
namespace stalthos::domain {
// Proposed horizontal bulk only. Original starting bulk and vertical datum stay intact.
struct LifeStagePresentationTuning {std::array<double,5> horizontalBulk{{1,1,1.25,1.25,1.25}};};
inline double lifeStageBulk(LifeStage stage,const LifeStagePresentationTuning& tuning={}){
 for(double value:tuning.horizontalBulk)if(!std::isfinite(value)||value<.5||value>2)throw std::invalid_argument("invalid life-stage bulk");
 return tuning.horizontalBulk.at(static_cast<unsigned>(stage));
}
inline const char* lifeStageName(LifeStage stage){static constexpr std::array<const char*,5> names{{"hatchling","juvenile","adult","old","dead"}};return names.at(static_cast<unsigned>(stage));}
}
