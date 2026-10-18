#include "engine/native_population_registry.hpp"
#include "domain/regional_archive.hpp"
#include <iostream>
#include <sstream>
using namespace stalthos;
using namespace stalthos::fauna;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
std::string ledgerBytes(const ResourceLedgerDto& d){std::ostringstream o;o.precision(17);for(auto&r:d.resources)o<<r.id<<','<<int(r.kind)<<','<<r.initialQuantity<<','<<r.consumedQuantity<<';';for(auto&h:d.sessions)o<<h.sessionId<<','<<h.resourceId<<','<<h.amount<<';';return o.str();}
int main(){try{
std::vector<HabitatSite> sites{{{0,0,0},false,true,{1,2}},{{3,0,0},false,true,{0,2}},{{1.5,0,0},true,false,{0,1}}};
Population pop(41,1001,sites,8);auto initial=pop.snapshot();
for(size_t i=0;i<8;++i){auto&a=initial.actors[i];a.site=i%2;a.target=a.site;a.position=sites[a.site].position;a.activity=Activity::Drink;a.thirst=.8;}
initial.actors[7].calendarAge=1e10;initial.actors[7].stage=LifeStage::Dead;initial.actors[7].activity=Activity::Rest;pop.restore(initial);
std::vector<std::pair<Identity,uint64_t>> saved;for(size_t i=0;i<8;++i)saved.emplace_back(initial.actors[i].id,100+i);
std::sort(saved.begin(),saved.end());ResourceLedgerDto old{{{100,ResourceKind::Creek,100,3},{200,ResourceKind::Shrub,10,1},{300,ResourceKind::Shrub,20,2}},{{1,100,1},{100,100,2},{107,300,2},{777,200,1}}};
std::vector<RegionalResourceAnchor> anchors{{0,100,sites[0].position},{1,100,sites[1].position},{2,200,sites[2].position}};
uint64_t next=1000;engine::NativePopulationRegistry host(pop,old,saved,anchors,next);
check(next==1000,"saved empty/dead bindings must not allocate new sessions");check(host.sessions()==saved,"all eight original bindings preserved");
check(ledgerBytes(host.ledger())==ledgerBytes(old),"all resources/history exact import, including unanchored shrub and hero/unbound/dead history");
check(host.snapshot().actors[7].stage==LifeStage::Dead,"dead actor retained");
const auto initialHunger=host.snapshot().actors[0].hunger;check(host.consume(1000,100,.5),"hero uses shared ledger");check(!host.consume(1000,100,.5),"hero replay idempotent");
check(host.snapshot().actors[0].hunger==initialHunger,"hero grants no NPC relief");
for(auto session:{100ull,107ull}){bool rejected=false;try{host.consume(session,200,.1);}catch(const std::exception&){rejected=true;}check(rejected,"hero cannot impersonate living/dead NPC, even on fresh resource");}
for(double amount:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){const auto exact=ledgerBytes(host.ledger());bool denied=false;try{host.consume(2000,100,amount);}catch(const std::exception&){denied=true;}check(denied&&exact==ledgerBytes(host.ledger()),"invalid hero quantity rejected atomically");}
{const auto exact=ledgerBytes(host.ledger());bool denied=false;try{host.consume(0,100,.1);}catch(const std::exception&){denied=true;}check(denied&&exact==ledgerBytes(host.ledger()),"zero hero session rejected atomically");}
host.advance({1,0},true);check(host.events().size()==7,"only seven living actors contact both creek banks");check(host.snapshot().actors[0].thirst<.8,"accepted NPC contact grants relief");
const auto before=host.ledger();host.advanceDormant({2,10});check(ledgerBytes(before)==ledgerBytes(host.ledger()),"dormancy preserves every ledger record");
Population reloaded(41,1001,sites,0);reloaded.restore(host.snapshot());auto mapping=host.sessions();engine::NativePopulationRegistry restored(reloaded,host.ledger(),mapping,anchors,next);
check(restored.sessions()==saved && ledgerBytes(restored.ledger())==ledgerBytes(host.ledger()),"reload complete mapping and ledger exact");
RegionalPopulationRegistry reg(pop,old,{3,256},saved);check(reg.configureContacts(1001,anchors),"shared creek sites accepted");std::string bytes;RegionalContinuationDto decoded;check(regional_archive::encode(reg.continuationSnapshot(),bytes),"shared creek encode");check(regional_archive::decode(bytes,decoded)&&reg.restoreContinuation(decoded),"shared creek decode/restore closure");
auto invalid=decoded;invalid.regions[0].anchors.push_back(invalid.regions[0].anchors[0]);check(!regional_archive::encode(invalid,bytes),"duplicate site-kind archive rejected");
ResourceLedgerDto full{{{100,ResourceKind::Creek,100,.256},{200,ResourceKind::Shrub,10,0}}, {}};for(uint64_t key=10000;key<10256;++key)full.sessions.push_back({key,100,.001});
engine::NativePopulationRegistry capped(pop,full,saved,anchors,next);const auto fullBefore=ledgerBytes(capped.ledger());capped.advance({1,0},true);check(capped.events().empty()&&capped.historyRejections()==7&&ledgerBytes(capped.ledger())==fullBefore,"history full denies seven new NPC keys without ledger mutation");check(capped.snapshot().actors[0].thirst>=initial.actors[0].thirst,"history rejection grants no NPC relief");bool cappedDenied=false;try{capped.consume(2000,100,.1);}catch(const std::exception&){cappedDenied=true;}check(cappedDenied&&ledgerBytes(capped.ledger())==fullBefore,"history full denies hero key atomically");
Population empty(41,1001,sites,0);bool emptyRejected=false;try{engine::NativePopulationRegistry emptyHost(empty,old,{},anchors,next);}catch(const std::exception&){emptyRejected=true;}check(emptyRejected,"compatibility observation: empty legacy population rejected by adapter");std::cout<<"COMPATIBILITY_EDGE empty_legacy_population_rejected=true previous_Population_restore_accepts_empty=true\n";
std::cout<<"INDEPENDENT_NATIVE_REGISTRY_OK actors=8 living_contacts=7 bindings=8 all_ledger_fields=true shared_creek_archive=true hero_impersonation_rejected=true\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
