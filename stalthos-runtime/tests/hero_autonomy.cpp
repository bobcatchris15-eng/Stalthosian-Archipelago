#include "domain/hero_autonomy.hpp"
#include <iostream>
int main(){try {using namespace stalthos;HeroSim hero;needs::NeedsCore needs(HeroSpecies::Smalltrotter);needs.restoreState(0,.8,0,0,false);domain::HeroDrinkAutonomy autonomy;
 auto a=autonomy.update(hero,needs,{24,2,24},-1.57079632679,.1,false,Vec3{6,0,24},true,false);if(!a.forward || !hero.perception.attention()) throw std::runtime_error("ordinary choice absent");
 a=autonomy.update(hero,needs,{24,2,24},-1.57,.1,true,Vec3{6,0,24},true,false);if(a.forward || a.contact || hero.authority()!=Authority::Direct) throw std::runtime_error("direct priority lost");
 a=autonomy.update(hero,needs,{40,2,24},-1.57,.1,false,Vec3{6,0,24},true,false);if(a.active || hero.perception.attention()) throw std::runtime_error("remote camera granted knowledge");
 a=autonomy.update(hero,needs,{8,0,24},-1.57,.1,false,Vec3{6,0,24},true,true);if(!a.contact || a.forward) throw std::runtime_error("contact not selected");
 needs.drink(.5);autonomy.completedContact(hero);a=autonomy.update(hero,needs,{8,0,24},-1.57,.1,false,Vec3{6,0,24},true,true);if(a.contact || a.forward) throw std::runtime_error("satisfied need repeats");
 std::cout<<"HERO_AUTONOMY_OK bodyperception, ordinarydrink, directpriority, remote rejection, satisfaction stop\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
