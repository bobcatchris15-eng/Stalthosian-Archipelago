#pragma once

#include "foundation.hpp"
#include "lifecycle.hpp"
#include "needs.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

namespace stalthos::fauna {

enum class Activity { Rest, Travel, Browse, Drink };

struct HabitatSite {
    Vec3 position;
    bool browse = false;
    bool freshwater = false;
    std::vector<std::size_t> neighbors;
};

struct ActorView {
    Identity id;
    HeroSpecies species;
    Vec3 position;
    double headingRadians;
    double calendarAge;
    double distanceTravelledMeters;
    double hunger;
    double thirst;
    double fatigue;
    bool alive;
    Activity activity;
};

struct ActorSnapshot {
    Identity id = 0;
    HeroSpecies species = HeroSpecies::Smalltrotter;
    Vec3 position{};
    double headingRadians = 0.0;
    double distanceTravelledMeters = 0.0;
    std::size_t site = 0;
    std::size_t target = 0;
    double calendarAge = 0.0;
    LifeStage stage = LifeStage::Hatchling;
    double hunger = 0.0, thirst = 0.0, fatigue = 0.0, injury = 0.0;
    bool isResting = false;
    Activity activity = Activity::Rest;
};
struct PopulationSnapshot {
    std::uint64_t seed = 0;
    RegionId region = 0;
    std::uint64_t habitatSignature = 0;
    std::vector<ActorSnapshot> actors;
};

class Population {
    struct Actor {
        Identity id{};
        HeroSpecies species{};
        Vec3 position{};
        double heading = 0;
        double distance = 0;
        std::size_t site = 0;
        std::size_t target = 0;
        Lifecycle life{HeroSpecies::Smalltrotter};
        needs::NeedsCore needs{HeroSpecies::Smalltrotter};
        Activity activity = Activity::Rest;
        Actor(Identity identity, HeroSpecies kind, double age, std::size_t at, Vec3 initialPosition, Activity mode,
              double initialHunger, double initialThirst, double initialFatigue, LifeStage initialStage = LifeStage::Adult)
            : id(identity), species(kind), position(initialPosition), site(at), target(at), life(kind), needs(kind), activity(mode) {
            life.restoreState(kind, age, initialStage);
            needs.restoreState(initialHunger, initialThirst, initialFatigue, 0, mode == Activity::Rest);
        }
    };

    std::uint64_t seed_;
    RegionId region_;
    std::vector<HabitatSite> sites_;
    std::vector<Actor> actors_;
    std::vector<ActorView> views_;

    std::uint64_t habitatSignature() const noexcept {
        std::uint64_t h = UINT64_C(1469598103934665603);
        auto add = [&h](std::uint64_t v) { for (int i=0;i<8;++i) { h ^= (v >> (i*8)) & 0xff; h *= UINT64_C(1099511628211); } };
        add(sites_.size());
        for (const auto& site : sites_) {
            std::uint64_t bits=0; std::memcpy(&bits,&site.position.x,8); add(bits);
            std::memcpy(&bits,&site.position.y,8); add(bits); std::memcpy(&bits,&site.position.z,8); add(bits);
            add(site.neighbors.size()); for (auto n:site.neighbors) add(n);
        }
        return h;
    }

