#pragma once
#include "persistence.hpp"
#include "regional_archive.hpp"
#include <sstream>
#include <atomic>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stalthos::persistence::host_save {
// Configuration revision must change whenever immutable native world construction changes.
inline constexpr std::uint64_t configurationRevision=1; // Library example; caller owns admitted configuration identity.
inline constexpr std::uint64_t maxBytes=MAX_SAVE_BYTES+fauna::regional_archive::maxBytes+80;
// Decode/encode validate syntax and mutual payload binding only. They cannot
// authenticate immutable admission. Use restore/write with external admission.
struct Candidate {
    SaveData host;
    std::optional<fauna::RegionalContinuationDto> regional;
    std::uint64_t configuration=0,generation=0;
};
inline std::string legacyBytes(const SaveData& host) {
    std::ostringstream out(std::ios::binary); encodeSave(out,host); return out.str();
}
inline void validateBinding(const Candidate& c) {
    detail::validate(c.host);
    if(!c.regional) return;
    if(!c.generation || c.regional->seed!=c.host.seed || !c.host.faunaPopulation || !c.host.resourceLedger)
        throw std::runtime_error("STL9 missing generation or regional host state");
    auto found=std::find_if(c.regional->regions.begin(),c.regional->regions.end(),[&](const auto& r){return r.population.region==c.regional->legacyRegion;});
    if(found==c.regional->regions.end()) throw std::runtime_error("STL9 missing legacy region");
    auto derived=c.host; derived.faunaPopulation=found->population; derived.resourceLedger=found->ledger;
    derived.faunaResourceSessions.clear();
    for(const auto& b:found->bindings) derived.faunaResourceSessions.emplace_back(b.actor,b.session);
    if(legacyBytes(derived)!=legacyBytes(c.host)) throw std::runtime_error("STL9 host/registry state mismatch");
    std::string regional;
    if(!fauna::regional_archive::encode(*c.regional,regional)) throw std::runtime_error("STL9 invalid regional state");
    (void)restoreSave(c.host); // Validate detached host reconstruction before publication.
}
inline std::uint64_t checksum(std::string_view bytes) {
    std::uint64_t h=14695981039346656037ull;
    for(unsigned char c:bytes) {h^=c;h*=1099511628211ull;} return h;
}
inline std::string encode(const Candidate& c) {
    validateBinding(c);
    if(!c.regional) throw std::runtime_error("STL9 requires registry continuation");
    const auto legacy=legacyBytes(c.host);std::string regional;
    if(!fauna::regional_archive::encode(*c.regional,regional)) throw std::runtime_error("STL9 regional encoding failed");
    std::ostringstream out(std::ios::binary);out.write("STL9",4);detail::write_u32(out,9);
    detail::write_u64(out,c.configuration);detail::write_u64(out,c.generation);detail::write_u64(out,c.host.seed);
    detail::write_double(out,c.host.physicalSeconds);detail::write_double(out,c.host.calendarSeconds);
    detail::write_u64(out,c.host.hero?c.host.hero->region:0);
    detail::write_u64(out,legacy.size());detail::write_u64(out,regional.size());
    out.write(legacy.data(),static_cast<std::streamsize>(legacy.size()));out.write(regional.data(),static_cast<std::streamsize>(regional.size()));
    const auto bytes=out.str();detail::write_u64(out,checksum(bytes));return out.str();
}
inline Candidate decode(std::string_view bytes,std::uint64_t expectedConfiguration) {
    if(bytes.size()>maxBytes) throw std::runtime_error("host save exceeds size bound");
    if(bytes.substr(0,4)!="STL9") {
        std::istringstream in{std::string(bytes),std::ios::binary};Candidate c;c.host=decodeSave(in,bytes.size());return c;
    }
    if(bytes.size()<80) throw std::runtime_error("truncated STL9 header");
    std::istringstream in{std::string(bytes),std::ios::binary};in.ignore(4);
    if(detail::read_u32(in)!=9) throw std::runtime_error("unsupported STL9 version");
    Candidate c;c.configuration=detail::read_u64(in);c.generation=detail::read_u64(in);
    const auto seed=detail::read_u64(in);const auto physical=detail::read_double(in),calendar=detail::read_double(in);
    const auto heroRegion=detail::read_u64(in),legacySize=detail::read_u64(in),regionalSize=detail::read_u64(in);
    if(c.configuration!=expectedConfiguration || legacySize>MAX_SAVE_BYTES || regionalSize>fauna::regional_archive::maxBytes || legacySize+regionalSize+80!=bytes.size())
        throw std::runtime_error("STL9 configuration or bounded payload lengths mismatch");
    in.seekg(static_cast<std::streamoff>(bytes.size()-8));
    if(detail::read_u64(in)!=checksum(bytes.substr(0,bytes.size()-8))) throw std::runtime_error("STL9 checksum mismatch");
    if(legacySize<8 || bytes.substr(72,8)!=std::string_view("STL8\x08\x00\x00\x00",8)) throw std::runtime_error("STL9 requires exact STL8 host payload");
    std::istringstream legacy{std::string(bytes.substr(72,static_cast<std::size_t>(legacySize))),std::ios::binary};
    c.host=decodeSave(legacy,legacySize);c.regional.emplace();
    if(!fauna::regional_archive::decode(bytes.substr(72+static_cast<std::size_t>(legacySize),static_cast<std::size_t>(regionalSize)),*c.regional)) throw std::runtime_error("invalid STL9 regional payload");
    if(seed!=c.host.seed || physical!=c.host.physicalSeconds || calendar!=c.host.calendarSeconds || heroRegion!=(c.host.hero?c.host.hero->region:0)) throw std::runtime_error("STL9 host identity or clock mismatch");
    validateBinding(c);return c;
}
inline Candidate read(const std::filesystem::path& path,std::uint64_t configuration) {
    const auto size=std::filesystem::file_size(path);
    if(size>maxBytes) throw std::runtime_error("host save exceeds size bound");
    std::ifstream in(path,std::ios::binary);if(!in) throw std::runtime_error("cannot open host save");
    std::string bytes(static_cast<std::size_t>(size),'\0');
    if(!in.read(bytes.data(),static_cast<std::streamsize>(size))) throw std::runtime_error("cannot read host save");
    return decode(bytes,configuration);
}
// This owner is detached. The caller may replace its entire live owner only
// after construction succeeds. The admitted registry supplies external world
// topology/configuration and an explicit individual/resource provenance policy.
// Save-declared identities/species/quantities can be admitted for legacy
// compatibility, but are then consistency checks, not independent history proof.
struct RestoredWorld {
    SimCore clock;
    StalthosState host;
    fauna::RegionalPopulationRegistry registry;
};
inline RestoredWorld restore(const Candidate& candidate,const fauna::RegionalPopulationRegistry& admitted,std::uint64_t expectedConfiguration) {
    validateBinding(candidate);
    if(!candidate.regional || candidate.configuration!=expectedConfiguration) throw std::runtime_error("regional restoration configuration mismatch");
    auto registry=admitted;
    if(!registry.restoreContinuation(*candidate.regional)) throw std::runtime_error("regional continuation rejected by external immutable world");
    auto host=restoreSave(candidate.host);
    return {std::move(host.first),std::move(host.second),std::move(registry)};
}
enum class Failure {None,Write,Close,Readback,Replace};
inline void write(const std::filesystem::path& path,const Candidate& candidate,const fauna::RegionalPopulationRegistry& admitted,std::uint64_t expectedConfiguration,Failure failure=Failure::None) {
    (void)restore(candidate,admitted,expectedConfiguration);
    const auto bytes=encode(candidate);(void)decode(bytes,candidate.configuration);
    static std::atomic<std::uint64_t> serial{0};
    auto temp=path;temp+=std::string(".candidate-")+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial);
