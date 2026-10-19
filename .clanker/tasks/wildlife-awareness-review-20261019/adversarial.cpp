#include "engine/native_population_registry.hpp"
#include "domain/host_save.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::fauna;
static void require(bool p,const char* m){if(!p)throw std::runtime_error(m);std::cout<<m<<"=1\n";}
static Population fixture(const std::vector<HabitatSite>& sites,Activity activity,std::size_t site=0,std::size_t target=0,Vec3 pos={0,0,0}){Population p(42,1001,sites,1);auto s=p.snapshot();auto& a=s.actors[0];a.site=site;a.target=target;a.position=pos;a.activity=activity;a.thirst=.8;a.hunger=.1;a.fatigue=.1;a.headingRadians=0;p.restore(s);return p;}
int main(){try{
 const TimeStep dt{.1,.1};BodyAwareness hero{{-2,0,0},{}};
 std::vector<HabitatSite> sites{{{0,0,0},true,true,{1,2}},{{4,0,0},true,true,{0}},{{0,0,4},true,true,{0}}};
 auto p=fixture(sites,Activity::Drink);const auto id=p.snapshot().actors[0].id;p.advance(dt,false,hero);auto s=p.snapshot();require(s.actors[0].id==id && s.actors[0].target==1 && s.actors[0].position.x>0 && s.actors[0].position.z==0,"onsite_selects_farther_existing_edge");require(std::abs(s.actors[0].distanceTravelledMeters-.07)<1e-10,"avoidance_speed_uses_physical_time");Population closure(42,1001,sites,1);closure.restore(s);
 auto mid=fixture(sites,Activity::Travel,0,1,{2,0,0});mid.advance(dt,false,BodyAwareness{{3,0,0},{}});auto m=mid.snapshot();require(m.actors[0].position.z==0 && m.actors[0].position.x>2 && m.actors[0].target==1,"mid_edge_finishes_existing_hop_no_diagonal_retarget");closure.restore(m);
 auto notice=fixture(sites,Activity::Drink);notice.advance(dt,false,BodyAwareness{{8,0,0},{}});auto n=notice.snapshot().actors[0];require(n.position.x==0 && n.position.z==0 && std::abs(n.headingRadians-1.5707963267948966)<1e-10,"notice_radius_orients_stationary_actor_without_flee");
 std::vector<HabitatSite> trappedSites{{{0,0,0},true,true,{1}},{{-1,0,0},true,true,{0}}};auto trapped=fixture(trappedSites,Activity::Drink);
 ResourceLedgerDto ledger{{{100,ResourceKind::Creek,10,0}}, {}};std::uint64_t next=1;engine::NativePopulationRegistry native(trapped,ledger,{},{{0,100,{0,0,0}},{1,100,{-1,0,0}}},next);const auto bindings=native.sessions();native.advance({1,1},true,hero);
 const auto out=native.snapshot().actors[0];std::cout<<"trapped_actual_consumed="<<native.ledger().resources[0].consumedQuantity<<" thirst="<<out.thirst<<'\n';
 require(native.ledger().resources[0].consumedQuantity==0 && native.ledger().sessions.empty(),"trapped_avoidance_suppresses_contact_without_escape_edge");require(native.sessions()==bindings && out.id==trapped.snapshot().actors[0].id,"awareness_preserves_ids_and_explicit_session_owners");
 auto local=trapped;local.advance({1,1},true,hero);require(local.snapshot().actors[0].thirst>=.8,"standalone_trapped_avoidance_grants_no_local_relief");
 std::cout<<"INDEPENDENT_WILDLIFE_AWARENESS_OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
