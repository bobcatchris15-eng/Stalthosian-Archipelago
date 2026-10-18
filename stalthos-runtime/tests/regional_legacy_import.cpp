#include "../domain/regional_population.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace stalthos;
using namespace stalthos::fauna;
static constexpr auto physical=RegionalAdmissionOrigin::PhysicalArrival;
static void require(bool v,const char* why) { if(!v) throw std::runtime_error(why); }
static std::vector<HabitatSite> habitat(double offset=0) {
    return {{{offset,0,0},true,false,{1}},{{offset+4,0,0},false,true,{0}}};
}
static Population population(RegionId region) {
    Population p(41,region,habitat(region==7?0:100),3);
    auto s=p.snapshot();
    s.actors[0].site=0; s.actors[0].target=0; s.actors[0].position=habitat(region==7?0:100)[0].position;
    s.actors[0].activity=Activity::Browse; s.actors[0].hunger=0.8;
    p.restore(s); return p;
}
static ResourceLedgerDto ledger(std::uint64_t base) {
    return {{{base,ResourceKind::Shrub,2,0},{base+1,ResourceKind::Creek,2,0}}, {}};
}
// Serialize every detached field at round-trip precision to check full failure atomicity.
static std::string exact(const RegionalContinuationDto& d) {
    std::ostringstream o; o.precision(17); o<<d.seed<<'/'<<d.legacyRegion;
    for(const auto& r:d.regions) {
        const auto& p=r.population;
        o<<'R'<<p.seed<<','<<p.region<<','<<p.habitatSignature<<','<<r.active<<','<<r.contactsConfigured<<','
            <<r.reservation.resources<<','<<r.reservation.historyKeys;
        for(const auto& a:p.actors) o<<'A'<<a.id<<','<<static_cast<int>(a.species)<<','<<a.position.x<<','<<a.position.y<<','
            <<a.position.z<<','<<a.headingRadians<<','<<a.distanceTravelledMeters<<','<<a.site<<','<<a.target<<','<<a.calendarAge<<','
            <<static_cast<int>(a.stage)<<','<<a.hunger<<','<<a.thirst<<','<<a.fatigue<<','<<a.injury<<','<<a.isResting<<','<<static_cast<int>(a.activity);
        for(const auto& v:r.ledger.resources) o<<'Q'<<v.id<<','<<static_cast<int>(v.kind)<<','<<v.initialQuantity<<','<<v.consumedQuantity;
        for(const auto& v:r.ledger.sessions) o<<'H'<<v.sessionId<<','<<v.resourceId<<','<<v.amount;
        for(const auto& v:r.bindings) o<<'B'<<v.actor<<','<<v.session;
        for(const auto& v:r.anchors) o<<'S'<<v.site<<','<<v.resource<<','<<v.position.x<<','<<v.position.y<<','<<v.position.z;
    }
    return o.str();
}
static Identity actor(const RegionalPopulationRegistry& r,RegionId region,std::size_t i=0) { return r.snapshot(region)->population.actors[i].id; }
static void rejected(RegionalPopulationRegistry& r,const RegionalContinuationDto& bad,const char* why) {
    const auto before=exact(r.continuationSnapshot());
    require(!r.restoreContinuation(bad),why);
    require(before==exact(r.continuationSnapshot()),"rejected continuation must be atomic");
}
int main() {
    try {
        auto history=ledger(100); history.resources[0].consumedQuantity=0.5;
        history.resources[1].consumedQuantity=0.1; history.sessions={{77,100,0.5},{900,101,0.1}};
        const auto id=population(7).actors()[0].id;
        const std::vector<std::pair<Identity,std::uint64_t>> mapping={{id,77}};
        RegionalPopulationRegistry r(population(7),history,{2,256},mapping);
        auto initial=r.continuationSnapshot();
        require(initial.regions[0].bindings[0].actor==id,"saved owner imported");
        require(initial.regions[0].population.habitatSignature==population(7).snapshot().habitatSignature,"habitat preserved");
        require(initial.regions[0].ledger.sessions.size()==2 && initial.regions[0].ledger.resources[0].consumedQuantity==0.5,"complete ledger preserved");
        require(r.restoreContinuation(initial),"unconfigured imported self restore");
        r.configureContacts(7,{{0,100,{0,0,0}},{1,101,{4,0,0}}}); r.setActive(7,true,physical);
        const auto before=exact(r.continuationSnapshot());
        require(!r.contact(7,id,77,0,100,0.5,physical).event,"imported replay no event");
        require(before==exact(r.continuationSnapshot()),"imported replay grants no relief");
        auto result=r.contact(7,id,77,0,100,0.75,physical);
        require(result.admission==ResourceAdmission::Update && result.event && result.event->amount==0.25,"saved cumulative update delta");
        require(!r.bindContactSession(7,actor(r,7,1),900,physical),"unrelated key cannot be claimed");
        const auto saved=r.continuationSnapshot();
        RegionalPopulationRegistry fresh(population(7),history,{2,256},mapping);
        fresh.configureContacts(7,{{0,100,{0,0,0}},{1,101,{4,0,0}}});
        require(fresh.restoreContinuation(saved),"fresh trusted provenance restore");
        require(exact(saved)==exact(fresh.continuationSnapshot()),"all snapshot fields exact");
        auto bad=saved; bad.regions[0].bindings[0].actor=actor(r,7,1); rejected(r,bad,"stolen mapping");
        bad=saved; bad.regions[0].population.actors[0].species=
            bad.regions[0].population.actors[0].species==HeroSpecies::Smalltrotter ? HeroSpecies::Longstride : HeroSpecies::Smalltrotter;
        rejected(r,bad,"retained actor species cannot change");
        bad=saved; bad.regions[0].bindings.clear(); rejected(r,bad,"lost imported owner");
        bad=saved; bad.regions[0].ledger.sessions.clear(); rejected(r,bad,"lost admission history");
        bad=saved; bad.regions[0].ledger.sessions[0].amount=0.25; rejected(r,bad,"imported cumulative floor");
        bad=saved; bad.regions[0].ledger.resources[0].consumedQuantity=0.1; rejected(r,bad,"depletion floor");
        bad=saved; bad.regions[0].bindings.push_back({actor(r,7,1),900}); rejected(r,bad,"unbound imported key theft");
        // The saved host may have allocated an owner before any contact occurred.
        // Such provenance remains immutable even without a matching ledger key.
        RegionalPopulationRegistry emptyHistory(population(7),ledger(100),{2,128},{{id,123}});
        auto emptySaved=emptyHistory.continuationSnapshot();
        require(emptyHistory.restoreContinuation(emptySaved),"empty-history owner self restore");
        bad=emptySaved; bad.regions[0].bindings.clear(); rejected(emptyHistory,bad,"empty-history owner deletion");
        bad=emptySaved; bad.regions[0].bindings[0].session=124; rejected(emptyHistory,bad,"empty-history owner rotation");
        bad=emptySaved; bad.regions[0].bindings[0].actor=actor(emptyHistory,7,1); rejected(emptyHistory,bad,"empty-history owner theft");
        require(emptyHistory.admit(physical,population(8),ledger(200),{2,128},{{population(8).actors()[0].id,123}})==RegionalAdmission::SessionConflict,"empty-history cross-region alias");
        auto aliasedHistory=ledger(200); aliasedHistory.resources[0].consumedQuantity=0.2;
        aliasedHistory.sessions={{123,200,0.2}};
        require(emptyHistory.admit(physical,population(8),aliasedHistory,{2,128})==RegionalAdmission::SessionConflict,"history cannot alias empty-history owner");
        require(exact(emptySaved)==exact(emptyHistory.continuationSnapshot()),"empty-history admission rejection atomic");
        for(const auto& malformed:std::vector<std::vector<std::pair<Identity,std::uint64_t>>>{
            {{0,77}},{{id,0}},{{999,77}},{{id,77},{id,78}},{{id,77},{actor(r,7,1),77}}}) {
            bool rejectedImport=false;
            try { RegionalPopulationRegistry invalid(population(7),history,{2,256},malformed); }
            catch(const std::invalid_argument&) { rejectedImport=true; }
            require(rejectedImport,"malformed bootstrap provenance rejected");
        }
        // Admission rejects cross-region ownership and imported bookkeeping aliases atomically.
        RegionalPopulationRegistry admissions(population(7),history,{2,128},mapping);
        const auto original=exact(admissions.continuationSnapshot());
        for(auto session:{77ULL,900ULL})
            require(admissions.admit(physical,population(8),ledger(200),{2,128},{{population(8).actors()[0].id,session}})==RegionalAdmission::SessionConflict,"cross-region saved alias denied");
        require(original==exact(admissions.continuationSnapshot()),"admission failure atomic");
        require(admissions.capacity().regions==1 && admissions.capacity().historyKeys==128,"failed import budget unchanged");
        require(admissions.admit(physical,population(8),ledger(200),{2,128},{{population(8).actors()[0].id,88}})==RegionalAdmission::Accepted,"valid physical admission provenance");
        require(admissions.restoreContinuation(admissions.continuationSnapshot()),"unconfigured multi-region closure");
        auto dead=population(7); dead.advanceDormant({0,1e10});
        RegionalPopulationRegistry retained(dead,history,{2,256},mapping);
        retained.configureContacts(7,{{0,100,{0,0,0}}}); retained.setActive(7,true,physical);
        const auto corpse=exact(retained.continuationSnapshot());
        require(!retained.contact(7,id,77,0,100,1,physical).event,"dead owner cannot contact");
        require(corpse==exact(retained.continuationSnapshot()) && retained.restoreContinuation(retained.continuationSnapshot()),"dead ownership retained exactly");
        auto full=ledger(100); full.resources[0].initialQuantity=1000; full.resources[0].consumedQuantity=256;
        for(std::uint64_t i=0;i<256;++i) full.sessions.push_back({1000+i,100,1});
        RegionalPopulationRegistry capped(population(7),full,{2,256},{{id,1000}});
        capped.configureContacts(7,{{0,100,{0,0,0}}}); capped.setActive(7,true,physical);
        require(capped.snapshot(7)->ledger.sessions.size()==256,"full key ledger retained");
        require(capped.contact(7,id,1000,0,100,1,physical).admission==ResourceAdmission::Replay,"full cap replay");
        require(capped.contact(7,id,1000,0,100,1.25,physical).admission==ResourceAdmission::Update,"full cap update");
        require(capped.bindContactSession(7,actor(capped,7,1),9000,physical),"fresh session binding at full cap");
        const auto atCap=exact(capped.continuationSnapshot());
        require(capped.contact(7,actor(capped,7,1),9000,0,100,0.1,physical).admission==ResourceAdmission::HistoryFull,"full cap rejects new key");
        require(atCap==exact(capped.continuationSnapshot()),"full cap denial atomic");
        std::cout<<"REGIONAL_LEGACY_IMPORT_OK exact_continuation=true host_integrated=false disk_format_changed=false\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
