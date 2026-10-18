#include "../engine/fauna_resources.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace stalthos;
using namespace stalthos::fauna;
using namespace stalthos::engine;

static void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }

int main() {
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
    std::cout << "fauna resource checks passed: shrub and creek mapping, contact range, cumulative clamping, unique sessions\n";
}
