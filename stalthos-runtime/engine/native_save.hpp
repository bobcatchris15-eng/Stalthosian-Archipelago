#pragma once
#include "native_population_registry.hpp"
#include "domain/host_save.hpp"
namespace stalthos::engine {
// Whole simulation ownership boundary. Renderer/control objects are initialized
// before the first tick; startup rejection exits without a running-world mutation.
struct NativeSimulationOwner {
    SimCore clock;
    StalthosState world;
    std::optional<NativePopulationRegistry> fauna;
};
inline constexpr std::uint64_t nativeSaveConfiguration(bool populated) {
    // Revision2 declares save-derived individual/resource admission provenance.
    return 4+(populated?1:0);
}
// Independent world configuration owns required IDs/kinds and anchors. Initial
// quantities and additional legal ledger entries are validated save declarations,
// preserving the existing legacy host import contract rather than inventing history.
inline void validateNativeResources(const ResourceLedgerDto& ledger,const std::vector<fauna::RegionalResourceAnchor>& anchors) {
    ResourceLedger validator(4096,256);
    if(!validator.restore(ledger)) throw std::runtime_error("invalid native declared resource admission");
    for(const auto& anchor:anchors) {
        const auto found=std::find_if(ledger.resources.begin(),ledger.resources.end(),[&](const auto& r){return r.id==anchor.resource;});
        const auto expected=anchor.resource==100?ResourceKind::Creek:ResourceKind::Shrub;
        if(found==ledger.resources.end() || found->kind!=expected) throw std::runtime_error("native anchored resource kind mismatch");
    }
}
inline NativeSimulationOwner restoreNativeSave(const persistence::host_save::Candidate& candidate,
    const NativePopulationRegistry& declaredAdmission,bool populated) {
    if(!populated || !candidate.regional || candidate.regional->legacyRegion!=1001 || candidate.regional->regions.size()!=1)
        throw std::runtime_error("native STL9 supports exactly original watershed1001");
    // Individual IDs/species/sessions are explicitly admitted declarations from
    // the validated save. This validates consistency, not historical authenticity.
    auto restored=persistence::host_save::restore(candidate,declaredAdmission.registry(),nativeSaveConfiguration(populated));
    return {std::move(restored.clock),std::move(restored.host),NativePopulationRegistry(std::move(restored.registry))};
}
} // namespace stalthos::engine
