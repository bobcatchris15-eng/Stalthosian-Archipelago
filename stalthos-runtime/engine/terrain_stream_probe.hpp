#pragma once
#include "WickedEngine.h"
#include "json.hpp"
#include "domain/terrain_tiles.hpp"
#include "domain/watershed_terrain.hpp"
#include "domain/rendered_ground_surface.hpp"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>

namespace stalthos::engine {
// Construction diagnostics; production activation and persistence remain separate.
template<class Sampler>
inline nlohmann::json runTerrainStreamProbeWithSampler(wi::Application& application,
    wi::RenderPath3D& path, HWND window, const std::filesystem::path& output,
    double duration, bool interactive, const Sampler& tiles, bool watershedJoin) {
    using namespace DirectX;
    using Key = std::pair<std::int64_t, std::int64_t>;
    struct Resident {
        wi::ecs::Entity entity;
        domain::RenderedGroundSurface ground;
        std::vector<XMFLOAT3> normals;
    };
    const auto cells = tiles.config().cells;
    const auto stride = cells + 1;
    auto& scene = wi::scene::GetScene();
    std::map<Key, Resident> resident;
    std::set<Key> visited, builtKeys;
    std::size_t regenerated = 0, watershedEvictions = 0;
    std::size_t regenerationMismatches = 0;
    std::map<Key,std::uint64_t> originalDigests;
    double maxInteriorMeshError = 0;
    std::uint64_t interiorMeshSamples = 0;
    domain::ReferenceLandscape reference(42);
    std::size_t created = 0, evicted = 0, peak = 0, crossings = 0;
    double maxSeamHeight = 0, maxSeamNormal = 0, maxContactError = 0, maxSourceError = 0;
    double maxRenderedBoundsCenterError = 0;
    std::uint64_t seamChecks = 0, supportSamples = 0, frames = 0;
    nlohmann::json transitions = nlohmann::json::array();
    nlohmann::json captureDiagnostics = nlohmann::json::array();
    auto indices = std::vector<std::uint32_t>{};
    for (std::uint32_t z = 0; z < cells; ++z) for (std::uint32_t x = 0; x < cells; ++x) {
        const auto a = z * stride + x, b = a + 1, c = a + stride, d = c + 1;
        indices.insert(indices.end(), {a,b,c,b,d,c});
    }
    auto stream = [&](double x, double z) {
        std::set<Key> wanted;
        for (const auto key : tiles.neighborhood({x,0,z})) wanted.emplace(key.x,key.z);
        if (watershedJoin) for (int iz=-1;iz<=0;++iz) for(int ix=-1;ix<=0;++ix) wanted.emplace(ix,iz);
        // Retire only our own entities; the engine owns deferred GPU disposal.
        for (auto it = resident.begin(); it != resident.end();) {
            if (!wanted.count(it->first)) {
                if(watershedJoin && it->first.first>=-1 && it->first.first<=0 && it->first.second>=-1 && it->first.second<=0) ++watershedEvictions;
                scene.Entity_Remove(it->second.entity); it = resident.erase(it); ++evicted;
            } else ++it;
        }
        for (const auto key : wanted) if (!resident.count(key)) {
            const auto tile = tiles.build({key.first,key.second});
            std::vector<XMFLOAT3> positions, normals;
            for (const auto& v : tile.vertices) {
                positions.push_back({float(v.position.x),float(v.position.y),float(v.position.z)});
                normals.push_back({float(v.normal.x),float(v.normal.y),float(v.normal.z)});
            }
            const auto entity = scene.Entity_CreateMeshFromData("coarse streamed tile", indices.size(),
                indices.data(), positions.size(), positions.data());
            auto* object = scene.objects.GetComponent(entity);
            auto* mesh = scene.meshes.GetComponent(object->meshID);
            mesh->vertex_normals = normals;
            for (const auto& v : tile.vertices) {
                const auto color = v.sea ? wi::Color(62,76,78) :
                    (v.moisture > .65 ? wi::Color(67,91,48) : wi::Color(113,112,72));
                mesh->vertex_colors.push_back(color.rgba);
            }
            for (const auto& subset : mesh->subsets) {
                auto* material = scene.materials.GetComponent(subset.materialID);
                material->SetBaseColor({1,1,1,1}); material->SetRoughness(.95f);
                material->SetUseVertexColors(true);
            }
            mesh->CreateRenderData();
            Resident entry{entity, {}, std::move(normals)};
            if (!entry.ground.build(mesh->vertex_positions.data(),mesh->vertex_positions.size(),mesh->indices.data(),mesh->indices.size()))
                throw std::runtime_error("streamed rendered ground cache: " + entry.ground.errorMessage());
            entry.normals=mesh->vertex_normals;
            const auto& vertices = entry.ground.worldVertices();
            std::uint64_t digest=1469598103934665603ULL;
            const auto hashDouble=[&](double value) {
                std::uint64_t bits=0; std::memcpy(&bits,&value,sizeof(bits));
                for(int b=0;b<8;++b) { digest^=(bits>>(b*8))&255; digest*=1099511628211ULL; }
            };
            for(const auto& v:vertices) { hashDouble(v.x); hashDouble(v.y); hashDouble(v.z); }
            for(const auto& n:entry.normals) { hashDouble(n.x); hashDouble(n.y); hashDouble(n.z); }
            if (builtKeys.count(key)) {
                ++regenerated;
                if(originalDigests.at(key)!=digest) ++regenerationMismatches;
            } else { builtKeys.insert(key); originalDigests.emplace(key,digest); }
            resident.emplace(key,std::move(entry)); ++created;
        }
        peak = std::max(peak,resident.size());
        // Compare the actual float mesh buffers at every shared resident edge.
        for (const auto& item : resident) for (int axis = 0; axis < 2; ++axis) {
            const Key next{item.first.first + (axis == 0), item.first.second + (axis == 1)};
            const auto neighbor = resident.find(next);
            if (neighbor == resident.end()) continue;
            for (std::size_t i = 0; i <= cells; ++i) {
                const auto a = axis == 0 ? i*stride+cells : cells*stride+i;
                const auto b = axis == 0 ? i*stride : i;
                const auto& p = item.second.ground.worldVertices()[a];
                const auto& q = neighbor->second.ground.worldVertices()[b];
                maxSeamHeight = std::max(maxSeamHeight,std::abs(p.y-q.y));
                const auto& n = item.second.normals[a]; const auto& m = neighbor->second.normals[b];
                maxSeamNormal = std::max(maxSeamNormal,double(std::max({std::abs(n.x-m.x),std::abs(n.y-m.y),std::abs(n.z-m.z)})));
                ++seamChecks;
            }
        }
    };
    scene.weather.ambient = {.35f,.35f,.35f};
    scene.weather.sunDirection = {-.48f,.8f,-.36f};
    scene.Entity_CreateLight("terrain probe sun",{0,300,0},{1,.96f,.9f},5,2000,wi::scene::LightComponent::DIRECTIONAL);
    const auto body = scene.Entity_CreateCube("terrain traversal contact proxy");
    scene.materials.GetComponent(body)->SetBaseColor({.85f,.04f,.015f,1});
    // Accelerated construction route: inspect residency/contact without temporal
    // occlusion hiding the diagnostic marker after a large camera displacement.
    wi::renderer::SetOcclusionCullingEnabled(false);
    wi::renderer::SetMeshletOcclusionCullingEnabled(false);
    path.init(application.canvas); path.Load();
    path.setSSREnabled(false); path.setSSGIEnabled(false);
    path.setEyeAdaptionEnabled(false); path.setMotionBlurEnabled(false);
    application.ActivatePath(&path);
    wi::scene::GetCamera().CreatePerspective(1920,1080,.1f,2000,3.14159265f/3);
    double x = watershedJoin ? 24 : -160, z = 24;
    Key previousKey{-2,0};
    const auto started = std::chrono::steady_clock::now(); auto previous = started;
    bool closed = false; std::array<bool,3> captured{};
    while (!closed) {
        MSG message;
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            if (message.message == WM_QUIT) closed = true;
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if (closed || (GetAsyncKeyState(VK_ESCAPE)&0x8000)) break;
        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now-started).count();
        const double dt = std::min(.1,std::chrono::duration<double>(now-previous).count()); previous = now;
        const bool finishing = !interactive && elapsed >= duration && frames > 0;
        if (interactive) {
            x += ((GetAsyncKeyState('D')&0x8000 ? 1 : 0)-(GetAsyncKeyState('A')&0x8000 ? 1 : 0))*dt*20;
            z += ((GetAsyncKeyState('W')&0x8000 ? 1 : 0)-(GetAsyncKeyState('S')&0x8000 ? 1 : 0))*dt*20;
            x = std::clamp(x,-13000.0,13000.0); z = std::clamp(z,-13000.0,13000.0);
        } else if (watershedJoin) {
            const double t=std::min(1.0,elapsed/duration);
            x = 24 + 520 * (t < .5 ? t*2 : (1-t)*2);
        } else x = -160 + 576*std::min(1.0,elapsed/duration);
        const Key key{std::int64_t(std::floor(x/tiles.spanMeters())),std::int64_t(std::floor(z/tiles.spanMeters()))};
        if (resident.empty() || key != previousKey) {
            stream(x,z);
            if (!resident.empty() && frames > 0) ++crossings;
            transitions.push_back({{"x",x},{"z",z},{"tile_x",key.first},{"tile_z",key.second},{"resident",resident.size()}});
            previousKey = key;
        }
        visited.insert(key);
        const double ground = resident.at(key).ground.sampleHeight(x,z);
        if(watershedJoin && std::abs(x)<96 && std::abs(z)<96) {
            const double ox=std::floor(x/2)*2, oz=std::floor(z/2)*2;
            const double u=(x-ox)/2, v=(z-oz)/2;
            // Same 2m lattice, float vertices and triangle winding as Terrain().
            const double a=float(reference.height(ox,oz)), b=float(reference.height(ox+2,oz));
            const double c=float(reference.height(ox,oz+2)), d=float(reference.height(ox+2,oz+2));
            const double oldGround=u+v<=1 ? a+(b-a)*u+(c-a)*v : d+(c-d)*(1-u)+(b-d)*(1-v);
            maxInteriorMeshError=std::max(maxInteriorMeshError,std::abs(ground-oldGround));
            ++interiorMeshSamples;
        }
        auto* transform = scene.transforms.GetComponent(body);
        transform->ClearTransform(); transform->Scale(XMFLOAT3(.5f,.5f,.9f));
        transform->Translate(XMFLOAT3(float(x),float(ground+.5),float(z))); transform->UpdateTransform();
        maxContactError = std::max(maxContactError,std::abs(double(transform->GetPosition().y)-.5-ground));
        maxSourceError = std::max(maxSourceError,std::abs(ground-tiles.sample(x,z).position.y)); ++supportSamples;
        const auto view = XMMatrixLookAtLH(XMVectorSet(float(x-25),float(ground+18),float(z-28),1),
            XMVectorSet(float(x+12),float(ground+1),float(z),1),XMVectorSet(0,1,0,0));
        wi::scene::GetCamera().TransformCamera(XMMatrixInverse(nullptr,view)); wi::scene::GetCamera().UpdateCamera();
        application.Run(); ++frames;
        const auto objectIndex = scene.objects.GetIndex(body);
        if (objectIndex >= scene.aabb_objects.size()) throw std::runtime_error("missing rendered proxy bounds");
        const auto renderedCenter = scene.aabb_objects[objectIndex].getCenter();
        maxRenderedBoundsCenterError = std::max(maxRenderedBoundsCenterError,
            std::max({std::abs(double(renderedCenter.x)-x),std::abs(double(renderedCenter.y)-ground-.5),std::abs(double(renderedCenter.z)-z)}));
        for (std::size_t i=0;i<captured.size();++i) if (!captured[i] && elapsed >= duration*(watershedJoin ? (.055+.18*i) : (.15+.3*i))) {
            captured[i] = wi::helper::saveTextureToFile(path.GetRenderResult3D(),(output/("terrain-"+std::to_string(i)+".png")).u8string());
            XMFLOAT3 projected;
            XMStoreFloat3(&projected,XMVector3TransformCoord(XMLoadFloat3(&renderedCenter),wi::scene::GetCamera().GetViewProjection()));
            const auto eye = wi::scene::GetCamera().Eye;
            captureDiagnostics.push_back({{"capture",i},{"body_x",x},{"body_z",z},
                {"bounds_center",{renderedCenter.x,renderedCenter.y,renderedCenter.z}},
                {"camera_eye",{eye.x,eye.y,eye.z}},{"projected_center",{projected.x,projected.y,projected.z}},
                {"visible_object",std::find(path.visibility_main.visibleObjects.begin(),path.visibility_main.visibleObjects.end(),objectIndex)!=path.visibility_main.visibleObjects.end()}});
        }
        if (finishing) break;
    }
    DestroyWindow(window);
    return {{"schema_version",1},{"mode",watershedJoin ? "watershed-join-probe" : "terrain-stream-probe"},{"frames",frames},
        {"regenerated_tiles",regenerated},{"regenerated_mesh_mismatches",regenerationMismatches},
        {"interior_mesh_samples",interiorMeshSamples},{"max_interior_mesh_height_error_m",maxInteriorMeshError},
        {"watershed_tile_evictions",watershedEvictions},{"watershed_tiles_pinned",watershedJoin ? 4 : 0},{"route_returned",watershedJoin && x < 26},
        {"final_body_x",x},{"created_tiles",created},{"evicted_tiles",evicted},{"peak_resident_tiles",peak},
        {"body_tile_crossings",crossings},{"visited_body_tiles",visited.size()},{"transitions",transitions},
        {"shared_edge_samples",seamChecks},{"max_shared_height_error_m",maxSeamHeight},
        {"max_shared_normal_component_error",maxSeamNormal},{"support_samples",supportSamples},
        {"max_proxy_bottom_center_error_m",maxContactError},{"max_source_interpolation_error_m",maxSourceError},
        {"max_rendered_bounds_center_error_m",maxRenderedBoundsCenterError},{"occlusion_culling",false},
        {"captures",captured},{"interactive",interactive},
        {"capture_diagnostics",captureDiagnostics},
        {"limitations","terrain construction probe; synchronous streaming; accelerated proxy traversal; no population activation or water surface in this probe; no final art or target FPS acceptance"}};
}
inline nlohmann::json runTerrainStreamProbe(wi::Application& app, wi::RenderPath3D& path,
    HWND window, const std::filesystem::path& output, double duration, bool interactive, bool watershedJoin=false) {
    if(watershedJoin) return runTerrainStreamProbeWithSampler(app,path,window,output,duration,interactive,
        world::WatershedTerrain(42,{64,2.0,1.0}),true);
    return runTerrainStreamProbeWithSampler(app,path,window,output,duration,interactive,
        world::TerrainTiles(42,{16,8.0,1.0}),false);
}
}
