#include "domain/host_save.hpp"
#include "engine/native_population_registry.hpp"
#include <iostream>
using namespace stalthos;
using namespace stalthos::fauna;
namespace hs=stalthos::persistence::host_save;
static void check(bool pass,const char* label){if(!pass)throw std::runtime_error(label);std::cout<<label<<"=1\n";}
template<class F>static bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
static std::string disk(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main(){std::cout.setf(std::ios::unitbuf);try{
 std::vector<HabitatSite> sites{{{0,0,0},false,true,{1}},{{2,0,0},true,false,{0}}};
 Population original(42,1001,sites,8);
 auto imported=original.snapshot();
 auto old=imported.actors.front().species;
 imported.actors.front().species=old==HeroSpecies::Smalltrotter?HeroSpecies::Brackenback:HeroSpecies::Smalltrotter;
 imported.actors.front().calendarAge=0;imported.actors.front().stage=LifeStage::Hatchling;
 ResourceLedgerDto ledger{{{100,ResourceKind::Creek,100,3}},{{90,100,3}}};
 std::vector<RegionalResourceAnchor> anchors{{0,100,sites[0].position}};
 Population restored(42,1001,sites,8);restored.restore(imported);
 const std::vector<std::pair<Identity,std::uint64_t>> sessions{{imported.actors.front().id,90}};
 RegionalPopulationRegistry registry(restored,ledger,{4096,256},sessions);
 check(registry.configureContacts(1001,anchors),"fixture_contacts");
 check(registry.setActive(1001,true,RegionalAdmissionOrigin::PhysicalArrival),"fixture_active");
 SimCore clock;StalthosState world(42);world.setHero(1,1);
 auto save=persistence::captureSave(clock,world);save.faunaPopulation=registry.snapshot(1001)->population;save.resourceLedger=registry.snapshot(1001)->ledger;save.faunaResourceSessions=sessions;
 hs::Candidate candidate{save,registry.continuationSnapshot(),2,1};
 RegionalPopulationRegistry admitted(restored,ledger,{4096,256},save.faunaResourceSessions);
 check(admitted.configureContacts(1001,anchors),"fixture_external_contacts");
 check(admitted.setActive(1001,true,RegionalAdmissionOrigin::PhysicalArrival),"fixture_external_active");
 RegionalPopulationRegistry originalAdmitted(original,ledger,{4096,256},save.faunaResourceSessions);
 check(originalAdmitted.configureContacts(1001,anchors),"original_external_contacts");
 check(originalAdmitted.setActive(1001,true,RegionalAdmissionOrigin::PhysicalArrival),"original_external_active");
 check(rejects([&]{hs::restore(candidate,originalAdmitted,2);}),"external_species_mismatch_rejected");
 auto decoded=hs::decode(hs::encode(candidate),2);
 std::cout<<"saved_derived_baseline_is_not_external_config="<<registry.restoreContinuation(*decoded.regional)<<" old="<<static_cast<int>(old)<<" new="<<static_cast<int>(registry.snapshot(1001)->population.actors.front().species)<<'\n';
 auto empty=decoded;empty.host.faunaPopulation->actors.clear();empty.host.faunaResourceSessions.clear();empty.regional->regions[0].population.actors.clear();empty.regional->regions[0].bindings.clear();
 auto e=hs::decode(hs::encode(empty),2);std::cout<<"legal_empty_legacy_roundtrip="<<e.host.faunaPopulation->actors.empty()<<'\n';
 auto altered=*decoded.regional;altered.regions[0].reservation.resources--;
 auto before=hs::encode(decoded);auto rejected=!registry.restoreContinuation(altered);
 decoded.regional=registry.continuationSnapshot();std::cout<<"reservation_rejection_unchanged="<<(rejected && hs::encode(decoded)==before)<<'\n';
 auto bad=candidate;bad.regional->regions[0].ledger.resources[0].consumedQuantity+=1;
 check(rejects([&]{hs::encode(bad);}),"complete_ledger_mismatch_rejected");
 bad=candidate;bad.regional->regions[0].bindings[0].session+=900;
 check(rejects([&]{hs::encode(bad);}),"actor_session_mismatch_rejected");
 auto bytes=hs::encode(candidate);
 for(auto offset:{24u,32u,40u,48u,56u,64u}){
  auto b=bytes;for(unsigned i=0;i<8;++i)b[offset+i]=char(-1);
  const auto h=hs::checksum(std::string_view(b).substr(0,b.size()-8));for(unsigned i=0;i<8;++i)b[b.size()-8+i]=static_cast<char>(h>>(8*i));
  check(rejects([&]{hs::decode(b,2);}),"rehash_header_mutation_rejected");
 }
 const auto path=std::filesystem::path("E:/Stalthosian Archipelago/.clanker/tasks/native-regional-save-review-20261018/readonly-save.stl");
 hs::write(path,candidate,admitted,2);const auto prior=disk(path);
#ifdef _WIN32
 check(SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_READONLY)!=0,"set_readonly_destination");
 auto newer=candidate;newer.generation=2;
 const bool denied=rejects([&]{hs::write(path,newer,admitted,2);});
 SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_NORMAL);
 check(denied && disk(path)==prior && hs::read(path,2).generation==1,"real_readonly_replacement_failure_retains_previous");
#endif
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
