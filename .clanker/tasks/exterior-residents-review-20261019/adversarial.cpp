#include "domain/exterior_residents.hpp"
#include "domain/host_save.hpp"
#include <iostream>
using namespace stalthos;using namespace stalthos::fauna;
static void require(bool p,const char* m){if(!p)throw std::runtime_error(m);std::cout<<m<<"=1\n";}
template<class F>static bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
static std::string bytes(PopulationSnapshot snapshot){persistence::SaveData s;s.seed=snapshot.seed;s.faunaPopulation=snapshot;return persistence::host_save::legacyBytes(s);}
int main(){std::cout.setf(std::ios::unitbuf);try{
 std::vector<HabitatSite> sites{{{0,0,24},true,true,{1}},{{12,0,24},true,true,{0}}};Population original(42,1001,sites,8,{1});const auto before=original.snapshot();
 auto habitat=exteriorHabitat(sites,[](double,double){return 0.;},[](double,double){return false;});auto combined=withExteriorResidents(original,habitat,{1});auto s=combined.snapshot();auto firstEight=s;firstEight.actors.resize(8);firstEight.habitatSignature=before.habitatSignature;
 require(bytes(firstEight)==bytes(before),"all_original_eight_snapshot_bytes_preserved_except_approved_signature");require(s.actors.size()==12 && habitat.sites.size()==sites.size()+8,"four_real_residents_and_appended_graph_nodes");
 unsigned added=0;for(std::size_t i=0;i<sites.size();++i){const auto& old=sites[i];const auto& now=habitat.sites[i];require(old.position.x==now.position.x && old.position.y==now.position.y && old.position.z==now.position.z && old.browse==now.browse && old.freshwater==now.freshwater,"old_site_geometry_resources_unchanged");for(auto edge:old.neighbors)require(std::find(now.neighbors.begin(),now.neighbors.end(),edge)!=now.neighbors.end(),"old_edges_preserved");added+=unsigned(now.neighbors.size()-old.neighbors.size());}require(added==1,"exactly_one_valid_old_graph_join_added");
 for(std::size_t i=sites.size();i<habitat.sites.size();++i)require(!habitat.sites[i].browse && !habitat.sites[i].freshwater,"new_graph_nodes_have_no_fake_food_water");
 std::set<Identity> ids;for(const auto& actor:s.actors)require(actor.id!=1 && ids.insert(actor.id).second,"all_twelve_ids_unique_excluded_hero");
 const auto blocked=s.actors[8].id;auto excluded=withExteriorResidents(original,habitat,{1,blocked});const auto excludedSnapshot=excluded.snapshot();require(std::none_of(excludedSnapshot.actors.begin(),excludedSnapshot.actors.end(),[&](const auto& a){return a.id==blocked;}),"new_resident_explicit_identity_exclusion_honoured");
 for(int tick=0;tick<80;++tick)combined.advance({.25,.25},false);auto later=combined.snapshot();Population restore(42,1001,habitat.sites,12,{1});restore.restore(later,{1});for(std::size_t i=8;i<12;++i)require(later.actors[i].id==s.actors[i].id && later.actors[i].distanceTravelledMeters>s.actors[i].distanceTravelledMeters && later.actors[i].calendarAge>s.actors[i].calendarAge,"resident_identity_motion_clock_retained_with_graph_restore_closure");
 require(rejects([&]{exteriorHabitat(sites,[](double,double){return 0.;},[](double x,double){return x>50;});}),"wet_corridor_rejects_fail_closed");
 require(rejects([&]{exteriorHabitat(sites,[](double x,double){return x>50?(x-50)*2:0.;},[](double,double){return false;});}),"steep_corridor_rejects_fail_closed");require(bytes(original.snapshot())==bytes(before),"failed_extension_leaves_original_population_exact");
 std::cout<<"INDEPENDENT_EXTERIOR_RESIDENTS_OK\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
