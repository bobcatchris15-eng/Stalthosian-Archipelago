// Initial Wicked host for the full draft. Geometry here is diagnostic blockout.
#include "WickedEngine.h"
#include "json.hpp"
#include "domain/foundation.hpp"
#include "domain/persistence.hpp"
#include "domain/reference_landscape.hpp"
#include <windows.h>
#include <shellapi.h>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

using namespace DirectX;
using json = nlohmann::json;
void ImportModel_GLTF(const std::string&, wi::scene::Scene&);
namespace {
wi::Application app;
wi::RenderPath3D renderPath;
constexpr float pi = 3.14159265358979323846f;
constexpr int width = 1920, height = 1080;
stalthos::domain::ReferenceLandscape landscape(42);

LRESULT CALLBACK WindowProc(HWND h, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (message == WM_INPUT) { wi::input::rawinput::ParseMessage((void*)l); return 0; }
    return DefWindowProcW(h, message, w, l);
}
void WriteJson(const std::filesystem::path& file, const json& value) {
    auto temp = file; temp += ".tmp";
    { std::ofstream stream(temp); if (!stream) throw std::runtime_error("cannot write receipt"); stream << value.dump(2); }
    std::filesystem::rename(temp, file);
}
float Ground(float x, float z) {
    return static_cast<float>(landscape.height(x,z));
}
void Color(wi::scene::Scene& scene, wi::ecs::Entity entity, XMFLOAT4 color) {
    auto* object = scene.objects.GetComponent(entity);
    auto* mesh = object ? scene.meshes.GetComponent(object->meshID) : nullptr;
    if (mesh) for (auto& subset : mesh->subsets) {
        if (auto* material = scene.materials.GetComponent(subset.materialID)) {
            material->SetBaseColor(color); material->SetRoughness(.85f);
        }
    }
}
wi::ecs::Entity Box(wi::scene::Scene& scene, const std::string& name, XMFLOAT3 position, XMFLOAT3 size, XMFLOAT4 color) {
    auto entity = scene.Entity_CreateCube(name);
    auto* transform = scene.transforms.GetComponent(entity);
    transform->Scale(size); transform->Translate(position); transform->UpdateTransform();
    Color(scene, entity, color); return entity;
}
void Terrain(wi::scene::Scene& scene) {
    std::vector<XMFLOAT3> vertices;
    std::vector<uint32_t> indices;
    constexpr int count = 97;
    for (int z = 0; z < count; ++z) for (int x = 0; x < count; ++x) {
        float px = (x - 48) * 2.f, pz = (z - 48) * 2.f;
        vertices.push_back({px, Ground(px, pz), pz});
    }
    for (int z = 0; z < count - 1; ++z) for (int x = 0; x < count - 1; ++x) {
        uint32_t a = z * count + x, b = a + 1, c = a + count, d = c + 1;
        // Wicked computes normals with cross(p2-p0, p1-p0).
        indices.insert(indices.end(), {a,b,c,b,d,c});
    }
    auto ground = scene.Entity_CreateMeshFromData("draft reference ground", indices.size(), indices.data(), vertices.size(), vertices.data());
    Color(scene, ground, {1,1,1,1});
    auto* groundObject=scene.objects.GetComponent(ground);
    auto* groundMesh=scene.meshes.GetComponent(groundObject->meshID);
    for (const auto& vertex : vertices) {
        using Habitat=stalthos::domain::Habitat;
        auto habitat=landscape.habitat(vertex.x,vertex.z);
        wi::Color color(83,97,56);
        if (habitat==Habitat::Creek) color=wi::Color(91,81,63);
        else if (habitat==Habitat::Riparian) color=wi::Color(72,93,48);
        else if (habitat==Habitat::Woodland) color=wi::Color(61,80,42);
        else if (habitat==Habitat::Bluff || habitat==Habitat::Outcrop) color=wi::Color(112,104,89);
        groundMesh->vertex_colors.push_back(color.rgba);
    }
    groundMesh->CreateRenderData();
    for (auto& subset : groundMesh->subsets)
        scene.materials.GetComponent(subset.materialID)->SetUseVertexColors(true);
    int rockIndex=0;
    for (const auto& placement : landscape.placements()) {
        using Kind=stalthos::domain::PlacementKind;
        if (placement.kind!=Kind::Rock && placement.kind!=Kind::Boulder) continue;
        float scale=static_cast<float>(placement.scale)*(placement.kind==Kind::Boulder?2.f:1.f);
        Box(scene, "provisional outcrop rock " + std::to_string(rockIndex++),
            {static_cast<float>(placement.x),static_cast<float>(placement.y)+scale*.4f,static_cast<float>(placement.z)},
            {scale,scale*.4f,scale*.8f}, {.35f,.34f,.30f,1});
    }
    vertices.clear(); indices.clear();
    for (int iz=-48; iz<=48; ++iz) {
        double z=iz*2.,left=0,right=0; bool found=false;
        for (double x=-96; x<=96; x+=1) if (landscape.waterContains(x,z)) {
            if (!found) left=x;
            right=x; found=true;
        }
        if (!found) throw std::runtime_error("reference creek has no wetted cross-section");
        double outside=left-1,inside=left;
        for (int iteration=0; iteration<24; ++iteration) {
            double middle=(outside+inside)*.5;
            if (landscape.waterContains(middle,z)) inside=middle; else outside=middle;
        }
        left=inside; inside=right; outside=right+1;
        for (int iteration=0; iteration<24; ++iteration) {
            double middle=(outside+inside)*.5;
            if (landscape.waterContains(middle,z)) inside=middle; else outside=middle;
        }
        right=inside;
        float y=static_cast<float>(landscape.waterSurface(left,z));
        vertices.push_back({static_cast<float>(left),y,static_cast<float>(z)});
        vertices.push_back({static_cast<float>(right),y,static_cast<float>(z)});
        if (iz>-48) {
            uint32_t a=static_cast<uint32_t>(vertices.size()-4);
            indices.insert(indices.end(),{a,a+1,a+2,a+1,a+3,a+2});
        }
    }
    auto water=scene.Entity_CreateMeshFromData("provisional creek water",indices.size(),indices.data(),vertices.size(),vertices.data());
    Color(scene,water,{.08f,.21f,.24f,.8f});
    auto* waterMesh=scene.meshes.GetComponent(scene.objects.GetComponent(water)->meshID);
    for (auto& subset : waterMesh->subsets) {
        auto* material=scene.materials.GetComponent(subset.materialID);
        material->shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
        material->SetRefractionAmount(.05f); material->SetRoughness(.18f);
    }
}
void Camera(float x, float y, float z, int mode) {
    XMVECTOR target = XMVectorSet(x, y + 1.f, z, 1);
    XMVECTOR eye = mode == 1 ? XMVectorSet(x+8,y+4,z,1) : mode == 2 ? XMVectorSet(x,y+4,z+8,1) : XMVectorSet(x,y+4,z-8,1);
    auto view = XMMatrixLookAtLH(eye, target, XMVectorSet(0,1,0,0));
    wi::scene::GetCamera().TransformCamera(XMMatrixInverse(nullptr, view));
    wi::scene::GetCamera().UpdateCamera();
}
}
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR commandLine, int) {
    std::filesystem::path output = "run-evidence";
    std::filesystem::path plant;
    std::filesystem::path loadState, saveState;
    double duration = 15;
    bool interactive = false, diagnosticWalk = false;
    int argumentCount = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    try {
        for (int i = 1; i < argumentCount; ++i) {
            std::wstring key = argv[i];
            if (key == L"--interactive") interactive = true;
            else if (key == L"--diagnostic-walk") diagnosticWalk = true;
            else if (key == L"--output" && i+1 < argumentCount) output = argv[++i];
            else if (key == L"--plant" && i+1 < argumentCount) plant = argv[++i];
            else if (key == L"--load-state" && i+1 < argumentCount) loadState = argv[++i];
            else if (key == L"--save-state" && i+1 < argumentCount) saveState = argv[++i];
            else if (key == L"--duration" && i+1 < argumentCount) duration = std::stod(argv[++i]);
        }
        LocalFree(argv); argv = nullptr;
        if (!std::isfinite(duration) || duration <= 0 || duration > 120) throw std::runtime_error("diagnostic duration must be 0..120 seconds");
        std::filesystem::create_directories(output);
        if (std::filesystem::exists(output / "runtime-receipt.json")) throw std::runtime_error("use a fresh evidence directory");
        stalthos::SimCore clock;
        stalthos::StalthosState world(42);
        float x=12,z=0,yaw=0; int cameraMode=0;
        double loadedCalendar=0, loadedPhysical=0;
        if (!loadState.empty()) {
            auto data = stalthos::persistence::readSave(loadState);
            if (!data.hero) throw std::runtime_error("diagnostic host requires a live hero in the loaded snapshot");
            auto restored = stalthos::persistence::restoreSave(data);
            clock = std::move(restored.first); world = std::move(restored.second);
            loadedCalendar = clock.calendarSeconds();
            loadedPhysical = clock.physicalSeconds();
            if (data.pose) {
                x=static_cast<float>(data.pose->position.x); z=static_cast<float>(data.pose->position.z);
                yaw=static_cast<float>(data.pose->headingRadians); cameraMode=data.pose->cameraMode;
                if (std::abs(x)>60 || std::abs(z)>60 || !std::isfinite(yaw))
                    throw std::runtime_error("saved pose is outside this diagnostic host's playable bounds");
            }
        } else { world.setHero(1, 1); }
        landscape=stalthos::domain::ReferenceLandscape(world.worldSeed());
        wi::arguments::Parse(commandLine);
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        WNDCLASSW cls{}; cls.hInstance = instance; cls.lpfnWndProc = WindowProc;
        cls.lpszClassName = L"StalthosRuntime"; cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
        if (!RegisterClassW(&cls)) throw std::runtime_error("window registration failed");
        auto window = CreateWindowW(cls.lpszClassName, L"The Stalthosian Archipelago — draft construction", WS_POPUP,
            0,0,width,height,nullptr,nullptr,instance,nullptr);
        if (!window) throw std::runtime_error("window creation failed");
        ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window);
        app.SetRenderResolution(width,height); app.swapChain.desc.vsync = false;
        app.setFrameRateLock(false); app.setFrameSkip(false); app.SetWindow(window);
        app.Initialize(); wi::initializer::WaitForInitializationsToFinish();
        auto& scene = wi::scene::GetScene();
        Terrain(scene);
        size_t importedObjects = 0, importedInstances = 0;
        if (!plant.empty()) {
            if (!std::filesystem::exists(plant)) throw std::runtime_error("plant import input does not exist");
            // Upstream importer indexes meshes from zero and converts the whole
            // supplied scene's axes. Import into a fresh scene before merging.
            std::vector<stalthos::domain::Placement> shrubs;
            for (const auto& placement : landscape.placements())
                if (placement.kind==stalthos::domain::PlacementKind::Shrub) shrubs.push_back(placement);
            auto distance=[](const auto& p){return (p.x-12)*(p.x-12)+(p.z-15)*(p.z-15);};
            std::stable_sort(shrubs.begin(),shrubs.end(),[&](const auto& a,const auto& b){return distance(a)<distance(b);});
            if (shrubs.size()>8) shrubs.resize(8);
            for (const auto& placement : shrubs) {
                wi::scene::Scene imported;
                ImportModel_GLTF(plant.u8string(), imported);
                if (imported.objects.GetCount()==0) throw std::runtime_error("plant import produced no objects");
                importedObjects+=imported.objects.GetCount(); ++importedInstances;
                for (size_t i = 0; i < imported.transforms.GetCount(); ++i) {
                    const auto entity = imported.transforms.GetEntity(i);
                    if (imported.hierarchy.GetComponent(entity) == nullptr) {
                        float scale=static_cast<float>(placement.scale);
                        imported.transforms[i].Scale(XMFLOAT3(scale,scale,scale));
                        imported.transforms[i].RotateRollPitchYaw(XMFLOAT3(0,static_cast<float>(placement.headingRadians),0));
                        imported.transforms[i].Translate(XMFLOAT3(static_cast<float>(placement.x),static_cast<float>(placement.y),static_cast<float>(placement.z)));
                        imported.transforms[i].UpdateTransform();
                    }
                }
                scene.Merge(imported);
            }
        }
        auto hero = Box(scene,"hero locomotion marker",{0,Ground(0,0)+.65f,0},{.45f,.4f,1.f},{.48f,.30f,.16f,1});
        scene.weather.SetRealisticSky(true);
        scene.weather.SetRealisticSkyAerialPerspective(false);
        scene.weather.SetRealisticSkyHighQuality(false);
        scene.weather.ambient = {.24f,.24f,.24f}; scene.weather.sunDirection = {-.48f,.8f,-.36f};
        auto sun = scene.Entity_CreateLight("draft sun",{0,30,0},{1,.96f,.9f},5,1000,wi::scene::LightComponent::DIRECTIONAL);
        scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3(.6751f,-.9273f,0));
        renderPath.init(app.canvas); renderPath.Load();
        renderPath.setAO(wi::RenderPath3D::AO_DISABLED); renderPath.setSSREnabled(false);
        renderPath.setSSGIEnabled(false); renderPath.setFSREnabled(false); renderPath.setFSR2Enabled(false);
        renderPath.setEyeAdaptionEnabled(false); renderPath.setMotionBlurEnabled(false); renderPath.setDepthOfFieldEnabled(false);
        renderPath.setShadowsEnabled(true); app.ActivatePath(&renderPath);
        wi::scene::GetCamera().CreatePerspective(width,height,.1f,1000,pi/3);
        stalthos::HeroSim heroIntent;
        uint64_t runCalls=0; bool screenshot=false, userClosed=false;
        const auto started = std::chrono::steady_clock::now(); auto previous = started;
        MSG message{};
        while (true) {
            while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            if (message.message == WM_QUIT) { userClosed=true; break; }
            const auto now = std::chrono::steady_clock::now();
            double elapsed=std::chrono::duration<double>(now-started).count();
            float dt=static_cast<float>(std::min(.1,std::chrono::duration<double>(now-previous).count())); previous=now;
            if (!interactive && elapsed >= duration) break;
            bool focused=GetForegroundWindow()==window;
            auto down=[&](int key){return focused && (GetAsyncKeyState(key)&0x8000)!=0;};
            if(down(VK_ESCAPE)) { userClosed=true; break; }
            if(down(VK_F1)) cameraMode=0; if(down(VK_F2)) cameraMode=1; if(down(VK_F3)) cameraMode=2;
            if(down('A')) yaw-=dt; if(down('D')) yaw+=dt;
            clock.advance(dt);
            bool walking = diagnosticWalk || down('W');
            heroIntent.setDirect(walking || down('S') || down('A') || down('D'));
            float speed=walking?2.f:down('S')?-1.f:0.f;
            auto forward=stalthos::BeastCore::bodyForward(yaw);
            x+=static_cast<float>(forward.x)*speed*dt; z+=static_cast<float>(forward.z)*speed*dt;
            x=std::clamp(x,-60.f,60.f); z=std::clamp(z,-60.f,60.f);
            float y=Ground(x,z);
            auto* transform=scene.transforms.GetComponent(hero); transform->ClearTransform();
            transform->Scale(XMFLOAT3(.45f,.4f,1.f)); transform->RotateRollPitchYaw(XMFLOAT3(0,yaw,0)); transform->Translate(XMFLOAT3(x,y+.65f,z)); transform->UpdateTransform();
            Camera(x,y,z,cameraMode); app.Run(); ++runCalls;
            if(!screenshot && elapsed>=std::min(5.,duration*.5)) screenshot=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/"draft-host.png").u8string());
        }
        if (!saveState.empty()) {
            stalthos::persistence::HostPose pose{{x,Ground(x,z),z},yaw,static_cast<std::uint8_t>(cameraMode)};
            stalthos::persistence::writeSave(saveState, stalthos::persistence::captureSave(clock, world, pose));
        }
        WriteJson(output/"runtime-receipt.json",{{"schema_version",1},{"ok",runCalls>0 && screenshot},{"stage","engine-host feasibility"},
            {"engine_revision","df44c3db4c4927492bc9c791eac715d98d7ed091"},{"process_id",GetCurrentProcessId()},
            {"render_size",{app.canvas.GetPhysicalWidth(),app.canvas.GetPhysicalHeight()}},{"run_calls",runCalls},
            {"screenshot_saved",screenshot},{"plant_import_objects",importedObjects},{"user_closed",userClosed},
            {"plant_import_instances",importedInstances},{"landscape_extent_meters",192},
            {"landscape_candidate_placements",landscape.placements().size()},
            {"calendar_seconds",clock.calendarSeconds()},{"domain_world_seed",world.worldSeed()},
            {"physical_seconds",clock.physicalSeconds()},{"loaded_calendar_seconds",loadedCalendar},{"loaded_physical_seconds",loadedPhysical},
            {"state_loaded",!loadState.empty()},{"state_saved",!saveState.empty()},
            {"diagnostic_walk",diagnosticWalk},
            {"save_scope","foundation inventories, clocks and diagnostic pose; lifecycle/needs not connected"},
            {"pose_vertical_policy","analytical ground projection; no saved vertical body dynamics"},
            {"save_path",saveState.u8string()},{"load_path",loadState.u8string()},
            {"host_pose",{{"x",x},{"z",z},{"heading_radians",yaw},{"camera_mode",cameraMode}}},
            {"frame_time_measured",false},{"full_draft_complete",false},{"visual_accepted",false},
            {"limits",{"diagnostic terrain and box hero; no anatomy/animation/physics acceptance","no wildlife/lifecycle implementation in engine host yet","no external frame-time measurement"}}});
        wi::jobsystem::ShutDown(); DestroyWindow(window);
        return runCalls>0 && screenshot?0:3;
    } catch(const std::exception& error) {
        if(argv) LocalFree(argv);
        try { std::filesystem::create_directories(output); WriteJson(output/"failure.json",{{"ok",false},{"error",error.what()}}); } catch(...) {}
        wi::jobsystem::ShutDown(); return 1;
    }
}
