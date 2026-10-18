#include "../domain/fauna_population.hpp"
#include "../domain/persistence.hpp"

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
static void sameActor(const ActorSnapshot& x, const ActorSnapshot& y) {
    require(x.id==y.id && x.species==y.species && x.site==y.site && x.target==y.target && x.activity==y.activity && x.stage==y.stage,"dormant identity/route/stage retained");
    require(x.position.x==y.position.x && x.position.y==y.position.y && x.position.z==y.position.z && x.headingRadians==y.headingRadians && x.distanceTravelledMeters==y.distanceTravelledMeters,"dormant pose/distance retained");
    require(x.calendarAge==y.calendarAge && x.hunger==y.hunger && x.thirst==y.thirst && x.fatigue==y.fatigue && x.injury==y.injury && x.isResting==y.isResting,"dormant needs retained");
}
static void dormantChecks() {
    const auto sites=habitat();
    Population dormant(41,7,sites,4);
    dormant.advance({0.1,0}); // Include an actual mid-route actor.
    auto before=dormant.snapshot();
    before.actors[0].injury=0.37;
    dormant.restore(before);
    require(before.actors[0].site!=before.actors[0].target && before.actors[0].position.x!=sites[before.actors[0].site].position.x,"dormancy fixture is mid-route");
    dormant.advanceDormant({2,60});
    auto after=dormant.snapshot();
    for(std::size_t i=0;i<before.actors.size();++i) {
        auto expected=before.actors[i];
        Lifecycle life(expected.species); life.restoreState(expected.species,expected.calendarAge,expected.stage); life.advance({2,60});
        needs::NeedsCore needs(expected.species); needs.restoreState(expected.hunger,expected.thirst,expected.fatigue,expected.injury,expected.isResting); needs.advance({2,60});
        expected.calendarAge=life.calendarAge(); expected.stage=life.stage();
        expected.hunger=needs.hunger(); expected.thirst=needs.thirst(); expected.fatigue=needs.fatigue();
        sameActor(expected,after.actors[i]);
        require(after.actors[i].hunger>before.actors[i].hunger && after.actors[i].thirst>before.actors[i].thirst,"dormant browse/drink grant no food or water");
    }
    Population restored(41,7,sites,0); restored.restore(after);
    for(std::size_t i=0;i<after.actors.size();++i) sameActor(after.actors[i],restored.snapshot().actors[i]);
    // Split elapsed updates agree within rounding, away from death transitions.
    Population split(41,7,sites,0); split.restore(before);
    split.advanceDormant({1,30}); split.advanceDormant({1,30});
    for(std::size_t i=0;i<after.actors.size();++i) {
        auto actual=split.snapshot().actors[i]; const auto& expected=after.actors[i];
        require(std::abs(actual.hunger-expected.hunger)<1e-14 && std::abs(actual.thirst-expected.thirst)<1e-14 && std::abs(actual.fatigue-expected.fatigue)<1e-14,"dormant split elapsed consistency");
        actual.hunger=expected.hunger; actual.thirst=expected.thirst; actual.fatigue=expected.fatigue; sameActor(expected,actual);
    }
    const TimeStep invalid[]={{-1,0},{0,-1},{std::numeric_limits<double>::quiet_NaN(),0},{0,std::numeric_limits<double>::infinity()}};
    for(const auto& dt:invalid) {
        bool rejected=false; try { dormant.advanceDormant(dt); } catch(const std::invalid_argument&) { rejected=true; }
        require(rejected,"dormant invalid step rejected");
        for(std::size_t i=0;i<after.actors.size();++i) sameActor(after.actors[i],dormant.snapshot().actors[i]);
    }
    dormant.advanceDormant({0,0});
    for(std::size_t i=0;i<after.actors.size();++i) sameActor(after.actors[i],dormant.snapshot().actors[i]);
    // A later actor overflow must not commit earlier actors' needs or ages.
    auto huge=after; huge.actors.back().calendarAge=std::numeric_limits<double>::max(); huge.actors.back().stage=LifeStage::Dead; huge.actors.back().activity=Activity::Rest;
    dormant.restore(huge);
    bool overflow=false; try { dormant.advanceDormant({1,std::numeric_limits<double>::max()}); } catch(const std::overflow_error&) { overflow=true; }
    require(overflow,"dormant calendar overflow rejected");
    for(std::size_t i=0;i<huge.actors.size();++i) sameActor(huge.actors[i],dormant.snapshot().actors[i]);
    // Dormant death on a route remains restorable and retains exact corpse pose.
    dormant.restore(before); dormant.advanceDormant({0,40000});
    const auto dead=dormant.snapshot(); restored.restore(dead);
    for(std::size_t i=0;i<dead.actors.size();++i) {
        require(dead.actors[i].stage==LifeStage::Dead && dead.actors[i].activity==Activity::Rest,"dormant calendar death");
        sameActor(dead.actors[i],restored.snapshot().actors[i]);
    }
    persistence::SaveData data; data.seed=41; data.faunaPopulation=after;
    data.faunaResourceSessions={{after.actors[0].id,700}};
    const auto path=std::filesystem::path("test_dormant_population.sav");
    persistence::writeSave(path,data); const auto loaded=persistence::readSave(path); std::filesystem::remove(path);
    require(loaded.faunaPopulation.has_value() && loaded.faunaResourceSessions==data.faunaResourceSessions,"dormant save and resource session history preserved");
    restored.restore(*loaded.faunaPopulation);
    for(std::size_t i=0;i<after.actors.size();++i) sameActor(after.actors[i],restored.snapshot().actors[i]);
    restored.advance({0.1,0}); // Resumption must accept the retained route intent.
}
int main() {
    dormantChecks();
    const auto sites=habitat();
    Population distributed(41,7,sites,8);
    require(distributed.actors()[1].position.x!=distributed.actors()[5].position.x,"initial browsers use distinct available sites");
    require(distributed.actors()[2].position.x!=distributed.actors()[6].position.x,"initial drinkers use distinct available sites");
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
    Population uninterrupted(41,7,sites,6), resumed(41,7,sites,6);
    uninterrupted.advance({7.25,11.5}); resumed.advance({7.25,11.5});
    const auto saved=resumed.snapshot();
    Population restored(41,7,sites,0);
    restored.restore(saved);
    for (int i=0;i<40;++i) { uninterrupted.advance({0.75,1.25}); restored.advance({0.75,1.25}); }
    const auto continued=uninterrupted.snapshot(), roundtripped=restored.snapshot();
    require(continued.actors.size()==roundtripped.actors.size(),"restore actor count");
    for(std::size_t i=0;i<continued.actors.size();++i) {
        const auto& x=continued.actors[i]; const auto& y=roundtripped.actors[i];
        require(x.id==y.id && x.species==y.species && x.site==y.site && x.target==y.target && x.activity==y.activity && x.stage==y.stage,"restore identity and route state");
        require(x.position.x==y.position.x && x.position.y==y.position.y && x.position.z==y.position.z && x.headingRadians==y.headingRadians && x.distanceTravelledMeters==y.distanceTravelledMeters,"continued motion matches exactly after restore");
        require(x.calendarAge==y.calendarAge && x.hunger==y.hunger && x.thirst==y.thirst && x.fatigue==y.fatigue && x.injury==y.injury && x.isResting==y.isResting,"continued needs match exactly after restore");
    }
    auto badSnapshot=saved; badSnapshot.region++;
    bool rejectedRestore=false; try { restored.restore(badSnapshot); } catch(const std::invalid_argument&) { rejectedRestore=true; }
    require(rejectedRestore,"habitat identity mismatch rejected");
    const auto unchanged=restored.snapshot();
    badSnapshot=saved; badSnapshot.actors[0].position.x+=1000;
    rejectedRestore=false; try { restored.restore(badSnapshot); } catch(const std::invalid_argument&) { rejectedRestore=true; }
    require(rejectedRestore && restored.snapshot().actors.size()==unchanged.actors.size(),"off-route position rejected atomically");
    auto oldSnapshot=saved; auto& oldActor=oldSnapshot.actors[0];
    const auto& profile=getHeroProfile(oldActor.species); oldActor.calendarAge=profile.hatchlingDuration+profile.juvenileDuration+profile.adultDuration+1; oldActor.stage=LifeStage::Old;
    restored.restore(oldSnapshot); require(restored.snapshot().actors[0].stage==LifeStage::Old,"old stage restore");
    oldActor.calendarAge+=profile.oldDuration; oldActor.stage=LifeStage::Dead; oldActor.activity=Activity::Rest; oldActor.isResting=true;
    restored.restore(oldSnapshot); require(restored.snapshot().actors[0].stage==LifeStage::Dead,"dead stage restore");
    // A generated death during travel must preserve its exact corpse position.
    Population dying(1,7,sites,4);
    dying.advance({0.01,0});
    dying.advance({0,40000});
    const auto deathSnapshot=dying.snapshot();
    Population corpses(1,7,sites,0);
    corpses.restore(deathSnapshot);
    bool midrouteDeath=false;
    for(std::size_t i=0;i<deathSnapshot.actors.size();++i) {
        const auto& dead=deathSnapshot.actors[i];
        require(dead.stage==LifeStage::Dead,"large calendar step reaches death");
        require(corpses.snapshot().actors[i].position.x==dead.position.x,"dead position survives restore exactly");
        midrouteDeath|=dead.position.x!=sites[dead.site].position.x;
    }
    require(midrouteDeath,"regression includes a generated midroute death");
    auto invalidCorpse=deathSnapshot; invalidCorpse.actors[0].position.z+=1;
    rejectedRestore=false; try { corpses.restore(invalidCorpse); } catch(const std::invalid_argument&) { rejectedRestore=true; }
    require(rejectedRestore,"off-route corpse still rejected");
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
    Population depleted(41,7,sites,6);
    for(std::size_t i=0;i<sites.size();++i) depleted.setSiteResources(i,false,false);
    const auto hungry=depleted.actors(); depleted.advance({.1,.1});
    for(std::size_t i=0;i<hungry.size();++i) {
        require(depleted.actors()[i].hunger>=hungry[i].hunger,"depleted browse cannot feed");
        require(depleted.actors()[i].thirst>=hungry[i].thirst,"unavailable water cannot drink");
    }
    std::cout << "fauna population checks passed: 6 actors, deterministic motion, bounded routes, core time separation, shared resource availability\n";
}