#ifdef _WIN32
    temp+=std::string("-")+std::to_string(GetCurrentProcessId());
#endif
    detail::TempFileCleanup cleanup{temp};
    std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out) throw std::runtime_error("cannot create host save candidate");
    if(failure==Failure::Write) throw std::runtime_error("injected write failure");
    out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.flush();
    if(!out.good()) throw std::runtime_error("cannot flush host save candidate");
    out.close();if(out.fail() || failure==Failure::Close) throw std::runtime_error("cannot close host save candidate");
    if(failure==Failure::Readback) throw std::runtime_error("injected readback failure");
    const auto verified=read(temp,expectedConfiguration);
    (void)restore(verified,admitted,expectedConfiguration);
    if(encode(verified)!=bytes) throw std::runtime_error("host save readback mismatch");
    if(failure==Failure::Replace) throw std::runtime_error("injected replacement failure");
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    const bool exists=attributes!=INVALID_FILE_ATTRIBUTES;
    const bool committed=exists ? ReplaceFileW(path.c_str(),temp.c_str(),nullptr,0,nullptr,nullptr)!=0 : MoveFileExW(temp.c_str(),path.c_str(),0)!=0;
    if(!committed) throw std::runtime_error("Windows host save replacement failed: "+std::to_string(GetLastError()));
#else
    std::error_code ec;std::filesystem::rename(temp,path,ec);
    if(ec) throw std::runtime_error("host save replacement failed");
#endif
    cleanup.keep=true;
}
inline void exportLegacy(const std::filesystem::path& path,const Candidate& c) {
    validateBinding(c);
    if(c.regional && c.regional->regions.size()!=1) throw std::runtime_error("refusing lossy multiregion STL8 export");
    writeSave(path,c.host);
}
} // namespace stalthos::persistence::host_save
