#include "../domain/regional_population.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace stalthos;
using namespace stalthos::fauna;
static constexpr auto physical=RegionalAdmissionOrigin::PhysicalArrival;
static constexpr auto camera=RegionalAdmissionOrigin::CameraPrewarm;
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
static RegionalPopulationRegistry registry(std::size_t history=4) {
    RegionalPopulationRegistry r(population(7),ledger(100),{2,history});
    require(r.admit(physical,population(8),ledger(200),{2,4})==RegionalAdmission::Accepted,"admit second region");
    require(r.configureContacts(7,{{0,100,{0,0,0}},{1,101,{4,0,0}}}),"legacy anchors");
    require(r.configureContacts(8,{{0,200,{100,0,0}},{1,201,{104,0,0}}}),"second anchors");
    r.setActive(7,true,physical); r.setActive(8,true,physical); return r;
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
static void contactChecks() {
    auto r=registry(1); auto id=actor(r,7);
    const auto initial=exact(r.continuationSnapshot());
    require(!r.bindContactSession(7,id,88,camera),"camera cannot bind");
    require(!r.bindContactSession(7,999,88,physical),"missing actor cannot bind");
    require(r.bindContactSession(7,id,88,physical),"actor session binds");
    require(r.bindContactSession(7,id,88,physical),"binding idempotent");
    require(!r.bindContactSession(7,id,89,physical),"actor cannot rotate session");
    require(!r.bindContactSession(7,actor(r,7,1),88,physical),"actor cannot steal session");
    require(!r.bindContactSession(8,actor(r,8),88,physical),"cross-region session collision denied");
    const auto before=exact(r.continuationSnapshot());
    for(auto result:{r.contact(7,id,88,0,100,0.5,camera),r.contact(7,id,89,0,100,0.5,physical),
        r.contact(7,999,88,0,100,0.5,physical),r.contact(7,id,88,1,101,0.5,physical),
        r.contact(7,id,88,0,200,0.5,physical)}) require(!result.event,"invalid contact no event");
    require(before==exact(r.continuationSnapshot()),"invalid contacts grant no relief/history");
    const auto hungry=r.snapshot(7)->population.actors[0].hunger;
    auto result=r.contact(7,id,88,0,100,0.5,physical);
    require(result.admission==ResourceAdmission::NewKey && result.event && result.event->amount==0.5,"new actual delta");
    require(r.snapshot(7)->population.actors[0].hunger==hungry-0.05,"accepted food relief");
    const auto accepted=exact(r.continuationSnapshot());
    result=r.contact(7,id,88,0,100,0.5,physical);
    require(result.admission==ResourceAdmission::Replay && !result.event,"replay no fresh event");
    require(accepted==exact(r.continuationSnapshot()),"replay no relief");
    result=r.contact(7,id,88,0,100,0.75,physical);
    require(result.admission==ResourceAdmission::Update && result.event && result.event->amount==0.25,"only update difference grants event");
    require(r.snapshot(7)->population.actors[0].hunger==hungry-0.05-0.025,"update delta relief");
    require(r.bindContactSession(7,actor(r,7,1),89,physical),"second actor session");
    const auto full=exact(r.continuationSnapshot());
    const auto s=r.snapshot(7)->population.actors[1];
    require(s.activity==Activity::Browse && s.site==0,"second actor contact fixture");
    result=r.contact(7,s.id,89,0,100,0.1,physical);
    require(result.admission==ResourceAdmission::HistoryFull && !result.event,"full history no event");
    require(full==exact(r.continuationSnapshot()),"history denial no resource/needs effects");
    const auto invalid=r.contact(7,id,88,0,100,3,physical);
    require(invalid.admission==ResourceAdmission::InsufficientQuantity && !invalid.event,"insufficient resource no relief");
    r.setActive(7,false,physical); const auto dormant=exact(r.continuationSnapshot());
    require(!r.contact(7,id,88,0,100,1,physical).event,"dormant no contacts");
    require(!r.bindContactSession(7,id,88,physical),"dormant no binding");
    require(dormant==exact(r.continuationSnapshot()),"dormant requests unchanged");
    require(initial!=accepted,"fixture exercised state");
    auto water=registry(); const auto drinker=actor(water,7,2);
    water.bindContactSession(7,drinker,90,physical);
    const double thirsty=water.snapshot(7)->population.actors[2].thirst;
    result=water.contact(7,drinker,90,1,101,0.25,physical);
    require(result.event && result.event->kind==ResourceKind::Creek && result.event->amount==0.25,"water contact actual delta");
    require(water.snapshot(7)->population.actors[2].thirst==thirsty-0.05,"water relief from actual delta");
    const auto waterBefore=exact(water.continuationSnapshot());
    require(!water.contact(7,drinker,90,1,101,0.25,physical).event,"water replay no event");
    require(!water.contact(7,drinker,90,999,101,0.5,physical).event,"out of range site rejected");
    require(!water.contact(7,drinker,90,1,101,std::numeric_limits<double>::infinity(),physical).event,"nonfinite cumulative amount rejected");
    require(waterBefore==exact(water.continuationSnapshot()),"water replay/invalid requests unchanged");
}
static void continuationChecks() {
    auto r=registry(); const auto id=actor(r,7), other=actor(r,8);
    r.bindContactSession(7,id,88,physical); r.bindContactSession(8,other,99,physical);
    r.contact(7,id,88,0,100,0.5,physical); r.contact(8,other,99,0,200,0.25,physical);
    r.setActive(7,false,physical); const auto depart=r.continuationSnapshot();
    r.advance(7,{3,30}); r.advance(8,{0.1,0});
    const auto saved=r.continuationSnapshot();
    const auto& a=saved.regions[0].population.actors[0]; const auto& b=depart.regions[0].population.actors[0];
    require(a.position.x==b.position.x && a.position.z==b.position.z && a.site==b.site && a.target==b.target && a.hunger>b.hunger,"elapsed dormant route/needs");
    auto restored=registry(); require(restored.restoreContinuation(saved),"restore into same immutable world");
    require(exact(saved)==exact(restored.continuationSnapshot()),"exact detached continuation");
    r.setActive(7,true,physical); restored.setActive(7,true,physical);
    const auto replayBefore=exact(restored.continuationSnapshot());
    require(!restored.contact(7,id,88,0,100,0.5,physical).event,"restored replay no event");
    require(replayBefore==exact(restored.continuationSnapshot()),"restored replay no relief");
    r.advance(7,{0.2,0}); restored.advance(7,{0.2,0});
    require(exact(r.continuationSnapshot())==exact(restored.continuationSnapshot()),"active continuation after elapsed dormancy exact");
    auto bad=saved; bad.regions.pop_back(); rejected(restored,bad,"missing region");
    bad=saved; bad.regions.push_back(bad.regions.back()); rejected(restored,bad,"extra region");
    bad=saved; bad.regions[1].population.actors[0].id=bad.regions[0].population.actors[0].id; rejected(restored,bad,"cross-region actor identity");
    bad=saved; bad.regions[1].bindings[0].session=88; rejected(restored,bad,"cross-region binding collision");
    bad=saved; bad.regions[0].bindings[0].actor=bad.regions[0].population.actors[1].id; rejected(restored,bad,"session hijacking");
    bad=saved; bad.regions[0].bindings.clear(); rejected(restored,bad,"retained bound history cannot lose owner");
    bad=saved; bad.regions[1].reservation.historyKeys++; rejected(restored,bad,"reservation drift");
    bad=saved; bad.regions[1].anchors[0].position.y=99; rejected(restored,bad,"anchor forgery");
    bad=saved; bad.regions[1].ledger.resources[0].consumedQuantity=-1; rejected(restored,bad,"late malformed ledger");
    bad=saved; bad.regions[1].ledger.sessions[0].amount=99; rejected(restored,bad,"malformed key totals");
    bad=saved; bad.regions[0].population.actors[0].stage=LifeStage::Dead; bad.regions[0].population.actors[0].activity=Activity::Rest;
    // Lifecycle validates dead state against age; take a real dormant death instead.
    auto dead=registry(); dead.bindContactSession(7,actor(dead,7),88,physical);
    dead.setActive(7,false,physical); dead.advance(7,{0,1e10}); dead.setActive(7,true,physical);
    const auto deadBefore=exact(dead.continuationSnapshot());
    require(!dead.contact(7,actor(dead,7),88,0,100,0.1,physical).event,"dead actor no relief");
    require(deadBefore==exact(dead.continuationSnapshot()),"dead contact unchanged");
}
static void importedAndAvailabilityChecks() {
    auto history=ledger(100); history.resources[0].consumedQuantity=0.5; history.sessions={{77,100,0.5}};
    RegionalPopulationRegistry r(population(7),history,{2,2});
    r.configureContacts(7,{{0,100,{0,0,0}},{1,101,{4,0,0}}}); r.setActive(7,true,physical);
    require(!r.bindContactSession(7,actor(r,7),77,physical),"unbound imported history cannot be stolen");
    auto bad=r.continuationSnapshot(); bad.regions[0].bindings.push_back({actor(r,7),77});
    rejected(r,bad,"restore cannot claim imported legacy session");
    require(r.snapshot(7)->ledger.sessions[0].sessionId==77,"complete imported ledger kept");
    require(r.bindContactSession(7,actor(r,7),88,physical),"fresh actor key accepted");
    require(r.contact(7,actor(r,7),88,0,100,1.5,physical).event.has_value(),"deplete actual food");
    const auto saved=r.continuationSnapshot();
    RegionalPopulationRegistry copy(population(7),history,{2,2});
    copy.configureContacts(7,{{0,100,{0,0,0}},{1,101,{4,0,0}}});
    require(copy.restoreContinuation(saved),"restore depleted availability derived from ledger");
    r.advance(7,{0.1,0}); copy.advance(7,{0.1,0});
    require(exact(r.continuationSnapshot())==exact(copy.continuationSnapshot()),"derived depleted routing exact after restore");
    const auto depleted=r.snapshot(7);
    for(const auto& a:depleted->population.actors) require(a.activity!=Activity::Browse,"depleted browse not selected");
    auto fresh=registry();
    require(!fresh.configureContacts(7,{}),"configuration immutable");
    RegionalPopulationRegistry malformed(population(7),ledger(100),{2,4});
    require(!malformed.configureContacts(7,{{0,100,{99,0,0}}}),"anchor outside physical reach rejected");
    require(!malformed.configureContacts(7,{{0,101,{0,0,0}}}),"wrong resource kind/site rejected");
    require(!malformed.configureContacts(7,{{0,999,{0,0,0}}}),"missing resource identity rejected");
    require(!malformed.configureContacts(7,{{0,100,{0,0,0}},{0,100,{0,0,0}}}),"duplicate anchor rejected");
}
int main() {
    try { contactChecks(); continuationChecks(); importedAndAvailabilityChecks();
        std::cout<<"REGIONAL_CONTACTS_OK actor_bound=true exact_continuation=true host_integrated=false\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
