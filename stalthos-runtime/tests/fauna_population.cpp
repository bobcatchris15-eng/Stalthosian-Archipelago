#include "../domain/fauna_population.hpp"

#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace stalthos;
using namespace stalthos::fauna;

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static std::vector<HabitatSite> habitat() {
    return {{{0,0,0},true,false,{1}},{{2,0,0},false,true,{0,2}},{{4,0,0},true,false,{1,3}},{{6,0,0},false,true,{2}}};
}
int main() {
    const auto sites=habitat();
    Population a(41,7,sites,6,{77,99});
    Population b(41,7,sites,6,{77,99});
    require(a.actors().size()==6,"actor count");
    std::set<Identity> ids;
    bool foundRest=false, foundTravel=false, foundBrowse=false, foundDrink=false;
    for (std::size_t i=0;i<a.actors().size();++i) {
        const auto& x=a.actors()[i]; const auto& y=b.actors()[i];
        require(x.id==y.id && x.id!=0 && x.id!=1 && x.id!=77 && x.id!=99,"stable safe id");
        require(ids.insert(x.id).second,"unique ids");
        require(x.species==y.species && x.position.x==y.position.x && x.calendarAge==y.calendarAge,"deterministic initial state");
        require(x.position.x==0 || x.position.x==2 || x.position.x==4 || x.position.x==6,"starts on supplied habitat site");
        foundRest|=x.activity==Activity::Rest; foundTravel|=x.activity==Activity::Travel;
        foundBrowse|=x.activity==Activity::Browse; foundDrink|=x.activity==Activity::Drink;
    }
    require(foundRest&&foundTravel&&foundBrowse&&foundDrink,"initial activities mixed");
    const auto before=a.actors(); a.advance({0,0});
    require(a.actors()[0].position.x==before[0].position.x && a.actors()[0].calendarAge==before[0].calendarAge,"zero step unchanged");
    bool rejected=false;
    try { a.advance({-1,1}); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected && a.actors()[0].position.x==before[0].position.x && a.actors()[0].calendarAge==before[0].calendarAge,"negative step atomic");
    rejected=false;
    try { a.advance({1,std::numeric_limits<double>::infinity()}); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected && a.actors()[0].distanceTravelledMeters==before[0].distanceTravelledMeters,"nonfinite step atomic");
    a.advance({5,0}); b.advance({5,0});
    for (std::size_t i=0;i<a.actors().size();++i) {
        const auto& x=a.actors()[i]; const auto& y=b.actors()[i];
        require(x.position.x==y.position.x && x.position.y==y.position.y && x.position.z==y.position.z,"repeat motion deterministic");
        require(x.distanceTravelledMeters==y.distanceTravelledMeters,"repeat distance deterministic");
        require(x.distanceTravelledMeters>=0 && std::isfinite(x.distanceTravelledMeters),"distance finite nonnegative");
        require(std::abs(x.calendarAge-before[i].calendarAge)<1e-10,"physical time does not advance age");
        require(x.distanceTravelledMeters<=2.251,"5s movement is speed bounded");
    }
    require(a.actors()[0].fatigue>before[0].fatigue,"physical time advances fatigue");
    a.advance({10,0});
    bool browsing=false, drinking=false;
    for (const auto& actor:a.actors()) {
        browsing|=actor.activity==Activity::Browse;
        drinking|=actor.activity==Activity::Drink;
    }
    require(browsing&&drinking,"browse and drink remain visible within 15 physical seconds");
    Population smallSteps(41,7,sites,6);
    bool reachedSite=false; double observed=0; auto previous=smallSteps.actors();
    for (int tick=0;tick<80;++tick) {
        smallSteps.advance({1,0});
        for (std::size_t i=0;i<smallSteps.actors().size();++i) {
            const auto& x=smallSteps.actors()[i]; const auto& old=previous[i];
            const double dx=x.position.x-old.position.x, dy=x.position.y-old.position.y, dz=x.position.z-old.position.z;
            const double moved=std::sqrt(dx*dx+dy*dy+dz*dz);
            require(moved<=0.451,"small step movement speed bound");
            require(x.distanceTravelledMeters+1e-9>=old.distanceTravelledMeters+moved,"distance ledger covers motion");
            observed+=moved;
            if (x.position.x==0 || x.position.x==2 || x.position.x==4 || x.position.x==6) reachedSite=true;
        }
        previous=smallSteps.actors();
    }
    require(reachedSite && observed>0,"small physical steps traverse to habitat sites");
    Population calendar(41,7,sites,1);
    const auto oldAge=calendar.actors()[0].calendarAge;
    calendar.advance({0,60});
    require(std::abs(calendar.actors()[0].calendarAge-oldAge-60)<1e-9,"calendar time advances life age");
    require(calendar.actors()[0].hunger>0.22,"calendar time advances hunger");
    require(calendar.actors()[0].distanceTravelledMeters==0,"calendar time does not move body");
    Population zero(41,7,sites,0);
    require(zero.actors().empty(),"zero population valid");
    rejected=false; try { Population invalid(1,1,{},1); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,"positive population requires real sites");
    rejected=false; try { Population invalid(1,1,sites,17); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,"actor cap enforced");
    auto invalidSites=sites; invalidSites[0].neighbors={sites.size()};
    rejected=false; try { Population invalid(1,1,invalidSites,1); } catch(const std::invalid_argument&) { rejected=true; }
    require(rejected,"invalid neighbor rejected");
    for (const auto& actor:a.actors()) {
        const auto& site=sites[static_cast<std::size_t>(std::round(actor.position.x/2.0))];
        if (actor.activity==Activity::Browse) require(site.browse,"browse only at browse site");
        if (actor.activity==Activity::Drink) require(site.freshwater,"drink only at water site");
    }
    std::cout << "fauna population checks passed: 6 actors, deterministic motion, bounded routes, core time separation\n";
}
