#include "../engine/fauna_resources.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace stalthos;
using namespace stalthos::fauna;
using namespace stalthos::engine;

static void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }

int main() try {
    const std::vector<HabitatSite> sites{
        {{0,0,0},false,true,{1}},
        {{1,0,0},true,false,{0}}
    };
    const std::vector<Vec3> shrubs{{1,0,0}};
    Population population(55,4,sites,4);
    ResourceLedger ledger(16,16);
    require(ledger.load({{100,ResourceKind::Creek,.015,0},{200,ResourceKind::Shrub,.02,0}}),"load resources");
    std::uint64_t nextSession=700;
    FaunaResources contacts(sites,shrubs,nextSession);
    contacts.refreshAvailability(population,ledger);
    require(nextSession==704,"one unique session allocated per actor");

    auto actors=population.actors();
    ActorView browser=actors[1], drinker=actors[2];
    browser.activity=Activity::Browse; browser.position={1,0,0};
    drinker.activity=Activity::Drink; drinker.position={0,0,0};
    contacts.applyContacts({browser,drinker},1,ledger);
    require(contacts.events().size()==2,"both contact events emitted");
    require(contacts.events()[0].resourceId==200 || contacts.events()[1].resourceId==200,"food maps to shrub 200");
    require(contacts.events()[0].resourceId==100 || contacts.events()[1].resourceId==100,"water maps to creek 100");
    require(std::abs(contacts.consumedFood()-.015)<1e-12,"food rate applied");
    require(std::abs(contacts.consumedWater()-.01)<1e-12,"water rate applied");

    contacts.applyContacts({browser,drinker},1,ledger);
    require(std::abs(contacts.consumedFood()-.02)<1e-12,"food clamped to remaining quantity");
    require(std::abs(contacts.consumedWater()-.015)<1e-12,"water clamped to remaining quantity");
    contacts.refreshAvailability(population,ledger);
    contacts.applyContacts({browser,drinker},1,ledger);
    require(contacts.events().empty(),"depleted resources cannot be consumed again");
    require(std::abs(contacts.consumedFood()-.02)<1e-12 && std::abs(contacts.consumedWater()-.015)<1e-12,"totals remain bounded");

    auto away=browser; away.position={10,0,0};
    contacts.applyContacts({away},1,ledger);
    require(contacts.events().empty(),"no consumption outside contact radius");
    bool rejected=false;
    try { contacts.applyContacts({browser},-1,ledger); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,"invalid elapsed time rejected");

    const auto saved=ledger.continuationSnapshot();
    require(saved.sessions.size()==2,"one cumulative ledger key per actor-resource contact");
    require(saved.sessions[0].sessionId!=saved.sessions[1].sessionId,"actor sessions remain distinct");
    ResourceLedger longLived(16,2);
    require(longLived.load({{100,ResourceKind::Creek,100,0},{200,ResourceKind::Shrub,100,0}}),"load long-lived resources");
    std::vector<std::pair<Identity,std::uint64_t>> mappings;
    for(int reload=0;reload<100;++reload) {
        FaunaResources resumed(sites,shrubs,nextSession,mappings);
        resumed.refreshAvailability(population,longLived);
        resumed.applyContacts({browser,drinker},1,longLived);
        mappings=resumed.sessions();
        require(longLived.continuationSnapshot().sessions.size()==2,"reload must reuse bounded consumption history");
    }
    const auto final=longLived.snapshot();
    require(std::abs(final[0].consumedQuantity-1)<1e-10 && std::abs(final[1].consumedQuantity-1.5)<1e-10,"cumulative consumption resumes for 100 reloads");
    // Ledger-driven relief: a full history must neither throw nor feed denied actors.
    Population boundedPopulation(55,4,sites,2);
    auto boundedState=boundedPopulation.snapshot();
    for (auto& actor:boundedState.actors) {
        actor.site=actor.target=1; actor.position=sites[1].position;
        actor.activity=Activity::Browse; actor.hunger=.7; actor.thirst=.2;
    }
    boundedPopulation.restore(boundedState);
    ResourceLedger boundedLedger(16,1);
    require(boundedLedger.load({{100,ResourceKind::Creek,100,0},{200,ResourceKind::Shrub,100,0}}),"bounded ledger loaded");
    std::uint64_t boundedNext=900;
    FaunaResources boundedContacts(sites,shrubs,boundedNext);
    boundedContacts.refreshAvailability(boundedPopulation,boundedLedger);
    boundedPopulation.advance({1,1},false);
    const auto beforeContact=boundedPopulation.actors();
    boundedContacts.applyContacts(boundedPopulation,1,boundedLedger);
    require(boundedContacts.events().size()==1 && boundedContacts.historyRejections()==1,"last slot admits one actor and rejects second");
    require(std::abs(boundedPopulation.actors()[0].hunger-(beforeContact[0].hunger-.0015))<1e-12,"accepted food relieves hunger");
    require(boundedPopulation.actors()[1].hunger==beforeContact[1].hunger,"denied food grants no hunger relief");
    const auto mappingsBefore=boundedContacts.sessions();
    const auto ledgerBefore=boundedLedger.continuationSnapshot();
    ResourceLedger resumedLedger(16,1); require(resumedLedger.restore(ledgerBefore),"full ledger restores");
    FaunaResources resumedContacts(sites,shrubs,boundedNext,mappingsBefore);
    resumedContacts.refreshAvailability(boundedPopulation,resumedLedger);
    // A replay before adapter hydration must not produce fresh events or relief.
    FaunaResources unhydrated(sites,shrubs,boundedNext,mappingsBefore);
    const auto beforeReplay=boundedPopulation.actors();
    unhydrated.applyContacts(boundedPopulation,1,resumedLedger);
    require(unhydrated.events().empty(),"ledger replay emits no fresh contact event");
    require(boundedPopulation.actors()[0].hunger==beforeReplay[0].hunger,"ledger replay grants no hunger relief");
    const auto beforeResume=boundedPopulation.actors();
    resumedContacts.applyContacts(boundedPopulation,1,resumedLedger);
    require(resumedContacts.events().size()==1 && resumedContacts.historyRejections()==1,"restored full ledger continues existing actor only");
    require(resumedLedger.continuationSnapshot().sessions.size()==1,"full history retains exact key count");
    require(resumedLedger.continuationSnapshot().sessions[0].sessionId==ledgerBefore.sessions[0].sessionId,"original history key retained");
    require(std::abs(resumedLedger.snapshot()[1].consumedQuantity-.03)<1e-12,"only accepted actor consumes food across restore");
    require(boundedPopulation.actors()[1].hunger==beforeResume[1].hunger,"restored denial grants no relief");
    std::cout << "fauna resource checks passed: shrub and creek mapping, contact range, cumulative clamping, unique sessions\n";
} catch (const std::exception& error) { std::cerr << error.what() << "\n"; return 1; }
