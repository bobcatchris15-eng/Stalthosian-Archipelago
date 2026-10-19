#include "domain/host_save.hpp"
#include "engine/native_population_registry.hpp"
#include <iostream>
using namespace stalthos;
int main(){try{
 std::vector<fauna::HabitatSite> sites{{{0,0,0},false,true,{1}},{{4,0,0},true,false,{0,2}},{{8,0,0},false,true,{1,3}},{{12,0,0},true,false,{2}}};
 fauna::Population population(42,1001,sites,8);ResourceLedgerDto ledger{{{100,ResourceKind::Creek,100,0},{200,ResourceKind::Shrub,100,0}}, {}};
 std::uint64_t next=1;engine::NativePopulationRegistry owner(population,ledger,{},{{0,100,sites[0].position},{1,200,sites[1].position},{2,100,sites[2].position},{3,200,sites[3].position}},next);
 SimCore clock;StalthosState world(42);world.setHero(1,1);for(int i=0;i<200;++i){auto dt=clock.advance(.05,2);owner.advance(dt,true);}
 auto saved=persistence::captureSave(clock,world);saved.faunaPopulation=owner.snapshot();saved.resourceLedger=owner.ledger();saved.faunaResourceSessions=owner.sessions();
 const auto bytes=persistence::host_save::legacyBytes(saved);std::cout<<"default_state_checksum="<<persistence::host_save::checksum(bytes)<<" bytes="<<bytes.size()<<" actors="<<saved.faunaPopulation->actors.size()<<" ledgerkeys="<<saved.resourceLedger->sessions.size()<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