    static std::uint64_t mix(std::uint64_t x) noexcept {
        x += UINT64_C(0x9e3779b97f4a7c15);
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        return x ^ (x >> 31);
    }
    std::uint64_t scoped(std::uint64_t purpose, std::size_t index) const noexcept {
        return mix(seed_ ^ mix(region_) ^ mix(purpose) ^ mix(static_cast<std::uint64_t>(index)));
    }
    static double distance(Vec3 a, Vec3 b) noexcept {
        const double x = b.x-a.x, y=b.y-a.y, z=b.z-a.z;
        return std::sqrt(x*x+y*y+z*z);
    }
    std::size_t eligibleSite(std::size_t from, bool browse) const {
        std::size_t best = sites_.size(); double bestDistance = std::numeric_limits<double>::infinity();
        for (std::size_t i=0; i<sites_.size(); ++i) {
            if (browse ? !sites_[i].browse : !sites_[i].freshwater) continue;
            const auto d = distance(sites_[from].position, sites_[i].position);
            if (d < bestDistance) { bestDistance=d; best=i; }
        }
        return best;
    }
    std::size_t nextHop(std::size_t from, std::size_t target) const {
        if (from == target) return from;
        // Bounded BFS: sites are caller-owned and at most 5000 are accepted.
        std::vector<std::size_t> previous(sites_.size(), sites_.size());
        std::vector<std::size_t> queue; queue.reserve(sites_.size());
        previous[from]=from; queue.push_back(from);
        for (std::size_t q=0; q<queue.size() && previous[target]==sites_.size(); ++q) {
            for (auto n: sites_[queue[q]].neighbors) if (previous[n]==sites_.size()) {
                previous[n]=queue[q]; queue.push_back(n);
            }
        }
        if (previous[target]==sites_.size()) return from;
        auto hop=target;
        while (previous[hop] != from) hop=previous[hop];
        return hop;
    }
    void chooseActivity(Actor& a) const {
        if (a.life.stage()==LifeStage::Dead) return;
        if (a.needs.thirst() > 0.48) {
            auto site=eligibleSite(a.site, false);
            if (site != sites_.size()) { a.target=site; a.activity=(a.site==site ? Activity::Drink : Activity::Travel); return; }
        }
        if (a.needs.hunger() > 0.42) {
            auto site=eligibleSite(a.site, true);
            if (site != sites_.size()) { a.target=site; a.activity=(a.site==site ? Activity::Browse : Activity::Travel); return; }
        }
        if (a.needs.fatigue() > 0.62) { a.target=a.site; a.activity=Activity::Rest; return; }
        // Seeded short local moves create visible activity from the first frame.
        if (!sites_[a.site].neighbors.empty() && (scoped(0x726f757465ULL, static_cast<std::size_t>(a.id)) % 3 == 0)) {
            const auto& ns=sites_[a.site].neighbors;
            a.target=ns[scoped(0x746172676574ULL, static_cast<std::size_t>(a.id)) % ns.size()];
            a.activity=Activity::Travel; return;
        }
        a.target=a.site; a.activity=Activity::Rest;
    }
public:
    static constexpr std::size_t maxActors = 16;
    static constexpr std::size_t maxSites = 5000;
    Population(std::uint64_t worldSeed, RegionId region, const std::vector<HabitatSite>& sites,
               std::size_t count, const std::set<Identity>& excludedIds = {})
        : seed_(worldSeed), region_(region), sites_(sites) {
        if (count > maxActors) throw std::invalid_argument("fauna population exceeds 16 active actors");
        if (count && sites.empty()) throw std::invalid_argument("fauna population requires supplied habitat sites");
        if (sites.size() > maxSites) throw std::invalid_argument("fauna habitat exceeds 5000 sites");
        for (const auto& s: sites) {
            if (!finite(s.position)) throw std::invalid_argument("fauna habitat position is non-finite");
            for (auto n: s.neighbors) if (n>=sites.size()) throw std::invalid_argument("fauna habitat neighbor is out of range");
        }
        std::vector<bool> reachable(sites.size(), false);
        if (!sites.empty()) {
            std::vector<std::size_t> q{0}; reachable[0]=true;
            for (std::size_t i=0; i<q.size(); ++i) for (auto n: sites[q[i]].neighbors) if (!reachable[n]) { reachable[n]=true; q.push_back(n); }
        }
        for (bool connected: reachable) if (!connected) throw std::invalid_argument("fauna habitat graph is disconnected");
        for (std::size_t i=0; i<sites.size(); ++i) if (reachable[i])
            for (auto n: sites[i].neighbors) if (!reachable[n]) throw std::invalid_argument("fauna habitat graph is disconnected");
        std::vector<std::size_t> browse, water;
        for (std::size_t i=0; i<sites.size(); ++i) if (reachable[i]) {
            if (sites[i].browse) browse.push_back(i);
            if (sites[i].freshwater) water.push_back(i);
        }
        if (count && browse.empty()) throw std::invalid_argument("fauna habitat has no browse sites");
        if (count && water.empty()) throw std::invalid_argument("fauna habitat has no freshwater sites");
        std::set<Identity> ids=excludedIds;
        actors_.reserve(count);
        static constexpr std::array<HeroSpecies,5> kinds{HeroSpecies::Smalltrotter, HeroSpecies::Brackenback, HeroSpecies::Reedcrest, HeroSpecies::Thistletail, HeroSpecies::Longstride};
        for (std::size_t i=0; i<count; ++i) {
            Identity id=0;
            for (std::size_t attempt=0; attempt<128; ++attempt) {
                const auto candidate=scoped(0x6964656e74697479ULL, i*128+attempt);
                if (candidate!=0 && candidate!=1 && !ids.count(candidate)) { id=candidate; break; }
            }
            if (!id) throw std::runtime_error("unable to allocate unique fauna identity after 128 deterministic attempts");
            ids.insert(id);
            const auto kind=kinds[scoped(0x73706563696573ULL,i)%kinds.size()];
            auto at=sites.empty() ? 0 : static_cast<std::size_t>(scoped(0x706f736974696f6eULL,i)%sites.size());
            const auto& p=getHeroProfile(kind);
            const auto age=p.hatchlingDuration+p.juvenileDuration+scoped(0x616765ULL,i)%static_cast<std::uint64_t>(p.adultDuration*0.8);
            const auto slot=i%4;
            Activity mode=slot==0 ? Activity::Travel : slot==1 ? Activity::Browse : slot==2 ? Activity::Drink : Activity::Rest;
            double hunger=0.22, thirst=0.25, fatigue=0.28;
            if (mode==Activity::Browse) { at=browse[(i/4)%browse.size()]; hunger=0.55; }
            else if (mode==Activity::Drink) { at=water[(i/4)%water.size()]; thirst=0.60; }
            else if (mode==Activity::Rest) fatigue=0.68;
            Actor a{id,kind,age,at,sites[at].position,mode,hunger,thirst,fatigue};
            if (mode==Activity::Travel) a.target=sites[at].neighbors.empty()?at:sites[at].neighbors.front();
            actors_.push_back(std::move(a));
        }
        refreshViews();
    }
    void advance(const TimeStep& dt) {
        if (!std::isfinite(dt.physicalSeconds)||dt.physicalSeconds<0||!std::isfinite(dt.calendarSeconds)||dt.calendarSeconds<0)
            throw std::invalid_argument("invalid fauna time step");
        if (dt.physicalSeconds==0 && dt.calendarSeconds==0) return;
        // Preflight all shared component arithmetic on copies. No actor is committed if any actor overflows.
        auto next=actors_;
        for (auto& a: next) {
            auto life=a.life; auto needsState=a.needs;
            life.advance(dt); needsState.advance(dt);
            if (life.stage()==LifeStage::Dead) { a.life=life; a.activity=Activity::Rest; continue; }
            a.life=life; a.needs=needsState;
            if (a.activity==Activity::Rest) a.needs.setResting(true); else a.needs.setResting(false);
            if (a.activity==Activity::Browse && sites_[a.site].browse) a.needs.feed(std::min(0.0015*dt.physicalSeconds,0.08));
            if (a.activity==Activity::Drink && sites_[a.site].freshwater) a.needs.drink(std::min(0.002*dt.physicalSeconds,0.10));
            if (a.activity!=Activity::Travel) chooseActivity(a);
            if (a.activity==Activity::Travel && a.target!=a.site && dt.physicalSeconds>0) {
                const auto hop=nextHop(a.site,a.target);
                if (hop!=a.site) {
                    const auto length=distance(a.position,sites_[hop].position);
                    const double speed=0.18+static_cast<double>(scoped(0x7370656564ULL,static_cast<std::size_t>(a.id))%2700)/10000.0;
                    const double step=std::min(length,speed*dt.physicalSeconds);
                    if (length>0) {
                        const double f=step/length; const Vec3 old=a.position, dest=sites_[hop].position;
                        a.position={old.x+(dest.x-old.x)*f,old.y+(dest.y-old.y)*f,old.z+(dest.z-old.z)*f};
                        a.distance+=step; a.heading=std::atan2(dest.x-old.x,dest.z-old.z);
                    }
                    if (step>=length) { a.site=hop; a.position=sites_[hop].position; }
                }
            }
            if (!std::isfinite(a.distance)||!finite(a.position)||!std::isfinite(a.heading)) throw std::overflow_error("fauna motion overflow");
            if (a.activity==Activity::Travel && a.site==a.target) chooseActivity(a);
            else if (a.activity==Activity::Browse && !sites_[a.site].browse) chooseActivity(a);
            else if (a.activity==Activity::Drink && !sites_[a.site].freshwater) chooseActivity(a);
        }
        actors_.swap(next);
        refreshViews();
    }
    const std::vector<ActorView>& actors() const noexcept { return views_; }
    PopulationSnapshot snapshot() const {
        PopulationSnapshot out{seed_,region_,habitatSignature(),{}}; out.actors.reserve(actors_.size());
        for (const auto& a:actors_) out.actors.push_back({a.id,a.species,a.position,a.heading,a.distance,a.site,a.target,
            a.life.calendarAge(),a.life.stage(),a.needs.hunger(),a.needs.thirst(),a.needs.fatigue(),a.needs.injury(),a.needs.isResting(),a.activity});
        return out;
    }
    void restore(const PopulationSnapshot& saved, const std::set<Identity>& excludedIds = {}) {
        if (saved.seed!=seed_ || saved.region!=region_ || saved.habitatSignature!=habitatSignature()) throw std::invalid_argument("fauna snapshot habitat identity mismatch");
        if (saved.actors.size()>maxActors) throw std::invalid_argument("fauna snapshot exceeds 16 active actors");
        std::set<Identity> ids=excludedIds; std::vector<Actor> replacement; replacement.reserve(saved.actors.size());
        for (const auto& s:saved.actors) {
            if (!s.id || s.id==1 || ids.count(s.id)) throw std::invalid_argument("fauna snapshot identity conflict");
            if (s.site>=sites_.size() || s.target>=sites_.size() || !finite(s.position) || !std::isfinite(s.headingRadians) || !std::isfinite(s.distanceTravelledMeters) || s.distanceTravelledMeters<0) throw std::invalid_argument("invalid fauna snapshot motion");
            if (static_cast<unsigned>(s.species)>static_cast<unsigned>(HeroSpecies::Longstride) || static_cast<unsigned>(s.stage)>static_cast<unsigned>(LifeStage::Dead) || static_cast<unsigned>(s.activity)>static_cast<unsigned>(Activity::Drink)) throw std::invalid_argument("invalid fauna snapshot enum");
            const auto at=sites_[s.site].position;
            if ((s.activity==Activity::Travel || s.stage==LifeStage::Dead) && s.site!=s.target) {
                const auto hop=nextHop(s.site,s.target); const auto end=sites_[hop].position;
                const double vx=end.x-at.x, vy=end.y-at.y, vz=end.z-at.z;
                const double wx=s.position.x-at.x, wy=s.position.y-at.y, wz=s.position.z-at.z;
                const double len2=vx*vx+vy*vy+vz*vz;
                const double t=len2>0?(wx*vx+wy*vy+wz*vz)/len2:0;
                const double px=at.x+t*vx, py=at.y+t*vy, pz=at.z+t*vz;
                if (t < -1e-6 || t > 1.0+1e-6 || distance(s.position,{px,py,pz})>1e-6) throw std::invalid_argument("fauna snapshot position is outside its current route");
            } else if (distance(s.position,at)>1e-6) throw std::invalid_argument("fauna snapshot stationary position does not match habitat site");
            ids.insert(s.id); Actor a{s.id,s.species,s.calendarAge,s.site,s.position,s.activity,s.hunger,s.thirst,s.fatigue,s.stage};
            a.distance=s.distanceTravelledMeters; a.heading=s.headingRadians; a.target=s.target;
            a.needs.restoreState(s.hunger,s.thirst,s.fatigue,s.injury,s.isResting); replacement.push_back(std::move(a));
        }
        actors_.swap(replacement); refreshViews();
    }
    // Resource ownership stays with the shared world ledger. The host supplies
    // current contact availability before each active-region step.
    void setSiteResources(std::size_t index, bool browse, bool freshwater) {
        if(index>=sites_.size()) throw std::out_of_range("fauna resource site index");
        sites_[index].browse=browse;
        sites_[index].freshwater=freshwater;
    }
private:
    void refreshViews() {
        views_.clear(); views_.reserve(actors_.size());
        for (const auto& a: actors_) views_.push_back({a.id,a.species,a.position,a.heading,a.life.calendarAge(),a.distance,a.needs.hunger(),a.needs.thirst(),a.needs.fatigue(),a.life.stage()!=LifeStage::Dead,a.life.stage()==LifeStage::Dead?Activity::Rest:a.activity});
    }
};

} // namespace stalthos::fauna
