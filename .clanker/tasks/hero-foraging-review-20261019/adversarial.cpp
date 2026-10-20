#include "domain/hero_autonomy.hpp"
#include "domain/resource_ledger.hpp"
#include <iostream>
#include <stdexcept>
using namespace stalthos;
void check(bool x,const char* s){if(!x)throw std::runtime_error(s);std::cout<<"PASS "<<s<<'\n';}
int main(){try{
HeroSim h; needs::NeedsCore n(HeroSpecies::Smalltrotter);n.restoreState(.8,0,0,0,false);domain::HeroDrinkAutonomy a;
auto eat=[&](Vec3 b,bool direct,bool available,bool eligible,Identity id=200){return a.update(h,n,b,0,.1,direct,Vec3{0,0,10},available,eligible,needs::SuggestedActivity::Eat,id);};
auto out=eat({0,0,0},false,true,false);check(out.forward&&out.active&&!out.contact&&h.perception.attention(),"hungry body chooses local browse without remote contact");
out=eat({0,0,0},true,true,true);check(!out.active&&!out.contact&&!out.forward&&h.authority()==Authority::Direct,"direct cancels forage same frame even eligible");
out=eat({0,0,0},false,true,false);check(out.active,"forage resumes after direct release");
out=eat({0,0,40},false,true,true);check(!out.active&&!out.contact&&!h.perception.attention(),"remote body cannot forge eligible contact after active goal");
out=eat({0,0,0},false,false,true);check(!out.active&&!out.contact,"depleted resource rejects forged eligibility");
out=eat({0,0,0},false,true,false,201);check(out.active&&h.perception.attention()->source==201,"resource change binds actual new identity");
check(n.hunger()==.8,"selection and motion grant no hunger relief");
ResourceLedger ledger;check(ledger.load({{201,ResourceKind::Shrub,2,0},{100,ResourceKind::Creek,2,0}}),"shared ledger initialized");InteractionsCore contact;contact.beginSession(9);
out=eat({0,0,9},false,true,true,201);check(out.contact,"eligible browse selected");double dummy=2;auto transfer=[&](){if(ledger.consume(9,201,.5)&&contact.applyContact(9,201,true,.5,dummy)){n.feed(.5);return true;}return false;};check(transfer(),"accepted shared-ledger contact relieves hunger");check(!transfer()&&std::abs(n.hunger()-.3)<1e-12,"same session replay cannot relieve twice");check(ledger.snapshot()[0].consumedQuantity==0,"browse transfer leaves creek unchanged");a.completedContact(h);out=eat({0,0,9},false,true,true,201);check(!out.contact&&!out.forward,"satisfied need stops forage");
HeroSim thirsty;needs::NeedsCore thirst(HeroSpecies::Smalltrotter);thirst.restoreState(0,.8,0,0,false);domain::HeroDrinkAutonomy drink;auto d=drink.update(thirsty,thirst,{0,0,0},0,.1,false,Vec3{0,0,10},true,false);check(d.forward,"original drink API approach preserved");d=drink.update(thirsty,thirst,{0,0,9},0,.1,false,Vec3{0,0,10},true,true);check(d.contact,"original drink API contact preserved");
return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
