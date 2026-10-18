#pragma once
#include "regional_population.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <istream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <ostream>
#include <string_view>

namespace stalthos::fauna::regional_archive {
// Companion format only: never passed to the legacy STL1-STL8 reader.
inline constexpr std::size_t maxBytes = 512 * 1024;
inline constexpr std::uint64_t version = 1;
namespace detail {
struct Invalid {};
struct Writer {
    std::string data;
    void integer(std::uint64_t& v) { for(unsigned i=0;i<8;++i) data.push_back(static_cast<char>(v>>(8*i))); }
    void boolean(bool& v) { data.push_back(v?1:0); }
    void real(double& v) { if(!std::isfinite(v)) throw Invalid{}; std::uint64_t bits; std::memcpy(&bits,&v,8); integer(bits); }
    std::size_t count(std::size_t n,std::size_t limit) { if(n>limit) throw Invalid{}; auto v=static_cast<std::uint64_t>(n); integer(v); return n; }
};
struct Reader {
    std::string_view data; std::size_t at=0;
    void integer(std::uint64_t& v) { if(data.size()-at<8) throw Invalid{}; v=0; for(unsigned i=0;i<8;++i) v|=std::uint64_t(static_cast<unsigned char>(data[at++]))<<(8*i); }
    void boolean(bool& v) { if(at==data.size()) throw Invalid{}; const auto b=static_cast<unsigned char>(data[at++]); if(b>1) throw Invalid{}; v=b!=0; }
    void real(double& v) { std::uint64_t bits; integer(bits); std::memcpy(&v,&bits,8); if(!std::isfinite(v)) throw Invalid{}; }
    std::size_t count(std::size_t,std::size_t limit) { std::uint64_t n; integer(n); if(n>limit) throw Invalid{}; return static_cast<std::size_t>(n); }
};
template<class C> void index(C& c,std::size_t& v,std::size_t limit) { std::uint64_t n=v; c.integer(n); if(n>=limit) throw Invalid{}; v=static_cast<std::size_t>(n); }
template<class C,class E> void enumeration(C& c,E& v,std::uint64_t limit) { auto n=static_cast<std::uint64_t>(v); c.integer(n); if(n>limit) throw Invalid{}; v=static_cast<E>(n); }
template<class C> void vector3(C& c,Vec3& v) { c.real(v.x); c.real(v.y); c.real(v.z); }
template<class C,class T,class F> void sequence(C& c,std::vector<T>& v,std::size_t limit,F f) { auto n=c.count(v.size(),limit); v.resize(n); for(auto& x:v) f(x); }
template<class C> void body(C& c,RegionalContinuationDto& dto) {
    c.integer(dto.seed); c.integer(dto.legacyRegion);
    std::size_t actorBudget=64,resourceBudget=4096,historyBudget=256,anchorBudget=4096,bindingBudget=64,reservedResources=4096,reservedHistory=256;
    sequence(c,dto.regions,8,[&](RetainedRegionSnapshot& r) {
        auto& p=r.population; c.integer(p.seed); c.integer(p.region); c.integer(p.habitatSignature);
        sequence(c,p.actors,std::min(std::size_t(16),actorBudget),[&](ActorSnapshot& a) {
            c.integer(a.id); enumeration(c,a.species,4); vector3(c,a.position); c.real(a.headingRadians); c.real(a.distanceTravelledMeters);
            index(c,a.site,5000); index(c,a.target,5000); c.real(a.calendarAge); enumeration(c,a.stage,4);
            c.real(a.hunger); c.real(a.thirst); c.real(a.fatigue); c.real(a.injury); c.boolean(a.isResting); enumeration(c,a.activity,3);
        });
        actorBudget-=p.actors.size();
        r.reservation.resources=c.count(r.reservation.resources,reservedResources); reservedResources-=r.reservation.resources;
        r.reservation.historyKeys=c.count(r.reservation.historyKeys,reservedHistory); reservedHistory-=r.reservation.historyKeys;
        c.boolean(r.active); c.boolean(r.contactsConfigured);
        sequence(c,r.ledger.resources,std::min(resourceBudget,r.reservation.resources),[&](ResourceDto& v) { c.integer(v.id); enumeration(c,v.kind,1); c.real(v.initialQuantity); c.real(v.consumedQuantity); });
        resourceBudget-=r.ledger.resources.size();
        sequence(c,r.ledger.sessions,std::min(historyBudget,r.reservation.historyKeys),[&](ResourceSessionDto& v) { c.integer(v.sessionId); c.integer(v.resourceId); c.real(v.amount); });
        historyBudget-=r.ledger.sessions.size();
        sequence(c,r.anchors,std::min(anchorBudget,r.reservation.resources),[&](RegionalResourceAnchor& v) { index(c,v.site,5000); c.integer(v.resource); vector3(c,v.position); });
        anchorBudget-=r.anchors.size();
        sequence(c,r.bindings,std::min(bindingBudget,p.actors.size()),[&](RegionalContactBinding& v) { c.integer(v.actor); c.integer(v.session); });
        bindingBudget-=r.bindings.size();
    });
}
inline void validate(const RegionalContinuationDto& d) {
    if(d.regions.empty() || d.regions.size()>RegionalPopulationRegistry::maxRegions) throw Invalid{};
    std::set<RegionId> regions; std::set<Identity> actors; std::set<std::uint64_t> resources,bound;
    std::map<std::uint64_t,std::set<RegionId>> historyOwners;
    std::size_t nr=0,nh=0,na=0,anchors=0,bindings=0;
    for(const auto& r:d.regions) {
        if(r.population.actors.size()>16 || r.reservation.resources>4096 || r.reservation.historyKeys>256 || r.ledger.resources.size()>r.reservation.resources || r.ledger.sessions.size()>r.reservation.historyKeys || r.anchors.size()>4096 || r.bindings.size()>64) throw Invalid{};
        if(!regions.insert(r.population.region).second || r.population.seed!=d.seed || r.population.actors.empty()) throw Invalid{};
        nr+=r.reservation.resources; nh+=r.reservation.historyKeys; na+=r.population.actors.size(); anchors+=r.anchors.size(); bindings+=r.bindings.size();
        if(nr>4096 || nh>256 || na>64 || anchors>4096 || bindings>64 || r.anchors.size()>r.reservation.resources || r.bindings.size()>r.population.actors.size()) throw Invalid{};
        ResourceLedger ledger(r.reservation.resources,r.reservation.historyKeys); if(!ledger.restore(r.ledger)) throw Invalid{};
        std::set<Identity> local; std::set<std::uint64_t> localResources;
        std::set<std::pair<std::size_t,ResourceKind>> anchorSlots;
        for(const auto& a:r.population.actors) {
            if(a.id<2 || !actors.insert(a.id).second || a.calendarAge<0 || a.distanceTravelledMeters<0 || a.hunger<0 || a.hunger>1 || a.thirst<0 || a.thirst>1 || a.fatigue<0 || a.fatigue>1 || a.injury<0 || a.injury>1) throw Invalid{};
            local.insert(a.id);
        }
        for(const auto& v:r.ledger.resources) { if(!resources.insert(v.id).second) throw Invalid{}; localResources.insert(v.id); }
        for(const auto& h:r.ledger.sessions) { historyOwners[h.sessionId].insert(r.population.region); }
        for(const auto& a:r.anchors) {
            const auto resource=std::find_if(r.ledger.resources.begin(),r.ledger.resources.end(),[&](const ResourceDto& v){return v.id==a.resource;});
            if(!r.contactsConfigured || resource==r.ledger.resources.end() ||
                !anchorSlots.emplace(a.site,resource->kind).second) throw Invalid{};
        }
        std::set<Identity> localBindings;
        for(const auto& b:r.bindings) if(!b.session || !local.count(b.actor) || !localBindings.insert(b.actor).second || !bound.insert(b.session).second) throw Invalid{};
    }
    if(!regions.count(d.legacyRegion)) throw Invalid{};
    for(const auto& r:d.regions) for(const auto& b:r.bindings) { auto i=historyOwners.find(b.session); if(i!=historyOwners.end() && (i->second.size()!=1 || !i->second.count(r.population.region))) throw Invalid{}; }
}
}
// Output arguments and live registry remain unchanged on rejection. Encoding
// preserves DTO order and IEEE-754 bits (including signed zero), little endian.
inline bool encode(const RegionalContinuationDto& dto,std::string& bytes) {
    try { detail::validate(dto); auto copy=dto; detail::Writer w; w.data="STLR"; auto v=version; w.integer(v); detail::body(w,copy); if(w.data.size()>maxBytes) return false; bytes.swap(w.data); return true; } catch(...) { return false; }
}
inline bool decode(std::string_view bytes,RegionalContinuationDto& dto) {
    try { if(bytes.size()>maxBytes || bytes.size()<12 || bytes.substr(0,4)!="STLR") return false; detail::Reader r{bytes,4}; std::uint64_t v; r.integer(v); if(v!=version) return false; RegionalContinuationDto next; detail::body(r,next); if(r.at!=bytes.size()) return false; detail::validate(next); dto=std::move(next); return true; } catch(...) { return false; }
}
// Reconstruction requires an externally admitted registry with the identical immutable
// world layout. The archive carries continuation state, not habitat definitions.
inline bool restore(RegionalPopulationRegistry& registry,std::string_view bytes) {
    RegionalContinuationDto dto; return decode(bytes,dto) && registry.restoreContinuation(dto);
}
inline bool write(std::ostream& stream,const RegionalContinuationDto& dto) {
    try { std::string bytes; if(!encode(dto,bytes)) return false; stream.write(bytes.data(),static_cast<std::streamsize>(bytes.size())); return bool(stream); } catch(...) { return false; }
}
inline bool read(std::istream& stream,RegionalContinuationDto& dto) {
    try { std::string bytes; char ch; while(stream.get(ch)) { if(bytes.size()==maxBytes) return false; bytes.push_back(ch); } if(!stream.eof() || stream.bad()) return false; return decode(bytes,dto); } catch(...) { return false; }
}
} // namespace stalthos::fauna::regional_archive
