#include "domain/courtship.hpp"
#include "domain/nesting.hpp"
#include <iostream>
using namespace stalthos;
static void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{
 fauna::ActorView mate{2,HeroSpecies::Smalltrotter,{5,2,0},0,3000,0,.1,.1,.1,true,fauna::Activity::Rest};std::vector<fauna::ActorView> actors{mate};domain::SmalltrotterCourtship pair;
 require(!pair.request(1,HeroSpecies::Smalltrotter,LifeStage::Hatchling,{0,2,0},true,actors),"adult hero required");require(!pair.request(1,HeroSpecies::Smalltrotter,LifeStage::Adult,{30,2,0},true,actors),"camera cannot court remote mate");
 actors[0].thirst=.8;require(!pair.request(1,HeroSpecies::Smalltrotter,LifeStage::Adult,{0,2,0},true,actors),"unavailable real mate refuses");actors[0]=mate;
 require(pair.request(1,HeroSpecies::Smalltrotter,LifeStage::Adult,{0,2,0},true,actors),"voluntary invitation selects actual adult");pair.advance(1,{0,2,0},true,true,false,2,actors);require(!pair.ready(),"elapsed timer alone cannot respond remotely");actors[0].position.x=3;pair.advance(1,{0,2,0},true,true,false,1,actors);require(!pair.ready(),"moving mate is not quiet response");pair.advance(1,{0,2,0},true,true,false,2,actors);require(pair.acceptedMate(1)==2 && pair.acceptedMate(3)==0 && pair.currentMateReady({0,2,0},actors),"real nearby quiet response tied hero and mate");
 domain::Nesting nest;domain::NestSite site{{0,2,2},true,true,true,0};require(!nest.lay(1,HeroSpecies::Smalltrotter,LifeStage::Adult,.1,.1,.1,site,0),"ordinary clutch cannot bypass accepted mate");require(nest.lay(1,HeroSpecies::Smalltrotter,LifeStage::Adult,.1,.1,.1,site,0,pair.acceptedMate(1)) && nest.clutch()->mate==2,"accepted actual response admits clutch with mate identity");
 pair.advance(1,{0,2,0},true,true,true,.1,actors);require(!pair.active() && pair.acceptedMate(1)==0,"direct or destination cancels role");require(pair.request(1,HeroSpecies::Smalltrotter,LifeStage::Adult,{0,2,0},true,actors),"may invite again");actors[0].alive=false;pair.advance(1,{0,2,0},true,true,false,.1,actors);require(!pair.active(),"actual mate death cancels");actors[0]=mate;pair.request(1,HeroSpecies::Smalltrotter,LifeStage::Adult,{0,2,0},true,actors);pair.advance(1,{0,2,0},true,false,false,.1,actors);require(!pair.active(),"ordinary needs priority");
 std::cout<<"COURTSHIP_OK voluntary actual local adult mate physical quiet response no bypass cancellation and mate-bound clutch\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
