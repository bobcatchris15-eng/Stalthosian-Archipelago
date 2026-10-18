#include "../domain/regional_population.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace stalthos;
using namespace stalthos::fauna;
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static std::vector<HabitatSite> habitat() {
    return {{{0,0,0},true,false,{1}},{{2,0,0},false,true,{0,2}},{{4,0,0},true,false,{1}}};
}
static Population population(RegionId region, std::size_t count = 8, std::uint64_t seed = 41) {
    Population value(seed,region,habitat(),count);
    value.advance({0.1,0},false);
    return value;
}
static ResourceLedgerDto ledger(std::uint64_t id) { return {{{id,ResourceKind::Shrub,1000,0}}, {}}; }
// Include every retained actor field and every ledger key/quantity: rejection
// checks compare exact state, rather than merely checking counts.
static std::string state(const RetainedRegionSnapshot& s) {
    std::ostringstream out; out.precision(17);
    out << s.population.seed << '/' << s.population.region << '/' << s.population.habitatSignature
        << '/' << s.reservation.resources << '/' << s.reservation.historyKeys << '/' << s.active;
    for (const auto& a : s.population.actors)
        out << '|' << a.id << ',' << static_cast<int>(a.species) << ',' << a.position.x << ',' << a.position.y << ','
            << a.position.z << ',' << a.headingRadians << ',' << a.distanceTravelledMeters << ',' << a.site << ','
            << a.target << ',' << a.calendarAge << ',' << static_cast<int>(a.stage) << ',' << a.hunger << ','
            << a.thirst << ',' << a.fatigue << ',' << a.injury << ',' << a.isResting << ',' << static_cast<int>(a.activity);
    for (const auto& r : s.ledger.resources)
        out << "r" << r.id << ',' << static_cast<int>(r.kind) << ',' << r.initialQuantity << ',' << r.consumedQuantity;
    for (const auto& h : s.ledger.sessions) out << "h" << h.sessionId << ',' << h.resourceId << ',' << h.amount;
    return out.str();
}
static void retainedChecks() {
    auto legacy = population(7);
    auto original = legacy.snapshot();
    RegionalPopulationRegistry registry(legacy,ledger(101),{1,240});
    require(registry.legacyRegion()==7 && registry.capacity().individuals==8,"legacy eight retained");
    require(registry.snapshot(7)->population.habitatSignature==original.habitatSignature,"legacy signature retained");
    const auto before = state(*registry.snapshot(7));
    const auto second = population(8);
    require(registry.admit(RegionalAdmissionOrigin::CameraPrewarm,second,ledger(102),{1,16})==RegionalAdmission::CameraOnly,"camera cannot admit");
    require(!registry.setActive(7,true,RegionalAdmissionOrigin::CameraPrewarm),"camera cannot activate");
    require(registry.capacity().regions==1 && state(*registry.snapshot(7))==before,"camera no side effects");
    require(registry.admit(static_cast<RegionalAdmissionOrigin>(99),second,ledger(102),{1,16})==RegionalAdmission::InvalidOrigin,"unknown origin rejected");
    require(registry.admit(RegionalAdmissionOrigin::PhysicalArrival,second,ledger(102),{1,17})==RegionalAdmission::ReservationLimit,"global reservation ceiling");
    auto collision=second; auto conflicting=collision.snapshot(); conflicting.actors[0].id=original.actors[0].id; collision.restore(conflicting);
    require(registry.admit(RegionalAdmissionOrigin::PhysicalArrival,collision,ledger(102),{1,16})==RegionalAdmission::IdentityConflict,"identity reserved across regions");
    require(registry.admit(RegionalAdmissionOrigin::PhysicalArrival,second,ledger(101),{1,16})==RegionalAdmission::ResourceConflict,"resource ID globally unique");
    require(registry.admit(RegionalAdmissionOrigin::PhysicalArrival,second,ledger(102),{0,16})==RegionalAdmission::InvalidReservation,"resource count fits reservation");
    auto malformed=ledger(102); malformed.resources[0].consumedQuantity=-1;
    require(registry.admit(RegionalAdmissionOrigin::PhysicalArrival,second,malformed,{1,16})==RegionalAdmission::InvalidReservation,"malformed ledger rejected");
    require(registry.capacity().regions==1 && state(*registry.snapshot(7))==before,"all rejected admissions atomic");
    require(registry.admit(RegionalAdmissionOrigin::PhysicalArrival,second,ledger(102),{1,16})==RegionalAdmission::Accepted,"physical admission succeeds");
    require(registry.capacity().regions==2 && registry.capacity().historyKeys==256 && registry.capacity().resources==2,"reservations sum once");
    require(registry.admit(RegionalAdmissionOrigin::PhysicalArrival,legacy,ledger(103),{1,0})==RegionalAdmission::DuplicateRegion,"no replacement of legacy");
    require(registry.consumeCumulative(8,1,102,0.1)==ResourceAdmission::InvalidRequest,"dormant cannot consume");
    require(registry.setActive(8,true,RegionalAdmissionOrigin::PhysicalArrival),"arrival activates");
    require(registry.consumeCumulative(8,1,102,0.1)==ResourceAdmission::NewKey,"owned ledger consumes");
    for (std::uint64_t key=2;key<=16;++key)
        require(registry.consumeCumulative(8,key,102,0.1)==ResourceAdmission::NewKey,"per-region reserved keys available");
    const auto atRegionalLimit=state(*registry.snapshot(8));
    require(registry.consumeCumulative(8,17,102,0.1)==ResourceAdmission::HistoryFull,"per-region history reservation enforced");
    require(state(*registry.snapshot(8))==atRegionalLimit,"per-region rejection atomic");
    auto departure=registry.snapshot(8);
    require(registry.setActive(8,false,RegionalAdmissionOrigin::PhysicalArrival),"departure retains region");
    require(registry.advance(8,{0,0}),"zero elapsed dormancy");
    require(registry.setActive(8,true,RegionalAdmissionOrigin::PhysicalArrival),"return activates");
    require(state(*registry.snapshot(8))==state(*departure),"departure return exact routes needs history");
    require(registry.advance(8,{0.1,0}),"active elapsed advances");
    auto expected=second; expected.advance({0.1,0},false);
    require(registry.snapshot(8)->population.actors[0].hunger==expected.snapshot().actors[0].hunger,"active simulation grants no unledgered relief");
    auto dormantBefore=registry.snapshot(8);
    registry.setActive(8,false,RegionalAdmissionOrigin::PhysicalArrival);
    registry.advance(8,{3,30});
    const auto dormant=registry.snapshot(8);
    for (std::size_t i=0;i<dormant->population.actors.size();++i) {
        const auto& a=dormant->population.actors[i]; const auto& b=dormantBefore->population.actors[i];
        require(a.id==b.id && a.site==b.site && a.target==b.target && a.position.x==b.position.x && a.position.z==b.position.z,"dormant route and identity retained");
        require(a.hunger>b.hunger && a.thirst>b.thirst,"dormant needs advance without resources");
    }
    Population expectedDormant(41,8,habitat(),0);
    expectedDormant.restore(dormantBefore->population); expectedDormant.advanceDormant({3,30});
    auto expectedReturn=*dormantBefore; expectedReturn.population=expectedDormant.snapshot(); expectedReturn.active=false;
    require(state(*dormant)==state(expectedReturn),"elapsed dormancy exact complete snapshot and ledger");
    registry.setActive(8,true,RegionalAdmissionOrigin::PhysicalArrival); expectedReturn.active=true;
    require(state(*registry.snapshot(8))==state(expectedReturn),"elapsed dormant return retains complete snapshot and ledger");
    require(state(*registry.snapshot(7))==before,"regional updates preserve legacy exactly");
    auto copy=*registry.snapshot(7); copy.population.actors[0].id=0;
    require(state(*registry.snapshot(7))==before,"snapshot mutation cannot bypass identity ownership");
    require(!registry.snapshot(999) && !registry.advance(999,{0,0}),"missing region explicit");
}
static void limitChecks() {
    RegionalPopulationRegistry regions(population(0,1),{},{});
    for (RegionId r=1;r<8;++r) require(regions.admit(RegionalAdmissionOrigin::PhysicalArrival,population(r,1),{}, {})==RegionalAdmission::Accepted,"bounded region admission");
    require(regions.admit(RegionalAdmissionOrigin::PhysicalArrival,population(8,1),{}, {})==RegionalAdmission::RegionLimit,"region cap no eviction");
    require(regions.capacity().regions==8 && regions.snapshot(0),"region cap preserves oldest");
    RegionalPopulationRegistry actors(population(0,16),{},{});
    for (RegionId r=1;r<4;++r) require(actors.admit(RegionalAdmissionOrigin::PhysicalArrival,population(r,16),{}, {})==RegionalAdmission::Accepted,"individual budget admission");
    require(actors.admit(RegionalAdmissionOrigin::PhysicalArrival,population(4,1),{}, {})==RegionalAdmission::IndividualLimit,"global individual cap");
    require(actors.capacity().individuals==64,"individual count retained");
    RegionalPopulationRegistry resources(population(0,1),{}, {4096,0});
    require(resources.admit(RegionalAdmissionOrigin::PhysicalArrival,population(1,1),ledger(500),{1,0})==RegionalAdmission::ReservationLimit,"global resource reservation cap");
    RegionalPopulationRegistry seed(population(0,1),{}, {});
    require(seed.admit(RegionalAdmissionOrigin::PhysicalArrival,population(1,1,42),{}, {})==RegionalAdmission::SeedConflict,"world seed consistency");
    require(seed.admit(RegionalAdmissionOrigin::PhysicalArrival,population(1,0),{}, {})==RegionalAdmission::EmptyPopulation,"estimates not empty actual populations");
}
static void fullHistoryChecks() {
    auto history=ledger(101); history.resources[0].consumedQuantity=256;
    for (std::uint64_t i=1;i<=256;++i) history.sessions.push_back({i,101,1});
    RegionalPopulationRegistry full(population(7),history,{1,256});
    require(full.capacity().historyKeys==256,"full legacy history counted once");
    require(full.admit(RegionalAdmissionOrigin::PhysicalArrival,population(8),ledger(102),{1,1})==RegionalAdmission::ReservationLimit,"full legacy cannot over-admit");
    full.setActive(7,true,RegionalAdmissionOrigin::PhysicalArrival);
    require(full.consumeCumulative(7,1,101,1)==ResourceAdmission::Replay,"legacy replay retained");
    require(full.consumeCumulative(7,1,101,2)==ResourceAdmission::Update,"existing history updates at capacity");
    const auto before=state(*full.snapshot(7));
    require(full.consumeCumulative(7,257,101,1)==ResourceAdmission::HistoryFull,"new key denied at capacity");
    require(state(*full.snapshot(7))==before,"history denial atomic");
    bool invalid=false; try { RegionalPopulationRegistry bad(population(7),history,{1,255}); } catch(const std::invalid_argument&) { invalid=true; }
    require(invalid,"legacy bootstrap cannot omit reserved keys");
}
int main() {
    retainedChecks(); limitChecks(); fullHistoryChecks();
    std::cout << "REGIONAL_POPULATION_FOUNDATION_OK regions=8 individuals=64 history=256 camera_denied=true retained_departure_return=true host_integrated=false\n";
}
