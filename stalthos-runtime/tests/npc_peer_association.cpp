#include "domain/npc_peer_association.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::fauna;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 ActorView a{10,HeroSpecies::Smalltrotter,{0,0,0},0,4000,0,.2,.2,.2,true,Activity::Rest};
 ActorView b=a;b.id=20;b.position={8,0,0};std::vector<ActorView> actors{b,a};NpcPeerAssociation pair;
 for(int i=0;i<4;++i)pair.update(actors,.5);
 check(pair.leader()==10 && pair.link().peer()==20 && pair.motion(actors),"stable deterministic existing IDs form pair");
 check(pair.link().links().getLinks(10)->front().role==relationships::Role::Peer,"uses relationship ownership");
 check(!pair.motion(actors,20),"hero peer priority suppresses conflicting pair movement");
 actors[0].thirst=.7;pair.update(actors,.1);check(!pair.motion(actors) && pair.link().peer()==20,"follower need interrupts without deleting relationship");actors[0].thirst=.2;
 actors[0].position.x=40;pair.update(actors,.1);check(!pair.link().present() && !pair.motion(actors),"actual departure separates");
 actors[0].position.x=8;pair.update(actors,.1);check(pair.link().present() && pair.link().peer()==20,"same identity reunion");
 actors[1].thirst=.7;pair.update(actors,.1);check(!pair.motion(actors),"leader needs interrupt both actors");actors[1].thirst=.2;
 actors[0].alive=false;pair.update(actors,.1);check(!pair.motion(actors) && pair.link().links().isConfirmedDead(20),"dead peer cannot move");
 std::vector<HabitatSite> sites{{{0,0,0},true,true,{1}},{{4,0,0},false,false,{0,2}},{{8,0,0},false,false,{1}}};Population population(42,1001,sites,2,{1});auto state=population.snapshot();
 for(std::size_t i=0;i<2;++i){auto& n=state.actors[i];n.species=HeroSpecies::Smalltrotter;n.calendarAge=4000;n.stage=LifeStage::Adult;n.site=i?2:0;n.target=n.site;n.position=sites[n.site].position;n.hunger=.2;n.thirst=.2;n.fatigue=.2;n.activity=Activity::Rest;n.isResting=true;}
 population.restore(state,{1});const auto leader=state.actors[0].id,follower=state.actors[1].id;NpcPeerMotion motion{leader,follower,{0,0,0},3,.7};population.advance({1,1},false,std::nullopt,motion);
 check(population.actors()[0].position.x==0 && std::abs(population.actors()[1].position.x-7.3)<1e-9,"leader waits follower uses physical graph motion");check(population.actors()[0].calendarAge==4001,"calendar advances once");
 for(int i=0;i<20;++i)population.advance({.5,.5},false,std::nullopt,motion);
 check(std::abs(population.actors()[1].position.x-3)<1e-6 && population.actors()[1].activity==Activity::Travel,"follower visibly waits without overlapping leader");
 auto paused=population.snapshot();paused.actors[1].thirst=.7;population.restore(paused,{1});population.advance({1,1},false,std::nullopt);check(population.actors()[1].position.x<3,"needs interruption resumes existing edge without teleport");
 auto need=state;need.actors[0].thirst=.6;need.actors[0].activity=Activity::Drink;need.actors[0].isResting=false;population.restore(need,{1});population.advance({1,1},false,std::nullopt,motion);check(population.actors()[0].activity==Activity::Drink,"water need keeps eligible contact activity");
 population.restore(state,{1});population.advance({1,1},false,BodyAwareness{{9,0,0},{}},motion);check(population.actors()[1].position.x<8,"hero avoidance retains graph ownership");
 NpcPeerAssociation first,second;auto stable=std::vector<ActorView>{a,b};for(int i=0;i<20;++i){first.update(stable,.1);second.update(stable,.1);}check(first.leader()==second.leader() && first.link().peer()==second.link().peer(),"no camera data in pair authority");
 std::cout<<"NPC_PEER_OK existing IDs relationship graph wait approach needs departure reunion death hero priority once-only time\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what();return 1;}}
