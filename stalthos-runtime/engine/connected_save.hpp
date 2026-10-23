#pragma once
#include "native_save.hpp"

namespace stalthos::engine::connected_save {
// Deliberately separate, partial connected profile. No legacy-format fallback.
inline constexpr std::uint64_t configuration(bool importedPlants) {return 202610230400ull+(importedPlants?1:0);}
// Asset paths are not identities: hash their exact bytes plus the configured motion numbers.
inline std::uint64_t fingerprint(bool plants,const std::vector<std::filesystem::path>& assets,const std::vector<double>& motion) {
    std::ostringstream options;options.precision(17);options<<configuration(plants);for(auto v:motion){if(!std::isfinite(v))throw std::runtime_error("invalid persistence motion configuration");options<<":"<<v;}
    std::uint64_t h=persistence::host_save::checksum(options.str());
    for(const auto& path:assets){h^=path.empty()?0:1;h*=1099511628211ull;if(path.empty())continue;std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot hash connected asset configuration");char data[8192];while(in){in.read(data,sizeof data);for(std::streamsize i=0;i<in.gcount();++i){h^=static_cast<unsigned char>(data[i]);h*=1099511628211ull;}}if(!in.eof())throw std::runtime_error("cannot finish connected asset fingerprint");}
    return h;
}
struct Candidate {
    persistence::host_save::Candidate simulation;
    std::uint64_t nextSession=0,physicalSession=0;
    double calendarScale=1,wetness=0;
};
inline void validateAdmission(const Candidate& c,const fauna::PopulationSnapshot& generated,const ResourceLedgerDto& expected) {
    validateNativeResources(*c.simulation.host.resourceLedger,{},true);
    const auto& saved=*c.simulation.host.faunaPopulation;
    if(saved.actors.size()!=generated.actors.size())throw std::runtime_error("partial connected roster count changed");
    for(const auto& a:generated.actors){auto found=std::find_if(saved.actors.begin(),saved.actors.end(),[&](const auto& b){return a.id==b.id&&a.species==b.species;});if(found==saved.actors.end())throw std::runtime_error("partial connected roster identity/species changed");}
    const auto& resources=c.simulation.host.resourceLedger->resources;
    if(resources.size()!=expected.resources.size())throw std::runtime_error("partial connected resource set changed");
    for(const auto& a:expected.resources){auto found=std::find_if(resources.begin(),resources.end(),[&](const auto& b){return a.id==b.id&&a.kind==b.kind&&a.initialQuantity==b.initialQuantity;});if(found==resources.end())throw std::runtime_error("partial connected resource identity/kind/initial quantity changed");}
}
inline void validate(const Candidate& c) {
    persistence::host_save::validateBinding(c.simulation);
    const auto& h=c.simulation.host;
    if(c.physicalSession>=c.nextSession)throw std::runtime_error("invalid current physical session");
    if(!std::isfinite(c.wetness)||c.wetness<0||c.wetness>1)throw std::runtime_error("invalid connected wetness");
    if(!c.simulation.regional || c.simulation.regional->legacyRegion!=1001 || c.simulation.regional->regions.size()!=1)throw std::runtime_error("partial connected save supports one existing connected watershed");
    for(const auto& region:c.simulation.regional->regions){for(const auto& b:region.bindings)if(b.session>=c.nextSession || (c.physicalSession && b.session==c.physicalSession))throw std::runtime_error("regional binding collides with next session");for(const auto& k:region.ledger.sessions)if(k.sessionId>=c.nextSession)throw std::runtime_error("regional history collides with next session");}
    if(!h.hero || !h.heroNeeds || !h.heroLifecycle || !h.pose || !h.cameraState || !h.resourceLedger || !c.simulation.regional)
        throw std::runtime_error("connected partial save requires complete living body, camera and registry");
    if(!std::isfinite(c.calendarScale)||c.calendarScale<0||c.calendarScale>10000||!c.nextSession)
        throw std::runtime_error("connected partial save clock/session invalid");
    for(const auto& s:h.resourceLedger->sessions)if(s.sessionId>=c.nextSession)throw std::runtime_error("connected next session collides with history");
    for(const auto& s:h.faunaResourceSessions)if(s.second>=c.nextSession)throw std::runtime_error("connected next session collides with actor binding");
    if(!h.retained.empty()||!h.activeTransients.empty())throw std::runtime_error("connected partial save refuses retained relationships/transients");
}
inline std::string encode(const Candidate& c) {
    validate(c);const auto payload=persistence::host_save::encode(c.simulation);
    std::ostringstream out(std::ios::binary);out.write("STLC",4);persistence::detail::write_u32(out,1);
    persistence::detail::write_u64(out,c.nextSession);persistence::detail::write_u64(out,c.physicalSession);persistence::detail::write_double(out,c.calendarScale);persistence::detail::write_double(out,c.wetness);
    persistence::detail::write_u64(out,payload.size());out.write(payload.data(),static_cast<std::streamsize>(payload.size()));
    const auto bytes=out.str();persistence::detail::write_u64(out,persistence::host_save::checksum(bytes));return out.str();
}
inline Candidate decode(std::string_view bytes,std::uint64_t config) {
    if(bytes.size()<56||bytes.size()>persistence::host_save::maxBytes+56||bytes.substr(0,4)!="STLC")throw std::runtime_error("expected connected partial STLC save");
    std::istringstream in{std::string(bytes),std::ios::binary};in.ignore(4);
    if(persistence::detail::read_u32(in)!=1)throw std::runtime_error("unsupported connected partial version");
    Candidate c;c.nextSession=persistence::detail::read_u64(in);c.physicalSession=persistence::detail::read_u64(in);c.calendarScale=persistence::detail::read_double(in);c.wetness=persistence::detail::read_double(in);
    const auto size=persistence::detail::read_u64(in);
    if(size!=bytes.size()-56)throw std::runtime_error("connected partial payload length mismatch");
    in.seekg(static_cast<std::streamoff>(bytes.size()-8));
    if(persistence::detail::read_u64(in)!=persistence::host_save::checksum(bytes.substr(0,bytes.size()-8)))throw std::runtime_error("connected partial checksum mismatch");
    if(bytes.substr(48,4)!="STL9")throw std::runtime_error("connected partial requires complete registry payload");
    c.simulation=persistence::host_save::decode(bytes.substr(48,static_cast<std::size_t>(size)),config);validate(c);return c;
}
inline Candidate read(const std::filesystem::path& path,std::uint64_t config) {
    const auto size=std::filesystem::file_size(path);if(size>persistence::host_save::maxBytes+56)throw std::runtime_error("connected partial save exceeds bound");
    std::ifstream in(path,std::ios::binary);std::string bytes(static_cast<std::size_t>(size),'\0');
    if(!in.read(bytes.data(),static_cast<std::streamsize>(size)))throw std::runtime_error("cannot read connected partial save");return decode(bytes,config);
}
inline NativeSimulationOwner restore(const Candidate& c,const NativePopulationRegistry& admitted,std::uint64_t config) {
    validate(c);auto r=persistence::host_save::restore(c.simulation,admitted.registry(),config);
    return {std::move(r.clock),std::move(r.host),NativePopulationRegistry(std::move(r.registry))};
}
inline void write(const std::filesystem::path& path,const Candidate& c,const NativePopulationRegistry& admitted,std::uint64_t config,
    persistence::host_save::Failure failure=persistence::host_save::Failure::None) {
    (void)restore(c,admitted,config);const auto bytes=encode(c);
    // Never overwrite an original save or an unrelated/corrupt file.
    if(std::filesystem::exists(path))(void)read(path,config);
    static std::atomic<std::uint64_t> serial{0};auto temp=path;temp+=".connected-candidate-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial);
    persistence::detail::TempFileCleanup cleanup{temp};std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    if(!out||failure==persistence::host_save::Failure::Write)throw std::runtime_error("cannot create connected save candidate");
    out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.flush();if(!out.good())throw std::runtime_error("connected candidate flush failed");out.close();
    if(out.fail()||failure==persistence::host_save::Failure::Close)throw std::runtime_error("connected candidate close failed");
    if(failure==persistence::host_save::Failure::Readback)throw std::runtime_error("injected connected readback failure");
    auto verified=read(temp,config);(void)restore(verified,admitted,config);if(encode(verified)!=bytes)throw std::runtime_error("connected candidate readback mismatch");
    if(failure==persistence::host_save::Failure::Replace)throw std::runtime_error("injected connected replacement failure");
#ifdef _WIN32
    const bool exists=std::filesystem::exists(path);
    const bool committed=exists?ReplaceFileW(path.c_str(),temp.c_str(),nullptr,0,nullptr,nullptr)!=0:MoveFileExW(temp.c_str(),path.c_str(),0)!=0;
    if(!committed)throw std::runtime_error("connected atomic replacement failed");
#else
    std::error_code ec;std::filesystem::rename(temp,path,ec);if(ec)throw std::runtime_error("connected atomic replacement failed");
#endif
    cleanup.keep=true;
}
}
