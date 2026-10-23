#include "engine/connected_save.hpp"
#include "domain/weather_response.hpp"
#include "domain/environmental_exposure.hpp"
#include <iostream>
#include <iterator>
using namespace stalthos;
using namespace stalthos::fauna;
namespace cs=engine::connected_save;
static void need(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}need(rejected,m);}
static std::string bytes(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main(){try{
 std::vector<HabitatSite> sites{{{0,0,0},false,true,{1}},{{4,0,0},true,false,{0}}};
 std::vector<RegionalResourceAnchor> anchors{{0,100,sites[0].position},{1,200,sites[1].position}};
 ResourceLedgerDto ledger{{{100,ResourceKind::Creek,70,3},{200,ResourceKind::Shrub,9,1},{210,ResourceKind::Shrub,.8,.8},{211,ResourceKind::Shrub,.8,.8}},{{90,100,3},{91,200,1},{101,210,.8},{102,211,.8}}};
 Population pop(42,1001,sites,2);auto snapshot=pop.snapshot();snapshot.actors[0].id=123;snapshot.actors[1].id=124;snapshot.actors[1].species=HeroSpecies::Longstride;
 const auto&p=getHeroProfile(HeroSpecies::Longstride);snapshot.actors[1].calendarAge=p.hatchlingDuration+p.juvenileDuration+p.adultDuration+p.oldDuration;snapshot.actors[1].stage=LifeStage::Dead;snapshot.actors[1].activity=Activity::Rest;pop.restore(snapshot);
 std::uint64_t allocator=110;engine::NativePopulationRegistry admitted(pop,ledger,{{123,90}},anchors,allocator);
 SimCore clock;clock.advance(3,7);StalthosState world(42);world.setHero(1,1001);world.heroDied();world.setHero(2,1001);world.recordTree(1001,{300,TreeCondition::StandingDamaged,21});
 persistence::HostPose pose{{2,0,3},.4,1};persistence::CameraStateSnapshot camera;camera.mode=1;camera.focus=1;camera.detachedAnchor={4,3,5};camera.orbitDistance=10;
 for(unsigned kind=0;kind<5;++kind){
 auto h=persistence::captureSave(clock,world,pose,camera);h.heroNeeds=persistence::HeroNeedsSnapshot{2,HeroSpecies(kind),.2,.3,.4,.1,false};h.heroLifecycle=persistence::HeroLifecycleSnapshot{2,HeroSpecies(kind),100,LifeStage::Hatchling};h.faunaPopulation=admitted.snapshot();h.resourceLedger=admitted.ledger();h.faunaResourceSessions=admitted.sessions();
 cs::Candidate c; c.simulation={h,admitted.continuation(),cs::configuration(false),12};c.nextSession=allocator;c.physicalSession=102;c.calendarScale=7;c.wetness=.63;
 cs::validateAdmission(c,admitted.snapshot(),ledger);
 auto rejectedLayout=c;rejectedLayout.simulation.host.resourceLedger->resources.pop_back();rejects([&]{cs::validateAdmission(rejectedLayout,admitted.snapshot(),ledger);},"missing211 admitted");rejectedLayout=c;rejectedLayout.simulation.host.faunaPopulation->actors[0].species=HeroSpecies((unsigned(rejectedLayout.simulation.host.faunaPopulation->actors[0].species)+1)%5);rejects([&]{cs::validateAdmission(rejectedLayout,admitted.snapshot(),ledger);},"changed species admitted");
 auto encoded=cs::encode(c);auto decoded=cs::decode(encoded,c.simulation.configuration);auto restored=cs::restore(decoded,admitted,c.simulation.configuration);
 need(cs::encode(decoded)==encoded,"canonical exact roundtrip");need(decoded.physicalSession==102&&decoded.nextSession==allocator&&decoded.wetness==.63,"session and wetness retention");
 environment::BodyWeatherResponse before,after;before.restoreWetness(.63);after.restoreWetness(decoded.wetness);auto conditions=environment::previewWeather(environment::sample(42,1001,21,{0,0,.35,0}),1);auto wb=before.advance(conditions,.25,.2),wa=after.advance(conditions,.25,.2);need(wb.wetness==wa.wetness&&wb.comfort==wa.comfort,"wetness continuation diverged");needs::NeedsCore nb{HeroSpecies(kind)},na{HeroSpecies(kind)};nb.restoreState(.2,.3,.4,.1,false);na.restoreState(.2,.3,.4,.1,false);environment::applyExposure(nb,wb,.25);environment::applyExposure(na,wa,.25);need(nb.fatigue()==na.fatigue(),"restored weather fatigue diverged");rejects([&]{after.restoreWetness(-.1);},"negative wetness accepted");need(restored.clock.calendarSeconds()==21&&restored.clock.physicalSeconds()==3,"shared clocks");need(restored.world.heroDeathKnown(1),"old death retained");need(restored.world.regionSnapshot(1001).revision==1,"tree revision retained");need(restored.fauna->snapshot().actors[1].stage==LifeStage::Dead,"dead NPC retained");need(restored.fauna->ledger().sessions.size()==4,"full exhausted history retained");need(restored.fauna->sessions()==admitted.sessions(),"binding identity");
 auto invalid=c;invalid.physicalSession=90;rejects([&]{cs::encode(invalid);},"hero current session aliases NPC");invalid=c;invalid.wetness=1.01;rejects([&]{cs::encode(invalid);},"wetness out of bounds");invalid=c;invalid.nextSession=90;rejects([&]{cs::encode(invalid);},"allocator collision accepted");invalid=c;invalid.calendarScale=std::numeric_limits<double>::quiet_NaN();rejects([&]{cs::encode(invalid);},"NaN scale accepted");
 rejects([&]{cs::decode(encoded,c.simulation.configuration+1);},"config mismatch accepted");auto broken=encoded;broken[20]^=1;rejects([&]{cs::decode(broken,c.simulation.configuration);},"corruption accepted");rejects([&]{cs::decode(encoded.substr(0,encoded.size()-1),c.simulation.configuration);},"truncation accepted");
 const std::filesystem::path destination="var/review-connected-persistence/destination.stlc";cs::write(destination,c,admitted,c.simulation.configuration);need(bytes(destination)==encoded,"disk bytes mismatch");
 for(auto f:{persistence::host_save::Failure::Write,persistence::host_save::Failure::Close,persistence::host_save::Failure::Readback,persistence::host_save::Failure::Replace}){rejects([&]{cs::write(destination,c,admitted,c.simulation.configuration,f);},"injected failure accepted");need(bytes(destination)==encoded,"failed write changed destination");}
 {std::ofstream out(destination,std::ios::binary|std::ios::trunc);out<<"STL9 original unrelated sentinel";}const auto original=bytes(destination);rejects([&]{cs::write(destination,c,admitted,c.simulation.configuration);},"unrelated destination overwritten");need(bytes(destination)==original,"unrelated destination changed");std::filesystem::remove(destination);
 }
 const std::filesystem::path asset="var/review-connected-persistence/asset.bin";{std::ofstream out(asset,std::ios::binary);out<<"asset-a";}const auto f=cs::fingerprint(true,{asset},{1,2,.3});need(f!=cs::fingerprint(true,{asset},{1,3,.3}),"stride change not bound");{std::ofstream out(asset,std::ios::binary|std::ios::trunc);out<<"asset-b";}need(f!=cs::fingerprint(true,{asset},{1,2,.3}),"asset byte change not bound");std::filesystem::remove(asset);
 std::cout<<"CONNECTED_REVIEW_OK: wetness/fatigue continuation; physical session; independent admission; asset/motion fingerprint; five species; exact clocks/camera/pose/archive; old death/tree revision/dead NPC/bindings/exhausted210+211; invalid allocator/config/NaN/truncation/checksum; four failure-atomic writes and unrelated destination preservation\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
