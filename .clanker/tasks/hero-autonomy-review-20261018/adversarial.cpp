#include "domain/hero_autonomy.hpp"
#include "engine/native_population_registry.hpp"
#include <iostream>
using namespace stalthos;
static void check(bool v,const char* label){if(!v)throw std::runtime_error(label);std::cout<<label<<"=1\n";}
int main(){try{
 needs::NeedsCore need(HeroSpecies::Smalltrotter);need.restoreState(0,.8,0,0,false);HeroSim hero;domain::HeroDrinkAutonomy ai;
 auto a=ai.update(hero,need,{24,0,24},-1.57079632679,.05,false,Vec3{6,0,24},true,false);check(a.forward && hero.perception.attention().has_value(),"body_observation_selects_drink_goal");
 a=ai.update(hero,need,{24,0,24},-1.57079632679,.05,true,Vec3{6,0,24},true,true);check(!a.forward && !a.contact && a.turn==0 && hero.authority()==Authority::Direct && hero.goal()->status==GoalStatus::PlayerCancelled,"direct_cancels_same_frame_even_if_contact_eligible");
 HeroView camera;camera.cameraAttention=CameraStimulus{100,{6,0,24},1};HeroSim remote;domain::HeroDrinkAutonomy remoteAI;
 a=remoteAI.update(remote,need,{40,0,24},0,.05,false,Vec3{6,0,24},true,false);check(camera.cameraAttention.has_value() && !remote.perception.attention() && !remote.goal() && !a.active && !a.forward,"camera_attention_does_not_grant_remote_body_knowledge");
 std::vector<fauna::HabitatSite> sites{{{6,0,24},false,true,{1}},{{24,0,24},true,false,{0}}};fauna::Population pop(42,1001,sites,0);
 ResourceLedgerDto ledger{{{100,ResourceKind::Creek,5,0}}, {}};std::uint64_t next=1;engine::NativePopulationRegistry registry(pop,ledger,{},{{0,100,sites[0].position}},next);
 HeroSim actor;domain::HeroDrinkAutonomy loop;needs::NeedsCore thirst(HeroSpecies::Smalltrotter);thirst.restoreState(0,.8,0,0,false);InteractionsCore contacts;contacts.beginSession(1);
 Vec3 body{24,0,24};double heading=1.57079632679,distance=0;unsigned applied=0;double before=0,after=0;
 for(int i=0;i<600;++i){const double dt=.05;thirst.advance({dt,dt});const bool eligible=std::abs(body.x-6)<2.8;auto action=loop.update(actor,thirst,body,heading,dt,false,Vec3{6,0,24},true,eligible);heading+=action.turn;if(action.forward){auto dir=BeastCore::bodyForward(heading);body.x+=dir.x*2*dt;body.z+=dir.z*2*dt;distance+=2*dt;}if(action.contact){if(registry.consume(1,100,.5)){double resource=5;if(contacts.applyContact(1,100,eligible,.5,resource)){before=thirst.thirst();thirst.drink(.5);after=thirst.thirst();++applied;loop.completedContact(actor);}}}}
 check(applied==1 && registry.ledger().sessions.size()==1 && registry.ledger().resources[0].consumedQuantity==.5,"ordinary_goal_loop_consumes_one_shared_ledger_contact");
 check(std::abs(before-after-.5)<1e-12 && thirst.thirst()<.5,"same_contact_applies_real_thirst_relief");
 check(distance>10 && distance<25 && std::abs(body.x-6)<2.8 && !registry.consume(1,100,.5),"bounded_integrated_approach_and_replay_no_extra_consumption");
 std::cout<<"INDEPENDENT_HERO_AUTONOMY_OK distance="<<distance<<" thirst_final="<<thirst.thirst()<<" applied="<<applied<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
