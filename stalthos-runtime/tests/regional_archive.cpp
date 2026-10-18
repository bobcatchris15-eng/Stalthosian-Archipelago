#include "../domain/regional_archive.hpp"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
using namespace stalthos;
using namespace stalthos::fauna;
namespace archive=stalthos::fauna::regional_archive;
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

static std::string bytes(const RegionalPopulationRegistry& r) { std::string out; require(archive::encode(r.continuationSnapshot(),out),"encode fixture"); return out; }
static void reject(RegionalPopulationRegistry& r,const std::string& bad) {
    auto before=exact(r.continuationSnapshot());
    require(!archive::restore(r,bad),"bad archive rejected");
    require(before==exact(r.continuationSnapshot()),"bad archive leaves live state unchanged");
    auto dto=r.continuationSnapshot(); const auto original=exact(dto);
    if(!archive::decode(bad,dto)) require(original==exact(dto),"decode output atomic");
}
int main() { try {
    auto original=registry(); const auto id=actor(original,7),other=actor(original,8);
    require(original.bindContactSession(7,id,88,physical),"bind first");
    require(original.bindContactSession(8,other,99,physical),"bind second");
    require(original.contact(7,id,88,0,100,0.5,physical).event.has_value(),"first depletion");
    require(original.contact(8,other,99,0,200,0.25,physical).event.has_value(),"second depletion");
    original.setActive(7,false,physical); const auto departure=original.continuationSnapshot();
    original.advance(7,{3,30}); original.advance(8,{0.1,0});
    require(original.snapshot(7)->population.actors[0].hunger>departure.regions[0].population.actors[0].hunger,"dormant needs advance");
    const auto saved=bytes(original); require(saved==bytes(original),"deterministic bytes");
    auto loaded=registry(); require(archive::restore(loaded,saved),"multi-region archive restore");
    require(exact(original.continuationSnapshot())==exact(loaded.continuationSnapshot()),"all fields exact roundtrip");
    require(bytes(loaded)==saved,"byte exact re-encode");
    original.setActive(7,true,physical); loaded.setActive(7,true,physical);
    require(!loaded.contact(7,id,88,0,100,0.5,physical).event,"history replay after return");
    for(auto* r:{&original,&loaded}) { r->advance(7,{0.1,0}); r->advance(8,{0.1,0}); }
    require(exact(original.continuationSnapshot())==exact(loaded.continuationSnapshot()),"return continuation equal");
    for(std::size_t n=0;n<saved.size();++n) reject(loaded,saved.substr(0,n));
    reject(loaded,saved+"x"); reject(loaded,std::string(archive::maxBytes+1,'x'));
    auto changed=saved; changed[4]=2; reject(loaded,changed);
    changed=saved; changed.replace(0,4,"STL8"); reject(loaded,changed);
    for(auto offset:{28u,60u}) { changed=saved; for(unsigned i=0;i<8;++i) changed[offset+i]=char(-1); reject(loaded,changed); }
    changed=saved; changed[76]=9; reject(loaded,changed); // species enum
    changed=saved; changed[188]=2; reject(loaded,changed); // actor resting bool
    changed=saved; for(unsigned i=0;i<8;++i) changed[84+i]=char(-1); reject(loaded,changed); // NaN position
    auto bad=original.continuationSnapshot(); std::string untouched="sentinel";
    bad.regions[1].population.actors[0].id=bad.regions[0].population.actors[0].id;
    require(!archive::encode(bad,untouched) && untouched=="sentinel","duplicate global actor encode rejected atomically");
    bad=original.continuationSnapshot(); bad.regions[1].ledger.resources[0].id=bad.regions[0].ledger.resources[0].id;
    require(!archive::encode(bad,untouched),"duplicate global resource");
    bad=original.continuationSnapshot(); bad.regions[1].population.region=7;
    require(!archive::encode(bad,untouched),"duplicate region");
    bad=original.continuationSnapshot(); bad.regions[1].bindings[0].session=88;
    require(!archive::encode(bad,untouched),"duplicate binding session");
    bad=original.continuationSnapshot(); bad.regions[0].reservation.historyKeys=0;
    require(!archive::encode(bad,untouched),"insufficient reservation");
    bad=original.continuationSnapshot(); bad.regions[0].population.habitatSignature^=1;
    std::string semantic; require(archive::encode(bad,semantic),"structural encode permits external habitat verification"); reject(loaded,semantic);
    bad=original.continuationSnapshot(); bad.regions[0].anchors[0].position.x+=0.01;
    require(archive::encode(bad,semantic),"anchor structural encode"); reject(loaded,semantic);
    std::stringstream stream; require(archive::write(stream,original.continuationSnapshot()),"stream write");
    RegionalContinuationDto dto; require(archive::read(stream,dto),"stream read");
    require(exact(dto)==exact(original.continuationSnapshot()),"stream all fields equal");
    std::istringstream tooLarge(std::string(archive::maxBytes+1,'x')); require(!archive::read(tooLarge,dto),"bounded stream read");
    // Real disk transport; replacement policy belongs to the host save adapter.
    const auto disk=std::filesystem::temp_directory_path()/
        ("stalthos-regional-archive-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bin");
    { std::ofstream output(disk,std::ios::binary); require(archive::write(output,original.continuationSnapshot()),"disk write"); }
    { std::ifstream input(disk,std::ios::binary); RegionalContinuationDto fromDisk;
      require(archive::read(input,fromDisk),"disk read");
      require(exact(fromDisk)==exact(original.continuationSnapshot()),"disk exact roundtrip"); }
    require(std::filesystem::remove(disk),"remove owned disk fixture");
    auto aliases=registry(); auto unbound=aliases.continuationSnapshot();
    unbound.regions[0].ledger.sessions.push_back({777,100,0.1});
    unbound.regions[1].ledger.sessions.push_back({777,200,0.1});
    unbound.regions[0].ledger.resources[0].consumedQuantity=0.1;
    unbound.regions[1].ledger.resources[0].consumedQuantity=0.1;
    std::string aliasBytes; require(archive::encode(unbound,aliasBytes),"unbound cross-region history alias permitted");
    RegionalContinuationDto aliasDto; require(archive::decode(aliasBytes,aliasDto),"unbound alias decode");
    require(exact(unbound)==exact(aliasDto),"unbound alias fields preserved");
    std::cout<<"REGIONAL_ARCHIVE_OK: exact multi-region continuation, deterministic bytes, bounded malformed inputs, atomic restore\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; } }
