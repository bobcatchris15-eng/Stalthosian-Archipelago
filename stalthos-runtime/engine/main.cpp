// Initial Wicked host for the full draft. Geometry here is diagnostic blockout.
#include "WickedEngine.h"
#include "json.hpp"
#include "domain/foundation.hpp"
#include "domain/persistence.hpp"
#include "domain/lifecycle.hpp"
#include "domain/needs.hpp"
#include "domain/reference_landscape.hpp"
#include "domain/locomotion_clip.hpp"
#include "domain/locomotion_pace.hpp"
#include "domain/animation_transition.hpp"
#include "domain/resource_ledger.hpp"
#include "domain/resource_presentation.hpp"
#include <cctype>
#include "environment_scene.hpp"
#include "foot_terrain_diagnostic.hpp"
#include "native_sole_adapter.hpp"
#include "shore_scene.hpp"
#include "terrain_surface.hpp"
#include "organic_rocks.hpp"
#include "meadow_scene.hpp"
#include "domain/camera_framing.hpp"
#include "domain/hero_view_controller.hpp"
#include "domain/body_terrain_support.hpp"
#include <windows.h>
#include <shellapi.h>
#include <chrono>
#include <algorithm>
#include <map>
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

struct TransitionPhaseRecord {
    int phase_index = 0;
    std::string phase_name;
    std::string intent;
    double duration_s = 0.0;
    uint64_t frame_count = 0;
    double start_time_s = 0.0;
    double end_time_s = 0.0;
    double start_x = 0.0, start_z = 0.0;
    double end_x = 0.0, end_z = 0.0;
    double displacement_m = 0.0;
    double signed_displacement_m = 0.0;
    double accumulated_distance_m = 0.0;
    std::string active_clip;
    float active_clip_amount = 0.0f;
    std::string inactive_clip;
    float inactive_clip_amount = 0.0f;
    double start_animation_phase_s = 0.0;
    double end_animation_phase_s = 0.0;
    float start_animation_timer_s = 0.0f;
    float end_animation_timer_s = 0.0f;
    float local_bone_delta = 0.0f;
    bool bone_pose_changed = false;
    bool inactive_clip_paused = true;
    bool inactive_clip_amount_zero = true;
    std::string capture_filename;
    bool capture_saved = false;
    std::map<std::pair<wi::ecs::Entity, uint8_t>, XMFLOAT4> baseline_poses;
};

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
stalthos::engine::TerrainSurfaceResult Terrain(wi::scene::Scene& scene) {
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
    return stalthos::engine::applyTerrainSurface(scene,ground,landscape,42);
}
}
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR commandLine, int) {
    std::filesystem::path output = "run-evidence";
    std::filesystem::path plant;
    std::filesystem::path creature;
    double creatureClipDuration = 0.0, creatureStrideLength = 0.0;
    std::filesystem::path loadState, saveState;
    std::optional<uint64_t> successorId;
    double duration = 15;
    double calendarScale = 1.0;
    double creatureSoleOffset = .652438;
    double cameraDistance = 8;
    bool inspectionCamera = false, cameraDistanceSpecified = false;
    bool interactive = false, diagnosticWalk = false, diagnosticReverseWalk = false, diagnosticNeeds = false, diagnosticCamera = false;
    bool diagnosticTransition = false, durationSpecified = false;
    bool disableTerrainSupport = false;
    int argumentCount = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    try {
        for (int i = 1; i < argumentCount; ++i) {
            std::wstring key = argv[i];
            if (key == L"--interactive") interactive = true;
            else if (key == L"--disable-terrain-support") disableTerrainSupport = true;
            else if (key == L"--inspection-camera") inspectionCamera = true;
            else if (key == L"--camera-distance" && i+1 < argumentCount) { cameraDistance=std::stod(argv[++i]); cameraDistanceSpecified=true; }
            else if (key == L"--diagnostic-walk") diagnosticWalk = true;
            else if (key == L"--diagnostic-reverse-walk") diagnosticReverseWalk = true;
            else if (key == L"--diagnostic-needs") diagnosticNeeds = true;
            else if (key == L"--diagnostic-transition" || key == L"--diagnostic-transitions") diagnosticTransition = true;
            else if (key == L"--diagnostic-camera") diagnosticCamera = true;
            else if (key == L"--output" && i+1 < argumentCount) output = argv[++i];
            else if (key == L"--plant" && i+1 < argumentCount) plant = argv[++i];
            else if (key == L"--creature" && i+1 < argumentCount) creature = argv[++i];
            else if (key == L"--walk-duration" && i+1 < argumentCount) creatureClipDuration = std::stod(argv[++i]);
            else if (key == L"--stride-length" && i+1 < argumentCount) creatureStrideLength = std::stod(argv[++i]);
            else if (key == L"--calendar-scale" && i+1 < argumentCount) calendarScale = std::stod(argv[++i]);
            else if (key == L"--creature-sole-offset" && i+1 < argumentCount) creatureSoleOffset = std::stod(argv[++i]);
            else if (key == L"--load-state" && i+1 < argumentCount) loadState = argv[++i];
            else if (key == L"--save-state" && i+1 < argumentCount) saveState = argv[++i];
            else if (key == L"--successor-id" && i+1 < argumentCount) {
                const std::wstring value = argv[++i];
                if (value.empty() || !std::all_of(value.begin(), value.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }))
                    throw std::runtime_error("successor ID must be an unsigned decimal identity");
                std::size_t consumed = 0;
                const auto parsed = std::stoull(value, &consumed, 10);
                if (consumed != value.size() || parsed == 0) throw std::runtime_error("successor ID must be greater than zero");
                successorId = parsed;
            }
            else if (key == L"--duration" && i+1 < argumentCount) { duration = std::stod(argv[++i]); durationSpecified = true; }
        }
        LocalFree(argv); argv = nullptr;
        int diagCount = (diagnosticWalk ? 1 : 0) + (diagnosticReverseWalk ? 1 : 0) +
                        (diagnosticNeeds ? 1 : 0) + (diagnosticTransition ? 1 : 0);
        if (diagCount > 1) {
            throw std::runtime_error("conflicting diagnostic modes specified: only one of --diagnostic-walk, --diagnostic-reverse-walk, --diagnostic-needs, --diagnostic-transition may be active");
        }
        if (diagnosticTransition && !durationSpecified) duration = 10.0;
        if (diagnosticTransition && (!std::isfinite(duration) || duration < 10.0)) throw std::runtime_error("transition probe requires at least ten seconds");
        if(inspectionCamera && !cameraDistanceSpecified) cameraDistance=3;
        if(!std::isfinite(cameraDistance) || cameraDistance<2 || cameraDistance>18) throw std::runtime_error("camera distance must be 2..18 meters");
        if (!std::isfinite(duration) || duration <= 0 || duration > 120) throw std::runtime_error("diagnostic duration must be 0..120 seconds");
        if (!std::isfinite(calendarScale) || calendarScale < 0 || calendarScale > 10000) throw std::runtime_error("calendar scale must be 0..10000");
        if (!std::isfinite(creatureSoleOffset) || creatureSoleOffset < 0 || creatureSoleOffset > 5) throw std::runtime_error("creature sole offset must be 0..5 meters");
        if (!creature.empty() && (!std::isfinite(creatureClipDuration) || creatureClipDuration <= 0 ||
            !std::isfinite(creatureStrideLength) || creatureStrideLength <= 0))
            throw std::runtime_error("animated creature requires finite positive walk duration and stride length metadata");
        std::filesystem::create_directories(output);
        if (std::filesystem::exists(output / "runtime-receipt.json")) throw std::runtime_error("use a fresh evidence directory");
        stalthos::SimCore clock;
        stalthos::StalthosState world(42);
        float x=12,z=0,yaw=0; int cameraMode=0;
        stalthos::domain::HeroViewController viewController;
        double loadedCalendar=0, loadedPhysical=0;
        stalthos::HeroSpecies heroSpecies=stalthos::HeroSpecies::Smalltrotter;
        std::optional<stalthos::Lifecycle> lifecycle;
        std::optional<stalthos::needs::NeedsCore> needs;
        std::optional<stalthos::persistence::HeroNeedsSnapshot> loadedNeeds;
        uint64_t heroId=1; bool lifecycleSnapshotLoaded=false; double loadedLifecycleAge=0; int loadedLifecycleStage=-1;
        uint64_t successorPriorDeadId=0; bool successorDeathTruthPreserved=false;
        bool successorSavedDeathTruthPreserved=false; std::size_t savedDeadHeroCount=0;
        std::optional<stalthos::persistence::SaveData> loadedSaveData;
        if (!loadState.empty()) {
            loadedSaveData = stalthos::persistence::readSave(loadState);
            const auto& data = *loadedSaveData;
            auto restored = stalthos::persistence::restoreSave(data);
            clock = std::move(restored.first); world = std::move(restored.second);
            loadedCalendar = clock.calendarSeconds();
            loadedPhysical = clock.physicalSeconds();
            if (data.pose) {
                x=static_cast<float>(data.pose->position.x); z=static_cast<float>(data.pose->position.z);
                yaw=static_cast<float>(data.pose->headingRadians);
                cameraMode=data.pose->cameraMode;
                // HostPose cameraMode 0/1/2 represents legacy framing angles (0: behind, 1: broadside, 2: frontal).
                // It does NOT indicate LocallyDetached mode. Full camera state (detached anchor, orbit, focus) is not persisted.
                if (std::abs(x)>60 || std::abs(z)>60 || !std::isfinite(yaw))
                    throw std::runtime_error("saved pose is outside this diagnostic host's playable bounds");
            }
            if (data.hero) {
                if (successorId) throw std::runtime_error("successor selection requires a save with no current hero");
                heroId=data.hero->id;
                if (data.heroNeeds && data.heroNeeds->heroId == heroId) {
                    loadedNeeds = data.heroNeeds;
                    if (!data.heroLifecycle) heroSpecies = data.heroNeeds->species;
                }
                lifecycleSnapshotLoaded=data.heroLifecycle.has_value();
                if(data.heroLifecycle) { loadedLifecycleAge=data.heroLifecycle->calendarAge; loadedLifecycleStage=static_cast<int>(data.heroLifecycle->stage); }
                heroSpecies = data.heroLifecycle ? data.heroLifecycle->species : heroSpecies;
                lifecycle=stalthos::persistence::restoreHeroLifecycle(data,heroSpecies);
                heroSpecies=lifecycle->species();
                loadedLifecycleAge=lifecycle->calendarAge();
                loadedLifecycleStage=static_cast<int>(lifecycle->stage());
            } else {
                if (!successorId) throw std::runtime_error("loaded snapshot has no live hero; pass --successor-id explicitly to select a successor");
                const auto snapshot=world.snapshot();
                if(snapshot.deadHeroes.empty()) throw std::runtime_error("successor selection requires recorded hero death truth");
                const auto dead=snapshot.deadHeroes.back();
                if(world.excluded(*successorId)) throw std::runtime_error("successor ID conflicts with an existing live, retained, or dead identity");
                successorPriorDeadId=dead.first;
                heroId=*successorId;
                world.setHero(heroId,dead.second);
                successorDeathTruthPreserved=world.heroDeathKnown(successorPriorDeadId);
                heroSpecies=stalthos::HeroSpecies::Smalltrotter;
                lifecycle.emplace(heroSpecies);
            }
        } else {
            if(successorId) throw std::runtime_error("--successor-id requires --load-state of a save with recorded hero death");
            world.setHero(1, 1);
        }
        if(!lifecycle) lifecycle.emplace(heroSpecies);
        needs.emplace(heroSpecies);
        if (loadedNeeds && loadedNeeds->species == heroSpecies)
            needs->restoreState(loadedNeeds->hunger, loadedNeeds->thirst, loadedNeeds->fatigue, loadedNeeds->injury, loadedNeeds->isResting);
        if (diagnosticNeeds && !loadedNeeds) needs->restoreState(.75, .75, 0, 0, false);
        stalthos::InteractionsCore interactions(256);
        stalthos::ResourceLedger resourceLedger(4096, 256);
        landscape=stalthos::domain::ReferenceLandscape(world.worldSeed());

        if (loadedSaveData && loadedSaveData->resourceLedger.has_value()) {
            stalthos::persistence::restoreResourceLedger(*loadedSaveData, resourceLedger);
        } else {
            resourceLedger.load({{100, stalthos::ResourceKind::Creek, 1000000.0, 0.0},
                                 {200, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {201, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {202, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {203, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {204, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {205, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {206, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {207, stalthos::ResourceKind::Shrub, 2.0, 0.0}});
        }

        uint64_t maxPersistedSessionId = 0;
        if (loadedSaveData && loadedSaveData->resourceLedger.has_value()) {
            for (const auto& s : loadedSaveData->resourceLedger->sessions) {
                if (s.sessionId > maxPersistedSessionId) maxPersistedSessionId = s.sessionId;
            }
        }
        if (maxPersistedSessionId == std::numeric_limits<uint64_t>::max()) {
            throw std::runtime_error("resource ledger session id overflow");
        }
        uint64_t nextInteractionSession = maxPersistedSessionId + 1;

        std::map<uint64_t, stalthos::ResourceDto> cachedResourceDtos;
        for (const auto& res : resourceLedger.snapshot()) {
            cachedResourceDtos[res.id] = res;
        }

        auto getResourceRemaining = [&](uint64_t id) -> double {
            auto it = cachedResourceDtos.find(id);
            if (it != cachedResourceDtos.end()) {
                return it->second.initialQuantity - it->second.consumedQuantity;
            }
            return 0.0;
        };

        auto getResourceFinite = [&]() -> bool {
            for (const auto& kv : cachedResourceDtos) {
                const auto& res = kv.second;
                if (!std::isfinite(res.initialQuantity - res.consumedQuantity) || res.initialQuantity - res.consumedQuantity < 0.0) return false;
            }
            return true;
        };

        // Cache closest shrubs for interaction
        std::vector<stalthos::domain::Placement> cachedShrubs;
        for (const auto& placement : landscape.placements())
            if (placement.kind==stalthos::domain::PlacementKind::Shrub) cachedShrubs.push_back(placement);
        auto distanceToPlant=[](const auto& p, double hx, double hz){return (p.x-hx)*(p.x-hx)+(p.z-hz)*(p.z-hz);};
        std::stable_sort(cachedShrubs.begin(),cachedShrubs.end(),[&](const auto& a,const auto& b){return distanceToPlant(a,12,15)<distanceToPlant(b,12,15);});
        if (cachedShrubs.size()>8) cachedShrubs.resize(8);

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
        const auto terrainSurface=Terrain(scene);
        const auto organicRockCounts=stalthos::engine::populateOrganicRocks(scene,landscape,42);
        const auto meadowCounts=stalthos::engine::populateMeadowScene(scene,landscape,42);
        const auto shoreCounts=stalthos::engine::populateShoreScene(scene,landscape,2.0,1);
        const auto sceneryCounts = stalthos::engine::populateEnvironmentScene(scene, landscape, 1);
        struct ShrubFoliageRef {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            XMFLOAT3 originalScale{1.0f, 1.0f, 1.0f};
            std::string objectName;
            std::string meshName;
        };
        struct ShrubStructureRef {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            std::string objectName;
            std::string meshName;
        };
        struct ShrubInstanceVisualRecord {
            size_t instanceIndex = 0;
            uint64_t resourceId = 0;
            stalthos::domain::Placement placement;
            std::vector<ShrubFoliageRef> foliage;
            std::vector<ShrubStructureRef> structure;
            stalthos::ShrubVisualState visualState{false, true, 1.0, 4};
            double appliedFraction = 1.0;
        };
        std::vector<ShrubInstanceVisualRecord> shrubVisualInstances;
        shrubVisualInstances.reserve(8);

        size_t importedObjects = 0, importedInstances = 0;
        if (!plant.empty()) {
            if (!std::filesystem::exists(plant)) throw std::runtime_error("plant import input does not exist");
            for (size_t shrubIdx = 0; shrubIdx < cachedShrubs.size(); ++shrubIdx) {
                const auto& placement = cachedShrubs[shrubIdx];
                uint64_t shrubResourceId = 200 + shrubIdx;

                wi::scene::Scene imported;
                ImportModel_GLTF(plant.u8string(), imported);
                if (imported.objects.GetCount() == 0) throw std::runtime_error("plant import produced no objects");
                importedObjects += imported.objects.GetCount();
                ++importedInstances;

                ShrubInstanceVisualRecord record;
                record.instanceIndex = shrubIdx;
                record.resourceId = shrubResourceId;
                record.placement = placement;

                auto toLower = [](std::string s) {
                    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    return s;
                };

                for (size_t k = 0; k < imported.objects.GetCount(); ++k) {
                    const auto entity = imported.objects.GetEntity(k);
                    const auto* object = &imported.objects[k];
                    const auto* nameComp = imported.names.GetComponent(entity);
                    const auto* meshNameComp = object ? imported.names.GetComponent(object->meshID) : nullptr;
                    std::string objName = nameComp ? nameComp->name : "";
                    std::string meshName = meshNameComp ? meshNameComp->name : "";

                    std::string lowObj = toLower(objName);
                    std::string lowMesh = toLower(meshName);

                    bool isFoliage = (lowObj.find("foliage") != std::string::npos || lowMesh.find("foliage") != std::string::npos ||
                                      lowObj.find("frond") != std::string::npos || lowMesh.find("frond") != std::string::npos ||
                                      lowObj.find("leaf") != std::string::npos || lowMesh.find("leaf") != std::string::npos);
                    bool isStructure = (lowObj.find("structure") != std::string::npos || lowMesh.find("structure") != std::string::npos ||
                                        lowObj.find("stem") != std::string::npos || lowMesh.find("stem") != std::string::npos ||
                                        lowObj.find("trunk") != std::string::npos || lowMesh.find("trunk") != std::string::npos ||
                                        lowObj.find("wood") != std::string::npos || lowMesh.find("wood") != std::string::npos);

                    float scale = static_cast<float>(placement.scale);
                    if (isFoliage) {
                        record.foliage.push_back({entity, XMFLOAT3(scale, scale, scale), objName, meshName});
                    } else if (isStructure) {
                        record.structure.push_back({entity, objName, meshName});
                    } else {
                        // Unrecognized object is preserved; never guess the first mesh is foliage.
                        record.structure.push_back({entity, objName, meshName});
                    }
                }

                for (size_t i = 0; i < imported.transforms.GetCount(); ++i) {
                    const auto entity = imported.transforms.GetEntity(i);
                    if (imported.hierarchy.GetComponent(entity) == nullptr) {
                        float scale = static_cast<float>(placement.scale);
                        imported.transforms[i].Scale(XMFLOAT3(scale, scale, scale));
                        imported.transforms[i].RotateRollPitchYaw(XMFLOAT3(0, static_cast<float>(placement.headingRadians), 0));
                        imported.transforms[i].Translate(XMFLOAT3(static_cast<float>(placement.x), static_cast<float>(placement.y), static_cast<float>(placement.z)));
                        imported.transforms[i].UpdateTransform();
                    }
                }

                for (auto& ref : record.foliage) {
                    const auto* original = imported.transforms.GetComponent(ref.entity);
                    if (original) ref.originalScale = original->scale_local;
                }
                scene.Merge(imported);
                shrubVisualInstances.push_back(std::move(record));
            }
        }

        auto applyShrubVisual = [&](ShrubInstanceVisualRecord& rec, const stalthos::ResourceDto& dto) {
            rec.visualState = stalthos::ShrubPresentation::evaluate(dto, rec.visualState.valid ? std::make_optional(rec.visualState) : std::nullopt);
            rec.appliedFraction = rec.visualState.foliageFraction;
            float factor = static_cast<float>(rec.visualState.foliageFraction);
            bool visible = rec.visualState.valid && rec.visualState.foliageFraction > 0.0;

            for (const auto& foliageRef : rec.foliage) {
                auto* obj = scene.objects.GetComponent(foliageRef.entity);
                if (obj) {
                    obj->SetRenderable(visible);
                }
                auto* trans = scene.transforms.GetComponent(foliageRef.entity);
                if (trans) {
                    if (visible) {
                        trans->scale_local = XMFLOAT3(
                            foliageRef.originalScale.x * factor,
                            foliageRef.originalScale.y * factor,
                            foliageRef.originalScale.z * factor
                        );
                    } else {
                        trans->scale_local = foliageRef.originalScale;
                    }
                    trans->SetDirty();
                    trans->UpdateTransform();
                }
            }
            for (const auto& structRef : rec.structure) {
                auto* obj = scene.objects.GetComponent(structRef.entity);
                if (obj) {
                    obj->SetRenderable(rec.visualState.woodyStemVisible);
                }
            }
        };

        for (auto& inst : shrubVisualInstances) {
            auto it = cachedResourceDtos.find(inst.resourceId);
            if (it != cachedResourceDtos.end()) {
                applyShrubVisual(inst, it->second);
            }
        }
        bool creatureLoaded=false;
        size_t creatureMeshes=0, creatureSkins=0, creatureAnimations=0, creatureChannels=0;
        size_t removedActorRootTranslationChannels=0;
        std::string actorRootTranslationTargetName;
        double engineClipLength=0.0;
        wi::ecs::Entity heroMotionRoot=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity hero=wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity sceneWalkAnimEntity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity sceneIdleAnimEntity = wi::ecs::INVALID_ENTITY;
        std::string sceneWalkAnimName = "Smalltrotter_WalkCycle";
        std::string sceneIdleAnimName = "Smalltrotter_NeutralIdle";
        if (!creature.empty()) {
            if (!std::filesystem::exists(creature)) throw std::runtime_error("creature import input does not exist");
            wi::scene::Scene imported;
            ImportModel_GLTF(creature.u8string(), imported);
            creatureMeshes=imported.objects.GetCount();
            creatureSkins=imported.armatures.GetCount();
            creatureAnimations=imported.animations.GetCount();
            for(size_t i=0;i<imported.animations.GetCount();++i) {
                auto& animation=imported.animations[i];
                const auto animEntity=imported.animations.GetEntity(i);
                const auto* nameComp=imported.names.GetComponent(animEntity);
                const std::string animName=nameComp ? nameComp->name : "";
                const double importedLength=animation.GetLength();

                bool isWalk=false;
                bool isIdle=false;
                if(imported.animations.GetCount()==1) {
                    isWalk=true;
                } else if(animName.find("Walk")!=std::string::npos || animName.find("walk")!=std::string::npos) {
                    isWalk=true;
                } else if(animName.find("Idle")!=std::string::npos || animName.find("idle")!=std::string::npos) {
                    isIdle=true;
                } else {
                    throw std::runtime_error("unknown creature animation clip: " + animName);
                }

                if(isWalk) {
                    if(!std::isfinite(importedLength) || importedLength<=0 ||
                        std::abs(importedLength-creatureClipDuration)>creatureClipDuration*.05)
                        throw std::runtime_error("walk metadata duration does not match imported animation length within 5 percent");
                    if(engineClipLength==0.0) engineClipLength=importedLength;
                } else if(isIdle) {
                    if(!std::isfinite(importedLength) || importedLength<=0)
                        throw std::runtime_error("idle animation length must be positive and finite");
                }

                for(const auto& channel:animation.channels)
                    if(channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION ||
                       channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION ||
                       channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::SCALE) ++creatureChannels;
                // Find the one animated transform that is an ancestor of every other
                // animated transform. On the aligned source this is creature_rig, the
                // non-skin actor root above spine_00. Remove only its translation channel; all bone TRS
                // channels and the imported root's initial transform remain intact.
                wi::ecs::Entity animatedRoot=wi::ecs::INVALID_ENTITY;
                for(const auto& candidate:animation.channels) {
                    if(candidate.path!=wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION) continue;
                    bool isSkinBone=false;
                    for(size_t armatureIndex=0;armatureIndex<imported.armatures.GetCount() && !isSkinBone;++armatureIndex)
                        for(const auto bone:imported.armatures[armatureIndex].boneCollection)
                            if(bone==candidate.target) { isSkinBone=true; break; }
                    if(isSkinBone) continue;
                    bool containsAll=true;
                    for(const auto& other:animation.channels) {
                        if(other.target!=candidate.target && !imported.Entity_IsDescendant(other.target,candidate.target)) {
                            containsAll=false; break;
                        }
                    }
                    if(containsAll) {
                        if(animatedRoot!=wi::ecs::INVALID_ENTITY) throw std::runtime_error("creature animation has ambiguous actor-root translation channels");
                        animatedRoot=candidate.target;
                    }
                }
                if(isWalk && animatedRoot==wi::ecs::INVALID_ENTITY)
                    throw std::runtime_error("creature walk animation is missing an identifiable actor-root translation channel");
                for(size_t channelIndex=0;channelIndex<animation.channels.size();) {
                    const auto& channel=animation.channels[channelIndex];
                    if(animatedRoot!=wi::ecs::INVALID_ENTITY && channel.target==animatedRoot && channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION) {
                        animation.channels.erase(animation.channels.begin()+channelIndex);
                        if(isWalk) ++removedActorRootTranslationChannels;
                        if(const auto* name=imported.names.GetComponent(animatedRoot)) actorRootTranslationTargetName=name->name;
                    } else ++channelIndex;
                }
            }
            if(creatureMeshes==0 || creatureSkins==0 || creatureAnimations==0 || creatureChannels==0)
                throw std::runtime_error("creature GLB requires mesh, skin and transform animation channels");
            heroMotionRoot=scene.Entity_CreateTransform("hero motion root");
            hero=scene.Instantiate(imported,true);
            if(hero==wi::ecs::INVALID_ENTITY) throw std::runtime_error("creature scene instantiation failed");
            scene.Component_Attach(hero,heroMotionRoot);
            for(size_t i=0;i<scene.animations.GetCount();++i) {
                auto& animation=scene.animations[i];
                animation.SetLooped(true); animation.Stop(); animation.amount=0.0f;
                const auto ent=scene.animations.GetEntity(i);
                const auto* nameComp=scene.names.GetComponent(ent);
                const std::string animName=nameComp ? nameComp->name : "";
                if(scene.animations.GetCount()==1 || animName.find("Walk")!=std::string::npos || animName.find("walk")!=std::string::npos) {
                    sceneWalkAnimEntity=ent;
                    if(!animName.empty()) sceneWalkAnimName = animName;
                } else if(animName.find("Idle")!=std::string::npos || animName.find("idle")!=std::string::npos) {
                    sceneIdleAnimEntity=ent;
                    if(!animName.empty()) sceneIdleAnimName = animName;
                }
            }
            creatureLoaded=true;
        } else {
            hero=Box(scene,"hero locomotion marker",{0,Ground(0,0)+.65f,0},{.45f,.4f,1.f},{.48f,.30f,.16f,1});
            heroMotionRoot=hero;
        }
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
        uint64_t runCalls=0; bool screenshot=false, userClosed=false, earlyScreenshot=false, lateScreenshot=false;
        std::vector<std::string> lifecycleTransitions;
        std::map<std::pair<wi::ecs::Entity, uint8_t>, XMFLOAT4> channelPoseBaseline; float localBoneDelta=0, minimumAnimationTimer=std::numeric_limits<float>::infinity(), maximumAnimationTimer=0; uint64_t animationPlayingFrames=0;
        std::optional<stalthos::domain::LocomotionClip> creatureClip;
        if(creatureLoaded) creatureClip.emplace(creatureClipDuration,creatureStrideLength);
        double creaturePhase=0, creatureDistanceAccumulated=0, lastSignedBodyDistance=0, lastExpectedPhase=0;
        std::vector<TransitionPhaseRecord> transitionPhases;
        int activeTransitionPhaseIndex = -1;
        bool inactiveClipWritesPrevented = true;
        bool walkPhaseFollowsSignedDistance = true;
        bool idleResetToNeutralVerified = true;
        bool previousIsDisplaced = false;
        bool transitionFinalCaptureSaved = false;
        double finalElapsed = 0.0;

        if (diagnosticTransition && (!creatureLoaded || sceneWalkAnimEntity == wi::ecs::INVALID_ENTITY || sceneIdleAnimEntity == wi::ecs::INVALID_ENTITY)) throw std::runtime_error("transition probe requires a creature with separate walk and idle clips");

        std::optional<stalthos::engine::FourFootDiagnosticReport> latestFootDiagnostic;
        stalthos::engine::NativeSoleAdapter nativeSoleAdapter;
        std::optional<stalthos::engine::NativeSoleReport> latestNativeSoleDiagnostic;
        std::array<wi::ecs::Entity, stalthos::engine::kFootCount> footLandmarks{};
        footLandmarks.fill(wi::ecs::INVALID_ENTITY);
        if (creatureLoaded) {
            for (size_t i = 0; i < footLandmarks.size(); ++i)
                footLandmarks[i] = scene.Entity_FindByName(stalthos::engine::kWickedFootLandmarks[i].boneName, heroMotionRoot);
            nativeSoleAdapter.initialize(scene, heroMotionRoot);
        }

        bool previousInspectionKey = false, previousToggleKey = false;
        uint64_t tetherTerrainConflictCount = 0;
        int diagnosticCameraStage = 0;
        uint64_t detachedCameraFrames = 0;
        double detachedBodyDistance = 0;
        {
            auto state = viewController.getState();
            if (inspectionCamera) {
                state.orbit = stalthos::domain::CameraFraming::closeInspectionPreset(yaw);
                state.orbit.distance = cameraDistance;
            } else {
                using Framing = stalthos::domain::CameraFraming;
                state.orbit = Framing::followPreset(cameraMode==1 ? Framing::kPi*.5 : cameraMode==2 ? 0 : Framing::kPi);
                if (cameraDistanceSpecified) state.orbit.distance = cameraDistance;
            }
            if (loadedSaveData && loadedSaveData->cameraState) {
                const auto& savedCamera = *loadedSaveData->cameraState;
                state.mode = static_cast<stalthos::domain::CameraMode>(savedCamera.mode);
                state.focus = static_cast<stalthos::domain::InputFocus>(savedCamera.focus);
                state.detachedAnchor = savedCamera.detachedAnchor;
                state.orbit.azimuth = savedCamera.orbitAzimuth;
                state.orbit.elevation = savedCamera.orbitElevation;
                state.orbit.distance = savedCamera.orbitDistance;
                cameraDistance = savedCamera.orbitDistance;
            }
            viewController.setState(state);
        }
        auto terrainSampler = [&](double cx, double cz) {
            if (!landscape.inBounds(cx, cz)) throw std::runtime_error("camera outside reference landscape");
            return landscape.height(cx, cz);
        };
        stalthos::domain::ControllerOutput cameraOut;
        cameraOut.cameraState = stalthos::domain::CameraFraming::computeFraming(
            {static_cast<double>(x), static_cast<double>(Ground(x, z)), static_cast<double>(z)},
            yaw,
            viewController.getState().orbit,
            stalthos::domain::CameraFraming::kDefaultTargetHeight,
            terrainSampler
        );
        cameraOut.currentFocus = viewController.getState().focus;
        cameraOut.currentMode = viewController.getState().mode;
        cameraOut.activeTravelDestination = std::nullopt;

        bool initialNeedsRecorded = false;
        double initialHunger = 0, initialThirst = 0, initialFatigue = 0;
        double initialCreekResource = getResourceRemaining(100), initialShrub0Resource = getResourceRemaining(200);

        bool eligibilityWater = false;
        bool eligibilityShrub = false;

        // Sequence state for diagnostic-needs
        int diagnosticSequenceStage = 0;
        double diagnosticTimer = 0.0;

        uint64_t physicalInteractionSession = 0;
        uint64_t resourceLedgerCapExhaustions = 0;
        bool previousInteractDown = false;
        uint64_t appliedDrinkContacts = 0, appliedBrowseContacts = 0;
        uint64_t waterEligibleFrames = 0, shrubEligibleFrames = 0;
        bool waterContactApplied = false, shrubContactApplied = false;

        constexpr double kCreatureLongitudinalHalfspan = 0.5;
        constexpr double kCreatureLateralHalfspan = 0.25;
        uint64_t supportFailureCount = 0;
        std::string latestSupportMode = disableTerrainSupport ? "disabled" : "terrain_supported";
        double latestSlopeRadians = 0.0;
        stalthos::domain::Vector3 latestNormal{0.0, 1.0, 0.0};

        const auto started = std::chrono::steady_clock::now(); auto previous = started;
        MSG message{};
        while (true) {
            while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            if (message.message == WM_QUIT) { userClosed=true; break; }
            const auto now = std::chrono::steady_clock::now();
            double elapsed=std::chrono::duration<double>(now-started).count(); finalElapsed = elapsed;
            float dt=static_cast<float>(std::min(.1,std::chrono::duration<double>(now-previous).count())); previous=now;
            if (!interactive && elapsed >= duration) break;
            bool focused=GetForegroundWindow()==window;
            auto down=[&](int key){return interactive && focused && (GetAsyncKeyState(key)&0x8000)!=0;};
            if(down(VK_ESCAPE)) { userClosed=true; break; }
            bool heroAlive=lifecycle->stage()!=stalthos::LifeStage::Dead;

            const auto preFrameState = viewController.getState();
            bool toggleKey = down(VK_F1);
            if (toggleKey && !previousToggleKey) {
                viewController.toggleDetachedMode({static_cast<double>(x), static_cast<double>(Ground(x, z)), static_cast<double>(z)});
            }
            previousToggleKey = toggleKey;
            if (diagnosticCamera && elapsed >= 2 && diagnosticCameraStage == 0) {
                viewController.applyOrbitInput(100, -20);
                viewController.applyZoomInput(2);
                diagnosticCameraStage = 1;
            }
            if (diagnosticCamera && elapsed >= 4 && diagnosticCameraStage == 1) {
                viewController.toggleDetachedMode({x, Ground(x,z), z});
                diagnosticCameraStage = 2;
            }
            if (diagnosticCamera && diagnosticCameraStage == 2) {
                viewController.applyDetachedPan(1, 0, dt, {x, Ground(x,z), z});
                if (elapsed >= 8) {
                    viewController.toggleDetachedMode({x, Ground(x,z), z});
                    diagnosticCameraStage = 3;
                }
            }

            bool userSteering = viewController.getState().focus == stalthos::domain::InputFocus::BodySteering;
            if (userSteering) {
                if(heroAlive && down('A')) yaw-=dt; if(heroAlive && down('D')) yaw+=dt;
            }
            const auto timeStep=clock.advance(dt,calendarScale);
            for(const auto transition:lifecycle->advance(timeStep)) {
                const char* names[]={"hatchling","juvenile","adult","old","dead"};
                lifecycleTransitions.push_back(names[static_cast<unsigned>(transition)]);
                if(transition==stalthos::LifeStage::Dead) { world.heroDied(); for(size_t i=0;i<scene.animations.GetCount();++i) scene.animations[i].Pause(); }
            }
            heroAlive=lifecycle->stage()!=stalthos::LifeStage::Dead;
            if (diagnosticTransition) {
                int targetPhaseIndex = std::clamp(static_cast<int>(elapsed / 2.0), 0, 4);
                if (targetPhaseIndex != activeTransitionPhaseIndex) {
                    if (activeTransitionPhaseIndex >= 0 && activeTransitionPhaseIndex < static_cast<int>(transitionPhases.size())) {
                        auto& prev = transitionPhases[activeTransitionPhaseIndex];
                        prev.end_time_s = elapsed;
                        prev.duration_s = prev.end_time_s - prev.start_time_s;
                        prev.end_x = x; prev.end_z = z;
                        prev.displacement_m = std::hypot(prev.end_x - prev.start_x, prev.end_z - prev.start_z);
                        prev.signed_displacement_m = (prev.phase_index == 1) ? prev.displacement_m : (prev.phase_index == 3) ? -prev.displacement_m : 0.0;
                        prev.end_animation_phase_s = creaturePhase;
                        prev.bone_pose_changed = (prev.local_bone_delta > 1e-5f);
                    }
                    TransitionPhaseRecord rec;
                    rec.phase_index = targetPhaseIndex;
                    rec.phase_name = (targetPhaseIndex == 0) ? "idle_1" :
                                     (targetPhaseIndex == 1) ? "forward_walk" :
                                     (targetPhaseIndex == 2) ? "idle_2" :
                                     (targetPhaseIndex == 3) ? "reverse_walk" : "idle_3";
                    rec.intent = (targetPhaseIndex == 1) ? "forward" :
                                 (targetPhaseIndex == 3) ? "reverse" : "idle";
                    rec.start_time_s = elapsed;
                    rec.start_x = x; rec.start_z = z;
                    rec.start_animation_phase_s = creaturePhase;
                    rec.active_clip = (targetPhaseIndex == 1 || targetPhaseIndex == 3) ? sceneWalkAnimName : sceneIdleAnimName;
                    rec.inactive_clip = (targetPhaseIndex == 1 || targetPhaseIndex == 3) ? sceneIdleAnimName : sceneWalkAnimName;
                    rec.capture_filename = (targetPhaseIndex == 0) ? "transition-0-initial-idle.png" :
                                           (targetPhaseIndex == 1) ? "transition-1-idle-to-forward.png" :
                                           (targetPhaseIndex == 2) ? "transition-2-forward-to-idle.png" :
                                           (targetPhaseIndex == 3) ? "transition-3-idle-to-reverse.png" :
                                                                     "transition-4-reverse-to-idle.png";
                    transitionPhases.push_back(rec);
                    activeTransitionPhaseIndex = targetPhaseIndex;
                }
            }
            bool userWalk = userSteering && down('W');
            bool userReverse = userSteering && down('S');
            bool walking = diagnosticWalk || diagnosticReverseWalk || (diagnosticTransition && (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3)) || userWalk;
            if (diagnosticNeeds && heroAlive && (diagnosticSequenceStage == 0 || diagnosticSequenceStage == 2)) {
                walking = true;
                double targetX = x;
                double targetZ = z;
                if (diagnosticSequenceStage == 0 && !cachedShrubs.empty()) {
                    targetX = cachedShrubs.front().x;
                    targetZ = cachedShrubs.front().z;
                } else {
                    targetX = 8.0 * std::sin(static_cast<double>(z) / 43.0) + 2.0 * std::sin(static_cast<double>(z) / 17.0);
                    targetZ = static_cast<double>(z) + 4.0;
                }
                const double targetYaw = std::atan2(targetX - x, targetZ - z);
                double diff = targetYaw - yaw;
                while (diff > pi) diff -= 2 * pi;
                while (diff < -pi) diff += 2 * pi;
                yaw += static_cast<float>(diff * std::min(1.0, 5.0 * dt));
            }
            bool userSteerDirect = userSteering && (down('S') || down('A') || down('D') || down('W'));
            heroIntent.setDirect(walking || userSteerDirect);
            float speed = 0.f;
            const float previousX=x, previousZ=z;

            const bool forwardIntent = diagnosticWalk || (diagnosticNeeds && (diagnosticSequenceStage == 0 || diagnosticSequenceStage == 2)) || (diagnosticTransition && activeTransitionPhaseIndex == 1) || userWalk;
            const bool reverseIntent = diagnosticReverseWalk || (diagnosticTransition && activeTransitionPhaseIndex == 3) || (!forwardIntent && userReverse);
            if (heroAlive && creatureLoaded) {
                using Pace = stalthos::domain::LocomotionPace;
                speed = static_cast<float>(Pace::calculate(creatureStrideLength, creatureClipDuration,
                    reverseIntent ? Pace::Intent::Reverse : forwardIntent ? Pace::Intent::Forward : Pace::Intent::Idle).signedSpeed);
            } else if (heroAlive) speed = reverseIntent ? -2.f : forwardIntent ? 2.f : 0.f;
            if (speed != 0.f) {
                auto forward=stalthos::BeastCore::bodyForward(yaw);
                x+=static_cast<float>(forward.x)*speed*dt; z+=static_cast<float>(forward.z)*speed*dt;
                x=std::clamp(x,-60.f,60.f); z=std::clamp(z,-60.f,60.f);
            }
            if (viewController.getState().focus == stalthos::domain::InputFocus::FreeCamera) {
                double panX = 0, panZ = 0;
                if (down('W')) panZ -= 1.0;
                if (down('S')) panZ += 1.0;
                if (down('A')) panX -= 1.0;
                if (down('D')) panX += 1.0;
                viewController.applyDetachedPan(panX, panZ, dt, {static_cast<double>(x), static_cast<double>(Ground(x, z)), static_cast<double>(z)});
            }
            const double actualDistance=std::hypot(static_cast<double>(x-previousX),static_cast<double>(z-previousZ));
            if (viewController.getState().mode == stalthos::domain::CameraMode::LocallyDetached) {
                ++detachedCameraFrames;
                detachedBodyDistance += actualDistance;
            }
            lastSignedBodyDistance=speed<0 ? -actualDistance : actualDistance;
            if(creatureClip) {
                const auto advance=creatureClip->advance(creaturePhase,lastSignedBodyDistance);
                creaturePhase=advance.phase;
                creatureDistanceAccumulated+=lastSignedBodyDistance;
                lastExpectedPhase=creatureClip->advance(0.0,std::fmod(creatureDistanceAccumulated,creatureStrideLength)).phase;
            }
            float y=Ground(x,z);
            stalthos::domain::TerrainSupport support;
            auto groundSampler = [&](double sx, double sz) {
                return static_cast<double>(landscape.height(sx, sz));
            };

            bool supportCalculated = false;
            try {
                support = stalthos::domain::calculateTerrainSupport(
                    static_cast<double>(x), static_cast<double>(z), static_cast<double>(yaw),
                    kCreatureLongitudinalHalfspan, kCreatureLateralHalfspan, groundSampler);
                supportCalculated = true;
                latestSlopeRadians = support.slopeAngleRadians;
                latestNormal = support.surfaceNormal;
            } catch (...) {
                supportCalculated = false;
                latestSlopeRadians = 0.0;
                latestNormal = {0.0, 1.0, 0.0};
            }

            if (disableTerrainSupport) {
                latestSupportMode = "disabled";
            } else if (!supportCalculated || !support.supported) {
                latestSupportMode = "unsupported_yaw_fallback";
                ++supportFailureCount;
            } else {
                latestSupportMode = "terrain_supported";
            }

            auto* transform=scene.transforms.GetComponent(heroMotionRoot); transform->ClearTransform();
            if(!creatureLoaded) {
                transform->Scale(XMFLOAT3(.45f,.4f,1.f));
                if (latestSupportMode == "terrain_supported") {
                    XMMATRIX supportMatrix = XMMATRIX(
                        static_cast<float>(support.right.x),   static_cast<float>(support.right.y),   static_cast<float>(support.right.z),   0.0f,
                        static_cast<float>(support.up.x),      static_cast<float>(support.up.y),      static_cast<float>(support.up.z),      0.0f,
                        static_cast<float>(support.forward.x), static_cast<float>(support.forward.y), static_cast<float>(support.forward.z), 0.0f,
                        0.0f,                                  0.0f,                                  0.0f,                                  1.0f
                    );
                    XMVECTOR quat = XMQuaternionNormalize(XMQuaternionRotationMatrix(supportMatrix));
                    transform->Rotate(quat);
                } else {
                    transform->RotateRollPitchYaw(XMFLOAT3(0,yaw,0));
                }
                transform->Translate(XMFLOAT3(x,y+.65f,z));
            } else {
                // Blender's authored +X longitudinal axis is turned to the host's +Z forward axis at the motion parent.
                // In DirectX row-vector basis, composite rotation is assetOffsetMatrix * supportMatrix.
                // If terrain support is disabled or unsupported (>20 deg slope), falls back to flat yaw (yaw - pi * 0.5f).
                // The provisional sole offset comes from the walk-cycle's measured rear-foot source bound; tail clearance remains unaccepted.
                if (latestSupportMode == "terrain_supported") {
                    XMMATRIX assetOffsetMatrix = XMMatrixRotationY(-pi * 0.5f);
                    XMMATRIX supportMatrix = XMMATRIX(
                        static_cast<float>(support.right.x),   static_cast<float>(support.right.y),   static_cast<float>(support.right.z),   0.0f,
                        static_cast<float>(support.up.x),      static_cast<float>(support.up.y),      static_cast<float>(support.up.z),      0.0f,
                        static_cast<float>(support.forward.x), static_cast<float>(support.forward.y), static_cast<float>(support.forward.z), 0.0f,
                        0.0f,                                  0.0f,                                  0.0f,                                  1.0f
                    );
                    XMMATRIX compositeMatrix = assetOffsetMatrix * supportMatrix;
                    XMVECTOR quat = XMQuaternionNormalize(XMQuaternionRotationMatrix(compositeMatrix));
                    transform->Rotate(quat);
                } else {
                    transform->RotateRollPitchYaw(XMFLOAT3(0,yaw-pi*.5f,0));
                }
                transform->Translate(XMFLOAT3(x,y-static_cast<float>(creatureSoleOffset),z));
            }
            transform->UpdateTransform();
            if (creatureLoaded) {
                auto* walkAnim = sceneWalkAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneWalkAnimEntity) : nullptr;
                auto* idleAnim = sceneIdleAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneIdleAnimEntity) : nullptr;
                const bool isDisplaced = (actualDistance > 1e-5);

                if (isDisplaced || idleAnim == nullptr) {
                    if (walkAnim) {
                        walkAnim->amount = 1.0f;
                        if (creatureClip) {
                            walkAnim->Pause();
                            walkAnim->timer = static_cast<float>(walkAnim->start + creaturePhase);
                        } else if (speed != 0 && heroAlive) {
                            walkAnim->Play();
                        } else {
                            walkAnim->Pause();
                        }
                    }
                    if (idleAnim) {
                        idleAnim->amount = 0.0f;
                        idleAnim->last_update_time = idleAnim->timer;
                        idleAnim->Pause();
                    }
                } else {
                    if (walkAnim) {
                        walkAnim->amount = 0.0f;
                        walkAnim->last_update_time = walkAnim->timer;
                        walkAnim->Pause();
                    }
                    if (idleAnim) {
                        idleAnim->amount = 1.0f;
                        if (previousIsDisplaced) {
                            idleAnim->timer = static_cast<float>(idleAnim->start);
                            idleAnim->last_update_time = idleAnim->timer;
                        }
                        if (heroAlive) {
                            idleAnim->Play();
                        } else {
                            idleAnim->Pause();
                        }
                    }
                }
                if (isDisplaced) {
                    if (idleAnim && (idleAnim->amount > 0.0f || idleAnim->IsPlaying() || idleAnim->last_update_time != idleAnim->timer)) {
                        inactiveClipWritesPrevented = false;
                    }
                } else {
                    if (walkAnim && (walkAnim->amount > 0.0f || walkAnim->IsPlaying() || walkAnim->last_update_time != walkAnim->timer)) {
                        inactiveClipWritesPrevented = false;
                    }
                }
                previousIsDisplaced = isDisplaced;
            } else {
                for(size_t i=0;i<scene.animations.GetCount();++i) {
                    auto& animation=scene.animations[i];
                    if(speed!=0 && heroAlive) animation.Play(); else animation.Pause();
                }
            }
            if(interactive && focused) {
                const bool inspectionKey=down('V');
                if(inspectionKey && !previousInspectionKey) {
                    inspectionCamera=!inspectionCamera;
                    cameraDistance=inspectionCamera?3:8;
                    auto state = viewController.getState();
                    if (inspectionCamera) {
                        state.orbit = stalthos::domain::CameraFraming::closeInspectionPreset(yaw);
                    } else {
                        using Framing = stalthos::domain::CameraFraming;
                        state.orbit = Framing::followPreset(cameraMode==1 ? Framing::kPi*.5 : cameraMode==2 ? 0 : Framing::kPi);
                    }
                    state.orbit.distance = cameraDistance;
                    viewController.setState(state);
                }
                previousInspectionKey=inspectionKey;

                auto mouseState = wi::input::GetMouseState();
                if (mouseState.delta_wheel != 0) {
                    viewController.applyZoomInput(mouseState.delta_wheel);
                    cameraDistance = viewController.getState().orbit.distance;
                }
                if (mouseState.left_button_press) {
                    viewController.applyOrbitInput(-mouseState.delta_position.x, mouseState.delta_position.y);
                }
            }

            try {
                auto terrainSampler = [&](double cx, double cz) {
                    if (!landscape.inBounds(cx, cz)) throw std::runtime_error("camera outside reference landscape");
                    return landscape.height(cx, cz);
                };
                stalthos::TimeStep viewDt{dt, dt * calendarScale};
                cameraOut = viewController.update(viewDt, {static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}, yaw, terrainSampler);
            } catch (const stalthos::domain::TetherTerrainConflictException&) {
                ++tetherTerrainConflictCount;
                viewController.setState(preFrameState);
                // cameraOut preserves previous valid camera output
            }

            XMVECTOR target = XMVectorSet(static_cast<float>(cameraOut.cameraState.target.x), static_cast<float>(cameraOut.cameraState.target.y), static_cast<float>(cameraOut.cameraState.target.z), 1);
            XMVECTOR eye = XMVectorSet(static_cast<float>(cameraOut.cameraState.eye.x), static_cast<float>(cameraOut.cameraState.eye.y), static_cast<float>(cameraOut.cameraState.eye.z), 1);
            auto view = XMMatrixLookAtLH(eye, target, XMVectorSet(0, 1, 0, 0));
            wi::scene::GetCamera().TransformCamera(XMMatrixInverse(nullptr, view));
            wi::scene::GetCamera().UpdateCamera();

            app.Run(); ++runCalls;
            if (creatureLoaded) {
                if (nativeSoleAdapter.isInitialized()) {
                    latestNativeSoleDiagnostic = nativeSoleAdapter.evaluate(scene, landscape);
                } else {
                    latestNativeSoleDiagnostic.reset();
                }
                std::array<stalthos::engine::FootBoneProxyInput, stalthos::engine::kFootCount> inputs{};
                bool resolved = true;
                for (size_t i = 0; i < inputs.size(); ++i) {
                    const auto* bone = scene.transforms.GetComponent(footLandmarks[i]);
                    if (!bone) { resolved = false; break; }
                    const auto pos = bone->GetPosition();
                    const auto& spec = stalthos::engine::kWickedFootLandmarks[i];
                    inputs[i] = {spec.foot, {pos.x, pos.y, pos.z}, spec.defaultSoleThicknessMeters};
                }
                if (resolved) latestFootDiagnostic = stalthos::engine::evaluateFourFeetOnLandscapeFromBoneProxies(inputs, 0.02, landscape);
                else latestFootDiagnostic.reset();
            }
            if (diagnosticTransition && activeTransitionPhaseIndex >= 0 && activeTransitionPhaseIndex < static_cast<int>(transitionPhases.size())) {
                auto& cur = transitionPhases[activeTransitionPhaseIndex];
                if (!cur.capture_saved) {
                    cur.capture_saved = wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(), (output / cur.capture_filename).u8string());
                }
                if (!transitionFinalCaptureSaved && elapsed >= 9.9) {
                    transitionFinalCaptureSaved = wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(), (output / "transition-5-final-idle.png").u8string());
                }
                cur.frame_count++;
                cur.accumulated_distance_m += actualDistance;
                auto* walkAnim = sceneWalkAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneWalkAnimEntity) : nullptr;
                auto* idleAnim = sceneIdleAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneIdleAnimEntity) : nullptr;
                cur.active_clip_amount = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? (walkAnim ? walkAnim->amount : 0.0f) : (idleAnim ? idleAnim->amount : 0.0f);
                cur.inactive_clip_amount = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? (idleAnim ? idleAnim->amount : 0.0f) : (walkAnim ? walkAnim->amount : 0.0f);
                auto* inactiveAnim = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? idleAnim : walkAnim;
                cur.inactive_clip_paused = cur.inactive_clip_paused && inactiveAnim && !inactiveAnim->IsPlaying();
                cur.inactive_clip_amount_zero = cur.inactive_clip_amount_zero && inactiveAnim && inactiveAnim->amount == 0.0f;

                if (cur.frame_count == 1) {
                    cur.start_animation_timer_s = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? (walkAnim ? walkAnim->timer : 0.0f) : (idleAnim ? idleAnim->timer : 0.0f);
                }
                cur.end_animation_timer_s = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? (walkAnim ? walkAnim->timer : 0.0f) : (idleAnim ? idleAnim->timer : 0.0f);
                if (creatureLoaded) {
                    for (size_t ai = 0; ai < scene.animations.GetCount(); ++ai) {
                        auto& animation = scene.animations[ai];
                        for (const auto& channel : animation.channels) {
                            const auto* bone = scene.transforms.GetComponent(channel.target);
                            if (!bone) continue;
                            uint8_t path = static_cast<uint8_t>(channel.path);
                            XMFLOAT4 pose;
                            if (channel.path == wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION) pose = bone->rotation_local;
                            else if (channel.path == wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION) pose = {bone->translation_local.x, bone->translation_local.y, bone->translation_local.z, 0};
                            else if (channel.path == wi::scene::AnimationComponent::AnimationChannel::Path::SCALE) pose = {bone->scale_local.x, bone->scale_local.y, bone->scale_local.z, 0};
                            else continue;
                            auto key = std::make_pair(channel.target, path);
                            auto it = cur.baseline_poses.find(key);
                            if (it == cur.baseline_poses.end()) {
                                cur.baseline_poses.emplace(key, pose);
                            } else {
                                cur.local_bone_delta = std::max({cur.local_bone_delta,
                                    std::abs(pose.x - it->second.x),
                                    std::abs(pose.y - it->second.y),
                                    std::abs(pose.z - it->second.z),
                                    std::abs(pose.w - it->second.w)});
                            }
                        }
                    }
                }
            }
            if(creatureLoaded) for(size_t ai=0;ai<scene.animations.GetCount();++ai) { auto& animation=scene.animations[ai]; minimumAnimationTimer=std::min(minimumAnimationTimer,animation.timer); maximumAnimationTimer=std::max(maximumAnimationTimer,animation.timer); if(animation.IsPlaying()) ++animationPlayingFrames; for(const auto& channel:animation.channels) { const auto* bone=scene.transforms.GetComponent(channel.target); if(!bone) continue; uint8_t path=static_cast<uint8_t>(channel.path); XMFLOAT4 pose; if(channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION) pose=bone->rotation_local; else if(channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION) pose={bone->translation_local.x,bone->translation_local.y,bone->translation_local.z,0}; else if(channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::SCALE) pose={bone->scale_local.x,bone->scale_local.y,bone->scale_local.z,0}; else continue; auto key=std::make_pair(channel.target,path); auto it=channelPoseBaseline.find(key); if(it==channelPoseBaseline.end()) channelPoseBaseline.emplace(key,pose); else localBoneDelta=std::max({localBoneDelta,std::abs(pose.x-it->second.x),std::abs(pose.y-it->second.y),std::abs(pose.z-it->second.z),std::abs(pose.w-it->second.w)}); } }
            if(!initialNeedsRecorded) {
                initialNeedsRecorded = true;
                initialHunger = needs->hunger();
                initialThirst = needs->thirst();
                initialFatigue = needs->fatigue();
                initialCreekResource = getResourceRemaining(100);
                initialShrub0Resource = getResourceRemaining(200);
            }

            // Needs stop advancing after the terminal hero transition.
            if (heroAlive) needs->advance(timeStep);

            // Retrieve body-relative physical position for eligibility, independent of camera
            XMFLOAT3 bodyPos;
            if (auto* transform = scene.transforms.GetComponent(heroMotionRoot)) {
                bodyPos = transform->GetPosition();
            } else {
                // If hero motion root is missing, the hero cannot physically interact.
                // explicitly avoid falling back to camera location (x, y, z).
                bodyPos = XMFLOAT3(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
            }

            // Interaction eligibility (plausible body-relative reach)
            // Water: hero is within 3.0 meters of creek center and low enough
            const bool bodyInLandscape = std::isfinite(bodyPos.x) && std::isfinite(bodyPos.y) && std::isfinite(bodyPos.z) && landscape.inBounds(bodyPos.x, bodyPos.z);
            const double creekCenterX = bodyInLandscape ? 8.0 * std::sin(static_cast<double>(bodyPos.z) / 43.0) + 2.0 * std::sin(static_cast<double>(bodyPos.z) / 17.0) : 0.0;
            const double surfaceY = bodyInLandscape ? landscape.waterSurface(bodyPos.x, bodyPos.z) : 0.0;
            eligibilityWater = heroAlive && bodyInLandscape && std::abs(static_cast<double>(bodyPos.x) - creekCenterX) < 3.0 &&
                landscape.waterContains(bodyPos.x, bodyPos.z) && static_cast<double>(bodyPos.y) - surfaceY < 1.0;
            if (eligibilityWater) ++waterEligibleFrames;

            // Shrub: hero is near one of the first 8 shrub placements
            eligibilityShrub = false;
            int nearbyShrubIndex = -1;
            for (size_t i = 0; !plant.empty() && importedInstances > 0 && i < cachedShrubs.size(); i++) {
                if (heroAlive && bodyInLandscape && std::abs(cachedShrubs[i].y - bodyPos.y) <= 1.5 && distanceToPlant(cachedShrubs[i], bodyPos.x, bodyPos.z) < 4.0) { // 2m horizontal reach and bounded vertical reach
                    eligibilityShrub = true;
                    nearbyShrubIndex = static_cast<int>(i);
                    break;
                }
            }
            if (eligibilityShrub) ++shrubEligibleFrames;

            bool tryInteract = false;

            if (diagnosticNeeds && heroAlive) {
                diagnosticTimer += dt;
                if (diagnosticSequenceStage == 0) {
                    if (eligibilityShrub) {
                        tryInteract = true;
                        diagnosticSequenceStage = 1;
                        diagnosticTimer = 0;
                    }
                } else if (diagnosticSequenceStage == 1) {
                    if (diagnosticTimer > 1.0) {
                        diagnosticSequenceStage = 2;
                    }
                } else if (diagnosticSequenceStage == 2) {
                    if (eligibilityWater) {
                        tryInteract = true;
                        diagnosticSequenceStage = 3;
                    }
                }
            }

            bool interactDown = down('E') || tryInteract;
            if (heroAlive && interactDown) {
                // Ensure idempotence: holding E produces one session. Re-pressing increments session.
                if (!previousInteractDown) {
                    if (nextInteractionSession == 0) throw std::runtime_error("interaction session ID overflow");
                    physicalInteractionSession = nextInteractionSession++;
                    interactions.beginSession(physicalInteractionSession);
                }

                if (eligibilityWater) {
                    double drinkAmount = 0.5;
                    double available = getResourceRemaining(100);
                    double actualDrink = std::min(drinkAmount, available);
                    if (actualDrink > 0) {
                        try {
                            if (resourceLedger.consume(physicalInteractionSession, 100, actualDrink)) {
                                for (const auto& res : resourceLedger.snapshot()) cachedResourceDtos[res.id] = res;
                                double dummyResource = available;
                                if (interactions.applyContact(physicalInteractionSession, 100, eligibilityWater, actualDrink, dummyResource)) {
                                    needs->drink(actualDrink);
                                    ++appliedDrinkContacts; waterContactApplied = true;
                                }
                            }
                        } catch (const std::exception&) {
                            ++resourceLedgerCapExhaustions;
                        }
                    }
                }

                if (eligibilityShrub && nearbyShrubIndex >= 0) {
                    double feedAmount = 0.5;
                    uint64_t shrubId = 200 + nearbyShrubIndex;
                    double available = getResourceRemaining(shrubId);
                    double actualFeed = std::min(feedAmount, available);
                    if (actualFeed > 0) {
                        try {
                            if (resourceLedger.consume(physicalInteractionSession, shrubId, actualFeed)) {
                                for (const auto& res : resourceLedger.snapshot()) cachedResourceDtos[res.id] = res;
                                if (nearbyShrubIndex >= 0 && nearbyShrubIndex < static_cast<int>(shrubVisualInstances.size())) {
                                    applyShrubVisual(shrubVisualInstances[nearbyShrubIndex], cachedResourceDtos[shrubId]);
                                }
                                double dummyResource = available;
                                if (interactions.applyContact(physicalInteractionSession, shrubId, eligibilityShrub, actualFeed, dummyResource)) {
                                    needs->feed(actualFeed);
                                    ++appliedBrowseContacts; shrubContactApplied = true;
                                }
                            }
                        } catch (const std::exception&) {
                            ++resourceLedgerCapExhaustions;
                        }
                    }
                }
            }
            previousInteractDown = interactDown;

            if(!earlyScreenshot && elapsed>=std::min(1.,duration*.25)) earlyScreenshot=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/"motion-early.png").u8string());
            if(!lateScreenshot && elapsed>=std::min(1.35,duration*.75)) lateScreenshot=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/"motion-late.png").u8string());
            if(!screenshot && elapsed>=std::min(5.,duration*.5)) screenshot=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/"draft-host.png").u8string());
        }
        if (!saveState.empty()) {
            stalthos::persistence::HostPose pose{{x,Ground(x,z),z},yaw,static_cast<std::uint8_t>(cameraMode)};
            auto data=stalthos::persistence::captureSave(clock, world, pose);
            if(data.hero) data.heroLifecycle=stalthos::persistence::captureHeroLifecycle(*lifecycle,data.hero->id);
            if(data.hero) data.heroNeeds=stalthos::persistence::captureHeroNeeds(*needs,data.hero->id);
            data.resourceLedger = stalthos::persistence::captureResourceLedger(resourceLedger);
            const auto& cameraState = viewController.getState();
            data.cameraState = stalthos::persistence::CameraStateSnapshot{
                static_cast<std::uint8_t>(cameraState.mode), static_cast<std::uint8_t>(cameraState.focus),
                cameraState.detachedAnchor, cameraState.orbit.azimuth, cameraState.orbit.elevation,
                cameraState.orbit.distance};
            stalthos::persistence::writeSave(saveState,data);
            if(successorId) {
                const auto saved=stalthos::persistence::readSave(saveState);
                savedDeadHeroCount=saved.deadHeroes.size();
                successorSavedDeathTruthPreserved=std::any_of(saved.deadHeroes.begin(),saved.deadHeroes.end(),
                    [&](const auto& dead) { return dead.id==successorPriorDeadId; });
            }
        }
        if (diagnosticTransition && activeTransitionPhaseIndex >= 0 && activeTransitionPhaseIndex < static_cast<int>(transitionPhases.size())) {
            auto& last = transitionPhases[activeTransitionPhaseIndex];
            last.end_time_s = finalElapsed;
            last.duration_s = last.end_time_s - last.start_time_s;
            last.end_x = x; last.end_z = z;
            last.displacement_m = std::hypot(last.end_x - last.start_x, last.end_z - last.start_z);
            last.signed_displacement_m = 0.0;
            last.end_animation_phase_s = creaturePhase;
            auto* idleAnim = sceneIdleAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneIdleAnimEntity) : nullptr;
            last.end_animation_timer_s = idleAnim ? idleAnim->timer : 0.0f;
            last.bone_pose_changed = (last.local_bone_delta > 1e-5f);
        }
        json transitionPhasesJson = json::array();
        json transitionCapturesJson = json::object();
        for (const auto& p : transitionPhases) {
            transitionPhasesJson.push_back({
                {"phase_index", p.phase_index},
                {"phase_name", p.phase_name},
                {"intent", p.intent},
                {"duration_s", p.duration_s},
                {"frame_count", p.frame_count},
                {"displacement_m", p.displacement_m},
                {"signed_displacement_m", p.signed_displacement_m},
                {"accumulated_distance_m", p.accumulated_distance_m},
                {"active_clip", p.active_clip},
                {"active_clip_amount", p.active_clip_amount},
                {"inactive_clip", p.inactive_clip},
                {"inactive_clip_amount", p.inactive_clip_amount},
                {"start_animation_phase_s", p.start_animation_phase_s},
                {"end_animation_phase_s", p.end_animation_phase_s},
                {"start_animation_timer_s", p.start_animation_timer_s},
                {"end_animation_timer_s", p.end_animation_timer_s},
                {"local_bone_delta", p.local_bone_delta},
                {"bone_pose_changed", p.bone_pose_changed},
                {"inactive_clip_paused", p.inactive_clip_paused},
                {"inactive_clip_amount_zero", p.inactive_clip_amount_zero},
                {"capture_filename", p.capture_filename},
                {"capture_saved", p.capture_saved}
            });
            if (p.capture_saved) {
                transitionCapturesJson[p.phase_name] = p.capture_filename;
            }
        }
        if (transitionFinalCaptureSaved) {
            transitionCapturesJson["final_idle"] = "transition-5-final-idle.png";
        }
        json footDiagnosticJson = {{"resolved", latestFootDiagnostic.has_value()}, {"method", "bone_proxy"},
            {"assumed_vertical_sole_offset_m", 0.02}, {"foot_contact_accepted", false}};
        if (latestFootDiagnostic) {
            footDiagnosticJson["all_valid"] = latestFootDiagnostic->allValid;
            footDiagnosticJson["limitations"] = latestFootDiagnostic->limitations;
            footDiagnosticJson["feet"] = json::array();
            for (const auto& sample : latestFootDiagnostic->feet) footDiagnosticJson["feet"].push_back({
                {"foot", stalthos::engine::footIdToString(sample.foot)}, {"valid", sample.valid},
                {"world_position_m", {sample.worldPosition.x, sample.worldPosition.y, sample.worldPosition.z}},
                {"terrain_height_m", sample.terrainHeight}, {"vertical_clearance_m", sample.clearanceMeters}, {"issue", sample.issue}});
        }
        json nativeSoleDiagnosticJson = {
            {"resolved", creatureLoaded && nativeSoleAdapter.isInitialized() && latestNativeSoleDiagnostic.has_value()},
            {"method", "native_body_proxy_skinned_surface"},
            {"initialization_issue", nativeSoleAdapter.getErrorMessage()},
            {"foot_contact_accepted", false}
        };
        if (latestNativeSoleDiagnostic) {
            nativeSoleDiagnosticJson["all_valid"] = latestNativeSoleDiagnostic->allValid;
            nativeSoleDiagnosticJson["strict_complete"] = latestNativeSoleDiagnostic->strictComplete;
            nativeSoleDiagnosticJson["strict_disjoint"] = latestNativeSoleDiagnostic->strictDisjoint;
            nativeSoleDiagnosticJson["strict_finite"] = latestNativeSoleDiagnostic->strictFinite;
            nativeSoleDiagnosticJson["rendered_ground_cached"] = nativeSoleAdapter.isGroundCached();
            nativeSoleDiagnosticJson["rendered_ground_issue"] = nativeSoleAdapter.getGroundErrorMessage();
            nativeSoleDiagnosticJson["limitations"] = latestNativeSoleDiagnostic->limitations;
            nativeSoleDiagnosticJson["feet"] = json::array();
            for (const auto& sample : latestNativeSoleDiagnostic->feet) {
                nativeSoleDiagnosticJson["feet"].push_back({
                    {"foot", stalthos::engine::footIdToString(sample.foot)},
                    {"valid", sample.valid},
                    {"complete", sample.complete},
                    {"disjoint", sample.disjoint},
                    {"finite", sample.finite},
                    {"selected_vertex_count", sample.selectedVertexCount},
                    {"min_clearance_m", sample.minClearanceMeters},
                    {"max_clearance_m", sample.maxClearanceMeters},
                    {"mean_clearance_m", sample.meanClearanceMeters},
                    {"min_clearance_analytical_m", sample.minAnalyticalClearanceMeters},
                    {"max_clearance_analytical_m", sample.maxAnalyticalClearanceMeters},
                    {"mean_clearance_analytical_m", sample.meanAnalyticalClearanceMeters},
                    {"min_clearance_rendered_m", sample.minRenderedClearanceMeters},
                    {"max_clearance_rendered_m", sample.maxRenderedClearanceMeters},
                    {"mean_clearance_rendered_m", sample.meanRenderedClearanceMeters},
                    {"min_height_difference_m", sample.minHeightDifferenceMeters},
                    {"max_height_difference_m", sample.maxHeightDifferenceMeters},
                    {"mean_height_difference_m", sample.meanHeightDifferenceMeters},
                    {"lowest_world_pos_m", {sample.lowestVertexWorldPos.x, sample.lowestVertexWorldPos.y, sample.lowestVertexWorldPos.z}},
                    {"highest_world_pos_m", {sample.highestVertexWorldPos.x, sample.highestVertexWorldPos.y, sample.highestVertexWorldPos.z}},
                    {"lowest_rendered_world_pos_m", {sample.lowestRenderedVertexWorldPos.x, sample.lowestRenderedVertexWorldPos.y, sample.lowestRenderedVertexWorldPos.z}},
                    {"highest_rendered_world_pos_m", {sample.highestRenderedVertexWorldPos.x, sample.highestRenderedVertexWorldPos.y, sample.highestRenderedVertexWorldPos.z}},
                    {"issue", sample.issue}
                });
            }
        }
        json foliageDepletionJson = json::array();
        for (const auto& inst : shrubVisualInstances) {
            json foliageEntitiesJson = json::array();
            for (const auto& f : inst.foliage) {
                foliageEntitiesJson.push_back({
                    {"entity_id", static_cast<uint64_t>(f.entity)},
                    {"object_name", f.objectName},
                    {"mesh_name", f.meshName},
                    {"original_scale", {f.originalScale.x, f.originalScale.y, f.originalScale.z}}
                });
            }
            json structureEntitiesJson = json::array();
            for (const auto& s : inst.structure) {
                structureEntitiesJson.push_back({
                    {"entity_id", static_cast<uint64_t>(s.entity)},
                    {"object_name", s.objectName},
                    {"mesh_name", s.meshName}
                });
            }
            foliageDepletionJson.push_back({
                {"instance_index", inst.instanceIndex},
                {"resource_id", inst.resourceId},
                {"placement", {
                    {"x", inst.placement.x},
                    {"y", inst.placement.y},
                    {"z", inst.placement.z},
                    {"scale", inst.placement.scale},
                    {"heading_radians", inst.placement.headingRadians}
                }},
                {"foliage_fraction", inst.visualState.foliageFraction},
                {"leaf_cluster_count", inst.visualState.leafClusterCount},
                {"woody_stem_visible", inst.visualState.woodyStemVisible},
                {"visual_valid", inst.visualState.valid},
                {"applied_scale_factor", inst.appliedFraction},
                {"foliage_renderable", inst.visualState.valid && inst.visualState.foliageFraction > 0.0},
                {"foliage_entities", foliageEntitiesJson},
                {"structure_entities", structureEntitiesJson}
            });
        }
        WriteJson(output/"runtime-receipt.json",{{"schema_version",1},{"ok",runCalls>0 && screenshot},{"stage","engine-host feasibility"},
            {"engine_revision","df44c3db4c4927492bc9c791eac715d98d7ed091"},{"process_id",GetCurrentProcessId()},
            {"render_size",{app.canvas.GetPhysicalWidth(),app.canvas.GetPhysicalHeight()}},{"run_calls",runCalls},
            {"scenery_objects",sceneryCounts.objectCount},{"scenery_vertices",sceneryCounts.vertexCount},{"scenery_triangles",sceneryCounts.triangleCount},
            {"scenery_logs",sceneryCounts.logCount},{"scenery_pebbles",sceneryCounts.pebbleCount},{"scenery_bank_clumps",sceneryCounts.bankClumpCount},{"scenery_bluff_accents",sceneryCounts.bluffAccentCount},
            {"screenshot_saved",screenshot},{"plant_import_objects",importedObjects},{"user_closed",userClosed},
            {"plant_import_instances",importedInstances},{"landscape_extent_meters",192},
            {"landscape_candidate_placements",landscape.placements().size()},
            {"calendar_seconds",clock.calendarSeconds()},{"domain_world_seed",world.worldSeed()},
            {"physical_seconds",clock.physicalSeconds()},{"loaded_calendar_seconds",loadedCalendar},{"loaded_physical_seconds",loadedPhysical},
            {"state_loaded",!loadState.empty()},{"state_saved",!saveState.empty()},{"camera_state_loaded",loadedSaveData && loadedSaveData->cameraState.has_value()},{"camera_state_saved",!saveState.empty()},
            {"needs_snapshot_loaded",loadedNeeds.has_value()},{"needs_snapshot_saved",!saveState.empty() && world.snapshot().hero.has_value()},
            {"diagnostic_walk",diagnosticWalk},{"diagnostic_needs",diagnosticNeeds},{"foot_terrain_diagnostic",footDiagnosticJson},{"native_sole_diagnostic",nativeSoleDiagnosticJson},{"diagnostic_transition",diagnosticTransition},{"calendar_scale",calendarScale},
            {"diagnostic_transition_phases", transitionPhasesJson},
            {"transition_captures", transitionCapturesJson},
            {"inactive_clip_state_guarded", inactiveClipWritesPrevented},
            {"walk_phase_follows_signed_distance", walkPhaseFollowsSignedDistance},
            {"idle_reset_to_neutral_verified", idleResetToNeutralVerified},
            {"clip_selection_policy", "exclusive binary clip selection (amount 1.0 / 0.0); paused inactive clip skipped by engine dependency scanner"},
            {"remaining_discontinuity_documented", true},
            {"needs_initial", {{"hunger", initialHunger}, {"thirst", initialThirst}, {"fatigue", initialFatigue}}},
            {"needs_final", {{"hunger", needs->hunger()}, {"thirst", needs->thirst()}, {"fatigue", needs->fatigue()}}},
            {"resources_initial", {{"creek", initialCreekResource}, {"shrub0", initialShrub0Resource}}},
            {"resources_final", {{"creek", getResourceRemaining(100)}, {"shrub0", getResourceRemaining(200)}}},
            {"resources_finite", getResourceFinite()},
            {"shrub_presentation_instances", foliageDepletionJson},
            {"shrub_presentation_active", !plant.empty() && importedInstances > 0},
            {"eligibility_water", eligibilityWater}, {"eligibility_shrub", eligibilityShrub},
            {"water_eligible_frames", waterEligibleFrames}, {"shrub_eligible_frames", shrubEligibleFrames},
            {"water_contact_applied", waterContactApplied}, {"shrub_contact_applied", shrubContactApplied},
            {"drink_contacts_applied", appliedDrinkContacts}, {"browse_contacts_applied", appliedBrowseContacts},
            {"resource_ledger_cap_exhaustions", resourceLedgerCapExhaustions},
            {"diagnostic_needs_stage", diagnosticSequenceStage},
            {"lifecycle_stage",static_cast<unsigned>(lifecycle->stage())},{"lifecycle_age_seconds",lifecycle->calendarAge()},{"lifecycle_transitions",lifecycleTransitions},
            {"lifecycle_snapshot_loaded",lifecycleSnapshotLoaded},{"loaded_lifecycle_age_seconds",loadedLifecycleAge},{"loaded_lifecycle_stage",loadedLifecycleStage},
            {"hero_alive",world.snapshot().hero.has_value()},{"hero_death_known",world.heroDeathKnown(heroId)},
            {"successor_selected",successorId.has_value()},{"successor_id",successorId.value_or(0)},
            {"successor_prior_dead_hero_id",successorPriorDeadId},{"successor_prior_death_truth_preserved",successorDeathTruthPreserved},
            {"successor_saved_prior_death_truth_preserved",successorSavedDeathTruthPreserved},{"saved_dead_hero_count",savedDeadHeroCount},
            {"terrain_surface_applied",terrainSurface.applied()},{"terrain_surface_status",static_cast<int>(terrainSurface.status)},{"terrain_texture_width",terrainSurface.textureWidth},{"terrain_texture_bytes",terrainSurface.textureBytes},{"terrain_uv_vertices",terrainSurface.uvVertexCount},{"meadow_batch_objects",meadowCounts.batchObjectCount},{"meadow_tufts",meadowCounts.tuftCount},{"meadow_vertices",meadowCounts.vertexCount},{"meadow_triangles",meadowCounts.triangleCount},{"meadow_rejected_wet_samples",meadowCounts.rejectedWetSamples},{"organic_rock_objects",organicRockCounts.objectCount},{"organic_rock_triangles",organicRockCounts.triangleCount},{"inspection_camera",inspectionCamera},{"camera_distance_m",cameraDistance},
            {"shore_objects",shoreCounts.objectCount},{"shore_vertices",shoreCounts.vertexCount},{"shore_triangles",shoreCounts.triangleCount},{"shore_surface_triangles",shoreCounts.surfaceTriangleCount},{"shore_bed_triangles",shoreCounts.bedTriangleCount},{"shore_bank_triangles",shoreCounts.bankTriangleCount},
            {"creature_loaded",creatureLoaded},{"creature_source_front_axis","+X"},{"creature_host_forward_axis","+Z"},{"creature_parent_yaw_offset_radians",creatureLoaded?-pi*.5f:0.f},{"creature_sole_offset_m",creatureLoaded?creatureSoleOffset:0.0},{"creature_tail_ground_clearance_accepted",false},{"creature_path",creature.u8string()},{"creature_mesh_objects",creatureMeshes},{"creature_skin_count",creatureSkins},{"creature_animation_count",creatureAnimations},{"creature_transform_channels",creatureChannels},
            {"locomotion_pace_policy", "proposed one cycle per second, maximum one meter per second; proxy fallback two meters per second"},{"walk_duration_s",creatureClipDuration},{"walk_stride_length_m",creatureStrideLength},{"imported_animation_length_s",engineClipLength},{"actor_root_translation_channels_removed",removedActorRootTranslationChannels},{"actor_root_translation_target_name",actorRootTranslationTargetName},{"actor_root_translation_target_is_skin_bone",false},{"body_signed_distance_last_m",lastSignedBodyDistance},{"body_distance_accumulated_signed_m",creatureDistanceAccumulated},{"animation_phase_expected_s",lastExpectedPhase},{"animation_phase_actual_s",creaturePhase},
            {"animation_timer_min",std::isfinite(minimumAnimationTimer)?minimumAnimationTimer:0},{"animation_timer_max",maximumAnimationTimer},{"animation_playing_frames",animationPlayingFrames},{"animated_target_channel_count",channelPoseBaseline.size()},
            {"animated_channel_local_pose_delta",localBoneDelta},{"animated_channel_pose_changed",localBoneDelta>1e-5f},{"motion_early_saved",earlyScreenshot},{"motion_late_saved",lateScreenshot},
            {"save_scope","foundation state, clocks, diagnostic pose, current hero lifecycle and needs; bonds not connected; camera mode, focus, anchor and orbit stored in optional STL6 snapshot; tether configuration and travel intent not persisted"},
            {"pose_vertical_policy","analytical ground projection; no saved vertical body dynamics"},
            {"save_path",saveState.u8string()},{"load_path",loadState.u8string()},
            {"host_pose",{{"x",x},{"z",z},{"heading_radians",yaw},{"camera_mode",cameraMode}}},
            {"camera_mode", viewController.getState().mode == stalthos::domain::CameraMode::LocallyDetached ? "LocallyDetached" : "TetheredFollow"},
            {"input_focus", viewController.getState().focus == stalthos::domain::InputFocus::FreeCamera ? "FreeCamera" : "BodySteering"},
            {"camera_eye", {{"x", wi::scene::GetCamera().Eye.x}, {"y", wi::scene::GetCamera().Eye.y}, {"z", wi::scene::GetCamera().Eye.z}}},
            {"camera_requested_eye", {{"x", cameraOut.cameraState.eye.x}, {"y", cameraOut.cameraState.eye.y}, {"z", cameraOut.cameraState.eye.z}}},
            {"camera_target", {{"x", cameraOut.cameraState.target.x}, {"y", cameraOut.cameraState.target.y}, {"z", cameraOut.cameraState.target.z}}},
            {"camera_orbit_distance", viewController.getState().orbit.distance},
            {"camera_tether_comfort_radius", viewController.getComfortRadius()},
            {"camera_tether_hard_radius", viewController.getHardRadius()},
            {"tether_terrain_conflicts", tetherTerrainConflictCount},
            {"diagnostic_camera", diagnosticCamera}, {"diagnostic_camera_stage", diagnosticCameraStage},
            {"detached_camera_frames", detachedCameraFrames}, {"body_distance_while_camera_detached_m", detachedBodyDistance},
            {"pending_audio", "none"},
            {"pending_travel_to", cameraOut.activeTravelDestination.has_value()},
            {"pending_raycast", "none"},
            {"supportmode", latestSupportMode},
            {"support_mode", latestSupportMode},
            {"slope", latestSlopeRadians},
            {"support_slope_radians", latestSlopeRadians},
            {"normal", {{"x", latestNormal.x}, {"y", latestNormal.y}, {"z", latestNormal.z}}},
            {"support_normal", {{"x", latestNormal.x}, {"y", latestNormal.y}, {"z", latestNormal.z}}},
            {"supportfailure", supportFailureCount},
            {"support_failure_count", supportFailureCount},
            {"disable_terrain_support", disableTerrainSupport},
            {"terrain_support_longitudinal_halfspan_m", kCreatureLongitudinalHalfspan},
            {"terrain_support_lateral_halfspan_m", kCreatureLateralHalfspan},
            {"terrain_support_policy", "proposed approximate visual body plane alignment only; per-foot IK and terrain foot locking not claimed"},
            {"frame_time_measured",false},{"full_draft_complete",false},{"visual_accepted",false},
            {"limits",{"proxy fallback remains diagnostic; imported creature playback is feasibility only, foot contact not accepted because low-tail clearance remains unresolved","no external frame-time measurement","resource ledger capped at 256 interaction sessions; cap exhaustion rejects further consumption without history eviction; production game requires unbounded or tiered epoch persistence","camera mode/focus/anchor/orbit persisted; tether configuration/travel intent, audio, ground clicking and mouse raycast pending","exclusive clip selection used for idle/walk/reverse transitions; unblended one-frame pose snap remains at transition boundaries pending proven crossfade/inertialization blend"}}});
        wi::jobsystem::ShutDown(); DestroyWindow(window);
        return runCalls>0 && screenshot?0:3;
    } catch(const std::exception& error) {
        if(argv) LocalFree(argv);
        try { std::filesystem::create_directories(output); WriteJson(output/"failure.json",{{"ok",false},{"error",error.what()}}); } catch(...) {}
        wi::jobsystem::ShutDown(); return 1;
    }
}
