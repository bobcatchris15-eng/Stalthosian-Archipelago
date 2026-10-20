#include "domain/hero_rest.hpp"
#include <iostream>
#include <stdexcept>
using namespace stalthos;
void check(bool condition,const char* label){if(!condition)throw std::runtime_error(label);}
int main(){try{
 needs::NeedsCore n(HeroSpecies::Smalltrotter);n.restoreState(0,0,.8,0,false);domain::HeroRestAutonomy r;
 check(r.update(n,true,false)&&n.isResting(),"rest enters");n.advance({1,1});check(std::abs(n.fatigue()-.75)<1e-12,"physical recovery once");
 check(!r.update(n,true,true)&&!n.isResting(),"direct immediate override");n.advance({1,1});check(std::abs(n.fatigue()-.755)<1e-12,"direct does not recover");
 check(r.update(n,true,false),"release resumes tired rest");for(int i=0;i<10;++i){r.update(n,true,false);n.advance({1,1});}check(n.fatigue()<.4,"rest hysteresis recovers below entry threshold");
 while(n.fatigue()>.3){r.update(n,true,false);n.advance({.1,.1});}check(!r.update(n,true,false),"rest leaves recovered state");
 needs::NeedsCore a(HeroSpecies::Smalltrotter),b(HeroSpecies::Smalltrotter);a.restoreState(0,0,.8,0,false);b.restoreState(0,0,.8,0,false);domain::HeroRestAutonomy ra,rb;ra.update(a,true,false);rb.update(b,true,false);a.advance({1,1});b.advance({1,100});check(a.fatigue()==b.fatigue(),"calendar does not multiply rest recovery");
 a.restoreState(.9,0,.7,0,true);check(!ra.update(a,true,false)&&a.suggestActivity()==needs::SuggestedActivity::Eat,"higher food need leaves rest");a.restoreState(0,0,.8,0,true);check(!ra.update(a,false,false)&&!a.isResting(),"terminal hero stops rest");
 std::cout<<"HERO_REST_OK physical recovery, hysteresis, direct override, higher need, terminal stop\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
