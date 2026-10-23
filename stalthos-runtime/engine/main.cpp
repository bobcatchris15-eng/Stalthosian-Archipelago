#include "domain/reedcrest.hpp"
#include "reedcrest_scene.hpp"
#include "domain/brackenback.hpp"
#include "brackenback_scene.hpp"
#include "domain/thistletail.hpp"
#include "thistletail_scene.hpp"
#include "domain/nesting.hpp"
#include "domain/hatchling_path.hpp"
#include "hatchling_scene.hpp"
#include "nest_scene.hpp"
#include "domain/sapling_disturbance.hpp"
#include "domain/hero_exploration.hpp"
#include "domain/next_life.hpp"
#include "domain/life_stage_presentation.hpp"
#include "domain/peer_companionship.hpp"
#include "domain/carnivore_awareness.hpp"
#include "domain/exterior_woodland.hpp"
#include "day_night_scene.hpp"
#include "exterior_water_scene.hpp"
#include "domain/exterior_browse.hpp"
#include "domain/hero_destination.hpp"
#include "domain/environmental_exposure.hpp"
#include "domain/hero_rest.hpp"
#include "domain/exterior_residents.hpp"
#include "native_save.hpp"
#include "domain/weather_response.hpp"
#include "domain/hero_autonomy.hpp"
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
#include "domain/animation_blend.hpp"
#include "crossfade_transition.hpp"
#include "domain/resource_ledger.hpp"
#include "domain/regional_habitat.hpp"
#include "domain/resource_presentation.hpp"
#include <cctype>
#include "environment_scene.hpp"
#include "foot_terrain_diagnostic.hpp"
#include "native_sole_adapter.hpp"
#include "shore_scene.hpp"
#include "exterior_shore_scene.hpp"
#include "terrain_surface.hpp"
#include "organic_rocks.hpp"
#include "meadow_scene.hpp"
#include "woodland_scene.hpp"
#include "domain/body_motion_audio.hpp"
#include "body_rustle.hpp"
#include "native_audio_capture.hpp"
#include "fauna_scene.hpp"
#include "populated_terrain.hpp"
#include "native_population_registry.hpp"
#include "world_conditions.hpp"
#include "domain/camera_framing.hpp"
#include "domain/hero_view_controller.hpp"
#include "domain/body_terrain_support.hpp"
#include <windows.h>
#include "terrain_stream_probe.hpp" // Optional coarse-world traversal with native visibility diagnostics.
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
std::function<double(double,double)> productionGround;

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
    float start_active_clip_amount = 0.0f;
    float start_inactive_clip_amount = 0.0f;
    double start_idle_weight = 0.0;
    double end_idle_weight = 0.0;
    double start_walk_weight = 0.0;
    double end_walk_weight = 0.0;
    uint64_t blend_frame_count = 0;
    uint64_t settled_frame_count = 0;
    double start_animation_phase_s = 0.0;
    double end_animation_phase_s = 0.0;
    float start_animation_timer_s = 0.0f;
    float end_animation_timer_s = 0.0f;
    float local_bone_delta = 0.0f;
    bool bone_pose_changed = false;
    bool inactive_clip_paused = true;
    bool inactive_clip_amount_zero = true;
    bool post_blend_inactive_clip_zero = true;
    std::string capture_filename;
    bool capture_saved = false;
    std::string interior_capture_filename;
    bool interior_capture_saved = false;
    float max_frame_translation_delta_m = 0.0f;
    float max_frame_rotation_delta_rad = 0.0f;
    bool all_blend_deltas_finite = true;
    std::vector<stalthos::engine::BlendFrameTelemetrySample> blend_frame_telemetry;
    std::map<std::pair<wi::ecs::Entity, uint8_t>, XMFLOAT4> baseline_poses;
};

std::optional<std::pair<int,int>> pendingDestinationClick;
LRESULT CALLBACK WindowProc(HWND h, UINT message, WPARAM w, LPARAM l) {
    if(message==WM_RBUTTONDOWN){pendingDestinationClick=std::make_pair(int(short(LOWORD(l))),int(short(HIWORD(l))));return 0;}
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
    return static_cast<float>(productionGround ? productionGround(x,z) : landscape.height(x,z));
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
    std::filesystem::path woodlandAdultTree;
    std::filesystem::path woodlandSaplingTree;
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
    bool populatedWorld = false, populatedTraversal = false, populatedDeparture = false, populatedResourceProbe = false;
    bool terrainStreamProbe = false, watershedJoinProbe = false;
    bool connectedWorld=false,connectedRouteWalk=false,weatherPreview=false;
    bool nestProbe=false;unsigned hatchProbe=0;unsigned birdProbe=0;unsigned protectionProbe=0;unsigned reedProbe=0;std::string requestedSpecies="Smalltrotter";bool speciesSpecified=false;
    unsigned carnivoreProbe=0;unsigned shoreProbe=0;unsigned npcPeerProbe=0;unsigned audioProbe=0;unsigned saplingProbe=0;unsigned explorationProbe=0;bool deathNextLifeProbe=false;bool lifeStageProbe=false;unsigned peerProbe=0;bool woodlandProbe=false;unsigned dayNightProbe=0;unsigned exteriorWaterProbe=0;unsigned exteriorBrowseProbe=0;unsigned destinationProbe=0;bool exposureProbe=false;
    unsigned autonomyProbe=0,wildlifeProbe=0,foragingProbe=0,restProbe=0;
    bool connectedEmbodimentProbe=false,exteriorResidentsProbe=false;
    int argumentCount = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    try {
        for (int i = 1; i < argumentCount; ++i) {
            std::wstring key = argv[i];
            if (key == L"--interactive") interactive = true;
            else if (key == L"--populated-world") populatedWorld = true;
            else if (key == L"--exterior-residents-probe") {exteriorResidentsProbe=true;connectedWorld=true;populatedWorld=true;connectedRouteWalk=true;}
            else if(key==L"--carnivore-probe"){carnivoreProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--carnivore-cancel-probe"){carnivoreProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--carnivore-camera-probe"){carnivoreProbe=3;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--wildlife-awareness-probe") {wildlifeProbe=1;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--wildlife-camera-probe") {wildlifeProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--body-audio-probe"){audioProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--body-audio-camera-probe"){audioProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--sapling-probe"){saplingProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--sapling-camera-probe"){saplingProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exploration-probe"){explorationProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exploration-override-probe"){explorationProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exploration-camera-probe"){explorationProbe=3;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--death-next-life-probe"){deathNextLifeProbe=true;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--hero-species" && i+1<argumentCount){const std::wstring value=argv[++i];requestedSpecies=std::string(value.begin(),value.end());speciesSpecified=true;}
            else if(key==L"--reed-peer-probe"){reedProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--reed-leading-probe"){reedProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--reed-camera-probe"){reedProbe=3;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--reed-separation-probe"){reedProbe=5;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--reed-need-probe"){reedProbe=4;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--protection-probe"){protectionProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--protection-override-probe"){protectionProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--protection-camera-probe"){protectionProbe=3;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--bird-probe"){birdProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--bird-override-probe"){birdProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--bird-camera-probe"){birdProbe=3;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--hatch-probe"){hatchProbe=1;nestProbe=true;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--hatch-camera-probe"){hatchProbe=2;nestProbe=true;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--nest-probe"){nestProbe=true;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--life-stage-probe"){lifeStageProbe=true;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--shore-route-probe"){shoreProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--shore-entry-probe"){shoreProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--shore-still-probe"){shoreProbe=4;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--shore-camera-probe"){shoreProbe=3;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--npc-peer-probe"){npcPeerProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--npc-peer-camera-probe"){npcPeerProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--npc-peer-need-probe"){npcPeerProbe=3;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--peer-probe"){peerProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--peer-camera-probe"){peerProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exterior-woodland-probe"){woodlandProbe=true;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--day-night-preview"){dayNightProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--night-controls-probe"){dayNightProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exterior-water-probe"){exteriorWaterProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exterior-water-camera-probe"){exteriorWaterProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exterior-water-override-probe"){exteriorWaterProbe=3;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exterior-browse-probe"){exteriorBrowseProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--exterior-browse-camera-probe"){exteriorBrowseProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--destination-probe"){destinationProbe=1;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--destination-override-probe"){destinationProbe=2;connectedWorld=true;populatedWorld=true;}
            else if(key==L"--destination-boundary-probe"){destinationProbe=3;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--exposure-probe") {exposureProbe=true;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--hero-rest-probe") {restProbe=1;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--hero-rest-override-probe") {restProbe=2;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--hero-foraging-probe") {foragingProbe=1;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--hero-foraging-override-probe") {foragingProbe=2;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--hero-foraging-camera-probe") {foragingProbe=3;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--hero-autonomy-probe") {autonomyProbe=1;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--hero-autonomy-override-probe") {autonomyProbe=2;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--hero-autonomy-camera-probe") {autonomyProbe=3;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--weather-preview") {weatherPreview=true;connectedWorld=true;populatedWorld=true;}
            else if (key == L"--connected-embodiment-probe") {connectedEmbodimentProbe=true;connectedWorld=true;populatedWorld=true;connectedRouteWalk=true;}
            else if (key == L"--connected-world") {connectedWorld=true;populatedWorld=true;}
            else if (key == L"--connected-route-walk") {connectedWorld=true;populatedWorld=true;connectedRouteWalk=true;}
            else if (key == L"--populated-resource-probe") populatedResourceProbe = true;
            else if (key == L"--populated-traversal") populatedTraversal = true;
            else if (key == L"--populated-departure") {populatedTraversal=true;populatedDeparture=true;}
            else if (key == L"--terrain-stream-probe") terrainStreamProbe = true;
            else if (key == L"--watershed-join-probe") { terrainStreamProbe = true; watershedJoinProbe = true; }
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
            else if (key == L"--woodland-adult-tree" && i+1 < argumentCount) woodlandAdultTree = argv[++i];
            else if (key == L"--woodland-sapling-tree" && i+1 < argumentCount) woodlandSaplingTree = argv[++i];
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
        if(connectedWorld && (!loadState.empty() || !saveState.empty())) throw std::runtime_error("connected world preview has no save support; use original populated world for existing saves");
        if(connectedWorld && (populatedTraversal || populatedResourceProbe || terrainStreamProbe || diagnosticCamera)) throw std::runtime_error("connected preview uses ordinary body controls; incompatible diagnostic mode");
        if(speciesSpecified && !connectedWorld)throw std::runtime_error("hero-species is connected-only; saved original mode keeps its own species");
        if(birdProbe){if(interactive || nestProbe || carnivoreProbe || diagCount || hatchProbe || shoreProbe || npcPeerProbe || audioProbe || saplingProbe || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview)throw std::runtime_error("bird diagnostic requires standalone timed connected mode");requestedSpecies="Thistletail";}
        if(protectionProbe){if(interactive || birdProbe || nestProbe || carnivoreProbe || diagCount || hatchProbe || shoreProbe || npcPeerProbe || audioProbe || saplingProbe || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview)throw std::runtime_error("protection diagnostic requires standalone timed connected mode");requestedSpecies="Brackenback";}
        if(reedProbe){if(interactive || protectionProbe || birdProbe || nestProbe || carnivoreProbe || diagCount || hatchProbe || shoreProbe || npcPeerProbe || audioProbe || saplingProbe || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview)throw std::runtime_error("Reedcrest diagnostic requires standalone timed connected mode");requestedSpecies="Reedcrest";}
        const auto selectedSpecies=stalthos::domain::playableSpecies(requestedSpecies);
        if(carnivoreProbe && (nestProbe || diagCount || interactive || shoreProbe || npcPeerProbe || audioProbe || saplingProbe || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("carnivore probe requires standalone timed connected mode");
        if(nestProbe && (carnivoreProbe || diagCount || interactive || shoreProbe || npcPeerProbe || audioProbe || saplingProbe || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("nest probe requires standalone timed connected mode");
        if(shoreProbe && (diagCount || interactive || npcPeerProbe || audioProbe || saplingProbe || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("shore probe requires standalone timed connected mode");
        if(npcPeerProbe && (diagCount || interactive || audioProbe || saplingProbe || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("NPC peer probe requires standalone timed connected mode");
        if(audioProbe && (diagCount || interactive || saplingProbe || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("audio probe requires standalone timed connected mode");
        if(saplingProbe && (diagCount || interactive || explorationProbe || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview || woodlandSaplingTree.empty()))throw std::runtime_error("sapling probe requires standalone timed connected mode and sapling asset");
        if(explorationProbe && (diagCount || interactive || deathNextLifeProbe || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("exploration probe requires standalone timed connected mode");
        if(deathNextLifeProbe && (diagCount || interactive || lifeStageProbe || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("death/next-life probe requires standalone timed connected mode");
        if(lifeStageProbe && (interactive || peerProbe || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("life-stage probe requires standalone timed connected mode");
        if(peerProbe && (interactive || woodlandProbe || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("peer probe requires standalone timed connected mode");
        if(woodlandProbe && (interactive || dayNightProbe || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview || woodlandAdultTree.empty() || woodlandSaplingTree.empty()))throw std::runtime_error("woodland probe requires timed connected world and both tree assets only");
        if(dayNightProbe && (interactive || exteriorWaterProbe || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("day/night probe uses timed connected world without another diagnostic");
        const stalthos::fauna::ExteriorWaterSource exteriorWater;
        if(exteriorWaterProbe && (interactive || !creature.empty() || exteriorBrowseProbe || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("exterior water probe uses timed connected proxy without other diagnostic");
        const bool exteriorBrowseEnabled=connectedWorld && !plant.empty();const stalthos::fauna::ExteriorBrowsePatch exteriorBrowse;
        if(exteriorBrowseProbe && (!exteriorBrowseEnabled || interactive || !creature.empty() || destinationProbe || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview))throw std::runtime_error("exterior browse probe requires imported Plant and timed proxy only");
        if(destinationProbe && (interactive || !creature.empty() || exposureProbe || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview || connectedEmbodimentProbe))throw std::runtime_error("destination probe uses timed connected proxy with no other diagnostic");
        if(exposureProbe && (interactive || !creature.empty() || restProbe || foragingProbe || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview)) throw std::runtime_error("exposure probe uses timed proxy controls with no other diagnostic");
        if(restProbe && (interactive || autonomyProbe || foragingProbe || wildlifeProbe || connectedRouteWalk || weatherPreview)) throw std::runtime_error("rest probe requires timed connected world without another diagnostic");
        if(foragingProbe && (plant.empty() || interactive || autonomyProbe || wildlifeProbe || connectedRouteWalk || weatherPreview)) throw std::runtime_error("foraging probe requires imported Plant and no other diagnostic");
        if(wildlifeProbe && (interactive || autonomyProbe || connectedRouteWalk || weatherPreview || connectedEmbodimentProbe || !creature.empty())) throw std::runtime_error("wildlife probe uses bounded connected proxy input and no other diagnostic");
        if(autonomyProbe && (interactive || connectedRouteWalk || weatherPreview || diagnosticCamera)) throw std::runtime_error("hero autonomy probe requires timed connected world with no other diagnostic");
        if(weatherPreview && (interactive || connectedRouteWalk || duration<6)) throw std::runtime_error("weather preview requires timed connected world, duration at least6s and no route walk");
        if(connectedRouteWalk && interactive) throw std::runtime_error("connected route walk is a bounded forward-intent proof; use ConnectedWorld Interactive for ordinary controls");
        if(populatedResourceProbe && (!populatedWorld || interactive || diagnosticCamera || diagCount || populatedTraversal || !loadState.empty())) throw std::runtime_error("populated resource probe requires a fresh timed populated world without other diagnostics");
        if(populatedTraversal && (!populatedWorld || interactive || diagnosticCamera)) throw std::runtime_error("populated traversal requires timed populated world without camera diagnostics");
        if (terrainStreamProbe && (populatedWorld || diagCount || diagnosticCamera || !loadState.empty() || !saveState.empty() || !creature.empty() || !plant.empty() || !woodlandAdultTree.empty() || !woodlandSaplingTree.empty() || successorId))
            throw std::runtime_error("terrain stream probe must run separately from watershed, assets and persistence");
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
        if(populatedWorld && (diagnosticTransition || diagnosticNeeds || diagnosticWalk || diagnosticReverseWalk))
            throw std::runtime_error("populated world and focused hero diagnostics must run separately");
        if (std::filesystem::exists(output / "runtime-receipt.json")) throw std::runtime_error("use a fresh evidence directory");
        stalthos::engine::NativeSimulationOwner simulationOwner{stalthos::SimCore{},stalthos::StalthosState(42),{}};
        auto& clock=simulationOwner.clock;
        auto& world=simulationOwner.world;
        const stalthos::world::ConnectedRouteLayout connectedLayout;
        float x=connectedWorld?float(connectedLayout.dryBankX):12.f,z=connectedWorld?float(connectedLayout.centerZ):0.f,yaw=connectedWorld?1.57079632679f:0.f; int cameraMode=0;
        if(exposureProbe){x=float(connectedLayout.joinX+16+12*.8);z=float(connectedLayout.centerZ-16+12*.6);yaw=float(std::atan2(-.8,-.6));} // Startup fixture only; subsequent motion uses shared forward controls.
        if(connectedEmbodimentProbe || exteriorResidentsProbe) x=104; // Explicit exterior startup fixture; ordinary integrator thereafter.
        if(autonomyProbe && autonomyProbe!=3 && !creature.empty()) x=10.5f; // Explicit near-water imported-body proof at measured pace.
        if(autonomyProbe==3) x=40; // Synthetic initial outside-perception body; never moved by camera probe.
        stalthos::domain::HeroViewController viewController;
        double loadedCalendar=0, loadedPhysical=0;
        stalthos::HeroSpecies heroSpecies=connectedWorld?selectedSpecies:stalthos::HeroSpecies::Smalltrotter;
        const bool reedLife=connectedWorld && heroSpecies==stalthos::HeroSpecies::Reedcrest;
        const bool brackenLife=connectedWorld && heroSpecies==stalthos::HeroSpecies::Brackenback;
        const bool birdLife=connectedWorld && heroSpecies==stalthos::HeroSpecies::Thistletail;const stalthos::domain::MicroforagePatch microforage;
        std::optional<stalthos::Lifecycle> lifecycle;
        std::optional<stalthos::needs::NeedsCore> needs;
        std::optional<stalthos::persistence::HeroNeedsSnapshot> loadedNeeds;
        uint64_t heroId=1; bool lifecycleSnapshotLoaded=false; double loadedLifecycleAge=0; int loadedLifecycleStage=-1;
        uint64_t successorPriorDeadId=0; bool successorDeathTruthPreserved=false;
        bool successorSavedDeathTruthPreserved=false; std::size_t savedDeadHeroCount=0;
        std::optional<stalthos::persistence::SaveData> loadedSaveData;
        std::optional<stalthos::persistence::host_save::Candidate> loadedHostSave;
        const auto hostConfiguration=stalthos::engine::nativeSaveConfiguration(populatedWorld);
        if (!loadState.empty()) {
            loadedHostSave=stalthos::persistence::host_save::read(loadState,hostConfiguration);
            if(loadedHostSave->regional && !populatedWorld) throw std::runtime_error("native STL9 requires populated world configuration");
            loadedSaveData = loadedHostSave->host;
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
                if (std::abs(x)>(populatedWorld?12000:60) || std::abs(z)>(populatedWorld?12000:60) || !std::isfinite(yaw))
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
        if(protectionProbe){const auto& p=stalthos::getHeroProfile(heroSpecies);lifecycle->restoreState(heroSpecies,p.hatchlingDuration+p.juvenileDuration,stalthos::LifeStage::Adult);}
        if(birdProbe){x=float(microforage.x-(birdProbe==3?36:6));z=float(microforage.z);yaw=pi*.5f;} // labelled startup only
        if(nestProbe){const auto& p=stalthos::getHeroProfile(heroSpecies);lifecycle->restoreState(heroSpecies,p.hatchlingDuration+p.juvenileDuration,stalthos::LifeStage::Adult);x=24;z=24;yaw=0;} // labelled adult diagnostic startup only
        if(lifeStageProbe){const auto& profile=stalthos::getHeroProfile(heroSpecies);lifecycle->restoreState(heroSpecies,profile.hatchlingDuration+profile.juvenileDuration-6,stalthos::LifeStage::Juvenile);}
        if(deathNextLifeProbe){const auto& p=stalthos::getHeroProfile(heroSpecies);lifecycle->restoreState(heroSpecies,p.hatchlingDuration+p.juvenileDuration+p.adultDuration+p.oldDuration-6,stalthos::LifeStage::Old);x=float(exteriorWater.bankX);z=float(exteriorWater.bankZ);}
        if(explorationProbe){x=float(exteriorWater.bankX);z=float(exteriorWater.bankZ);}
        const double initialHeroLifecycleAge=lifecycle->calendarAge();double heroStageBulk=connectedWorld?stalthos::domain::lifeStageBulk(lifecycle->stage()):1;
        needs.emplace(heroSpecies);
        if (loadedNeeds && loadedNeeds->species == heroSpecies)
            needs->restoreState(loadedNeeds->hunger, loadedNeeds->thirst, loadedNeeds->fatigue, loadedNeeds->injury, loadedNeeds->isResting);
        if(deathNextLifeProbe)needs->restoreState(0,.8,0,0,false); // Labelled contact-history fixture for both lives.
        if(lifeStageProbe)needs->restoreState(0,0,.8,0,false); // Explicit rest/override fixture only.
        if(birdProbe)needs->restoreState(.8,0,0,0,false);
        if(restProbe) needs->restoreState(0,0,.8,0,false); // Explicit synthetic fatigue only.
        if(dayNightProbe==2) needs->restoreState(0,0,.8,0,false);
        if(exteriorWaterProbe) needs->restoreState(0,.8,0,0,false);
        if(exteriorBrowseProbe) needs->restoreState(.8,0,0,0,false);
        if(foragingProbe) needs->restoreState(.8,0,0,0,false);
        if(autonomyProbe) needs->restoreState(0,.8,0,0,false); // Explicit synthetic need setup only.
        if (diagnosticNeeds && !loadedNeeds) needs->restoreState(.75, .75, 0, 0, false);
        stalthos::InteractionsCore interactions(256);
        std::optional<stalthos::ResourceLedger> bootstrapLedger(std::in_place,4096,256);
        auto& faunaPopulation=simulationOwner.fauna;
        auto ledgerSnapshot=[&]() {return faunaPopulation ? faunaPopulation->ledger() : bootstrapLedger->continuationSnapshot();};
        auto resourceSnapshot=[&]() {return ledgerSnapshot().resources;};
        auto resourceHistoryCapacity=[&]() {const auto used=ledgerSnapshot().sessions.size(); return stalthos::ResourceHistoryCapacity{used,256,256-used};};
        auto consumeResource=[&](uint64_t session,uint64_t resource,double amount) {return faunaPopulation ? faunaPopulation->consume(session,resource,amount) : bootstrapLedger->consume(session,resource,amount);};
        landscape=stalthos::domain::ReferenceLandscape(world.worldSeed());

        if (loadedSaveData && loadedSaveData->resourceLedger.has_value()) {
            stalthos::persistence::restoreResourceLedger(*loadedSaveData, *bootstrapLedger);
        } else {
            bootstrapLedger->load({{100, stalthos::ResourceKind::Creek, 1000000.0, 0.0},
                                 {200, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {201, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {202, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {203, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {204, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {205, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {206, stalthos::ResourceKind::Shrub, 2.0, 0.0},
                                 {207, stalthos::ResourceKind::Shrub, 2.0, 0.0}});
        }

        if(exteriorBrowseEnabled){auto resources=bootstrapLedger->continuationSnapshot().resources;resources.push_back({stalthos::fauna::ExteriorBrowsePatch::resource,stalthos::ResourceKind::Shrub,exteriorBrowse.quantity,0});if(!bootstrapLedger->load(resources))throw std::runtime_error("exterior browse ledger initialization rejected");}

        if(connectedWorld){auto resources=bootstrapLedger->continuationSnapshot().resources;resources.push_back({209,stalthos::ResourceKind::Creek,exteriorWater.quantity,0});if(!bootstrapLedger->load(resources))throw std::runtime_error("exterior water initialization rejected");}
        uint64_t maxPersistedSessionId = 0;
        if (loadedSaveData && loadedSaveData->resourceLedger.has_value()) {
            for (const auto& s : loadedSaveData->resourceLedger->sessions) {
                if (s.sessionId > maxPersistedSessionId) maxPersistedSessionId = s.sessionId;
            }
        }
        if(loadedSaveData) for(const auto& s:loadedSaveData->faunaResourceSessions)
            maxPersistedSessionId=std::max(maxPersistedSessionId,s.second);
        if (maxPersistedSessionId == std::numeric_limits<uint64_t>::max()) {
            throw std::runtime_error("resource ledger session id overflow");
        }
        uint64_t nextInteractionSession = maxPersistedSessionId + 1;

        if(birdLife){auto resources=bootstrapLedger->continuationSnapshot().resources;resources.push_back({microforage.resource,stalthos::ResourceKind::Shrub,microforage.quantity,0});if(!bootstrapLedger->load(resources))throw std::runtime_error("microforage resource admission rejected");}
        std::map<uint64_t, stalthos::ResourceDto> cachedResourceDtos;
        for (const auto& res : resourceSnapshot()) {
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
        if (terrainStreamProbe) {
            const auto receipt = stalthos::engine::runTerrainStreamProbe(app,renderPath,window,output,duration,interactive,watershedJoinProbe);
            WriteJson(output / "runtime-receipt.json",receipt);
            return 0;
        }
        const auto terrainSurface=Terrain(scene);
        std::optional<stalthos::engine::PopulatedTerrain> populatedTerrain;
        if(populatedTraversal) {
            wi::renderer::SetOcclusionCullingEnabled(false);
            wi::renderer::SetMeshletOcclusionCullingEnabled(false);
        }
        if(populatedWorld) {
            populatedTerrain.emplace(world.worldSeed(),scene,connectedWorld);
            populatedTerrain->update(scene,0,0);
            productionGround=[&](double gx,double gz){return populatedTerrain->height(gx,gz);};
        }
        double exteriorWaterHeight=0;wi::ecs::Entity exteriorWaterEntity=wi::ecs::INVALID_ENTITY;
        if(connectedWorld){exteriorWaterHeight=exteriorWater.surfaceHeight([&](double gx,double gz){return populatedTerrain->height(gx,gz);});if(!exteriorWater.eligible({exteriorWater.bankX,Ground(float(exteriorWater.bankX),float(exteriorWater.bankZ)),exteriorWater.bankZ},Ground(float(exteriorWater.bankX),float(exteriorWater.bankZ)),exteriorWaterHeight,true))throw std::runtime_error("exterior spring water too far vertically from dry bank");exteriorWaterEntity=stalthos::engine::exteriorWaterScene(scene,exteriorWater,[&](double gx,double gz){return populatedTerrain->height(gx,gz);},exteriorWaterHeight);}
        if(exteriorWaterProbe){x=float(exteriorWater.bankX+(exteriorWaterProbe==2?28:16));z=float(exteriorWater.bankZ);yaw=-1.57079632679f;populatedTerrain->update(scene,x,z);} // Startup fixture only.
        if(exteriorBrowseEnabled){if(cachedShrubs.size()!=stalthos::fauna::ExteriorBrowsePatch::shrubIndex)throw std::runtime_error("exterior browse resource mapping requires original eight shrubs");cachedShrubs.push_back({stalthos::domain::PlacementKind::Shrub,exteriorBrowse.x,populatedTerrain->height(exteriorBrowse.x,exteriorBrowse.z),exteriorBrowse.z,exteriorBrowse.scale,0});}
        if(exteriorBrowseProbe){x=float(exteriorBrowse.x+(exteriorBrowseProbe==2?28:16));z=float(exteriorBrowse.z);yaw=-1.57079632679f;populatedTerrain->update(scene,x,z);} // Labelled startup fixture only.
        if(populatedResourceProbe) {x=0;z=0;} // Real creek location; eligibility still gates the contact.
        const auto organicRockCounts=stalthos::engine::populateOrganicRocks(scene,landscape,42);
        const auto meadowCounts=stalthos::engine::populateMeadowScene(scene,landscape,42);
        const auto shoreCounts=stalthos::engine::populateShoreScene(scene,landscape,2.0,1);
        const stalthos::world::ConnectedRoute exteriorShoreRoute(world.worldSeed());
        const auto exteriorShoreRocks=connectedWorld?stalthos::engine::exteriorShoreScene(scene,exteriorShoreRoute):0;
        if(shoreProbe){x=shoreProbe==2?254.f:184.f;z=24;yaw=pi*.5f;populatedTerrain->update(scene,x,z);}
        const auto sceneryCounts = stalthos::engine::populateEnvironmentScene(scene, landscape, 1);
        scene.Update(0.0f);
        const auto exteriorTrees=connectedWorld?stalthos::domain::exteriorWoodland([&](double gx,double gz){return populatedTerrain->height(gx,gz);}):std::vector<stalthos::domain::TreePlacement>{};
        if(woodlandProbe){x=146;z=47;yaw=pi*.5f;populatedTerrain->update(scene,x,z);}
        const auto woodlandReport = stalthos::engine::populateWoodlandScene(
            scene, landscape, woodlandAdultTree, woodlandSaplingTree, world.worldSeed(), 20,connectedWorld?&exteriorTrees:nullptr,connectedWorld?std::function<double(double,double)>([&](double gx,double gz){return populatedTerrain->height(gx,gz);}):std::function<double(double,double)>{});
        stalthos::domain::SaplingDisturbance sapling;wi::ecs::Entity saplingRoot=wi::ecs::INVALID_ENTITY;
        for(const auto& tree:woodlandReport.placedTrees)if(connectedWorld && tree.id==sapling.tree && tree.stage=="sapling"){saplingRoot=tree.rootEntity;sapling.position={tree.x,tree.y,tree.z};}
        std::optional<wi::scene::TransformComponent> saplingBase;if(saplingRoot!=wi::ecs::INVALID_ENTITY)saplingBase=*scene.transforms.GetComponent(saplingRoot);
        bool saplingVisualApplied=false,previousSaplingKey=false;json saplingSamples=json::array(),saplingEvents=json::array(),saplingCaptures=json::array();double nextSaplingSample=0;std::array<bool,4> saplingCaptured{};
        wi::SpriteFont saplingCue("F - disturb nearby sapling");saplingCue.params.size=22;saplingCue.params.h_align=wi::font::WIFALIGN_CENTER;saplingCue.params.color=wi::Color(255,220,160,255);saplingCue.params.shadowColor=wi::Color(0,0,0,255);saplingCue.SetHidden(true);renderPath.AddFont(&saplingCue);
        auto treeVisualSnapshot=[&](){json trees=json::array();for(const auto& tree:woodlandReport.placedTrees){auto* tr=scene.transforms.GetComponent(tree.rootEntity);json parts=json::array();for(size_t i=0;i<scene.objects.GetCount();++i)if(scene.Entity_IsDescendant(scene.objects.GetEntity(i),tree.rootEntity)){const auto& object=scene.objects[i];parts.push_back({{"entity",scene.objects.GetEntity(i)},{"mesh",object.meshID},{"color",{object.color.x,object.color.y,object.color.z,object.color.w}}});}trees.push_back({{"id",tree.id},{"stage",tree.stage},{"rotation",{tr->rotation_local.x,tr->rotation_local.y,tr->rotation_local.z,tr->rotation_local.w}},{"parts",parts}});}return trees;};
        if(saplingProbe){if(!saplingBase)throw std::runtime_error("sapling9006 unavailable");x=float(sapling.position.x-(saplingProbe==2?21.5:1.5));z=float(sapling.position.z);yaw=pi*.5f;populatedTerrain->update(scene,x,z);}
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
        stalthos::Vec3 foragingAnchor{};
        if(foragingProbe) {
            if(!importedInstances || cachedShrubs.empty()) throw std::runtime_error("foraging probe imported shrub missing");
            bool placed=false;for(const auto& shrub:cachedShrubs) {if(placed)break;for(int k=0;k<72;++k){const double a=k*6.283185307179586/72,r=foragingProbe==3?28:8,gx=shrub.x+r*std::sin(a),gz=shrub.z+r*std::cos(a);if(!landscape.inBounds(gx,gz)||landscape.waterContains(gx,gz))continue;bool valid=true;if(foragingProbe==3){for(const auto& other:cachedShrubs)if(std::hypot(gx-other.x,gz-other.z)<26)valid=false;}else {double previous=Ground(float(gx),float(gz));for(int step=1;step<=32;++step){double f=double(step)/32,sx=gx+(shrub.x-gx)*f,sz=gz+(shrub.z-gz)*f,y=Ground(float(sx),float(sz));if(landscape.waterContains(sx,sz)||std::abs(y-previous)>.15)valid=false;previous=y;}}if(valid){x=float(gx);z=float(gz);yaw=float(std::atan2(shrub.x-gx,shrub.z-gz));foragingAnchor={shrub.x,shrub.y,shrub.z};placed=true;break;}}}if(!placed)throw std::runtime_error("no dry foraging startup fixture");
        }
        std::vector<wi::ecs::Entity> microforageObjects;if(birdLife)microforageObjects=stalthos::engine::microforageScene(scene,microforage,[&](double gx,double gz){return populatedTerrain->height(gx,gz);});
        stalthos::engine::ReedPresentation reedPresentation;
        stalthos::engine::BrackenPresentation brackenPresentation;
        stalthos::engine::BirdPresentation birdPresentation;
        double birdDistance=0;json birdSamples=json::array(),birdCaptures=json::array();std::array<bool,4> birdCaptured{};double nextBirdSample=0;bool birdDeathRequested=false;
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
        if (!creature.empty() && !birdLife && !brackenLife && !reedLife) {
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
        } else if(reedLife){reedPresentation=stalthos::engine::reedcrestScene(scene);hero=reedPresentation.root;heroMotionRoot=hero;
        } else if(brackenLife){brackenPresentation=stalthos::engine::brackenbackScene(scene);hero=brackenPresentation.root;heroMotionRoot=hero;
        } else if(birdLife){birdPresentation=stalthos::engine::thistletailScene(scene);hero=birdPresentation.root;heroMotionRoot=hero;
        } else {
            hero=Box(scene,"hero locomotion marker",{0,Ground(0,0)+.65f,0},{.45f,.4f,1.f},{.48f,.30f,.16f,1});
            heroMotionRoot=hero;
        }
        scene.weather.SetRealisticSky(true);
        scene.weather.SetRealisticSkyAerialPerspective(false);
        scene.weather.SetRealisticSkyHighQuality(false);
        scene.weather.ambient = {.24f,.24f,.24f}; scene.weather.sunDirection = {-.48f,.8f,-.36f};
        auto sun = scene.Entity_CreateLight("draft sun",{0,30,0},{1,.96f,.9f},5,1000,wi::scene::LightComponent::DIRECTIONAL);
        if (auto* sunLight = scene.lights.GetComponent(sun)) {
            sunLight->SetCastShadow(true);
        }
        scene.transforms.GetComponent(sun)->RotateRollPitchYaw(XMFLOAT3(.6751f,-.9273f,0));
        auto nightFill=wi::ecs::INVALID_ENTITY;if(connectedWorld){nightFill=scene.Entity_CreateLight("proposed night visibility fill",{0,30,0},{.42f,.58f,1},0,1000,wi::scene::LightComponent::DIRECTIONAL);scene.lights.GetComponent(nightFill)->SetCastShadow(false);scene.transforms.GetComponent(nightFill)->RotateRollPitchYaw(XMFLOAT3(.6751f,.9273f,0));}
        renderPath.init(app.canvas); renderPath.Load();
        renderPath.setAO(wi::RenderPath3D::AO_MSAO);
        renderPath.setSSREnabled(false);
        renderPath.setSSGIEnabled(false); renderPath.setFSREnabled(false); renderPath.setFSR2Enabled(false);
        renderPath.setEyeAdaptionEnabled(false); renderPath.setMotionBlurEnabled(false); renderPath.setDepthOfFieldEnabled(false);
        renderPath.setShadowsEnabled(true); app.ActivatePath(&renderPath);
        wi::scene::GetCamera().CreatePerspective(width,height,.1f,1000,pi/3);
        stalthos::fauna::CarnivoreAwareness carnivoreAwareness;
        stalthos::Identity carnivoreWatched=0;stalthos::Vec3 carnivoreAnchor{};
        json carnivoreSamples=json::array(),carnivoreCaptures=json::array();double nextCarnivoreSample=0;bool carnivoreCancelled=false;std::array<bool,3> carnivoreCaptured{};
        uint64_t carnivoreRecallRequests=0;
        wi::SpriteFont carnivoreCue("Longstride nearby - potential danger");carnivoreCue.params.size=24;carnivoreCue.params.h_align=wi::font::WIFALIGN_CENTER;carnivoreCue.params.color=wi::Color(255,205,135,255);carnivoreCue.params.shadowColor=wi::Color(0,0,0,255);carnivoreCue.SetHidden(true);renderPath.AddFont(&carnivoreCue);
        stalthos::fauna::AwarenessTuning wildlifeTuning;
        json wildlifeSamples=json::array(),wildlifeCaptures=json::array();
        std::array<bool,3> wildlifeCaptured{};
        double nextWildlifeSample=0;
        stalthos::Identity wildlifeWatched=0;stalthos::Vec3 wildlifeAnchor{};
        stalthos::fauna::PeerCompanionship peerCompanion;json peerSamples=json::array(),peerCaptures=json::array(),peerEvents=json::array();double nextPeerSample=0;std::array<bool,4> peerCaptured{};stalthos::Identity previousPeer=0;bool previousPeerPresent=false;stalthos::Vec3 peerAnchor{};json npcPeerSamples=json::array(),npcPeerCaptures=json::array(),npcPeerEvents=json::array();bool lastNpcPeerMotion=false;double nextNpcPeerSample=0;std::array<bool,3> npcPeerCaptured{};
        wi::SpriteFont peerCue("Peer nearby");peerCue.params.size=22;peerCue.params.h_align=wi::font::WIFALIGN_CENTER;peerCue.params.color=wi::Color(180,240,220,255);peerCue.params.shadowColor=wi::Color(0,0,0,255);peerCue.SetHidden(true);renderPath.AddFont(&peerCue);
        json lifeStageSamples=json::array(),lifeStageMilestones=json::array(),lifeStageCaptures=json::array();double nextLifeStageSample=0,lastLifeStageMilestone=-1e30;std::array<bool,4> lifeStageCaptured{};
        wi::SpriteFont lifeStageCue("Life stage: hatchling");lifeStageCue.params.size=22;lifeStageCue.params.h_align=wi::font::WIFALIGN_CENTER;lifeStageCue.params.color=wi::Color(230,235,185,255);lifeStageCue.params.shadowColor=wi::Color(0,0,0,255);lifeStageCue.SetHidden(!connectedWorld);renderPath.AddFont(&lifeStageCue);
        stalthos::HeroSim heroIntent;
        stalthos::domain::HeroDrinkAutonomy heroAutonomy;
        stalthos::domain::HeroExploration heroExploration;
        const bool explorationEnabled=connectedWorld && !reedProbe && !protectionProbe && !birdProbe && !nestProbe && !carnivoreProbe && !shoreProbe && !npcPeerProbe && !audioProbe && !saplingProbe && !diagCount && !deathNextLifeProbe && !lifeStageProbe && !peerProbe && !woodlandProbe && !dayNightProbe && !exteriorWaterProbe && !exteriorBrowseProbe && !destinationProbe && !exposureProbe && !restProbe && !foragingProbe && !autonomyProbe && !wildlifeProbe && !connectedRouteWalk && !weatherPreview;
        wi::SpriteFont explorationCue("Exploring nearby ground - WASD takes control");explorationCue.params.size=22;explorationCue.params.h_align=wi::font::WIFALIGN_CENTER;explorationCue.params.color=wi::Color(220,240,210,255);explorationCue.params.shadowColor=wi::Color(0,0,0,255);explorationCue.SetHidden(true);renderPath.AddFont(&explorationCue);
        json explorationSamples=json::array(),explorationCaptures=json::array(),explorationEvents=json::array();double nextExplorationSample=0;std::array<bool,3> explorationCaptured{};bool explorationNeedInjected=false,explorationDestinationInjected=false;
        wi::SpriteFont phasePreviewCue(reedProbe?"Reedcrest test - body/NPC needs/route/input/noon fixtures":protectionProbe?"Protection test - synthetic adult/body/NPC edge-position/condition, input/noon fixtures":birdProbe?"Thistletail test - synthetic hunger/start/input/death/N/noon fixtures":nestProbe?(hatchProbe==1?"Hatch test - adult/start/time/B/movement/death/N fixtures":hatchProbe==2?"Hatch camera test - adult/start/time/B/camera fixtures":"Nest test - synthetic adult start, B intention, reverse input and noon"):shoreProbe?"Shore test - synthetic local start/input/noon, authored cove":npcPeerProbe?"NPC pair test - synthetic body/actor/needs/noon fixtures":audioProbe?"Body audio test - synthetic body or camera input":saplingProbe?"Sapling test - synthetic body/camera start, input and noon":explorationProbe?"Exploration test - bank start/input/need/destination/noon fixtures":deathNextLifeProbe?"Death test - near-terminal age, thirst, input and noon fixtures":lifeStageProbe?"Growth test - near-adult age, fatigue/input/noon fixtures":peerProbe?"Peer test - synthetic start/satisfied NPC/input/noon light":woodlandProbe?"Woodland entry test - synthetic start/input/noon light":dayNightProbe==1?"Synthetic lighting preview - clocks run normally":"Night controls test - synthetic fatigue/input");phasePreviewCue.params.size=22;phasePreviewCue.params.color=wi::Color(255,240,180,255);phasePreviewCue.params.shadowColor=wi::Color(0,0,0,255);phasePreviewCue.SetHidden(!reedProbe && !protectionProbe && !birdProbe && !nestProbe && !dayNightProbe && !woodlandProbe && !peerProbe && !lifeStageProbe && !deathNextLifeProbe && !explorationProbe && !saplingProbe && !audioProbe && !npcPeerProbe && !shoreProbe);renderPath.AddFont(&phasePreviewCue);
        wi::SpriteFont destinationCue("Right-click nearby ground to go there"),destinationPin("+");
        for(auto* font:{&destinationCue,&destinationPin}){font->params.size=22;font->params.h_align=wi::font::WIFALIGN_CENTER;font->params.color=wi::Color(255,240,180,255);font->params.shadowColor=wi::Color(0,0,0,255);font->SetHidden(!connectedWorld);renderPath.AddFont(font);}
        destinationPin.SetHidden(true);std::string destinationStatus="ready";json destinationEvents=json::array(),destinationSamples=json::array(),destinationCaptures=json::array();std::array<bool,3> destinationCaptured{};double nextDestinationSample=0;bool destinationInjected=false;
        auto supportedDestination=[&](const stalthos::Vec3& body,const stalthos::Vec3& target){
            if(!stalthos::finite(target)||std::hypot(target.x-body.x,target.z-body.z)>stalthos::domain::HeroDestination::radius)return false;
            try{const double length=std::hypot(target.x-body.x,target.z-body.z);const int count=std::max(1,int(std::ceil(length/.5)));double previousY=body.y;
                for(int i=1;i<=count;++i){const double f=double(i)/count,gx=body.x+(target.x-body.x)*f,gz=body.z+(target.z-body.z)*f,gy=populatedTerrain->height(gx,gz);if(std::abs(gy-previousY)>.6*length/count+.02 || (landscape.inBounds(gx,gz)&&landscape.waterContains(gx,gz)))return false;previousY=gy;}
                if(std::abs(previousY-target.y)>.15)return false;
                const DirectX::XMFLOAT3 start{float(body.x),float(body.y+.8),float(body.z)},end{float(target.x),float(target.y+.8),float(target.z)};const auto delta=DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&end),DirectX::XMLoadFloat3(&start));const float distance=DirectX::XMVectorGetX(DirectX::XMVector3Length(delta));
                if(distance>1.8f){wi::primitive::Ray ray(DirectX::XMLoadFloat3(&start),DirectX::XMVector3Normalize(delta),1.3f,distance-.3f);if(scene.Intersects(ray).entity!=wi::ecs::INVALID_ENTITY)return false;}
                return true;
            }catch(const std::out_of_range&){return false;}
        };
        stalthos::domain::HeroRestAutonomy heroRest;
        wi::SpriteFont exposureCue("Weather exposure - tiring faster; seek a rock lee");exposureCue.params.size=22;exposureCue.params.h_align=wi::font::WIFALIGN_CENTER;exposureCue.params.color=wi::Color(255,225,160,255);exposureCue.params.shadowColor=wi::Color(0,0,0,255);exposureCue.SetHidden(true);renderPath.AddFont(&exposureCue);
        stalthos::environment::ExposureResponse exposureResponse;json exposureSamples=json::array(),exposureCaptures=json::array();std::array<bool,3> exposureCaptured{};double nextExposureSample=0;bool previousExposureLee=false;
        bool resting=false,previousRestCue=false;unsigned restCueChanges=0;
        wi::SpriteFont restCue("Resting - recovering energy");restCue.params.size=24;restCue.params.h_align=wi::font::WIFALIGN_CENTER;restCue.params.color=wi::Color(255,255,255,255);restCue.params.shadowColor=wi::Color(0,0,0,255);restCue.SetHidden(true);renderPath.AddFont(&restCue);
        float cueCanvasWidth=-1,cueCanvasHeight=-1;
        json restSamples=json::array(),restCaptures=json::array();std::array<bool,3> restCaptured{};double nextRestSample=0;
        stalthos::domain::DrinkAction autonomyAction;
        bool autonomyBrowsing=false;uint64_t autonomyResource=100;
        json autonomySamples=json::array(),autonomyContacts=json::array(),autonomyCaptures=json::array();
        std::array<bool,3> autonomyCaptured{};
        unsigned previousInteractionOwner=0;
        double nextAutonomySample=0;
        bool syntheticDirect=false;
        const bool heroAutonomyEnabled=connectedWorld && !connectedRouteWalk && !weatherPreview;
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
        std::map<std::pair<wi::ecs::Entity, uint8_t>, XMFLOAT4> previousFrameBonePoses;
        bool previousIsDisplaced = false;
        bool transitionFinalCaptureSaved = false;
        double finalElapsed = 0.0;
        stalthos::domain::AnimationBlend::State blendState;
        blendState.targetClip = stalthos::domain::AnimationBlend::ClipKind::Idle;
        blendState.transitionProgress = 1.0;
        constexpr double kCrossfadeDurationSeconds = 0.25;
        bool crossfadeWeightsNonzeroOnlyDuringBlend = true;
        uint64_t crossfadeBlendFramesTotal = 0;
        stalthos::engine::AnimationTopologyAnalysis crossfadeTopology;
        uint32_t crossfadeTopologyAnalysisCount = 0;
        if (auto* walk = scene.animations.GetComponent(sceneWalkAnimEntity)) {
            if (auto* idle = scene.animations.GetComponent(sceneIdleAnimEntity)) {
                crossfadeTopology = stalthos::engine::analyzeAnimationTopology(
                    *walk, sceneWalkAnimName, *idle, sceneIdleAnimName);
                ++crossfadeTopologyAnalysisCount;
            }
        }

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
            scene.Update(0.0f);
            latestNativeSoleDiagnostic = nativeSoleAdapter.evaluate(scene, landscape);
        }

        const auto heroAnimationCount=scene.animations.GetCount();
        bool previousInspectionKey = false, previousToggleKey = false;
        json connectedTrajectory=json::array(),shoreTrajectory=json::array(),shoreCaptures=json::array();std::array<bool,4> shoreCaptured{};double nextShoreSample=0;
        double connectedMaxGrade=0,connectedMaxStep=0;
        stalthos::environment::BodyWeatherResponse bodyWeather;
        stalthos::environment::LocalWeatherResponse localWeather;
        json weatherResponseSamples=json::array(),weatherResponseCaptures=json::array();
        std::array<bool,3> weatherCaptured{};
        double nextWeatherSample=0;
        const float clearSunIntensity=scene.lights.GetComponent(sun)->intensity;
        bool faunaPopulationRestored=false;
        stalthos::engine::FaunaScene faunaScene;
        std::vector<stalthos::fauna::HabitatSite> faunaSites;
        std::array<uint64_t,4> faunaActivityFrames{};
        uint64_t faunaSimulationFrames=0;
        uint64_t faunaResourceEvents=0;
        stalthos::world::Archipelago archipelago(world.worldSeed());
        std::vector<stalthos::engine::RegionConditionsSnapshot> regionalConditions;
        double nextConditionsUpdate=0;
        uint64_t conditionsUpdates=0;
        auto actorJson=[](const std::vector<stalthos::fauna::ActorView>& actors) {
            json result=json::array();
            for(const auto& a:actors) result.push_back({{"id",a.id},{"species",stalthos::heroSpeciesNames[static_cast<unsigned>(a.species)]},
                {"x",a.position.x},{"y",a.position.y},{"z",a.position.z},{"heading",a.headingRadians},{"age",a.calendarAge},
                {"distance_m",a.distanceTravelledMeters},{"hunger",a.hunger},{"thirst",a.thirst},{"fatigue",a.fatigue},
                {"alive",a.alive},{"activity",static_cast<unsigned>(a.activity)}});
            return result;
        };
        auto populationJson=[](const stalthos::fauna::PopulationSnapshot& snapshot) {
            json actors=json::array();
            for(const auto& a:snapshot.actors) actors.push_back({{"id",a.id},{"species",static_cast<unsigned>(a.species)},
                {"x",a.position.x},{"y",a.position.y},{"z",a.position.z},{"heading",a.headingRadians},
                {"distance_m",a.distanceTravelledMeters},{"site",a.site},{"target",a.target},{"age",a.calendarAge},
                {"stage",static_cast<unsigned>(a.stage)},{"hunger",a.hunger},{"thirst",a.thirst},{"fatigue",a.fatigue},
                {"injury",a.injury},{"is_resting",a.isResting},{"activity",static_cast<unsigned>(a.activity)}});
            return json{{"seed",snapshot.seed},{"region",snapshot.region},{"habitat_signature",snapshot.habitatSignature},{"actors",actors}};
        };
        // This bounded watershed owns the original habitat. Physical activation
        // depends only on the body; camera residency never grants simulation.
        auto watershedPhysical=[](double bx,double bz) { return std::hypot(bx,bz)<=192.0; };
        const double traversalStartX=x;
        bool faunaPhysical=watershedPhysical(x,z);
        const bool faunaPhysicalInitial=faunaPhysical;
        uint64_t faunaDormantFrames=0;
        double faunaDormantPhysicalSeconds=0,faunaDormantCalendarSeconds=0;
        json faunaTransitions=json::array(), faunaDormantIntervals=json::array();
        json faunaFullInitial, dormantStart;
        double dormantPhysical=0,dormantCalendar=0,dormantFood=0,dormantWater=0;
        uint64_t dormantEvents=0;
        std::vector<std::pair<stalthos::Identity,uint64_t>> dormantSessions;
        auto beginFaunaDormancy=[&]() {
            dormantStart=populationJson(faunaPopulation->snapshot());
            dormantPhysical=0;dormantCalendar=0;
            dormantFood=faunaPopulation->consumedFood();dormantWater=faunaPopulation->consumedWater();
            dormantEvents=faunaResourceEvents;dormantSessions=faunaPopulation->sessions();
        };
        auto endFaunaDormancy=[&](bool reactivated) {
            faunaDormantIntervals.push_back({{"before",dormantStart},{"after",populationJson(faunaPopulation->snapshot())},
                {"physical_seconds",dormantPhysical},{"calendar_seconds",dormantCalendar},{"reactivated",reactivated},
                {"food_consumed",faunaPopulation->consumedFood()-dormantFood},
                {"water_consumed",faunaPopulation->consumedWater()-dormantWater},
                {"resource_events",faunaResourceEvents-dormantEvents},
                {"resource_sessions_unchanged",dormantSessions==faunaPopulation->sessions()}});
        };
        bool reedFollowOwned=false,reedNeedInjected=false;stalthos::domain::ReedPeerDecision reedDecision;double reedGoalRefresh=0;stalthos::Identity reedFixturePeer=0;
        json reedSamples=json::array(),reedEvents=json::array(),reedCaptures=json::array();double nextReedSample=0;std::array<bool,5> reedCaptured{};
        bool defenseOwned=false;double defenseCooldown=0,braceUntil=0;std::optional<stalthos::domain::ProtectionObservation> defenseObservation;std::uint64_t defenseStarts=0;stalthos::Identity fixtureSmall=0,fixtureThreat=0;
        json defenseSamples=json::array(),defenseEvents=json::array(),defenseCaptures=json::array();double nextDefenseSample=0;std::array<bool,4> defenseCaptured{};
        wi::SpriteFont defenseCue("");defenseCue.params.size=22;defenseCue.params.h_align=wi::font::WIFALIGN_CENTER;defenseCue.params.color=wi::Color(255,218,145,255);defenseCue.params.shadowColor=wi::Color(0,0,0,255);defenseCue.SetHidden(true);renderPath.AddFont(&defenseCue);
        wi::SpriteFont distressCue("Distressed animal - critical hunger/thirst");distressCue.params.size=20;distressCue.params.h_align=wi::font::WIFALIGN_CENTER;distressCue.params.color=wi::Color(255,180,120,255);distressCue.params.shadowColor=wi::Color(0,0,0,255);distressCue.SetHidden(true);renderPath.AddFont(&distressCue);
        json faunaInitial=json::array();
        json faunaSessionsInitial=json::array();
        auto ledgerJson=[](const stalthos::ResourceLedgerDto& dto) {
            json resources=json::array(),sessions=json::array();
            for(const auto& r:dto.resources) resources.push_back({{"id",r.id},{"kind",static_cast<unsigned>(r.kind)},{"initial_quantity",r.initialQuantity},{"consumed_quantity",r.consumedQuantity}});
            for(const auto& k:dto.sessions) sessions.push_back({{"session_id",k.sessionId},{"resource_id",k.resourceId},{"amount",k.amount}});
            return json{{"resources",resources},{"sessions",sessions}};
        };
        json faunaLedgerInitial;
        if(populatedWorld) {
            faunaScene.cacheGround(scene);
            faunaScene.setGroundQuery([&](double gx,double gz){return populatedTerrain->height(gx,gz);});
            populatedTerrain->update(scene,x,z);
            std::vector<stalthos::Vec3> food;
            for(const auto& p:cachedShrubs) food.push_back({p.x,p.y,p.z});
            faunaSites=faunaScene.habitat(landscape,food);
            // Construction probe only: do not replace legacy sites/signature or create actors.
            const auto candidate=stalthos::fauna::regionalHabitatCandidate(world.worldSeed(),1001,
                {0,-12,32,0},[&](double gx,double gz)->std::optional<stalthos::fauna::HabitatGround> {
                    try {
                        const double gy=populatedTerrain->height(gx,gz), h=.25;
                        const double dx=(populatedTerrain->height(gx+h,gz)-populatedTerrain->height(gx-h,gz))/(2*h);
                        const double dz=(populatedTerrain->height(gx,gz+h)-populatedTerrain->height(gx,gz-h))/(2*h);
                        const bool wet=landscape.waterContains(gx,gz);
                        bool browse=false;
                        for(const auto& f:food) if(std::hypot(std::hypot(f.x-gx,f.z-gz),f.y-gy)<=.8) browse=true;
                        bool water=false;
                        // Match the contact radius in 3D and reject banks beyond draft vertical reach.
                        for(double reach:{.25,.5,.75}) for(const auto offset:std::array<std::pair<double,double>,4>{{{reach,0},{-reach,0},{0,reach},{0,-reach}}}) {
                            const double wx=gx+offset.first,wz=gz+offset.second;
                            const double drop=std::abs(gy-landscape.waterSurface(wx,wz));
                            if(!wet && landscape.waterContains(wx,wz) && drop<=.45 && std::hypot(reach,drop)<=.8) water=true;
                        }
                        return stalthos::fauna::HabitatGround{gy,std::hypot(dx,dz),!wet,wet,browse,water};
                    } catch(const std::out_of_range&) { return std::nullopt; }
                },{1,.5,.6,20000});
            json candidateSites=json::array(); std::size_t candidateLinks=0;
            for(const auto& site:candidate.sites) {
                candidateLinks+=site.neighbors.size();
                candidateSites.push_back({{"x",site.position.x},{"y",site.position.y},{"z",site.position.z},
                    {"browse",site.browse},{"freshwater",site.freshwater},{"neighbors",site.neighbors}});
            }
            WriteJson(output/"regional-habitat-candidate.json",{{"schema_version",1},{"region",1001},
                {"status",static_cast<unsigned>(candidate.status)},{"ground_queries",candidate.groundQueries},
                {"ground_source","joined rendered triangles"},{"bounds",{0,-12,32,0}},
                {"lattice_m",1},{"route_sample_m",.5},{"max_slope",.6},{"contact_radius_m",.8},{"max_water_vertical_reach_m",.45},{"directed_links",candidateLinks},
                {"sites",candidateSites},{"creates_population",false},{"legacy_habitat_replaced",false},
                {"scope","watershed construction probe; no regional registry or cross-region links"}});

            std::set<stalthos::Identity> excluded{heroId};
            for(const auto& active:world.snapshot().active) excluded.insert(active.first);
            for(const auto& r:world.snapshot().retained) excluded.insert(r.id);
            for(const auto& d:world.snapshot().deadHeroes) excluded.insert(d.first);
            stalthos::fauna::Population legacyPopulation(world.worldSeed(),1001,faunaSites,8,excluded);
            if(connectedWorld) {
                auto habitat=stalthos::fauna::exteriorHabitat(faunaSites,[&](double gx,double gz){return populatedTerrain->height(gx,gz);},[&](double gx,double gz){return landscape.inBounds(gx,gz)?landscape.waterContains(gx,gz):populatedTerrain->height(gx,gz)<=0;});
                legacyPopulation=stalthos::fauna::withExteriorWater(legacyPopulation,habitat,excluded,exteriorBrowseEnabled,exteriorWater);faunaSites=habitat.sites;
                json nodes=json::array();for(const auto& site:faunaSites)nodes.push_back({{"x",site.position.x},{"y",site.position.y},{"z",site.position.z},{"neighbors",site.neighbors},{"browse",site.browse},{"freshwater",site.freshwater}});
                WriteJson(output/"exterior-residents-habitat.json",{{"sites",nodes},{"resident_sites",habitat.residents},{"max_measured_grade",habitat.maxMeasuredGrade},{"sample_count",habitat.samples},{"sample_m",.5},{"max_grade",.6},{"original_eight_state_preserved",true},{"new_residents",4},{"activation","same coarse body-radius192m watershed group; no separate region"},{"exterior_resource_anchors",exteriorBrowseEnabled?2:1},{"exterior_water_resource",209},{"exterior_food_resource",exteriorBrowseEnabled?208:0}});
            }            if(loadedSaveData && loadedSaveData->faunaPopulation) {
                legacyPopulation.restore(*loadedSaveData->faunaPopulation,excluded);
                faunaPopulationRestored=true;
            }
            if(npcPeerProbe) {
                // Diagnostic placement only: two existing Smalltrotter IDs on the existing final dry edge.
                auto fixture=legacyPopulation.snapshot();std::vector<stalthos::fauna::ActorSnapshot*> pair;
                for(auto& a:fixture.actors)if(a.species==stalthos::HeroSpecies::Smalltrotter)pair.push_back(&a);
                std::sort(pair.begin(),pair.end(),[](auto a,auto b){return a->id<b->id;});
                if(pair.size()<2)throw std::runtime_error("NPC pair fixture missing existing conspecifics");
                const auto bank=faunaSites.size()-2,other=faunaSites.size()-1;
                for(std::size_t i=0;i<2;++i) {
                    auto& a=*pair[i];a.site=i?other:bank;a.target=bank;a.position=faunaSites[bank].position;
                    if(i){const auto end=faunaSites[other].position;const double f=4/std::hypot(end.x-a.position.x,end.z-a.position.z);a.position={a.position.x+(end.x-a.position.x)*f,a.position.y+(end.y-a.position.y)*f,a.position.z+(end.z-a.position.z)*f};}
                    a.hunger=.2;a.thirst=(npcPeerProbe==3 && i==0)?.478:.2;a.fatigue=.2;
                    a.activity=i?stalthos::fauna::Activity::Travel:stalthos::fauna::Activity::Rest;a.isResting=!i;
                }
                legacyPopulation.restore(fixture,excluded);x=124;z=36;yaw=2.2142974f;populatedTerrain->update(scene,x,z);
            }
            if(peerProbe){auto snapshot=legacyPopulation.snapshot();bool chosen=false;for(auto& a:snapshot.actors)if(!chosen && a.species==heroSpecies && a.position.x>96){a.target=a.site;a.hunger=.2;a.thirst=.2;a.fatigue=.2;a.activity=stalthos::fauna::Activity::Rest;a.isResting=true;peerAnchor=a.position;chosen=true;}if(!chosen)throw std::runtime_error("no stationary conspecific fixture");legacyPopulation.restore(snapshot,excluded);x=float(peerAnchor.x+(peerProbe==2?24:7));z=float(peerAnchor.z);yaw=pi*.5f;populatedTerrain->update(scene,x,z);}
            if(protectionProbe){auto state=legacyPopulation.snapshot();bool found=false;
                for(auto& threat:state.actors){if(found)break;if(threat.species!=stalthos::HeroSpecies::Longstride || threat.stage==stalthos::LifeStage::Dead || threat.calendarAge<stalthos::getHeroProfile(threat.species).hatchlingDuration+stalthos::getHeroProfile(threat.species).juvenileDuration || std::hypot(threat.position.x-faunaSites.at(threat.site).position.x,threat.position.z-faunaSites.at(threat.site).position.z)>1e-6)continue;
                    for(auto neighbor:faunaSites.at(threat.site).neighbors){if(found)break;const auto start=faunaSites.at(threat.site).position,end=faunaSites.at(neighbor).position;const double length=std::hypot(end.x-start.x,end.z-start.z);if(length<4.2)continue;const double fraction=4/length;const stalthos::Vec3 distressedPosition{start.x+(end.x-start.x)*fraction,start.y+(end.y-start.y)*fraction,start.z+(end.z-start.z)*fraction};
                        const double dx=threat.position.x-distressedPosition.x,dz=threat.position.z-distressedPosition.z,d=std::hypot(dx,dz);if(d<3 || d>6 || std::abs(threat.position.y-distressedPosition.y)>1)continue;
                        const double tx=distressedPosition.x+dx/d,tz=distressedPosition.z+dz/d,offset=protectionProbe==3?36:3;const double gx=tx-dz/d*offset,gz=tz+dx/d*offset;
                        bool supported=true;double previous=Ground(float(gx),float(gz));for(int i=0;i<=64;++i){const double f=double(i)/64,sx=gx+(tx-gx)*f,sz=gz+(tz-gz)*f,h=Ground(float(sx),float(sz));if((landscape.inBounds(sx,sz)&&landscape.waterContains(sx,sz)) || std::abs(h-previous)>.35)supported=false;previous=h;}if(!supported)continue;
                        for(auto& distressedActor:state.actors){if(distressedActor.id==threat.id || distressedActor.stage==stalthos::LifeStage::Dead)continue;
                            distressedActor.calendarAge=0;distressedActor.stage=stalthos::LifeStage::Hatchling;distressedActor.hunger=.99;distressedActor.thirst=.1;distressedActor.fatigue=0;distressedActor.site=threat.site;distressedActor.target=neighbor;distressedActor.position=distressedPosition;distressedActor.activity=stalthos::fauna::Activity::Travel;distressedActor.isResting=false;
                            threat.hunger=0;threat.thirst=0;threat.fatigue=.9;threat.target=threat.site;threat.activity=stalthos::fauna::Activity::Rest;threat.isResting=true;
                            fixtureSmall=distressedActor.id;fixtureThreat=threat.id;x=float(gx);z=float(gz);yaw=float(std::atan2(tx-gx,tz-gz));defenseEvents.push_back({{"event","synthetic_existing_actor_edge_fixture"},{"actor",distressedActor.id},{"threat",threat.id},{"site",distressedActor.site},{"target",distressedActor.target},{"x",distressedPosition.x},{"y",distressedPosition.y},{"z",distressedPosition.z}});found=true;break;
                        }
                    }
                }if(!found)throw std::runtime_error("no supported original habitat edge for protection fixture");legacyPopulation.restore(state,excluded);populatedTerrain->update(scene,x,z);
            }
            if(reedProbe){auto state=legacyPopulation.snapshot();bool found=false;for(auto& actor:state.actors){if(found)break;if(actor.species!=stalthos::HeroSpecies::Reedcrest || actor.stage==stalthos::LifeStage::Dead)continue;const auto at=faunaSites.at(actor.site).position;if(std::hypot(actor.position.x-at.x,actor.position.z-at.z)>1e-6)continue;
                for(auto neighbor:faunaSites.at(actor.site).neighbors){const auto end=faunaSites.at(neighbor).position;const double dx=end.x-at.x,dz=end.z-at.z,d=std::hypot(dx,dz);if(d<4)continue;const double offset=reedProbe==3?30:6.5,gx=at.x-dx/d*offset,gz=at.z-dz/d*offset;bool dry=true;if(reedProbe==3)for(const auto& other:state.actors)if(other.species==stalthos::HeroSpecies::Reedcrest && std::hypot(gx-other.position.x,gz-other.position.z)<20)dry=false;double previous=Ground(float(reedProbe==5?at.x-dx/d*38:gx),float(reedProbe==5?at.z-dz/d*38:gz));for(int i=0;i<=64;++i){const double f=double(i)/64,checkOffset=reedProbe==5?38:offset,sx=at.x-dx/d*checkOffset*(1-f),sz=at.z-dz/d*checkOffset*(1-f),h=Ground(float(sx),float(sz));if((landscape.inBounds(sx,sz)&&landscape.waterContains(sx,sz)) || std::abs(h-previous)>.3)dry=false;previous=h;}if(!dry)continue;
                    actor.hunger=.1;actor.thirst=.1;actor.fatigue=.1;actor.activity=stalthos::fauna::Activity::Travel;actor.target=neighbor;actor.isResting=false;reedFixturePeer=actor.id;x=float(gx);z=float(gz);yaw=float(std::atan2(dx,dz));found=true;break;
                }
            }if(!found)throw std::runtime_error("no supported real Reedcrest peer fixture");legacyPopulation.restore(state,excluded);populatedTerrain->update(scene,x,z);}
            std::vector<stalthos::fauna::RegionalResourceAnchor> anchors;
            for(std::size_t site=0;site<faunaSites.size();++site) {
                const auto& habitat=faunaSites[site];
                if(habitat.freshwater) anchors.push_back({site,connectedWorld && std::hypot(habitat.position.x-exteriorWater.bankX,habitat.position.z-exteriorWater.bankZ)<.01?209u:100u,habitat.position});
                if(habitat.browse) {
                    double nearest=.8*.8; std::optional<std::size_t> shrub;
                    for(std::size_t i=0;i<food.size();++i) {
                        const auto dx=food[i].x-habitat.position.x,dy=food[i].y-habitat.position.y,dz=food[i].z-habitat.position.z;
                        const auto d=dx*dx+dy*dy+dz*dz;
                        if(d<=nearest) {nearest=d;shrub=i;}
                    }
                    if(!shrub) throw std::runtime_error("native browse site has no rendered shrub anchor");
                    anchors.push_back({site,200+*shrub,food[*shrub]});
                }
            }
            stalthos::engine::validateNativeResources(bootstrapLedger->continuationSnapshot(),anchors,connectedWorld);
            faunaPopulation.emplace(std::move(legacyPopulation),bootstrapLedger->continuationSnapshot(),
                loadedSaveData ? loadedSaveData->faunaResourceSessions : std::vector<std::pair<stalthos::Identity,uint64_t>>{},
                anchors,nextInteractionSession);
            if(loadedHostSave && loadedHostSave->regional) {
                auto detached=stalthos::engine::restoreNativeSave(*loadedHostSave,*faunaPopulation,populatedWorld);
                simulationOwner=std::move(detached); // One complete owner assignment before any simulation/contact.
                // Successor selection is a new explicit user transition, applied only
                // after complete persisted restoration; it must not be overwritten.
                if(successorId) world.setHero(heroId,world.snapshot().deadHeroes.back().second);
            }
            if(carnivoreProbe) {
                const auto initial=faunaPopulation->snapshot();
                for(const auto& a:initial.actors) if(!carnivoreWatched && a.species==stalthos::HeroSpecies::Longstride && (a.stage==stalthos::LifeStage::Adult || a.stage==stalthos::LifeStage::Old)) {
                    // Labelled body startup only: retained population actor is never moved or duplicated.
                    for(int angle=0;angle<24;++angle){const double heading=angle*6.283185307179586/24,dx=std::sin(heading),dz=std::cos(heading),offset=carnivoreProbe==3?28:8;
                        bool dry=true;double previous=Ground(float(a.position.x-dx*offset),float(a.position.z-dz*offset));
                        for(int step=0;step<=56;++step){const double d=offset+step*.5,gx=a.position.x-dx*d,gz=a.position.z-dz*d,gy=Ground(float(gx),float(gz));if((landscape.inBounds(gx,gz) && landscape.waterContains(gx,gz))||!std::isfinite(gy)||std::abs(gy-previous)>.3){dry=false;break;}previous=gy;}
                        if(!dry)continue;
                        const double gx=a.position.x-dx*offset,gz=a.position.z-dz*offset;
                        if(carnivoreProbe==3){bool remote=true;for(const auto& other:initial.actors)if(other.species==stalthos::HeroSpecies::Longstride && std::hypot(other.position.x-gx,other.position.z-gz)<18)remote=false;if(!remote)continue;}
                        carnivoreWatched=a.id;carnivoreAnchor=a.position;x=float(gx);z=float(gz);yaw=float(heading);break;
                    }
                }
                if(!carnivoreWatched) throw std::runtime_error("no retained eligible adult Longstride for diagnostic encounter");
                populatedTerrain->update(scene,x,z);
            }
            if(wildlifeProbe) {
                bool selected=false;
                const auto initial=faunaPopulation->snapshot();
                for(const auto& actor:initial.actors) {
                    if(selected || actor.stage==stalthos::LifeStage::Dead || actor.site!=actor.target || actor.activity==stalthos::fauna::Activity::Travel) continue;
                    for(auto neighbor:faunaSites[actor.site].neighbors) {
                        const auto p=faunaSites[neighbor].position;const double dx=p.x-actor.position.x,dz=p.z-actor.position.z,length=std::hypot(dx,dz);if(length<1) continue;
                        const double sx=actor.position.x-dx/length*14,sz=actor.position.z-dz/length*14;
                        bool dry=true;double old=Ground(float(sx),float(sz));
                        for(int step=0;step<=28;++step) {const double f=double(step)/28,gx=sx+(actor.position.x-sx)*f,gz=sz+(actor.position.z-sz)*f;const double gy=Ground(float(gx),float(gz));if(!landscape.inBounds(gx,gz)||landscape.waterContains(gx,gz)||std::abs(gy-old)>.45) {dry=false;break;}old=gy;}
                        if(!dry) continue;
                        x=float(sx);z=float(sz);yaw=float(std::atan2(dx,dz));wildlifeWatched=actor.id;wildlifeAnchor=actor.position;selected=true;break;
                    }
                }
                if(!selected) throw std::runtime_error("wildlife proof could not find dry approach to existing stationary actor");
                if(wildlifeProbe==2) {
                    bool remote=false; for(int angle=0;angle<24;++angle) {const double a=angle*6.283185307179586/24,gx=wildlifeAnchor.x+28*std::sin(a),gz=wildlifeAnchor.z+28*std::cos(a);if(!landscape.inBounds(gx,gz)||landscape.waterContains(gx,gz)) continue;bool clear=true;for(const auto& actor:initial.actors) if(std::hypot(gx-actor.position.x,gz-actor.position.z)<=wildlifeTuning.noticeMeters+6) clear=false;if(clear){x=float(gx);z=float(gz);remote=true;break;}} if(!remote) throw std::runtime_error("no remote camera startup fixture");
                    for(const auto& actor:initial.actors) if(std::hypot(x-actor.position.x,z-actor.position.z)<=wildlifeTuning.noticeMeters+2) throw std::runtime_error("camera fixture body is too near wildlife");
                }
            }
            bootstrapLedger.reset(); // No mutable ledger remains outside registry authority.
            faunaLedgerInitial=ledgerJson(faunaPopulation->ledger());
            faunaSessionsInitial=faunaPopulation->sessions();
            faunaInitial=actorJson(faunaPopulation->actors());
            faunaFullInitial=populationJson(faunaPopulation->snapshot());
            if(!faunaPhysical) beginFaunaDormancy();
            auto proxy=[&](std::size_t index) {
                const auto root=scene.Entity_CreateTransform("best effort fauna proxy "+std::to_string(index));
                auto part=[&](const char* name,XMFLOAT3 pos,XMFLOAT3 size) {auto e=Box(scene,name,pos,size,{.38f,.27f,.16f,1}); scene.Component_Attach(e,root);};
                part("fauna body",{0,.65f,0},{.32f,.30f,.65f});
                part("fauna head",{0,1.12f,.65f},{.18f,.2f,.28f});
                part("fauna neck",{0,.91f,.4f},{.12f,.3f,.13f});
                for(float lx:{-.24f,.24f}) for(float lz:{-.4f,.4f}) part("fauna leg",{lx,.24f,lz},{.08f,.24f,.08f});
                return root;
            };
            faunaScene.load(scene,creature.u8string(),faunaPopulation->actors(),proxy);
            faunaScene.update(scene,faunaPopulation->actors(),creatureClipDuration,creatureStrideLength,connectedWorld?faunaPopulation->npcPeers().waitingFollower(faunaPopulation->actors(),peerCompanion.motionPeer()):0);
            faunaScene.setPhysical(scene,faunaPhysical);
            if(plant.empty()) for(const auto& s:faunaSites) if(s.browse)
                Box(scene,"best effort browse patch",{float(s.position.x),float(s.position.y+.25),float(s.position.z)},{.6f,.25f,.6f},{.23f,.4f,.12f,1});
        }
        stalthos::domain::Nesting nesting;bool previousNestKey=false,nestProbeRequested=false;double nestMessageUntil=0,nextNestSample=0;std::string nestMessage;
        std::vector<wi::ecs::Entity> nestEntities;json nestEvents=json::array(),nestSamples=json::array(),nestCaptures=json::array();std::array<bool,5> nestCaptured{};
        json hatchEvents=json::array();double nextHatchAttempt=0,bornCalendar=-1;std::uint64_t childSession=0;std::string hatchStatus="incubating";bool hatchDiagnosticDeath=false;
        wi::SpriteFont nestCue("B - select nearby nest site (adult Smalltrotter)");nestCue.params.size=20;nestCue.params.h_align=wi::font::WIFALIGN_CENTER;nestCue.params.color=wi::Color(240,225,180,255);nestCue.params.shadowColor=wi::Color(0,0,0,255);nestCue.SetHidden(!connectedWorld);renderPath.AddFont(&nestCue);
        auto nestSite=[&](){
            stalthos::domain::NestSite site;site.position={x+1.9*std::cos(yaw),0,z-1.9*std::sin(yaw)};site.supported=!disableTerrainSupport;site.dry=true;site.clear=true;double lo=1e30,hi=-1e30;
            for(int i=0;i<9;++i){const double a=i*6.283185307179586/8,r=i==8?0:.9,gx=site.position.x+r*std::sin(a),gz=site.position.z+r*std::cos(a);if(!landscape.inBounds(gx,gz)){site.supported=false;site.clear=false;continue;}if(landscape.waterContains(gx,gz))site.dry=false;try{const double h=populatedTerrain->height(gx,gz);lo=std::min(lo,h);hi=std::max(hi,h);if(i==8)site.position.y=h;}catch(const std::out_of_range&){site.supported=false;}}
            site.relief=hi-lo;
            for(const auto& p:landscape.placements()){const double padding=p.kind==stalthos::domain::PlacementKind::Boulder?7:p.kind==stalthos::domain::PlacementKind::Rock?4:p.kind==stalthos::domain::PlacementKind::Tree?3:1.5;if(std::hypot(site.position.x-p.x,site.position.z-p.z)<.9+padding*std::max(.4,p.scale))site.clear=false;}
            for(const auto& t:woodlandReport.placedTrees)if(std::hypot(site.position.x-t.x,site.position.z-t.z)<3.5*std::max(.5,t.scale)+.9)site.clear=false;
            for(const auto& a:faunaPopulation->actors())if(a.alive && std::hypot(site.position.x-a.position.x,site.position.z-a.position.z)<3)site.clear=false;
            return site;
        };
        if(nestProbe){bool found=false;for(int gx=24;gx<=70 && !found;gx+=2)for(int gz=24;gz<=64 && !found;gz+=2){x=float(gx);z=float(gz);const auto site=nestSite();if(nesting.refusal(heroId,heroSpecies,lifecycle->stage(),needs->hunger(),needs->thirst(),needs->fatigue(),site).empty())found=true;}if(!found)throw std::runtime_error("no supported clear nest diagnostic fixture");populatedTerrain->update(scene,x,z);}
        double populatedMaxX=x,populatedRootError=0,populatedProxyError=0,populatedBoundsError=0;
        std::uint64_t populatedSupportSamples=0;
        json populatedCaptures=json::array();
        std::array<bool,4> populatedCaptured{};
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
                state.orbit = Framing::followPreset(connectedWorld ? yaw+Framing::kPi : cameraMode==1 ? Framing::kPi*.5 : cameraMode==2 ? 0 : Framing::kPi);
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
            if(populatedTerrain) return populatedTerrain->height(cx,cz);
            if (!landscape.inBounds(cx, cz)) throw std::runtime_error("camera outside reference landscape");
            return landscape.height(cx, cz);
        };
        stalthos::domain::ControllerOutput cameraOut;
        if(carnivoreProbe){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor=carnivoreProbe==3?carnivoreAnchor:stalthos::Vec3{x+20,Ground(x+20,z),z};viewController.setState(state);}
        if(shoreProbe==3){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={224,Ground(224,24),24};viewController.setState(state);}
        if(npcPeerProbe==2){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={132,Ground(132,30),30};viewController.setState(state);}
        if(audioProbe==2){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={x,Ground(x,z),z};viewController.setState(state);}
        if(saplingProbe==2){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={sapling.position.x-1.5,sapling.position.y,sapling.position.z};viewController.setState(state);}
        if(explorationProbe==3){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={x+20,Ground(x+20,z),z};viewController.setState(state);}
        if(peerProbe==2){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor=peerAnchor;viewController.setState(state);}
        if(wildlifeProbe==2) {
            auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;
            state.detachedAnchor=wildlifeAnchor;viewController.setState(state);
        }
        if(exteriorWaterProbe==2){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={exteriorWater.bankX,exteriorWaterHeight,exteriorWater.centerZ};viewController.setState(state);}
        if(exteriorBrowseProbe==2){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={exteriorBrowse.x,Ground(float(exteriorBrowse.x),float(exteriorBrowse.z)),exteriorBrowse.z};viewController.setState(state);}
        if(destinationProbe==3){auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={x+40,Ground(x+40,z),z};viewController.setState(state);}
        if(foragingProbe==3) {auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor=foragingAnchor;viewController.setState(state);}
        if(autonomyProbe==3) {
            auto state=viewController.getState();state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;
            const double creek=8*std::sin(z/43.0)+2*std::sin(z/17.0);state.detachedAnchor={creek,Ground(float(creek),z),z};viewController.setState(state);
        }
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
        std::uint64_t eligibleWaterResource=100;
        bool eligibilityShrub = false;

        // Sequence state for diagnostic-needs
        int diagnosticSequenceStage = 0;
        double diagnosticTimer = 0.0;

        uint64_t physicalInteractionSession = 0;
        bool previousNextLifeKey=false,nextLifeProbeRequested=false;
        uint64_t priorDeadHero=0;
        json nextLifeEvents=json::array(),nextLifeSamples=json::array(),nextLifeCaptures=json::array();
        double nextLifeSampleAt=0;std::array<bool,4> nextLifeCaptured{};
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
        uint64_t renderedSupportSampleCount = 0;
        uint64_t renderedSupportFallbackCount = 0;
        std::string latestRenderedSupportFallbackReason;
        std::string latestGroundSampleSource = "analytical_fallback";
        std::string latestCreatureRootSource = "analytical_fallback";
        double latestCreatureRootGroundY = 0.0;
        double latestCreatureRootAnalyticalGroundY = 0.0;
        double latestCreatureRootRenderedGroundY = 0.0;
        double latestCreatureRootGroundDeltaM = 0.0;

        json dayNightSamples=json::array(),dayNightCaptures=json::array();double nextDayNightSample=0;std::array<bool,4> dayNightCaptured{};stalthos::environment::DayNightLight dayNightLight{};double renderedDayFraction=0;
        json exteriorWaterSamples=json::array(),exteriorWaterCaptures=json::array(),exteriorWaterNpcContacts=json::array();double nextExteriorWaterSample=0;std::array<bool,3> exteriorWaterCaptured{};
        json exteriorBrowseSamples=json::array(),exteriorBrowseCaptures=json::array(),exteriorBrowseNpcContacts=json::array();double nextExteriorBrowseSample=0;std::array<bool,3> exteriorBrowseCaptured{};
        stalthos::domain::BodyMotionAudio bodyMotionAudio;wi::audio::Sound rustleSound;wi::audio::SoundInstance rustleInstance;bool rustleReady=false;uint64_t rustleMaxCursor=0,rustleRequests=0,rustleEventsDropped=0;json rustleEvents=json::array();
        if(connectedWorld){const auto bytes=stalthos::engine::bodyRustleWave();std::ofstream source(output/"body-rustle-source.wav",std::ios::binary);source.write((const char*)bytes.data(),bytes.size());rustleReady=wi::audio::CreateSound(bytes.data(),bytes.size(),&rustleSound)&&wi::audio::CreateSoundInstance(&rustleSound,&rustleInstance);if(rustleReady)wi::audio::SetVolume(.12f,&rustleInstance);}
        stalthos::engine::NativeAudioCapture audioCapture;if(audioProbe)audioCapture.start();
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

            if(reedProbe==3 && clock.physicalSeconds()>=2 && clock.physicalSeconds()<10){auto state=viewController.getState();if(state.mode!=stalthos::domain::CameraMode::LocallyDetached){state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;for(const auto& a:faunaPopulation->actors())if(a.id==reedFixturePeer)state.detachedAnchor=a.position;viewController.setState(state);}viewController.applyDetachedPan(.3,0,dt,{x,Ground(x,z),z});}
            if(reedProbe==4 && !reedNeedInjected && clock.physicalSeconds()>=5){needs->restoreState(0,.8,0,0,false);reedNeedInjected=true;}
            if(protectionProbe==3 && clock.physicalSeconds()>=2 && clock.physicalSeconds()<10){auto state=viewController.getState();if(state.mode!=stalthos::domain::CameraMode::LocallyDetached){state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;const auto actors=faunaPopulation->actors();for(const auto& a:actors)if(a.id==fixtureSmall)state.detachedAnchor=a.position;viewController.setState(state);}viewController.applyDetachedPan(.3,0,dt,{x,Ground(x,z),z});}
            if(birdProbe==3 && clock.physicalSeconds()>=2 && clock.physicalSeconds()<9){auto state=viewController.getState();if(state.mode!=stalthos::domain::CameraMode::LocallyDetached){state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={microforage.x,Ground(float(microforage.x),float(microforage.z)),microforage.z};viewController.setState(state);}viewController.applyDetachedPan(.5,0,dt,{x,Ground(x,z),z});}
            if(hatchProbe==2 && clock.physicalSeconds()>=12 && clock.physicalSeconds()<20){auto state=viewController.getState();if(state.mode!=stalthos::domain::CameraMode::LocallyDetached){state.mode=stalthos::domain::CameraMode::LocallyDetached;state.focus=stalthos::domain::InputFocus::FreeCamera;state.detachedAnchor={x,Ground(x,z),z};viewController.setState(state);}viewController.applyDetachedPan(1,0,dt,{x,Ground(x,z),z});}
            if(audioProbe==2 && clock.physicalSeconds()>=2 && clock.physicalSeconds()<6)viewController.applyDetachedPan(1,0,dt,{x,Ground(x,z),z});
            bool userSteering = viewController.getState().focus == stalthos::domain::InputFocus::BodySteering;
            if (userSteering) {
                if(heroAlive && down('A')) yaw-=dt; if(heroAlive && down('D')) yaw+=dt;
            }
            const auto timeStep=clock.advance(dt,calendarScale);
            if(connectedWorld)nesting.advance(timeStep);
            for(const auto transition:lifecycle->advance(timeStep)) {
                const char* names[]={"hatchling","juvenile","adult","old","dead"};
                lifecycleTransitions.push_back(names[static_cast<unsigned>(transition)]);
                if(connectedWorld){lastLifeStageMilestone=clock.physicalSeconds();lifeStageMilestones.push_back({{"stage",names[static_cast<unsigned>(transition)]},{"age",lifecycle->calendarAge()},{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()}});}
                if(transition==stalthos::LifeStage::Dead) { priorDeadHero=heroId;world.heroDied();
                    if(connectedWorld){peerCue.SetHidden(true);nextLifeEvents.push_back({{"event","death"},{"hero_id",heroId},{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"body_x",x},{"body_z",z},{"ledger",ledgerSnapshot().sessions.size()}});} for(size_t i=0;i<heroAnimationCount;++i) scene.animations[i].Pause(); }
            }
            heroAlive=lifecycle->stage()!=stalthos::LifeStage::Dead;
            if(hatchProbe==1 && nesting.offspring() && !hatchDiagnosticDeath && clock.physicalSeconds()>=24){lifecycle->restoreState(heroSpecies,stalthos::getHeroProfile(heroSpecies).hatchlingDuration+stalthos::getHeroProfile(heroSpecies).juvenileDuration+stalthos::getHeroProfile(heroSpecies).adultDuration+stalthos::getHeroProfile(heroSpecies).oldDuration,stalthos::LifeStage::Dead);priorDeadHero=heroId;world.heroDied();heroAlive=false;hatchDiagnosticDeath=true;hatchEvents.push_back({{"event","synthetic_parent_death_fixture"},{"physical_seconds",clock.physicalSeconds()},{"child",nesting.offspring()}});}
            if(birdProbe==1 && !birdDeathRequested && clock.physicalSeconds()>=17){const auto& p=stalthos::getHeroProfile(heroSpecies);lifecycle->restoreState(heroSpecies,p.hatchlingDuration+p.juvenileDuration+p.adultDuration+p.oldDuration,stalthos::LifeStage::Dead);priorDeadHero=heroId;world.heroDied();heroAlive=false;birdDeathRequested=true;}
            const bool syntheticNextLife=(birdProbe==1 && birdDeathRequested && !nextLifeProbeRequested && clock.physicalSeconds()>=18) || (deathNextLifeProbe && !nextLifeProbeRequested && clock.physicalSeconds()>=10) || (hatchProbe==1 && hatchDiagnosticDeath && !nextLifeProbeRequested && clock.physicalSeconds()>=25);
            const bool nextLifeKey=down('N') || syntheticNextLife;
            if(connectedWorld && !heroAlive && nextLifeKey && !previousNextLifeKey){
                if(syntheticNextLife)nextLifeProbeRequested=true;
                const auto beforeLedger=ledgerJson(ledgerSnapshot());
                const auto beforePopulation=faunaPopulation->snapshot();
                std::vector<stalthos::Identity> npcIds;for(const auto& a:faunaPopulation->actors())npcIds.push_back(a.id);
                const auto selected=stalthos::domain::beginNextLife(world,priorDeadHero,npcIds,nextInteractionSession);heroId=selected.hero;
                lifecycle.emplace(heroSpecies);needs.emplace(heroSpecies);defenseOwned=false;defenseObservation.reset();braceUntil=0;reedFollowOwned=false;reedDecision={};if(birdLife)birdDistance=0;
                bodyMotionAudio=stalthos::domain::BodyMotionAudio{};if(rustleReady)wi::audio::Pause(&rustleInstance);
                heroExploration=stalthos::domain::HeroExploration{};explorationCue.SetHidden(true);
                heroIntent=stalthos::HeroSim{};heroAutonomy=stalthos::domain::HeroDrinkAutonomy{};heroRest=stalthos::domain::HeroRestAutonomy{};
                peerCompanion=stalthos::fauna::PeerCompanionship{};previousPeer=0;previousPeerPresent=false;peerCue.SetHidden(true);
                bodyWeather=stalthos::environment::BodyWeatherResponse{};exposureResponse={};
                resting=false;previousRestCue=false;restCue.SetHidden(true);pendingDestinationClick.reset();destinationStatus="ready";
                eligibilityWater=false;eligibilityShrub=false;previousInteractDown=false;previousInteractionOwner=0;
                physicalInteractionSession=selected.session;interactions.beginSession(physicalInteractionSession);
                heroAlive=true;lastLifeStageMilestone=-1e30;
                if(deathNextLifeProbe)needs->restoreState(0,.8,0,0,false);
                nextLifeEvents.push_back({{"event","explicit_next_life"},{"input",syntheticNextLife?"labelled synthetic N action":"N key rising edge"},{"old_hero",priorDeadHero},{"hero_id",heroId},{"fresh_session",physicalInteractionSession},{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"old_death_known",world.heroDeathKnown(priorDeadHero)},{"old_id_excluded",world.excluded(priorDeadHero)},{"ledger_unchanged_at_handoff",beforeLedger==ledgerJson(ledgerSnapshot())},{"npc_count",faunaPopulation->actors().size()},{"npc_snapshot_unchanged_at_handoff",populationJson(beforePopulation)==populationJson(faunaPopulation->snapshot())},{"new_age",lifecycle->calendarAge()},{"new_stage",stalthos::domain::lifeStageName(lifecycle->stage())}});
            }
            previousNextLifeKey=nextLifeKey;
            heroStageBulk=connectedWorld?stalthos::domain::lifeStageBulk(lifecycle->stage()):1;
            if(connectedWorld){const auto name=std::string(stalthos::domain::lifeStageName(lifecycle->stage()));lifeStageCue.SetText(!heroAlive?"This life has ended. World continues. Press N to begin a new life":clock.physicalSeconds()-lastLifeStageMilestone<6?"Reached "+name+" stage - growth milestone":std::string(reedLife?"Reedcrest - life stage: ":brackenLife?"Brackenback - life stage: ":birdLife?"Thistletail - life stage: ":"Life stage: ")+name);lifeStageCue.params.posX=app.canvas.GetLogicalWidth()*.5f;lifeStageCue.params.posY=app.canvas.GetLogicalHeight()*.7f;}
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
            syntheticDirect=(reedProbe==5 && clock.physicalSeconds()>=4 && clock.physicalSeconds()<29) || (reedProbe==2 && clock.physicalSeconds()>=4 && clock.physicalSeconds()<10) || (protectionProbe==2 && clock.physicalSeconds()>=1 && clock.physicalSeconds()<3) || (birdProbe==2 && clock.physicalSeconds()>=1 && clock.physicalSeconds()<3) || (hatchProbe==1 && clock.physicalSeconds()>=13 && clock.physicalSeconds()<18) || (!hatchProbe && nestProbe && clock.physicalSeconds()>=4 && clock.physicalSeconds()<7) || (audioProbe==1 && clock.physicalSeconds()>=8 && clock.physicalSeconds()<10) || (saplingProbe==1 && clock.physicalSeconds()>=4 && clock.physicalSeconds()<(creatureLoaded?18:7)) || ((autonomyProbe==2 || foragingProbe==2) && elapsed>=2 && elapsed<4) || (restProbe==2 && elapsed>=4 && elapsed<6) || (destinationProbe==2 && clock.physicalSeconds()>=2.5 && clock.physicalSeconds()<3.5) || (exteriorWaterProbe==3 && clock.physicalSeconds()>=2 && clock.physicalSeconds()<4) || (dayNightProbe==2 && clock.physicalSeconds()>=2 && clock.physicalSeconds()<4) || (explorationProbe==2 && clock.physicalSeconds()>=4 && clock.physicalSeconds()<5) || (lifeStageProbe && clock.physicalSeconds()>=9 && clock.physicalSeconds()<11) || (deathNextLifeProbe && ((clock.physicalSeconds()>=7 && clock.physicalSeconds()<9) || (clock.physicalSeconds()>=12 && clock.physicalSeconds()<14)));
            const bool wildlifeForward=wildlifeProbe==1 && elapsed<6,wildlifeReverse=wildlifeProbe==1 && elapsed>=10 && elapsed<18;
            bool userWalk = (reedProbe==5 && clock.physicalSeconds()>=29 && clock.physicalSeconds()<49) || (reedProbe==2 && clock.physicalSeconds()>=10 && clock.physicalSeconds()<16) || (hatchProbe==1 && clock.physicalSeconds()>=18 && clock.physicalSeconds()<23) || ((shoreProbe==1 || shoreProbe==2) && x<258) || (audioProbe==1 && clock.physicalSeconds()>=2 && clock.physicalSeconds()<6) || (saplingProbe==1 && clock.physicalSeconds()>=(creatureLoaded?18:7) && clock.physicalSeconds()<(creatureLoaded?32:10)) || (peerProbe==1 && clock.physicalSeconds()>=10 && clock.physicalSeconds()<24) || (woodlandProbe && x<184) || (userSteering && down('W')) || wildlifeForward || (exposureProbe && clock.physicalSeconds()>=11 && clock.physicalSeconds()<12);
            bool userReverse = ((carnivoreProbe==1 || carnivoreProbe==2) && clock.physicalSeconds()>=6 && clock.physicalSeconds()<20) || (peerProbe==1 && clock.physicalSeconds()>=24 && clock.physicalSeconds()<38) || (userSteering && down('S')) || syntheticDirect || wildlifeReverse;
            bool walking = diagnosticWalk || diagnosticReverseWalk || (diagnosticTransition && (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3)) || userWalk || (connectedRouteWalk && x<connectedLayout.meadowX+2);
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
            const bool syntheticSaplingAction=saplingProbe && ((clock.physicalSeconds()>=3 && clock.physicalSeconds()<3.5) || (clock.physicalSeconds()>=(creatureLoaded?33:11) && clock.physicalSeconds()<(creatureLoaded?33.5:11.5)));
            const bool directInput=((carnivoreProbe==1 || carnivoreProbe==2) && userReverse) || syntheticSaplingAction || (userSteering && down('F')) || walking || userSteerDirect || syntheticDirect || wildlifeReverse || down('E');
            heroIntent.setDirect(directInput);
            const stalthos::Vec3 destinationBody{x,Ground(x,z),z};
            if(reedLife){const stalthos::fauna::ActorView* partner=nullptr;if(faunaPhysical && peerCompanion.peer())for(const auto& a:faunaPopulation->actors())if(a.id==peerCompanion.peer())partner=&a;
                const bool playerDestination=heroIntent.goal() && heroIntent.goal()->status==stalthos::GoalStatus::Active && !reedFollowOwned;
                reedDecision=stalthos::domain::reedPeerDecision(destinationBody,heroAlive,directInput,playerDestination || bool(pendingDestinationClick),needs->suggestActivity()!=stalthos::needs::SuggestedActivity::None,partner);
                if(reedFollowOwned && (directInput || pendingDestinationClick || reedDecision.leader!=stalthos::domain::ReedLeader::Partner || !reedDecision.heroTarget)){if(stalthos::domain::HeroDestination::active(heroIntent))heroIntent.setGoalStatus(stalthos::GoalStatus::Abandoned);reedFollowOwned=false;}
                if(reedFollowOwned && !stalthos::domain::HeroDestination::active(heroIntent))reedFollowOwned=false;
                if(reedDecision.heroTarget && !directInput && !pendingDestinationClick && (!stalthos::domain::HeroDestination::active(heroIntent) || reedFollowOwned) && clock.physicalSeconds()>=reedGoalRefresh){auto target=*reedDecision.heroTarget;target.y=Ground(float(target.x),float(target.z));if(stalthos::domain::HeroDestination::submit(heroIntent,destinationBody,target,supportedDestination(destinationBody,target))){heroAutonomy.yieldToDestination();reedFollowOwned=true;}reedGoalRefresh=clock.physicalSeconds()+.5;}
            }
            if(brackenLife){const auto empty=std::vector<stalthos::fauna::ActorView>{};defenseObservation=stalthos::domain::brackenProtection(heroId,heroSpecies,destinationBody,lifecycle->stage(),heroAlive,faunaPhysical?faunaPopulation->actors():empty);
                if(defenseOwned && (directInput || pendingDestinationClick || needs->suggestActivity()!=stalthos::needs::SuggestedActivity::None || !defenseObservation)){if(stalthos::domain::HeroDestination::active(heroIntent))heroIntent.setGoalStatus(stalthos::GoalStatus::Abandoned);defenseOwned=false;braceUntil=0;defenseCooldown=clock.physicalSeconds()+3;if(defenseEvents.size()<256)defenseEvents.push_back({{"event","yield_or_lost_stimulus"},{"physical_seconds",clock.physicalSeconds()},{"direct",directInput}});}
                if(defenseOwned && !stalthos::domain::HeroDestination::active(heroIntent)){const bool arrived=heroIntent.goal() && heroIntent.goal()->status==stalthos::GoalStatus::Completed;braceUntil=arrived?clock.physicalSeconds()+2:0;defenseOwned=false;defenseCooldown=clock.physicalSeconds()+8;}
                if(defenseObservation && !defenseOwned && !pendingDestinationClick && !directInput && !stalthos::domain::HeroDestination::active(heroIntent) && needs->suggestActivity()==stalthos::needs::SuggestedActivity::None && clock.physicalSeconds()>=defenseCooldown){auto target=defenseObservation->interpose;target.y=Ground(float(target.x),float(target.z));const bool accepted=stalthos::domain::HeroDestination::submit(heroIntent,destinationBody,target,supportedDestination(destinationBody,target));if(accepted){heroAutonomy.yieldToDestination();defenseOwned=true;++defenseStarts;if(defenseEvents.size()<256)defenseEvents.push_back({{"event","interposition_intention"},{"vulnerable",defenseObservation->vulnerable},{"threat",defenseObservation->threat},{"target_x",target.x},{"target_z",target.z},{"physical_seconds",clock.physicalSeconds()}});}}
            }
            if(connectedWorld && pendingDestinationClick){const auto click=*pendingDestinationClick;pendingDestinationClick.reset();RECT client{};GetClientRect(window,&client);const auto ray=wi::renderer::GetPickRay(long(click.first*app.canvas.GetLogicalWidth()/std::max(1L,client.right)),long(click.second*app.canvas.GetLogicalHeight()/std::max(1L,client.bottom)),app.canvas);const auto hit=scene.Intersects(ray);const stalthos::Vec3 target{hit.position.x,hit.position.y,hit.position.z};const bool accepted=heroAlive && hit.entity!=wi::ecs::INVALID_ENTITY && stalthos::domain::HeroDestination::submit(heroIntent,destinationBody,target,supportedDestination(destinationBody,target));if(accepted)heroAutonomy.yieldToDestination();destinationStatus=accepted?"moving":"rejected";destinationEvents.push_back({{"event",destinationStatus},{"physical_seconds",clock.physicalSeconds()},{"target_x",target.x},{"target_z",target.z},{"pixel_x",click.first},{"pixel_y",click.second},{"source","native WM_RBUTTONDOWN -> scene raycast"}});if(destinationProbe==1 && accepted)needs->restoreState(0,.8,0,0,false);}
            if(explorationProbe==2 && !explorationNeedInjected && clock.physicalSeconds()>=10){needs->restoreState(0,.8,0,0,false);explorationNeedInjected=true;explorationEvents.push_back({{"event","synthetic_thirst"},{"physical_seconds",clock.physicalSeconds()}});}
            if(explorationProbe==2 && !explorationDestinationInjected && clock.physicalSeconds()>=15){const stalthos::Vec3 target{x+1,Ground(x+1,z),z};const bool accepted=stalthos::domain::HeroDestination::submit(heroIntent,destinationBody,target,supportedDestination(destinationBody,target));if(accepted)heroAutonomy.yieldToDestination();explorationDestinationInjected=true;explorationEvents.push_back({{"event","synthetic_contextual_destination"},{"accepted",accepted},{"physical_seconds",clock.physicalSeconds()}});}
            const bool destinationWasActive=stalthos::domain::HeroDestination::active(heroIntent);
            const auto destinationAction=stalthos::domain::HeroDestination::update(heroIntent,destinationBody,yaw,timeStep.physicalSeconds,heroAlive,directInput,!destinationWasActive || supportedDestination(destinationBody,heroIntent.goal()->target));
            const bool destinationActive=stalthos::domain::HeroDestination::active(heroIntent);
            if(brackenLife && defenseOwned && !destinationActive){const bool arrived=heroIntent.goal() && heroIntent.goal()->status==stalthos::GoalStatus::Completed;braceUntil=arrived?clock.physicalSeconds()+2:0;defenseOwned=false;defenseCooldown=clock.physicalSeconds()+8;}
            if(brackenLife && defenseObservation && clock.physicalSeconds()<braceUntil && !directInput && !destinationActive && needs->suggestActivity()==stalthos::needs::SuggestedActivity::None)yaw=float(std::atan2(defenseObservation->threatBody.x-x,defenseObservation->threatBody.z-z)+pi*.5);

            if(destinationStatus=="moving" && !destinationActive){destinationStatus=heroIntent.goal() && heroIntent.goal()->status==stalthos::GoalStatus::Completed?"arrived":directInput?"cancelled":"stopped";destinationEvents.push_back({{"event",destinationStatus},{"physical_seconds",clock.physicalSeconds()}});}
            destinationCue.SetHidden(!connectedWorld || !heroAlive);static std::string previousDestinationStatus;
            if(destinationStatus!=previousDestinationStatus){destinationCue.SetText(destinationStatus=="moving"?"Going to selected ground - WASD cancels":destinationStatus=="arrived"?"Arrived - ordinary activity resumed":destinationStatus=="rejected"?"Cannot go there - choose nearby clear dry ground":destinationStatus=="cancelled"?"Destination cancelled - direct control":destinationStatus=="stopped"?"Destination stopped - route unsupported":"Right-click nearby ground to go there");previousDestinationStatus=destinationStatus;}
            phasePreviewCue.params.posX=20;phasePreviewCue.params.posY=20;
            destinationCue.params.posX=app.canvas.GetLogicalWidth()*.5f;destinationCue.params.posY=app.canvas.GetLogicalHeight()*.8f;
            destinationPin.SetHidden(!destinationActive);if(destinationActive){const auto& t=heroIntent.goal()->target;DirectX::XMFLOAT3 projected;DirectX::XMStoreFloat3(&projected,DirectX::XMVector3Project(DirectX::XMVectorSet(float(t.x),float(t.y+.15),float(t.z),1),0,0,app.canvas.GetLogicalWidth(),app.canvas.GetLogicalHeight(),0,1,wi::scene::GetCamera().GetProjection(),wi::scene::GetCamera().GetView(),DirectX::XMMatrixIdentity()));destinationPin.params.posX=projected.x;destinationPin.params.posY=projected.y;}
            resting=heroAutonomyEnabled?heroRest.update(*needs,heroAlive,directInput||destinationActive):false; // Original saved-mode resting state is untouched.
            if(resting!=previousRestCue){restCue.SetHidden(!resting);previousRestCue=resting;++restCueChanges;}
            if(cueCanvasWidth!=app.canvas.GetLogicalWidth() || cueCanvasHeight!=app.canvas.GetLogicalHeight()){cueCanvasWidth=app.canvas.GetLogicalWidth();cueCanvasHeight=app.canvas.GetLogicalHeight();restCue.params.posX=cueCanvasWidth*.5f;restCue.params.posY=cueCanvasHeight*.9f;exposureCue.params.posX=cueCanvasWidth*.5f;exposureCue.params.posY=cueCanvasHeight*.85f;}
            autonomyAction={};
            stalthos::domain::DrinkAction explorationAction{};
            auto explorationSupport=[&](stalthos::Vec3 from,stalthos::Vec3 target){if(!supportedDestination(from,target))return false;for(int i=0;i<=6;++i){const double f=double(i)/6,gx=from.x+(target.x-from.x)*f,gz=from.z+(target.z-from.z)*f;if(std::hypot(gx-exteriorWater.bankX,gz-exteriorWater.centerZ)<exteriorWater.radius+.2)return false;}return true;};
            if(explorationEnabled)explorationAction=heroExploration.update(heroIntent,destinationBody,yaw,timeStep.physicalSeconds,heroAlive&&!directInput&&!destinationActive&&!resting&&clock.physicalSeconds()>=braceUntil&&needs->suggestActivity()==stalthos::needs::SuggestedActivity::None,
                [&](stalthos::Vec3 body,double heading,double step,unsigned choice)->std::optional<stalthos::Vec3>{const double angle=heading+choice*1.5707963267948966,gx=body.x+step*std::sin(angle),gz=body.z+step*std::cos(angle);try{return stalthos::Vec3{gx,populatedTerrain->height(gx,gz),gz};}catch(const std::out_of_range&){return std::nullopt;}},explorationSupport);
            explorationCue.SetHidden(!explorationEnabled || heroExploration.phase()==stalthos::domain::ExplorationPhase::Waiting);explorationCue.SetText(heroExploration.phase()==stalthos::domain::ExplorationPhase::Looking?"Pausing to look around":"Exploring nearby ground - WASD takes control");explorationCue.params.posX=app.canvas.GetLogicalWidth()*.5f;explorationCue.params.posY=app.canvas.GetLogicalHeight()*.65f;
            if(heroAutonomyEnabled && heroAlive && !destinationActive) {
                std::optional<stalthos::Vec3> water;autonomyBrowsing=needs->suggestActivity()==stalthos::needs::SuggestedActivity::Eat;autonomyResource=100;
                if(landscape.inBounds(x,z)) {const double creek=8*std::sin(z/43.0)+2*std::sin(z/17.0);water=stalthos::Vec3{creek,Ground(float(creek),z),z};}
                if(connectedWorld && getResourceRemaining(209)>0){const stalthos::Vec3 bank{exteriorWater.bankX,Ground(float(exteriorWater.bankX),float(exteriorWater.bankZ)),exteriorWater.bankZ};if(!water || std::hypot(bank.x-x,bank.z-z)<std::hypot(water->x-x,water->z-z)){water=bank;autonomyResource=209;}}
                if(autonomyBrowsing) {
                    std::optional<stalthos::Vec3> food;double nearest=24*24;bool eligible=false;
                    for(std::size_t i=0;!birdLife && i<cachedShrubs.size() && i<shrubVisualInstances.size();++i){const auto& p=cachedShrubs[i];const double dx=p.x-x,dy=p.y-Ground(x,z),dz=p.z-z,d=dx*dx+dy*dy+dz*dz;if(getResourceRemaining(200+i)>0 && d<nearest){nearest=d;food=stalthos::Vec3{p.x,p.y,p.z};autonomyResource=200+i;eligible=std::abs(dy)<=1.5 && dx*dx+dz*dz<4;}}
                    if(birdLife){const double ground=Ground(float(microforage.x),float(microforage.z));if(microforage.sensed({x,Ground(x,z),z},ground,getResourceRemaining(microforage.resource))){food=stalthos::Vec3{microforage.x,ground,microforage.z};autonomyResource=microforage.resource;eligible=microforage.eligible({x,Ground(x,z),z},ground,!disableTerrainSupport,heroAlive);}}
                    autonomyAction=heroAutonomy.update(heroIntent,*needs,{x,Ground(x,z),z},yaw,timeStep.physicalSeconds,directInput,food,food.has_value(),eligible,stalthos::needs::SuggestedActivity::Eat,autonomyResource);
                } else autonomyAction=heroAutonomy.update(heroIntent,*needs,{x,Ground(x,z),z},yaw,timeStep.physicalSeconds,directInput,water,getResourceRemaining(autonomyResource)>0,eligibilityWater && eligibleWaterResource==autonomyResource,stalthos::needs::SuggestedActivity::Drink,autonomyResource);
                yaw+=float(autonomyAction.turn);
            }
            if(explorationAction.active){autonomyAction=explorationAction;yaw+=float(explorationAction.turn);}
            if(destinationActive){autonomyAction=destinationAction;yaw+=float(destinationAction.turn);}
            float speed = 0.f;
            const float previousX=x, previousZ=z;

            const bool forwardIntent = diagnosticWalk || (diagnosticNeeds && (diagnosticSequenceStage == 0 || diagnosticSequenceStage == 2)) || (diagnosticTransition && activeTransitionPhaseIndex == 1) || userWalk || (connectedRouteWalk && x<connectedLayout.meadowX+2) || autonomyAction.forward;
            const bool reverseIntent = diagnosticReverseWalk || (diagnosticTransition && activeTransitionPhaseIndex == 3) || (!forwardIntent && userReverse);
            if(heroAutonomyEnabled && elapsed>=nextAutonomySample) {
                autonomySamples.push_back({{"elapsed",elapsed},{"dt",dt},{"body_x",x},{"body_z",z},{"thirst",needs->thirst()},{"hunger",needs->hunger()},{"browse",autonomyBrowsing},{"resource",autonomyResource},{"direct",directInput},{"synthetic_direct",syntheticDirect},{"auto_forward",autonomyAction.forward},{"auto_contact",autonomyAction.contact},{"auto_active",autonomyAction.active},{"body_attention",heroIntent.perception.attention().has_value()},{"authority",unsigned(heroIntent.authority())},{"target_x",heroIntent.goal()?heroIntent.goal()->target.x:0},{"target_z",heroIntent.goal()?heroIntent.goal()->target.z:0},{"camera_target_x",cameraOut.cameraState.target.x},{"camera_target_z",cameraOut.cameraState.target.z}});
                nextAutonomySample=elapsed+.1;
            }
            if (heroAlive && creatureLoaded) {
                using Pace = stalthos::domain::LocomotionPace;
                speed = static_cast<float>(Pace::calculate(creatureStrideLength, creatureClipDuration,
                    reverseIntent ? Pace::Intent::Reverse : forwardIntent ? Pace::Intent::Forward : Pace::Intent::Idle).signedSpeed);
            } else if (heroAlive) speed = reverseIntent ? (brackenLife?-1.0f:reedLife?-1.2f:-2.f) : forwardIntent ? (brackenLife?1.2f:reedLife?1.6f:2.f) : 0.f;
            if (speed != 0.f) {
                auto forward=stalthos::BeastCore::bodyForward(yaw);
                x+=static_cast<float>(forward.x)*speed*dt; z+=static_cast<float>(forward.z)*speed*dt;
                const float extent=populatedWorld?12000.f:60.f;
                x=std::clamp(x,-extent,extent); z=std::clamp(z,-extent,extent);
            }
            if(populatedTraversal) {
                const double t=std::min(1.0,elapsed/(duration*.95));
                x=float(t<.45?traversalStartX+(544-traversalStartX)*t/.45:t<.55 || populatedDeparture?544:24+520*(1-t)/.45);z=24; yaw=pi*.5f;
            }
            if(populatedTerrain) populatedTerrain->update(scene,x,z);
            if(shoreProbe && elapsed>=nextShoreSample && shoreTrajectory.size()<1200) {
                shoreTrajectory.push_back({{"physical_seconds",clock.physicalSeconds()},{"dt",dt},{"x",x},{"z",z},{"ground_y",Ground(x,z)},{"forward_intent",forwardIntent},{"integrator_speed_m_s",speed},{"step_distance_m",std::hypot(double(x-previousX),double(z-previousZ))}});nextShoreSample=elapsed+.1;
            }
            if(connectedRouteWalk) {
                const double distance=std::hypot(double(x-previousX),double(z-previousZ));
                const double gy=Ground(x,z),oldGy=Ground(previousX,previousZ);
                if(distance>1e-6) connectedMaxGrade=std::max(connectedMaxGrade,std::abs(gy-oldGy)/distance);
                connectedMaxStep=std::max(connectedMaxStep,std::abs(gy-oldGy));
                connectedTrajectory.push_back({{"elapsed",elapsed},{"dt",dt},{"x",x},{"z",z},{"ground_y",gy},{"forward_intent",forwardIntent},{"integrator_speed_m_s",speed},{"step_distance_m",distance}});
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
            const double analyticalRootY = (populatedTerrain ? populatedTerrain->analytical(x,z) : static_cast<double>(landscape.height(static_cast<double>(x), static_cast<double>(z))));
            double renderedRootY = analyticalRootY;
            bool rootUsesRendered = false;
            std::string frameFallbackReason;

            const bool groundCached = creatureLoaded && nativeSoleAdapter.isGroundCached();
            const stalthos::domain::RenderedGroundSurface* renderedSurface = groundCached ? &nativeSoleAdapter.getRenderedGroundSurface() : nullptr;

            if(populatedTerrain) { renderedRootY=populatedTerrain->height(x,z);rootUsesRendered=true; }
            else if (renderedSurface && renderedSurface->inBounds(static_cast<double>(x), static_cast<double>(z))) {
                try {
                    const double rH = renderedSurface->sampleHeight(static_cast<double>(x), static_cast<double>(z));
                    if (std::isfinite(rH)) {
                        renderedRootY = rH;
                        rootUsesRendered = true;
                    } else {
                        frameFallbackReason = "nonfinite_rendered_root_elevation";
                    }
                } catch (const std::exception& e) {
                    frameFallbackReason = std::string("rendered_root_sample_exception: ") + e.what();
                }
            } else if (creatureLoaded) {
                if (!groundCached) {
                    frameFallbackReason = "rendered_ground_cache_unavailable: " + nativeSoleAdapter.getGroundErrorMessage();
                } else {
                    frameFallbackReason = "creature_root_out_of_bounds";
                }
            }

            auto groundSampler = [&](double sx, double sz) -> double {
                if(populatedTerrain) {++renderedSupportSampleCount;return populatedTerrain->height(sx,sz);}
                if (renderedSurface) {
                    if (!renderedSurface->inBounds(sx, sz)) {
                        frameFallbackReason = "support_sample_out_of_bounds";
                        throw std::out_of_range(frameFallbackReason);
                    }
                    const double h = renderedSurface->sampleHeight(sx, sz);
                    if (!std::isfinite(h)) {
                        frameFallbackReason = "nonfinite_rendered_support_sample";
                        throw std::runtime_error(frameFallbackReason);
                    }
                    ++renderedSupportSampleCount;
                    return h;
                }
                ++renderedSupportFallbackCount;
                return static_cast<double>(landscape.height(sx, sz));
            };

            stalthos::domain::TerrainSupport support;
            bool supportCalculated = false;
            try {
                support = stalthos::domain::calculateTerrainSupport(
                    static_cast<double>(x), static_cast<double>(z), static_cast<double>(yaw),
                    kCreatureLongitudinalHalfspan*heroStageBulk, kCreatureLateralHalfspan*heroStageBulk, groundSampler);
                supportCalculated = true;
                latestSlopeRadians = support.slopeAngleRadians;
                latestNormal = support.surfaceNormal;
            } catch (...) {
                // Retry the entire five-point support and root against one surface.
                // Never combine rendered and analytical heights in a single pose.
                rootUsesRendered = false;
                if (frameFallbackReason.empty()) frameFallbackReason = "support_calculation_exception";
                try {
                    support = stalthos::domain::calculateTerrainSupport(
                        static_cast<double>(x), static_cast<double>(z), static_cast<double>(yaw),
                        kCreatureLongitudinalHalfspan*heroStageBulk, kCreatureLateralHalfspan*heroStageBulk,
                        [&](double sx, double sz) {
                            ++renderedSupportFallbackCount;
                            return static_cast<double>(landscape.height(sx, sz));
                        });
                    supportCalculated = true;
                    latestSlopeRadians = support.slopeAngleRadians;
                    latestNormal = support.surfaceNormal;
                } catch (...) {
                    supportCalculated = false;
                    latestSlopeRadians = 0.0;
                    latestNormal = {0.0, 1.0, 0.0};
                }
            }

            if (disableTerrainSupport) {
                latestSupportMode = "disabled";
            } else if (!supportCalculated || !support.supported) {
                latestSupportMode = "unsupported_yaw_fallback";
                ++supportFailureCount;
            } else {
                latestSupportMode = "terrain_supported";
            }

            float y = static_cast<float>(analyticalRootY);
            if (creatureLoaded) {
                if (latestSupportMode == "terrain_supported" && supportCalculated && std::isfinite(support.centerHeight)) {
                    y = static_cast<float>(support.centerHeight);
                    latestCreatureRootSource = rootUsesRendered ? "rendered_triangles" : "analytical_fallback";
                } else if (rootUsesRendered) {
                    y = static_cast<float>(renderedRootY);
                    latestCreatureRootSource = "rendered_triangles";
                } else {
                    y = static_cast<float>(analyticalRootY);
                    latestCreatureRootSource = "analytical_fallback";
                }
            } else {
                y = Ground(x, z);
                latestCreatureRootSource = populatedTerrain ? "rendered_triangles" : "analytical_fallback";
            }

            latestGroundSampleSource = rootUsesRendered && frameFallbackReason.empty() ? "rendered_triangles" : "analytical_fallback";
            latestCreatureRootGroundY = static_cast<double>(y);
            latestCreatureRootAnalyticalGroundY = analyticalRootY;
            latestCreatureRootRenderedGroundY = renderedRootY;
            latestCreatureRootGroundDeltaM = renderedRootY - analyticalRootY;
            latestRenderedSupportFallbackReason = frameFallbackReason;

            auto* transform=scene.transforms.GetComponent(heroMotionRoot); transform->ClearTransform();
            if(!creatureLoaded) {
                transform->Scale((birdLife || brackenLife || reedLife)?XMFLOAT3(float(heroStageBulk),1,float(heroStageBulk)):XMFLOAT3(.45f*float(heroStageBulk),.4f,float(heroStageBulk)));
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
                transform->Translate(XMFLOAT3(x,y+((birdLife || brackenLife || reedLife)?0:.65f),z));
            } else {
                transform->Scale(XMFLOAT3(float(heroStageBulk),1,float(heroStageBulk)));
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
            if(reedLife){for(std::size_t i=0;i<reedPresentation.legs.size();++i){auto* leg=scene.transforms.GetComponent(reedPresentation.legs[i]);XMStoreFloat4(&leg->rotation_local,XMQuaternionRotationRollPitchYaw(heroAlive && speed!=0?float(.25*std::sin(clock.physicalSeconds()*7+(i%2?pi:0))):0,0,0));leg->SetDirty();leg->UpdateTransform();}}
            if(brackenLife){for(std::size_t i=0;i<brackenPresentation.legs.size();++i){auto* leg=scene.transforms.GetComponent(brackenPresentation.legs[i]);XMStoreFloat4(&leg->rotation_local,XMQuaternionRotationRollPitchYaw(heroAlive && speed!=0?float(.24*std::sin(clock.physicalSeconds()*7+(i==0 || i==3?0:pi))):0,0,0));leg->SetDirty();leg->UpdateTransform();}const bool armored=lifecycle->stage()==stalthos::LifeStage::Adult || lifecycle->stage()==stalthos::LifeStage::Old;for(auto e:brackenPresentation.armor)scene.objects.GetComponent(e)->color=armored?XMFLOAT4(1,1,1,1):XMFLOAT4(.65f,.72f,.54f,1);scene.objects.GetComponent(brackenPresentation.club)->SetRenderable(armored);scene.objects.GetComponent(brackenPresentation.club)->SetCastShadow(armored);}
            if(birdLife){birdDistance+=std::abs(speed)*timeStep.physicalSeconds;for(std::size_t i=0;i<birdPresentation.legs.size();++i){auto* leg=scene.transforms.GetComponent(birdPresentation.legs[i]);XMStoreFloat4(&leg->rotation_local,XMQuaternionRotationRollPitchYaw(heroAlive && speed!=0?float(.45*std::sin(birdDistance*12+i*pi)):0,0,0));leg->SetDirty();leg->UpdateTransform();}}
            if (creatureLoaded) {
                auto* walkAnim = sceneWalkAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneWalkAnimEntity) : nullptr;
                auto* idleAnim = sceneIdleAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneIdleAnimEntity) : nullptr;

                stalthos::domain::AnimationBlend::ClipKind requestedClip = (actualDistance > 1e-5 && heroAlive)
                    ? stalthos::domain::AnimationBlend::ClipKind::Walk
                    : stalthos::domain::AnimationBlend::ClipKind::Idle;

                if (walkAnim && idleAnim) {
                    blendState = stalthos::domain::AnimationBlend::advance(blendState, requestedClip, heroAlive ? dt : 0.0, kCrossfadeDurationSeconds);
                    const auto blendWeights = stalthos::domain::AnimationBlend::resolve(blendState);
                    const double wIdle = blendWeights.idleWeight;
                    const double wWalk = blendWeights.walkWeight;
                    const bool isBlending = (blendState.transitionProgress < 1.0);

                    const auto crossfadeOrder = stalthos::engine::determineEvaluationOrder(scene, sceneWalkAnimEntity, sceneWalkAnimName, sceneIdleAnimEntity, sceneIdleAnimName);

                    const double wFirstLogical = crossfadeOrder.firstIsWalk ? wWalk : wIdle;
                    const double wSecondLogical = crossfadeOrder.firstIsWalk ? wIdle : wWalk;

                    if (isBlending) {
                        ++crossfadeBlendFramesTotal;
                        if (crossfadeTopology.has_full_shared_tracks) {
                            // Sequential evaluation correction:
                            // The first evaluated clip overwrites the target bones with amount 1.0, eliminating previous-frame contamination.
                            // The second evaluated clip blends on top of the first using its nominal logical weight.
                            crossfadeOrder.firstAnim->amount = 1.0f;
                            crossfadeOrder.secondAnim->amount = static_cast<float>(wSecondLogical);
                        } else {
                            // Conservative fallback for mismatched tracks: do not falsely assume exact sequential mixing
                            crossfadeOrder.firstAnim->amount = static_cast<float>(wFirstLogical);
                            crossfadeOrder.secondAnim->amount = static_cast<float>(wSecondLogical);
                        }

                        if (creatureClip) {
                            walkAnim->Pause();
                            walkAnim->timer = static_cast<float>(walkAnim->start + creaturePhase);
                            walkAnim->last_update_time = walkAnim->timer - 1.0f;
                        } else if (heroAlive) {
                            walkAnim->Play();
                        } else {
                            walkAnim->Pause();
                            walkAnim->last_update_time = walkAnim->timer - 1.0f;
                        }

                        if (heroAlive) {
                            idleAnim->Play();
                        } else {
                            idleAnim->Pause();
                            idleAnim->last_update_time = idleAnim->timer - 1.0f;
                        }

                        if (wWalk <= 0.0 || wIdle <= 0.0) {
                            crossfadeWeightsNonzeroOnlyDuringBlend = false;
                        }
                    } else {
                        // Settled in target clip
                        if (blendState.targetClip == stalthos::domain::AnimationBlend::ClipKind::Walk) {
                            walkAnim->amount = 1.0f;
                            if (creatureClip) {
                                walkAnim->Pause();
                                walkAnim->timer = static_cast<float>(walkAnim->start + creaturePhase);
                                walkAnim->last_update_time = walkAnim->timer - 1.0f;
                            } else if (speed != 0 && heroAlive) {
                                walkAnim->Play();
                            } else {
                                walkAnim->Pause();
                            }

                            idleAnim->amount = 0.0f;
                            idleAnim->last_update_time = idleAnim->timer;
                            idleAnim->Pause();

                            if (idleAnim->amount > 0.0f || idleAnim->IsPlaying() || idleAnim->last_update_time != idleAnim->timer) {
                                inactiveClipWritesPrevented = false;
                            }
                        } else {
                            // Settled in Idle
                            walkAnim->amount = 0.0f;
                            walkAnim->last_update_time = walkAnim->timer;
                            walkAnim->Pause();

                            idleAnim->amount = 1.0f;
                            if (heroAlive) {
                                idleAnim->Play();
                            } else {
                                idleAnim->Pause();
                            }

                            if (walkAnim->amount > 0.0f || walkAnim->IsPlaying() || walkAnim->last_update_time != walkAnim->timer) {
                                inactiveClipWritesPrevented = false;
                            }
                        }

                        if (wWalk > 0.0 && wIdle > 0.0) {
                            crossfadeWeightsNonzeroOnlyDuringBlend = false;
                        }
                    }
                } else if (walkAnim) {
                    // Single-clip fallback: Walk only
                    walkAnim->amount = 1.0f;
                    if (creatureClip) {
                        walkAnim->Pause();
                        walkAnim->timer = static_cast<float>(walkAnim->start + creaturePhase);
                    } else if (speed != 0 && heroAlive) {
                        walkAnim->Play();
                    } else {
                        walkAnim->Pause();
                    }
                } else if (idleAnim) {
                    // Single-clip fallback: Idle only
                    idleAnim->amount = 1.0f;
                    if (heroAlive) {
                        idleAnim->Play();
                    } else {
                        idleAnim->Pause();
                    }
                }
                previousIsDisplaced = (actualDistance > 1e-5);
            } else {
                for(size_t i=0;i<heroAnimationCount;++i) {
                    auto& animation=scene.animations[i];
                    if(speed!=0 && heroAlive) animation.Play(); else animation.Pause();
                }
            }
            if(connectedWorld) {
                auto conditions=stalthos::environment::sample(world.worldSeed(),1001,clock.calendarSeconds(),{0,0,.35,0});
                const unsigned phase=unsigned(std::clamp(int(elapsed/(duration/3)),0,2));
                if(exposureProbe) conditions=stalthos::environment::previewWeather(conditions,1);
                if(weatherPreview) conditions=stalthos::environment::previewWeather(conditions,phase);
                const double shelter=weatherPreview?(phase==2?.85:0):stalthos::environment::routeWindShelter(x,z,connectedLayout);
                localWeather=bodyWeather.advance(conditions,timeStep.physicalSeconds,shelter);
                stalthos::engine::applyWeatherToScene(scene,conditions,{.8f,0,.6f});
                scene.weather.rain_amount=0; // Pinned native emitter AV on rain transition; use lighting/wetting draft.
                const std::array<double,4> previewPhases{{.28,.5,.72,0}};renderedDayFraction=(reedProbe || protectionProbe || birdProbe || nestProbe || shoreProbe || npcPeerProbe || woodlandProbe || peerProbe || lifeStageProbe || deathNextLifeProbe || explorationProbe || saplingProbe)?.5:dayNightProbe==1?previewPhases[std::min(std::size_t(3),std::size_t(elapsed/(duration/4)))]:dayNightProbe==2?0:conditions.dayFraction;
                dayNightLight=stalthos::environment::dayNight(renderedDayFraction,conditions.cloud);stalthos::engine::dayNightScene(scene,sun,nightFill,clearSunIntensity,dayNightLight);
                scene.weather.fogStart=30;scene.weather.fogDensity=float(.0025*conditions.rainfall);

                const auto* bodyObject=!creatureLoaded?scene.objects.GetComponent(hero):nullptr;
                if(bodyObject) if(const auto* mesh=scene.meshes.GetComponent(bodyObject->meshID)) for(const auto& subset:mesh->subsets) {
                    auto* material=scene.materials.GetComponent(subset.materialID);
                    const float shade=float(1-.48*localWeather.wetness);
                    material->SetBaseColor({.48f*shade,.30f*shade,.16f*shade,1});material->SetRoughness(float(.85-.5*localWeather.wetness));
                }
                if(elapsed>=nextWeatherSample) {
                    weatherResponseSamples.push_back({{"elapsed",elapsed},{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"rainfall",conditions.rainfall},{"cloud",conditions.cloud},{"body_wetness",localWeather.wetness},{"exposed_wind",conditions.windMetersPerSecond},{"local_wind",localWeather.wind},{"comfort_proxy",localWeather.comfort},{"shelter",shelter},{"synthetic_weather",weatherPreview},{"synthetic_shelter",weatherPreview && phase==2},{"body_x",x},{"body_z",z}});
                    nextWeatherSample=elapsed+.25;
                }
            }
            if(faunaPopulation) {
                if(regionalConditions.empty() || clock.physicalSeconds()>=nextConditionsUpdate) {
                    regionalConditions=stalthos::engine::sampleRegionConditions(archipelago,world.worldSeed(),clock.calendarSeconds());
                    if(!connectedWorld) stalthos::engine::applyWeatherToScene(scene,regionalConditions.front().conditions,{.8f,0,.6f});
                    nextConditionsUpdate=clock.physicalSeconds()+1;
                    ++conditionsUpdates;
                }
                const bool physicalNow=watershedPhysical(x,z);
                if(physicalNow!=faunaPhysical) {
                    faunaTransitions.push_back({{"physical_active",physicalNow},{"body_x",x},{"body_z",z},
                        {"physical_seconds",clock.physicalSeconds()},{"snapshot",populationJson(faunaPopulation->snapshot())}});
                    if(physicalNow) endFaunaDormancy(true); else beginFaunaDormancy();
                    faunaPhysical=physicalNow;
                    faunaScene.setPhysical(scene,faunaPhysical);
                }
                if(!faunaPhysical) {
                    faunaPopulation->advanceDormant(timeStep);
                    ++faunaDormantFrames;
                    dormantPhysical+=timeStep.physicalSeconds;dormantCalendar+=timeStep.calendarSeconds;
                    faunaDormantPhysicalSeconds+=timeStep.physicalSeconds;faunaDormantCalendarSeconds+=timeStep.calendarSeconds;
                } else {
                std::optional<stalthos::fauna::BodyAwareness> actualBodyAwareness;
                if(connectedWorld && heroAlive) {const auto body=scene.transforms.GetComponent(heroMotionRoot)->GetPosition();peerCompanion.update(heroId,heroSpecies,{body.x,body.y,body.z},!directInput,timeStep.physicalSeconds,clock.physicalSeconds(),faunaPopulation->actors());actualBodyAwareness=stalthos::fauna::BodyAwareness{{body.x,body.y,body.z},wildlifeTuning,reedLife && peerCompanion.present()?peerCompanion.peer():peerCompanion.motionPeer(),peerCompanion.tuning().waitMeters,reedLife && reedDecision.leader!=stalthos::domain::ReedLeader::Hero,reedLife};
                    peerCue.SetHidden(!peerCompanion.peer());peerCue.SetText(reedLife?( !peerCompanion.present()?"Reedcrest peer separated - ordinary activity":reedDecision.leader==stalthos::domain::ReedLeader::Hero?"Reedcrest peer follows or waits - own needs first":reedDecision.leader==stalthos::domain::ReedLeader::Partner?"Following Reedcrest peer - WASD takes lead":"Reedcrest peer tending its own needs"):!peerCompanion.present()?"Peer separated - ordinary activity":peerCompanion.motionPeer()?"Peer nearby - approaching or waiting":"Peer nearby - tending its needs");peerCue.params.posX=app.canvas.GetLogicalWidth()*.5f;peerCue.params.posY=app.canvas.GetLogicalHeight()*.75f;
                    if(peerCompanion.peer()!=previousPeer || peerCompanion.present()!=previousPeerPresent){peerEvents.push_back({{"peer",peerCompanion.peer()},{"present",peerCompanion.present()},{"physical_seconds",clock.physicalSeconds()}});previousPeer=peerCompanion.peer();previousPeerPresent=peerCompanion.present();}}
                faunaPopulation->advance(timeStep,true,actualBodyAwareness,connectedWorld);
                if(connectedWorld) {const auto& pair=faunaPopulation->npcPeers();const bool moving=bool(pair.motion(faunaPopulation->actors(),peerCompanion.motionPeer()));if(moving!=lastNpcPeerMotion && npcPeerEvents.size()<128){npcPeerEvents.push_back({{"physical_seconds",clock.physicalSeconds()},{"motion",moving},{"leader",pair.leader()},{"follower",pair.link().peer()},{"present",pair.link().present()}});lastNpcPeerMotion=moving;}}
                if(connectedWorld && elapsed>=nextNpcPeerSample && npcPeerSamples.size()<600) {
                    const auto& pair=faunaPopulation->npcPeers();json actors=json::array();
                    for(const auto& a:faunaPopulation->actors())if(a.id==pair.leader() || a.id==pair.link().peer())actors.push_back({{"id",a.id},{"x",a.position.x},{"y",a.position.y},{"z",a.position.z},{"distance",a.distanceTravelledMeters},{"activity",unsigned(a.activity)},{"hunger",a.hunger},{"thirst",a.thirst},{"fatigue",a.fatigue}});
                    npcPeerSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"leader",pair.leader()},{"follower",pair.link().peer()},{"present",pair.link().present()},{"waiting_follower",pair.waitingFollower(faunaPopulation->actors(),peerCompanion.motionPeer(),actualBodyAwareness)},{"motion",bool(pair.motion(faunaPopulation->actors(),peerCompanion.motionPeer()))},{"hero_peer",peerCompanion.peer()},{"body_x",x},{"body_z",z},{"actors",actors}});nextNpcPeerSample=elapsed+.2;
                }
                if(connectedWorld && elapsed>=nextPeerSample){json actors=json::array();for(const auto& a:faunaPopulation->actors())if(a.id==peerCompanion.peer() || peerProbe)actors.push_back({{"id",a.id},{"x",a.position.x},{"z",a.position.z},{"distance",a.distanceTravelledMeters},{"activity",unsigned(a.activity)},{"hunger",a.hunger},{"thirst",a.thirst}});peerSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"body_x",x},{"body_z",z},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z},{"peer",peerCompanion.peer()},{"present",peerCompanion.present()},{"motion_peer",peerCompanion.motionPeer()},{"direct",directInput},{"actors",actors}});nextPeerSample=elapsed+.2;}
                if(connectedWorld && elapsed>=nextWildlifeSample) {
                    json responses=json::array();
                    for(const auto& actor:faunaPopulation->actors()) {const auto response=actualBodyAwareness?stalthos::fauna::awareness(actor.position,*actualBodyAwareness):stalthos::fauna::AwarenessDecision{};responses.push_back({{"id",actor.id},{"x",actor.position.x},{"z",actor.position.z},{"heading",actor.headingRadians},{"distance_travelled",actor.distanceTravelledMeters},{"activity",unsigned(actor.activity)},{"noticed",response.noticed},{"avoid",response.avoid},{"body_distance",response.distance}});}
                    wildlifeSamples.push_back({{"elapsed",elapsed},{"body_x",x},{"body_z",z},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z},{"phase",wildlifeProbe==2?"camera_only":elapsed<6?"approach":elapsed<10?"hold":elapsed<18?"withdraw":"idle"},{"actors",responses}});nextWildlifeSample=elapsed+.2;
                }
                if(connectedWorld)for(const auto& event:faunaPopulation->events())if(event.resource==209)exteriorWaterNpcContacts.push_back({{"actor",event.actor},{"amount",event.amount},{"physical_seconds",clock.physicalSeconds()}});
                if(exteriorBrowseEnabled)for(const auto& event:faunaPopulation->events())if(event.resource==208)exteriorBrowseNpcContacts.push_back({{"actor",event.actor},{"amount",event.amount},{"physical_seconds",clock.physicalSeconds()}});
                faunaResourceEvents+=faunaPopulation->events().size();
                if(!faunaPopulation->events().empty()) {
                    for(const auto& res:resourceSnapshot()) cachedResourceDtos[res.id]=res;
                    for(auto& rec:shrubVisualInstances) applyShrubVisual(rec,cachedResourceDtos.at(rec.resourceId));
                }
                auto waitingNpc=connectedWorld?faunaPopulation->npcPeers().waitingFollower(faunaPopulation->actors(),peerCompanion.motionPeer(),actualBodyAwareness):0;
                if(reedLife && reedDecision.leader==stalthos::domain::ReedLeader::Hero)for(const auto& a:faunaPopulation->actors())if(a.id==peerCompanion.peer() && std::hypot(a.position.x-x,a.position.z-z)<=3.01)waitingNpc=a.id;
                faunaScene.update(scene,faunaPopulation->actors(),creatureClipDuration,creatureStrideLength,waitingNpc);
                ++faunaSimulationFrames;
                for(const auto& actor:faunaPopulation->actors()) ++faunaActivityFrames[static_cast<unsigned>(actor.activity)];
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
                    viewController.cancelDangerRecall();
                    viewController.applyOrbitInput(-mouseState.delta_position.x, mouseState.delta_position.y);
                }
            }

            if(connectedWorld && faunaPopulation) {
                const auto body=scene.transforms.GetComponent(heroMotionRoot)->GetPosition();
                static const std::vector<stalthos::fauna::ActorView> noPhysicalActors;
                carnivoreAwareness.update(heroId,{body.x,body.y,body.z},heroAlive,faunaPhysical?faunaPopulation->actors():noPhysicalActors);
                if(carnivoreAwareness.began() && !down('V') && !toggleKey && !down('W') && !down('A') && !down('S') && !down('D') && wi::input::GetMouseState().delta_wheel==0 && !wi::input::GetMouseState().left_button_press && viewController.getState().mode==stalthos::domain::CameraMode::LocallyDetached){viewController.requestDangerRecall();++carnivoreRecallRequests;}
                if(!carnivoreAwareness.actor())viewController.cancelDangerRecall();
                // This injected orbit is a diagnostic action through the same explicit-input API.
                if(carnivoreProbe==2 && !carnivoreCancelled && clock.physicalSeconds()>=.5){viewController.applyOrbitInput(40,0);carnivoreCancelled=true;}
                carnivoreCue.SetHidden(!carnivoreAwareness.actor());
                carnivoreCue.SetText(viewController.dangerRecallActive()?"Longstride nearby - returning view (camera input cancels)":"Longstride nearby - potential danger");
                carnivoreCue.params.posX=app.canvas.GetLogicalWidth()*.5f;carnivoreCue.params.posY=app.canvas.GetLogicalHeight()*.67f;
            }

            try {
                auto terrainSampler = [&](double cx, double cz) {
                    if(populatedTerrain) return populatedTerrain->height(cx,cz);
                    if (!landscape.inBounds(cx, cz)) throw std::runtime_error("camera outside reference landscape");
                    return landscape.height(cx, cz);
                };
                stalthos::TimeStep viewDt{dt, dt * calendarScale};
                if(shoreProbe==3){auto state=viewController.getState();state.detachedAnchor.x=224+6*std::sin(clock.physicalSeconds()*.3);state.detachedAnchor.z=24;viewController.setState(state);}
                if(npcPeerProbe==2){auto state=viewController.getState();state.detachedAnchor.x=132+10*std::sin(clock.physicalSeconds()*.5);state.detachedAnchor.z=30;viewController.setState(state);}
                if(peerProbe==2){auto state=viewController.getState();state.detachedAnchor.x=peerAnchor.x+2*std::sin(clock.physicalSeconds());state.detachedAnchor.z=peerAnchor.z;viewController.setState(state);}
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

            if(connectedWorld){const bool pulse=bodyMotionAudio.advance({x,y,z},timeStep.physicalSeconds,heroAlive,latestSupportMode=="terrain_supported");if(rustleReady){rustleMaxCursor=std::max(rustleMaxCursor,wi::audio::GetTotalSamplesPlayed(&rustleInstance));const double range=24,distance=std::sqrt(std::pow(cameraOut.cameraState.eye.x-x,2)+std::pow(cameraOut.cameraState.eye.y-y,2)+std::pow(cameraOut.cameraState.eye.z-z,2));const float gain=float(.12*std::clamp(1-distance/range,0.0,1.0));wi::audio::SetVolume(gain,&rustleInstance);if(!bodyMotionAudio.moving())wi::audio::Pause(&rustleInstance);if(pulse){wi::audio::Stop(&rustleInstance);wi::audio::Play(&rustleInstance);++rustleRequests;if(rustleEvents.size()<256)rustleEvents.push_back({{"elapsed",elapsed},{"physical_seconds",clock.physicalSeconds()},{"capture_seconds",audioCapture.seconds()},{"hero_id",heroId},{"body_x",x},{"body_z",z},{"gain",gain}});else ++rustleEventsDropped;}}}
            audioCapture.poll();
            if(connectedWorld){
                const auto site=nestSite();const auto refusal=nesting.refusal(heroId,heroSpecies,lifecycle->stage(),needs->hunger(),needs->thirst(),needs->fatigue(),site);
                const bool syntheticNest=nestProbe && !nestProbeRequested && clock.physicalSeconds()>=2;
                const bool requested=(userSteering && down('B')) || syntheticNest;
                if(requested && !previousNestKey){if(syntheticNest)nestProbeRequested=true;const bool placed=nesting.lay(heroId,heroSpecies,lifecycle->stage(),needs->hunger(),needs->thirst(),needs->fatigue(),site,clock.calendarSeconds());if(placed){nestEntities=stalthos::engine::nestScene(scene,*nesting.clutch(),[&](double gx,double gz){return populatedTerrain->height(gx,gz);});nestMessage="Clutch laid - incubation follows world time; WASD remains immediate";}else nestMessage=refusal;nestMessageUntil=clock.physicalSeconds()+5;nestEvents.push_back({{"placed",placed},{"refusal",refusal},{"hero",heroId},{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"x",site.position.x},{"y",site.position.y},{"z",site.position.z},{"supported",site.supported},{"dry",site.dry},{"clear",site.clear},{"relief",site.relief},{"input",syntheticNest?"labelled synthetic B intention":"B body-focus rising edge"}});}
                if(nesting.incubated() && !nesting.offspring() && clock.physicalSeconds()>=nextHatchAttempt){
                    nextHatchAttempt=clock.physicalSeconds()+1;hatchStatus="Waiting for body-active supported hatch site";
                    if(faunaPhysical){
                        const auto& nest=*nesting.clutch();const auto path=stalthos::domain::hatchlingPath(nest.position,faunaPopulation->habitat(),[&](double gx,double gz)->std::optional<double>{if(!landscape.inBounds(gx,gz) || landscape.waterContains(gx,gz))return std::nullopt;try{return populatedTerrain->height(gx,gz);}catch(const std::out_of_range&){return std::nullopt;}});
                        if(path){
                            std::set<stalthos::Identity> excluded;const auto ws=world.snapshot();for(const auto& a:ws.active)excluded.insert(a.first);for(const auto& a:ws.retained)excluded.insert(a.id);for(const auto& a:ws.deadHeroes)excluded.insert(a.first);
                            stalthos::Identity candidate=2;for(const auto& a:faunaPopulation->actors())excluded.insert(a.id);while(excluded.count(candidate) && candidate<std::numeric_limits<stalthos::Identity>::max())++candidate;
                            const auto before=faunaPopulation->snapshot();const auto oldLedger=ledgerJson(ledgerSnapshot());const auto oldSessions=faunaPopulation->sessions();const auto session=nextInteractionSession;
                            const auto result=faunaPopulation->admitHatchling(candidate,path->points,path->attach,nextInteractionSession,excluded);
                            hatchStatus=result==stalthos::fauna::RegionalAdmission::Accepted?"One independent hatchling admitted":"Hatch delayed: population/site/session capacity";
                            if(result==stalthos::fauna::RegionalAdmission::Accepted){
                                if(!nesting.recordAdmittedOffspring(candidate))throw std::runtime_error("committed hatch missing clutch ownership");bornCalendar=clock.calendarSeconds();childSession=session;
                                const auto visual=stalthos::engine::hatchlingScene(scene,candidate);faunaScene.appendHatchling(scene,faunaPopulation->actors(),visual.root,visual.legs);faunaScene.update(scene,faunaPopulation->actors(),creatureClipDuration,creatureStrideLength);faunaScene.setPhysical(scene,faunaPhysical);
                                // Close exactly the first egg only after a real population commit.
                                for(auto e:nestEntities){const auto* name=scene.names.GetComponent(e);if(name && name->name=="Smalltrotter egg - incubation only"){if(auto* o=scene.objects.GetComponent(e)){auto* m=scene.meshes.GetComponent(o->meshID);for(auto& v:m->vertex_positions){const float ground=Ground(v.x,v.z);v.y=ground+.02f+(v.y-ground)*.15f;}m->CreateRenderData();o->color=XMFLOAT4(.52f,.43f,.24f,1);}break;}}
                            }
                            auto after=faunaPopulation->snapshot();if(result==stalthos::fauna::RegionalAdmission::Accepted)after.actors.pop_back();after.habitatSignature=before.habitatSignature;
                            if(hatchEvents.size()<128)hatchEvents.push_back({{"event","admission_attempt"},{"result",unsigned(result)},{"child",result==stalthos::fauna::RegionalAdmission::Accepted?candidate:0},{"session",session},{"physical_seconds",clock.physicalSeconds()},{"birth_calendar",bornCalendar},{"position",{nest.position.x,nest.position.y,nest.position.z}},{"path_points",path->points.size()},{"old_actors_unchanged",populationJson(before)==populationJson(after)},{"ledger_unchanged",oldLedger==ledgerJson(ledgerSnapshot())},{"old_session_count",oldSessions.size()},{"session_count",faunaPopulation->sessions().size()}});
                        }else hatchStatus="Hatch delayed: no dry supported local route";
                    }
                }
                previousNestKey=requested;std::string text;
                if(clock.physicalSeconds()<nestMessageUntil)text=nestMessage;
                else if(nesting.clutch()){const auto& c=*nesting.clutch();if(std::hypot(x-c.position.x,z-c.position.z)<12)text=nesting.offspring()?"One Smalltrotter hatched - independent young; two eggs remain unmodelled":nesting.incubated()?hatchStatus:"Nearby clutch incubating: "+std::to_string(int(nesting.progress()*100))+"% - world calendar";}
                else text=refusal.empty()?"B - choose this nearby nest site and lay clutch":"B - nest intention: "+refusal;
                nestCue.SetText(text);nestCue.SetHidden(text.empty());nestCue.params.posX=app.canvas.GetLogicalWidth()*.5f;nestCue.params.posY=app.canvas.GetLogicalHeight()*.56f;
                if(clock.physicalSeconds()>=nextNestSample){json sample={{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"hero",heroId},{"stage",stalthos::domain::lifeStageName(lifecycle->stage())},{"body_x",x},{"body_z",z},{"direct",directInput},{"speed",speed},{"progress",nesting.progress()},{"incubated",nesting.incubated()},{"nest_objects",nestEntities.size()},{"cue",text}};if(nesting.clutch()){sample["parent"]=nesting.clutch()->parent;sample["nest_x"]=nesting.clutch()->position.x;sample["nest_z"]=nesting.clutch()->position.z;}sample["offspring"]=nesting.offspring();sample["child_session"]=childSession;sample["birth_calendar"]=bornCalendar;sample["physical_active"]=faunaPhysical;sample["actor_count"]=faunaPopulation->actors().size();sample["replica_count"]=faunaScene.replicaCount();sample["camera_x"]=cameraOut.cameraState.target.x;sample["camera_z"]=cameraOut.cameraState.target.z;sample["hero_attention"]=heroIntent.perception.attention().has_value();sample["hero_session"]=physicalInteractionSession;
                    if(nesting.offspring()){const auto snapshot=faunaPopulation->snapshot();for(const auto& a:snapshot.actors)if(a.id==nesting.offspring()){sample["child"]={{"id",a.id},{"age",a.calendarAge},{"stage",unsigned(a.stage)},{"x",a.position.x},{"y",a.position.y},{"z",a.position.z},{"distance",a.distanceTravelledMeters},{"hunger",a.hunger},{"thirst",a.thirst},{"fatigue",a.fatigue},{"site",a.site},{"target",a.target}};try{sample["child_rendered_ground_error"]=std::abs(a.position.y-populatedTerrain->height(a.position.x,a.position.z));}catch(const std::out_of_range&){sample["child_ground_missing"]=true;}}}
                    if(nestSamples.size()<8192)nestSamples.push_back(sample);nextNestSample=clock.physicalSeconds()+.5;}
            }
            if(brackenLife){const bool observed=defenseObservation.has_value();const bool bracing=observed && clock.physicalSeconds()<braceUntil && !directInput;defenseCue.SetHidden(!observed || directInput);defenseCue.SetText(defenseOwned?"Critical-condition small animal near adult Longstride - interposing; WASD overrides":bracing?"Bracing armored flank - temporary protection, no adoption":"Distressed small animal + nearby adult Longstride - cautious protection");defenseCue.params.posX=app.canvas.GetLogicalWidth()*.5f;defenseCue.params.posY=app.canvas.GetLogicalHeight()*.48f;
                distressCue.SetHidden(!observed);if(observed){const auto p=defenseObservation->vulnerableBody;XMFLOAT3 projected;XMStoreFloat3(&projected,XMVector3Project(XMVectorSet(float(p.x),float(p.y+1.4),float(p.z),1),0,0,app.canvas.GetLogicalWidth(),app.canvas.GetLogicalHeight(),0,1,wi::scene::GetCamera().GetProjection(),wi::scene::GetCamera().GetView(),XMMatrixIdentity()));distressCue.params.posX=projected.x;distressCue.params.posY=projected.y;distressCue.SetHidden(projected.z<0 || projected.z>1);}
                if(clock.physicalSeconds()>=nextDefenseSample && defenseSamples.size()<4096){json actors=json::array();for(const auto& a:faunaPopulation->actors())if(a.id==fixtureSmall || a.id==fixtureThreat || (observed && (a.id==defenseObservation->vulnerable || a.id==defenseObservation->threat)))actors.push_back({{"id",a.id},{"species",unsigned(a.species)},{"age",a.calendarAge},{"x",a.position.x},{"y",a.position.y},{"z",a.position.z},{"hunger",a.hunger},{"thirst",a.thirst},{"alive",a.alive}});defenseSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"hero",heroId},{"species",unsigned(heroSpecies)},{"stage",unsigned(lifecycle->stage())},{"needs_species",unsigned(needs->species())},{"lifecycle_species",unsigned(lifecycle->species())},{"body_x",x},{"body_y",y},{"body_z",z},{"speed",speed},{"direct",directInput},{"hunger",needs->hunger()},{"thirst",needs->thirst()},{"fatigue",needs->fatigue()},{"observed",observed},{"vulnerable",observed?defenseObservation->vulnerable:0},{"threat",observed?defenseObservation->threat:0},{"interposing",defenseOwned},{"bracing",bracing},{"starts",defenseStarts},{"cue_visible",!defenseCue.IsHidden()},{"distress_visible",!distressCue.IsHidden()},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z},{"body_supported",latestSupportMode=="terrain_supported"},{"physical_active",faunaPhysical},{"actors",actors},{"npc_count",faunaPopulation->actors().size()},{"npc_sessions",faunaPopulation->sessions().size()},{"club_visible",(scene.objects.GetComponent(brackenPresentation.club)->_flags & wi::scene::ObjectComponent::RENDERABLE)!=0}});nextDefenseSample=clock.physicalSeconds()+.1;}
            }
            if(reedLife && clock.physicalSeconds()>=nextReedSample && reedSamples.size()<4096){json partner=nullptr;for(const auto& a:faunaPopulation->actors())if(a.id==peerCompanion.peer() || (!peerCompanion.peer() && a.id==reedFixturePeer))partner={{"id",a.id},{"x",a.position.x},{"y",a.position.y},{"z",a.position.z},{"distance",a.distanceTravelledMeters},{"hunger",a.hunger},{"thirst",a.thirst},{"fatigue",a.fatigue},{"age",a.calendarAge},{"activity",unsigned(a.activity)},{"alive",a.alive}};reedSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"hero",heroId},{"species",unsigned(heroSpecies)},{"needs_species",unsigned(needs->species())},{"lifecycle_species",unsigned(lifecycle->species())},{"age",lifecycle->calendarAge()},{"stage",unsigned(lifecycle->stage())},{"body_x",x},{"body_y",y},{"body_z",z},{"speed",speed},{"direct",directInput},{"hunger",needs->hunger()},{"thirst",needs->thirst()},{"fatigue",needs->fatigue()},{"peer",peerCompanion.peer()},{"present",peerCompanion.present()},{"motion_peer",peerCompanion.motionPeer()},{"leader",unsigned(reedDecision.leader)},{"follow_owned",reedFollowOwned},{"target_x",reedDecision.heroTarget?reedDecision.heroTarget->x:x},{"target_z",reedDecision.heroTarget?reedDecision.heroTarget->z:z},{"partner",partner},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z},{"physical_active",faunaPhysical},{"body_supported",latestSupportMode=="terrain_supported"},{"npc_count",faunaPopulation->actors().size()},{"npc_sessions",faunaPopulation->sessions().size()}});nextReedSample=clock.physicalSeconds()+.2;}
            app.Run(); ++runCalls;
            if(saplingRoot!=wi::ecs::INVALID_ENTITY){
                const stalthos::Vec3 body{x,y,z};const bool supported=latestSupportMode=="terrain_supported";const bool eligible=sapling.eligible(body,heroAlive,supported);
                const bool requested=(userSteering && down('F')) || syntheticSaplingAction;
                if(requested && !previousSaplingKey){const bool applied=sapling.apply(world,body,heroAlive,supported,clock.calendarSeconds());saplingEvents.push_back({{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"hero_id",heroId},{"body_x",x},{"body_z",z},{"eligible",eligible},{"applied",applied},{"input",syntheticSaplingAction?"labelled synthetic F action":"F body-focus rising edge"},{"revision",world.regionSnapshot(sapling.region).revision}});}
                previousSaplingKey=requested;const auto exception=sapling.exception(world);const bool damaged=exception && exception->condition==stalthos::TreeCondition::StandingDamaged;
                if(damaged && !saplingVisualApplied){auto* tr=scene.transforms.GetComponent(saplingRoot);*tr=*saplingBase;tr->RotateRollPitchYaw(XMFLOAT3(0,0,.32f));tr->UpdateTransform();for(size_t i=0;i<scene.objects.GetCount();++i)if(scene.Entity_IsDescendant(scene.objects.GetEntity(i),saplingRoot))scene.objects[i].color=XMFLOAT4(1,.35f,.12f,1);saplingVisualApplied=true;}
                saplingCue.SetHidden(!eligible);saplingCue.SetText(damaged?"Disturbed sapling - this individual stays changed":"F - disturb nearby sapling");saplingCue.params.posX=app.canvas.GetLogicalWidth()*.5f;saplingCue.params.posY=app.canvas.GetLogicalHeight()*.6f;
                if(elapsed>=nextSaplingSample){saplingSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"body_x",x},{"body_z",z},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z},{"eligible",eligible},{"damaged",damaged},{"visual_applied",saplingVisualApplied},{"revision",world.regionSnapshot(sapling.region).revision},{"changed_at",exception?exception->changedAt:0},{"cue_visible",!saplingCue.IsHidden()},{"trees",treeVisualSnapshot()}});nextSaplingSample=elapsed+.25;}
            }
            if(nestProbe)for(std::size_t i=0;i<(hatchProbe?5:3);++i)if(!nestCaptured[i] && clock.physicalSeconds()>=(i==0?3:i==1?8:i==2?12:i==3?22:27)){const auto filename="nest-"+std::to_string(i)+".png";nestCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());nestCaptures.push_back({{"filename",filename},{"saved",nestCaptured[i]},{"progress",nesting.progress()},{"body_x",x},{"body_z",z}});}
            if(saplingProbe)for(std::size_t i=0;i<4;++i)if(!saplingCaptured[i] && clock.physicalSeconds()>=(i==0?2:i==1?3.5:i==2?(creatureLoaded?17:6):(creatureLoaded?33.5:11.5))){const auto filename="sapling-"+std::to_string(i)+".png";saplingCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());saplingCaptures.push_back({{"filename",filename},{"saved",saplingCaptured[i]},{"body_x",x},{"body_z",z},{"damaged",sapling.exception(world).has_value()}});}
            if(explorationProbe)for(std::size_t i=0;i<3;++i)if(!explorationCaptured[i] && clock.physicalSeconds()>=(i==0?4:i==1?12:17)){const auto filename="exploration-"+std::to_string(i)+".png";explorationCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());explorationCaptures.push_back({{"filename",filename},{"saved",explorationCaptured[i]},{"phase",unsigned(heroExploration.phase())}});}
            if(deathNextLifeProbe)for(std::size_t i=0;i<4;++i)if(!nextLifeCaptured[i] && clock.physicalSeconds()>=(i==0?2:i==1?8:i==2?11:15)){const auto filename="death-next-life-"+std::to_string(i)+".png";nextLifeCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());nextLifeCaptures.push_back({{"filename",filename},{"saved",nextLifeCaptured[i]},{"hero_id",heroId},{"alive",heroAlive}});}
            if(lifeStageProbe)for(std::size_t i=0;i<4;++i)if(!lifeStageCaptured[i] && clock.physicalSeconds()>=(i==0?2:i==1?7.5:i==2?11.5:14)){const auto filename="life-stage-"+std::to_string(i)+".png";lifeStageCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());lifeStageCaptures.push_back({{"filename",filename},{"saved",lifeStageCaptured[i]},{"age",lifecycle->calendarAge()},{"stage",stalthos::domain::lifeStageName(lifecycle->stage())},{"bulk",heroStageBulk},{"body_x",x},{"body_z",z}});}
            if(shoreProbe)for(std::size_t i=0;i<4;++i)if(!shoreCaptured[i] && (shoreProbe==1?x>=(i==0?185:i==1?220:i==2?246:258):clock.physicalSeconds()>=(i==0?2:i==1?10:i==2?20:28))){const auto filename="shore-"+std::to_string(i)+".png";shoreCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());shoreCaptures.push_back({{"filename",filename},{"saved",shoreCaptured[i]},{"x",x},{"z",z}});}
            if(npcPeerProbe)for(std::size_t i=0;i<3;++i)if(!npcPeerCaptured[i] && clock.physicalSeconds()>=(i==0?2:i==1?8:14)){const auto filename="npc-peer-"+std::to_string(i)+".png";npcPeerCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());npcPeerCaptures.push_back({{"filename",filename},{"saved",npcPeerCaptured[i]}});}
            if(peerProbe)for(std::size_t i=0;i<4;++i)if(!peerCaptured[i] && clock.physicalSeconds()>=(i==0?3:i==1?9:i==2?25:39)){const auto filename="peer-"+std::to_string(i)+".png";peerCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());peerCaptures.push_back({{"filename",filename},{"saved",peerCaptured[i]}});}
            if(connectedWorld && elapsed>=nextDayNightSample){const auto* lamp=scene.lights.GetComponent(sun);const auto sky=scene.weather.sunDirection;const double error=std::sqrt(std::pow(lamp->direction.x-dayNightLight.sunDirection.x,2)+std::pow(lamp->direction.y-dayNightLight.sunDirection.y,2)+std::pow(lamp->direction.z-dayNightLight.sunDirection.z,2));dayNightSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"calendar_fraction",std::fmod(clock.calendarSeconds(),86400)/86400},{"rendered_fraction",renderedDayFraction},{"sun_intensity",lamp->intensity},{"fill_intensity",scene.lights.GetComponent(nightFill)->intensity},{"direction_error",error},{"sky_light_direction_error",std::hypot(std::hypot(double(sky.x-lamp->direction.x),double(sky.y-lamp->direction.y)),double(sky.z-lamp->direction.z))},{"body_x",x},{"body_z",z},{"resting",resting},{"direct",directInput},{"fatigue",needs->fatigue()},{"thirst",needs->thirst()}});nextDayNightSample=elapsed+.1;}
            if(dayNightProbe || woodlandProbe)for(std::size_t i=0;i<4;++i)if(!dayNightCaptured[i] && elapsed>=duration*(double(i)+.5)/4){const auto filename="day-night-"+std::to_string(i)+".png";dayNightCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());dayNightCaptures.push_back({{"filename",filename},{"saved",dayNightCaptured[i]},{"rendered_fraction",renderedDayFraction},{"body_x",x},{"body_z",z}});}

            if(connectedWorld)scene.objects.GetComponent(exteriorWaterEntity)->SetRenderable(getResourceRemaining(209)>0);
            if(exteriorWaterProbe)for(std::size_t i=0;i<3;++i)if(!exteriorWaterCaptured[i] && elapsed>=(i==0?1:i==1?6:13)){const auto filename="exterior-water-"+std::to_string(i)+".png";exteriorWaterCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());exteriorWaterCaptures.push_back({{"filename",filename},{"saved",exteriorWaterCaptured[i]}});}
            if(exteriorBrowseProbe)for(std::size_t i=0;i<3;++i)if(!exteriorBrowseCaptured[i] && (i==0?elapsed>=1:i==1?elapsed>=5:!autonomyContacts.empty() || elapsed>=12)){const auto filename="exterior-browse-"+std::to_string(i)+".png";exteriorBrowseCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());exteriorBrowseCaptures.push_back({{"filename",filename},{"saved",exteriorBrowseCaptured[i]}});}
            if(destinationProbe && !destinationInjected && clock.physicalSeconds()>=1){
                if(destinationProbe==3){DirectX::XMFLOAT3 screen;DirectX::XMStoreFloat3(&screen,DirectX::XMVector3Project(DirectX::XMVectorSet(x+40,Ground(x+40,z),z,1),0,0,app.canvas.GetLogicalWidth(),app.canvas.GetLogicalHeight(),0,1,wi::scene::GetCamera().GetProjection(),wi::scene::GetCamera().GetView(),DirectX::XMMatrixIdentity()));RECT client{};GetClientRect(window,&client);const int px=int(screen.x*client.right/app.canvas.GetLogicalWidth()),py=int(screen.y*client.bottom/app.canvas.GetLogicalHeight());PostMessageW(window,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(px,py));PostMessageW(window,WM_RBUTTONUP,0,MAKELPARAM(px,py));destinationInjected=true;}
                else for(int k=0;k<48 && !destinationInjected;++k){const double angle=k*6.283185307179586/48;const float gx=x+float(4*std::sin(angle)),gz=z+float(4*std::cos(angle));const stalthos::Vec3 target{gx,Ground(gx,gz),gz};if(!supportedDestination({x,Ground(x,z),z},target))continue;DirectX::XMFLOAT3 screen;DirectX::XMStoreFloat3(&screen,DirectX::XMVector3Project(DirectX::XMVectorSet(gx,float(target.y),gz,1),0,0,app.canvas.GetLogicalWidth(),app.canvas.GetLogicalHeight(),0,1,wi::scene::GetCamera().GetProjection(),wi::scene::GetCamera().GetView(),DirectX::XMMatrixIdentity()));if(screen.x<10||screen.y<10||screen.x>=app.canvas.GetLogicalWidth()-10||screen.y>=app.canvas.GetLogicalHeight()-10)continue;const auto hit=scene.Intersects(wi::renderer::GetPickRay(long(screen.x),long(screen.y),app.canvas));if(hit.entity==wi::ecs::INVALID_ENTITY || std::hypot(double(hit.position.x)-gx,double(hit.position.z)-gz)>.2)continue;RECT client{};GetClientRect(window,&client);const int px=int(screen.x*client.right/app.canvas.GetLogicalWidth()),py=int(screen.y*client.bottom/app.canvas.GetLogicalHeight());PostMessageW(window,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(px,py));PostMessageW(window,WM_RBUTTONUP,0,MAKELPARAM(px,py));destinationInjected=true;}
            }
            if(connectedWorld && elapsed>=nextDestinationSample){destinationSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"body_x",x},{"body_z",z},{"active",destinationActive},{"status",destinationStatus},{"direct",directInput},{"forward",autonomyAction.forward},{"contact",autonomyAction.contact},{"thirst",needs->thirst()},{"goal_contextual",heroIntent.goal()?heroIntent.goal()->contextual:false},{"resting",resting},{"camera_target_x",cameraOut.cameraState.target.x},{"camera_target_z",cameraOut.cameraState.target.z}});nextDestinationSample=elapsed+.1;}
            if(destinationProbe)for(std::size_t i=0;i<3;++i)if(!destinationCaptured[i] && clock.physicalSeconds()>=(i==0?1.5:i==1?3:8)){const auto filename="destination-"+std::to_string(i)+".png";destinationCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());destinationCaptures.push_back({{"filename",filename},{"saved",destinationCaptured[i]}});}

            if(exposureProbe) for(std::size_t phase=0;phase<3;++phase) if(!exposureCaptured[phase] && clock.physicalSeconds()>=(phase==0?10:phase==1?14:18)){const auto filename="exposure-"+std::to_string(phase)+".png";exposureCaptured[phase]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());exposureCaptures.push_back({{"filename",filename},{"saved",exposureCaptured[phase]}});}
            if(restProbe) for(std::size_t phase=0;phase<3;++phase) if(!restCaptured[phase] && elapsed>=(phase==0?2:phase==1?5:12)){const auto filename="hero-rest-"+std::to_string(phase)+".png";restCaptured[phase]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/filename).u8string());restCaptures.push_back({{"filename",filename},{"saved",restCaptured[phase]},{"resting",resting},{"fatigue",needs->fatigue()}});}
            if(wildlifeProbe || exteriorResidentsProbe) for(std::size_t phase=0;phase<3;++phase) if(!wildlifeCaptured[phase] && elapsed>=(phase==0?2:phase==1?8:19)) {
                const auto filename="wildlife-awareness-"+std::to_string(phase)+".png";wildlifeCaptured[phase]=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/filename).u8string());wildlifeCaptures.push_back({{"filename",filename},{"saved",wildlifeCaptured[phase]},{"body_x",x},{"body_z",z}});
            }
            if(weatherPreview) for(std::size_t phase=0;phase<3;++phase) if(!weatherCaptured[phase] && elapsed>=duration*(double(phase)/3+1.0/6)) {
                const auto filename="weather-response-"+std::to_string(phase)+".png";
                weatherCaptured[phase]=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/filename).u8string());
                weatherResponseCaptures.push_back({{"filename",filename},{"saved",weatherCaptured[phase]},{"phase",phase},{"synthetic_shelter",phase==2}});
            }
            if(faunaPopulation) faunaScene.verifyPhysical(scene,faunaPhysical);
            if(populatedTerrain) {
                populatedMaxX=std::max(populatedMaxX,double(x));
                const double renderedGround=populatedTerrain->height(x,z);
                populatedRootError=std::max(populatedRootError,std::abs(double(y)-renderedGround));
                ++populatedSupportSamples;
                const auto renderedIndex=scene.objects.GetIndex(hero);
                const auto center=scene.transforms.GetComponent(heroMotionRoot)->GetPosition();
                if(renderedIndex<scene.aabb_objects.size()) {
                    const auto actual=scene.aabb_objects[renderedIndex].getCenter();
                    populatedBoundsError=std::max(populatedBoundsError,std::max({std::abs(double(actual.x)-center.x),std::abs(double(actual.y)-center.y),std::abs(double(actual.z)-center.z)}));
                    if(!creatureLoaded && !birdLife && !brackenLife && !reedLife) {
                        const auto matrix=scene.transforms.GetComponent(heroMotionRoot)->GetWorldMatrix();
                        XMFLOAT3 bottom;XMStoreFloat3(&bottom,XMVector3TransformCoord(XMVectorSet(0,-1,0,1),matrix));
                        populatedProxyError=std::max(populatedProxyError,std::abs(double(bottom.y)-renderedGround));
                    }
                }
                const std::array<double,4> thresholds=connectedWorld?std::array<double,4>{connectedLayout.joinX-24,connectedLayout.joinX+14,connectedLayout.meadowX-30,connectedLayout.meadowX}:std::array<double,4>{72,110,256,400};
                for(std::size_t i=0;i<thresholds.size();++i) if((populatedTraversal || connectedRouteWalk) && !populatedCaptured[i] && x>=thresholds[i]) {
                    const auto filename="populated-terrain-"+std::to_string(i)+".png";
                    const bool saved=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/filename).u8string());
                    populatedCaptured[i]=saved;
                    populatedCaptures.push_back({{"capture",i},{"filename",filename},{"saved",saved},{"body_x",x},{"body_z",z},
                        {"visible_object",std::find(renderPath.visibility_main.visibleObjects.begin(),renderPath.visibility_main.visibleObjects.end(),renderedIndex)!=renderPath.visibility_main.visibleObjects.end()}});
                }
            }
            if (creatureLoaded) {
                if (nativeSoleAdapter.isInitialized() && (!populatedTerrain || landscape.inBounds(x,z))) {
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
                const auto currentBlendWeights = stalthos::domain::AnimationBlend::resolve(blendState);
                const bool isBlendingPhase = (blendState.transitionProgress < 1.0);
                if (!cur.capture_saved) {
                    cur.capture_saved = wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(), (output / cur.capture_filename).u8string());
                }
                if (isBlendingPhase && !cur.interior_capture_saved && blendState.transitionProgress >= 0.5) {
                    cur.interior_capture_filename = "transition-" + std::to_string(activeTransitionPhaseIndex) + "-interior-blend.png";
                    cur.interior_capture_saved = wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(), (output / cur.interior_capture_filename).u8string());
                }
                if (!transitionFinalCaptureSaved && elapsed >= 9.9) {
                    transitionFinalCaptureSaved = wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(), (output / "transition-5-final-idle.png").u8string());
                }
                cur.frame_count++;
                cur.accumulated_distance_m += actualDistance;
                auto* walkAnim = sceneWalkAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneWalkAnimEntity) : nullptr;
                auto* idleAnim = sceneIdleAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneIdleAnimEntity) : nullptr;
                auto* inactiveAnim = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? idleAnim : walkAnim;

                if (isBlendingPhase) {
                    cur.blend_frame_count++;
                } else {
                    cur.settled_frame_count++;
                    if (inactiveAnim) {
                        if (inactiveAnim->amount != 0.0f) cur.post_blend_inactive_clip_zero = false;
                        if (inactiveAnim->IsPlaying()) cur.inactive_clip_paused = false;
                    }
                }

                cur.active_clip_amount = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? (walkAnim ? walkAnim->amount : 0.0f) : (idleAnim ? idleAnim->amount : 0.0f);
                cur.inactive_clip_amount = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? (idleAnim ? idleAnim->amount : 0.0f) : (walkAnim ? walkAnim->amount : 0.0f);
                cur.inactive_clip_amount_zero = cur.post_blend_inactive_clip_zero;

                if (cur.frame_count == 1) {
                    cur.start_active_clip_amount = cur.active_clip_amount;
                    cur.start_inactive_clip_amount = cur.inactive_clip_amount;
                    cur.start_idle_weight = currentBlendWeights.idleWeight;
                    cur.start_walk_weight = currentBlendWeights.walkWeight;
                    cur.start_animation_timer_s = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? (walkAnim ? walkAnim->timer : 0.0f) : (idleAnim ? idleAnim->timer : 0.0f);
                }
                cur.end_idle_weight = currentBlendWeights.idleWeight;
                cur.end_walk_weight = currentBlendWeights.walkWeight;
                cur.end_animation_timer_s = (activeTransitionPhaseIndex == 1 || activeTransitionPhaseIndex == 3) ? (walkAnim ? walkAnim->timer : 0.0f) : (idleAnim ? idleAnim->timer : 0.0f);

                float frameMaxTransDelta = 0.0f;
                float frameMaxRotDelta = 0.0f;
                bool frameDeltasFinite = true;

                if (creatureLoaded) {
                    for (size_t ai = 0; ai < heroAnimationCount; ++ai) {
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

                            auto prevIt = previousFrameBonePoses.find(key);
                            if (prevIt != previousFrameBonePoses.end()) {
                                if (channel.path == wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION) {
                                    const float dx = pose.x - prevIt->second.x;
                                    const float dy = pose.y - prevIt->second.y;
                                    const float dz = pose.z - prevIt->second.z;
                                    const float dTrans = std::sqrt(dx*dx + dy*dy + dz*dz);
                                    if (std::isfinite(dTrans)) {
                                        frameMaxTransDelta = std::max(frameMaxTransDelta, dTrans);
                                    } else {
                                        frameDeltasFinite = false;
                                    }
                                } else if (channel.path == wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION) {
                                    const auto& prev = prevIt->second;
                                    const double dot = std::abs(double(pose.x)*prev.x + double(pose.y)*prev.y + double(pose.z)*prev.z + double(pose.w)*prev.w);
                                    const double normNow = double(pose.x)*pose.x + double(pose.y)*pose.y + double(pose.z)*pose.z + double(pose.w)*pose.w;
                                    const double normPrev = double(prev.x)*prev.x + double(prev.y)*prev.y + double(prev.z)*prev.z + double(prev.w)*prev.w;
                                    const double normProduct = std::sqrt(normNow * normPrev);
                                    const float dRot = normProduct > 1e-12 && std::isfinite(normProduct)
                                        ? static_cast<float>(2.0 * std::acos(std::clamp(dot / normProduct, 0.0, 1.0)))
                                        : std::numeric_limits<float>::quiet_NaN();
                                    if (std::isfinite(dRot)) {
                                        frameMaxRotDelta = std::max(frameMaxRotDelta, dRot);
                                    } else {
                                        frameDeltasFinite = false;
                                    }
                                }
                            }
                            previousFrameBonePoses[key] = pose;
                        }
                    }
                }

                if (isBlendingPhase) {
                    cur.max_frame_translation_delta_m = std::max(cur.max_frame_translation_delta_m, frameMaxTransDelta);
                    cur.max_frame_rotation_delta_rad = std::max(cur.max_frame_rotation_delta_rad, frameMaxRotDelta);
                    if (!frameDeltasFinite) cur.all_blend_deltas_finite = false;

                    if (cur.blend_frame_telemetry.size() < 64) {
                        const auto crossfadeOrder = stalthos::engine::determineEvaluationOrder(scene, sceneWalkAnimEntity, sceneWalkAnimName, sceneIdleAnimEntity, sceneIdleAnimName);
                        stalthos::engine::BlendFrameTelemetrySample sample;
                        sample.frame_index = cur.frame_count;
                        sample.elapsed_s = elapsed;
                        sample.logical_idle_weight = currentBlendWeights.idleWeight;
                        sample.logical_walk_weight = currentBlendWeights.walkWeight;
                        sample.applied_first_amount = crossfadeOrder.firstAnim ? crossfadeOrder.firstAnim->amount : 0.0f;
                        sample.applied_second_amount = crossfadeOrder.secondAnim ? crossfadeOrder.secondAnim->amount : 0.0f;
                        sample.first_evaluated_clip = crossfadeOrder.firstName;
                        sample.second_evaluated_clip = crossfadeOrder.secondName;
                        sample.max_bone_translation_delta_m = frameMaxTransDelta;
                        sample.max_bone_rotation_delta_rad = frameMaxRotDelta;
                        sample.deltas_finite = frameDeltasFinite;
                        cur.blend_frame_telemetry.push_back(sample);
                    }
                }
            }
            if(creatureLoaded) for(size_t ai=0;ai<heroAnimationCount;++ai) { auto& animation=scene.animations[ai]; minimumAnimationTimer=std::min(minimumAnimationTimer,animation.timer); maximumAnimationTimer=std::max(maximumAnimationTimer,animation.timer); if(animation.IsPlaying()) ++animationPlayingFrames; for(const auto& channel:animation.channels) { const auto* bone=scene.transforms.GetComponent(channel.target); if(!bone) continue; uint8_t path=static_cast<uint8_t>(channel.path); XMFLOAT4 pose; if(channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::ROTATION) pose=bone->rotation_local; else if(channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION) pose={bone->translation_local.x,bone->translation_local.y,bone->translation_local.z,0}; else if(channel.path==wi::scene::AnimationComponent::AnimationChannel::Path::SCALE) pose={bone->scale_local.x,bone->scale_local.y,bone->scale_local.z,0}; else continue; auto key=std::make_pair(channel.target,path); auto it=channelPoseBaseline.find(key); if(it==channelPoseBaseline.end()) channelPoseBaseline.emplace(key,pose); else localBoneDelta=std::max({localBoneDelta,std::abs(pose.x-it->second.x),std::abs(pose.y-it->second.y),std::abs(pose.z-it->second.z),std::abs(pose.w-it->second.w)}); } }
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
            if(connectedWorld){exposureResponse=stalthos::environment::applyExposure(*needs,localWeather,timeStep.physicalSeconds,heroAlive);const bool cue=exposureResponse.severity>.01;const bool inLee=localWeather.shelter>=.35;if(inLee!=previousExposureLee){exposureCue.SetText(inLee?"Rock lee - exposure eased; still tiring":"Weather exposure - tiring faster; seek a rock lee");previousExposureLee=inLee;}if(cue==exposureCue.IsHidden())exposureCue.SetHidden(!cue);if(elapsed>=nextExposureSample){exposureSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"body_x",x},{"body_z",z},{"shelter",localWeather.shelter},{"comfort",localWeather.comfort},{"wetness",localWeather.wetness},{"severity",exposureResponse.severity},{"fatigue_added",exposureResponse.fatigueAdded},{"fatigue",needs->fatigue()},{"dt",timeStep.physicalSeconds},{"direct",directInput},{"speed",speed},{"cue_visible",!exposureCue.IsHidden()},{"lee_cue",inLee}});nextExposureSample=elapsed+.1;}}
            if(heroAutonomyEnabled && elapsed>=nextRestSample){restSamples.push_back({{"elapsed",elapsed},{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"body_x",x},{"body_z",z},{"fatigue",needs->fatigue()},{"resting",resting},{"direct",directInput},{"cue_visible",!restCue.IsHidden()},{"speed",speed},{"camera_target_x",cameraOut.cameraState.target.x},{"camera_target_z",cameraOut.cameraState.target.z}});nextRestSample=elapsed+.1;}

            if(connectedWorld && elapsed>=nextLifeStageSample){stalthos::Vec3 lo{1e30,1e30,1e30},hi{-1e30,-1e30,-1e30};std::size_t objects=0;for(std::size_t i=0;i<scene.objects.GetCount();++i){auto entity=scene.objects.GetEntity(i);if(entity!=heroMotionRoot && !scene.Entity_IsDescendant(entity,heroMotionRoot))continue;const auto low=scene.aabb_objects[i].getMin(),high=scene.aabb_objects[i].getMax();lo={std::min(lo.x,double(low.x)),std::min(lo.y,double(low.y)),std::min(lo.z,double(low.z))};hi={std::max(hi.x,double(high.x)),std::max(hi.y,double(high.y)),std::max(hi.z,double(high.z))};++objects;}const auto scale=scene.transforms.GetComponent(heroMotionRoot)->scale_local;lifeStageSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"age",lifecycle->calendarAge()},{"stage",stalthos::domain::lifeStageName(lifecycle->stage())},{"bulk",heroStageBulk},{"scale_x",scale.x},{"scale_y",scale.y},{"scale_z",scale.z},{"bounds_x",hi.x-lo.x},{"bounds_y",hi.y-lo.y},{"bounds_z",hi.z-lo.z},{"body_objects",objects},{"body_x",x},{"body_z",z},{"fatigue",needs->fatigue()},{"thirst",needs->thirst()},{"resting",resting},{"direct",directInput},{"speed",speed}});nextLifeStageSample=elapsed+.1;}
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
            eligibleWaterResource=100;
            if(connectedWorld && getResourceRemaining(209)>0 && exteriorWater.eligible({bodyPos.x,bodyPos.y,bodyPos.z},Ground(float(exteriorWater.bankX),float(exteriorWater.bankZ)),exteriorWaterHeight,rootUsesRendered)){eligibilityWater=heroAlive;eligibleWaterResource=209;}
            if (eligibilityWater) ++waterEligibleFrames;

            // Shrub: hero is near one of the first 8 shrub placements
            eligibilityShrub = false;
            int nearbyShrubIndex = -1;
            for (size_t i = 0; !birdLife && !plant.empty() && importedInstances > 0 && i < cachedShrubs.size(); i++) {
                if (heroAlive && (bodyInLandscape || (connectedWorld && rootUsesRendered)) && (!autonomyAction.contact || !autonomyBrowsing || autonomyResource==200+i) && std::abs(cachedShrubs[i].y - bodyPos.y) <= 1.5 && distanceToPlant(cachedShrubs[i], bodyPos.x, bodyPos.z) < 4.0) { // 2m horizontal reach and bounded vertical reach
                    eligibilityShrub = true;
                    nearbyShrubIndex = static_cast<int>(i);
                    break;
                }
            }
            const bool microEligible=birdLife && microforage.eligible({bodyPos.x,bodyPos.y,bodyPos.z},Ground(float(microforage.x),float(microforage.z)),rootUsesRendered && latestSupportMode=="terrain_supported",heroAlive);
            if(microEligible && getResourceRemaining(microforage.resource)>0){eligibilityShrub=true;nearbyShrubIndex=-2;}
            if (eligibilityShrub) ++shrubEligibleFrames;

            bool tryInteract = populatedResourceProbe && heroAlive && eligibilityWater && appliedDrinkContacts==0 && elapsed>=.5;

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

            const bool autoInteract=heroAutonomyEnabled && autonomyAction.contact && !directInput && (autonomyBrowsing?eligibilityShrub:eligibilityWater);
            bool interactDown = down('E') || tryInteract || autoInteract;
            const unsigned interactionOwner=down('E')?1:autoInteract?2:tryInteract?3:0;
            if (heroAlive && interactDown) {
                // Ensure idempotence: holding E produces one session. Re-pressing increments session.
                if (!previousInteractDown || interactionOwner!=previousInteractionOwner) {
                    if (nextInteractionSession == 0) throw std::runtime_error("interaction session ID overflow");
                    physicalInteractionSession = nextInteractionSession++;
                    interactions.beginSession(physicalInteractionSession);
                }

                if (eligibilityWater && (!autoInteract || (!autonomyBrowsing && autonomyResource==eligibleWaterResource))) {
                    double drinkAmount = 0.5;
                    double available = getResourceRemaining(eligibleWaterResource);
                    double actualDrink = std::min(drinkAmount, available);
                    if (actualDrink > 0) {
                        try {
                            if (consumeResource(physicalInteractionSession, eligibleWaterResource, actualDrink)) {
                                for (const auto& res : resourceSnapshot()) cachedResourceDtos[res.id] = res;
                                double dummyResource = available;
                                if (interactions.applyContact(physicalInteractionSession, eligibleWaterResource, eligibilityWater, actualDrink, dummyResource)) {
                                    const double beforeThirst=needs->thirst();needs->drink(actualDrink);
                                    if(autoInteract) {
                                        autonomyContacts.push_back({{"elapsed",elapsed},{"body_x",bodyPos.x},{"body_z",bodyPos.z},{"session",physicalInteractionSession},{"resource",eligibleWaterResource},{"amount",actualDrink},{"thirst_before",beforeThirst},{"thirst_after",needs->thirst()},{"eligible",eligibilityWater}});
                                        heroAutonomy.completedContact(heroIntent);
                                    }
                                    ++appliedDrinkContacts; waterContactApplied = true;
                                }
                            }
                        } catch (const std::exception&) {
                            ++resourceLedgerCapExhaustions;
                        }
                    }
                }

                if (eligibilityShrub && (nearbyShrubIndex>=0 || nearbyShrubIndex==-2) && (!autoInteract || (autonomyBrowsing && autonomyResource==(nearbyShrubIndex==-2?microforage.resource:200+uint64_t(nearbyShrubIndex))))) {
                    double feedAmount = 0.5;
                    uint64_t shrubId = nearbyShrubIndex==-2?microforage.resource:200+nearbyShrubIndex;
                    double available = getResourceRemaining(shrubId);
                    double actualFeed = std::min(feedAmount, available);
                    if (actualFeed > 0) {
                        try {
                            if (consumeResource(physicalInteractionSession, shrubId, actualFeed)) {
                                for (const auto& res : resourceSnapshot()) cachedResourceDtos[res.id] = res;
                                if (nearbyShrubIndex >= 0 && nearbyShrubIndex < static_cast<int>(shrubVisualInstances.size())) {
                                    applyShrubVisual(shrubVisualInstances[nearbyShrubIndex], cachedResourceDtos[shrubId]);
                                }
                                double dummyResource = available;
                                if (interactions.applyContact(physicalInteractionSession, shrubId, eligibilityShrub, actualFeed, dummyResource)) {
                                    const double beforeHunger=needs->hunger();needs->feed(actualFeed);
                                    if(autoInteract){autonomyContacts.push_back({{"elapsed",elapsed},{"body_x",bodyPos.x},{"body_z",bodyPos.z},{"session",physicalInteractionSession},{"resource",shrubId},{"amount",actualFeed},{"hunger_before",beforeHunger},{"hunger_after",needs->hunger()},{"eligible",eligibilityShrub},{"foliage_fraction",nearbyShrubIndex==-2?microforage.fraction(getResourceRemaining(microforage.resource)):shrubVisualInstances[nearbyShrubIndex].appliedFraction}});heroAutonomy.completedContact(heroIntent);}
                                    ++appliedBrowseContacts; shrubContactApplied = true;
                                }
                            }
                        } catch (const std::exception&) {
                            ++resourceLedgerCapExhaustions;
                        }
                    }
                }
            }
            if(connectedWorld && elapsed>=nextExteriorWaterSample){exteriorWaterSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"body_x",x},{"body_z",z},{"thirst",needs->thirst()},{"body_awareness",heroIntent.perception.attention().has_value()},{"selected_resource",autonomyResource},{"remaining",getResourceRemaining(209)},{"hero_drinks",appliedDrinkContacts},{"eligible",eligibilityWater},{"direct",directInput},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z}});nextExteriorWaterSample=elapsed+.1;}
            if(exteriorBrowseEnabled && elapsed>=nextExteriorBrowseSample){exteriorBrowseSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"body_x",x},{"body_z",z},{"hunger",needs->hunger()},{"body_food_awareness",heroIntent.perception.attention().has_value()},{"selected_resource",autonomyResource},{"remaining",getResourceRemaining(208)},{"foliage_fraction",shrubVisualInstances.at(8).appliedFraction},{"hero_contacts",appliedBrowseContacts},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z}});nextExteriorBrowseSample=elapsed+.1;}
            if(birdLife){const double fraction=microforage.fraction(getResourceRemaining(microforage.resource));for(std::size_t i=0;i<microforageObjects.size();++i)if(auto* o=scene.objects.GetComponent(microforageObjects[i])){const bool visible=double(i)/microforageObjects.size()<fraction;o->SetRenderable(visible);o->SetCastShadow(visible);}
                if(clock.physicalSeconds()>=nextBirdSample && birdSamples.size()<4096){birdSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"species",stalthos::heroSpeciesNames[unsigned(heroSpecies)]},{"needs_species",unsigned(needs->species())},{"lifecycle_species",unsigned(lifecycle->species())},{"hero",heroId},{"session",physicalInteractionSession},{"age",lifecycle->calendarAge()},{"stage",unsigned(lifecycle->stage())},{"body_x",x},{"body_y",y},{"body_z",z},{"speed",speed},{"distance",birdDistance},{"leg_angle",scene.transforms.GetComponent(birdPresentation.legs[0])->rotation_local.x},{"hunger",needs->hunger()},{"thirst",needs->thirst()},{"fatigue",needs->fatigue()},{"resting",resting},{"direct",directInput},{"auto_forward",autonomyAction.forward},{"auto_contact",autonomyAction.contact},{"attention",heroIntent.perception.attention().has_value()},{"resource",autonomyResource},{"remaining",getResourceRemaining(microforage.resource)},{"fraction",fraction},{"eligible",microEligible},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z},{"npc_count",faunaPopulation->actors().size()},{"npc_sessions",faunaPopulation->sessions().size()},{"body_supported",latestSupportMode=="terrain_supported"}});nextBirdSample=clock.physicalSeconds()+.1;}
            }
            previousInteractDown = interactDown;previousInteractionOwner=interactionOwner;
            if(explorationEnabled && elapsed>=nextExplorationSample){explorationSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"body_x",x},{"body_z",z},{"yaw",yaw},{"phase",unsigned(heroExploration.phase())},{"target_x",heroExploration.target().x},{"target_z",heroExploration.target().z},{"direct",directInput},{"speed",speed},{"thirst",needs->thirst()},{"resting",resting},{"destination",destinationActive},{"goal_contextual",heroIntent.goal()?heroIntent.goal()->contextual:false},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z},{"cue_visible",!explorationCue.IsHidden()},{"need",unsigned(needs->suggestActivity())}});nextExplorationSample=elapsed+.1;}
            if(connectedWorld && elapsed>=nextCarnivoreSample && carnivoreSamples.size()<2400){const auto b=scene.transforms.GetComponent(heroMotionRoot)->GetPosition();json actors=json::array();for(const auto&a:faunaPopulation->actors())if(a.species==stalthos::HeroSpecies::Longstride)actors.push_back({{"id",a.id},{"age",a.calendarAge},{"alive",a.alive},{"x",a.position.x},{"y",a.position.y},{"z",a.position.z}});carnivoreSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"body_x",b.x},{"body_y",b.y},{"body_z",b.z},{"actor",carnivoreAwareness.actor()},{"distance",carnivoreAwareness.distance()},{"cue_visible",!carnivoreCue.IsHidden()},{"recall",viewController.dangerRecallActive()},{"recall_requests",carnivoreRecallRequests},{"detached",viewController.getState().mode==stalthos::domain::CameraMode::LocallyDetached},{"camera_x",cameraOut.cameraState.target.x},{"camera_z",cameraOut.cameraState.target.z},{"direct",directInput},{"thirst",needs->thirst()},{"hunger",needs->hunger()},{"actors",actors}});nextCarnivoreSample=elapsed+.1;}
            if(reedProbe)for(std::size_t i=0;i<(reedProbe==5?5:4);++i)if(!reedCaptured[i] && clock.physicalSeconds()>=(i==0?2:i==1?5:i==2?(reedProbe==5?25:9):i==3?(reedProbe==5?39:16):51)){const auto name="reed-"+std::to_string(i)+".png";reedCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/name).u8string());reedCaptures.push_back({{"filename",name},{"saved",reedCaptured[i]}});}
            if(protectionProbe)for(std::size_t i=0;i<4;++i)if(!defenseCaptured[i] && clock.physicalSeconds()>=(i==0?.5:i==1?2:i==2?5:10)){const auto name="protection-"+std::to_string(i)+".png";defenseCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/name).u8string());defenseCaptures.push_back({{"filename",name},{"saved",defenseCaptured[i]}});}
            if(birdProbe)for(std::size_t i=0;i<4;++i)if(!birdCaptured[i] && clock.physicalSeconds()>=(i==0?.5:i==1?5:i==2?12:20)){const auto name="bird-"+std::to_string(i)+".png";birdCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/name).u8string());birdCaptures.push_back({{"filename",name},{"saved",birdCaptured[i]}});}
            if(carnivoreProbe)for(size_t i=0;i<3;++i)if(!carnivoreCaptured[i] && clock.physicalSeconds()>=(i==0?.25:i==1?3:22)){const auto name="carnivore-"+std::to_string(i)+".png";carnivoreCaptured[i]=wi::helper::saveTextureToFile(wi::graphics::GetDevice()->GetBackBuffer(&app.swapChain),(output/name).u8string());carnivoreCaptures.push_back({{"filename",name},{"saved",carnivoreCaptured[i]}});}
            if(connectedWorld && elapsed>=nextLifeSampleAt){json actors=json::array();for(const auto& a:faunaPopulation->actors())actors.push_back({{"id",a.id},{"x",a.position.x},{"z",a.position.z},{"distance",a.distanceTravelledMeters}});nextLifeSamples.push_back({{"physical_seconds",clock.physicalSeconds()},{"calendar_seconds",clock.calendarSeconds()},{"hero_id",heroId},{"alive",heroAlive},{"age",lifecycle->calendarAge()},{"body_x",x},{"body_z",z},{"speed",speed},{"direct",directInput},{"thirst",needs->thirst()},{"session",physicalInteractionSession},{"old_death_known",priorDeadHero && world.heroDeathKnown(priorDeadHero)},{"actors",actors},{"ledger",ledgerJson(ledgerSnapshot())}});nextLifeSampleAt=elapsed+.25;}

            if(autonomyProbe || foragingProbe) for(std::size_t phase=0;phase<3;++phase) if(!autonomyCaptured[phase] && (phase==0?elapsed>=1:phase==1?elapsed>=5:!autonomyContacts.empty())) {
                const auto filename="hero-autonomy-"+std::to_string(phase)+".png";autonomyCaptured[phase]=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/filename).u8string());autonomyCaptures.push_back({{"filename",filename},{"saved",autonomyCaptured[phase]},{"body_x",x},{"body_z",z}});
            }
            if(!earlyScreenshot && elapsed>=std::min(1.,duration*.25)) earlyScreenshot=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/"motion-early.png").u8string());
            if(!lateScreenshot && elapsed>=std::min(1.35,duration*.75)) lateScreenshot=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/"motion-late.png").u8string());
            if(!screenshot && elapsed>=std::min(5.,duration*.5)) screenshot=wi::helper::saveTextureToFile(renderPath.GetRenderResult3D(),(output/"draft-host.png").u8string());
        }
        if (!saveState.empty()) {
            stalthos::persistence::HostPose pose{{x,Ground(x,z),z},yaw,static_cast<std::uint8_t>(cameraMode)};
            auto data=stalthos::persistence::captureSave(clock, world, pose);
            if(data.hero) data.heroLifecycle=stalthos::persistence::captureHeroLifecycle(*lifecycle,data.hero->id);
            if(data.hero) data.heroNeeds=stalthos::persistence::captureHeroNeeds(*needs,data.hero->id);
            data.resourceLedger = ledgerSnapshot();
            if(faunaPopulation) {
                data.faunaPopulation=faunaPopulation->snapshot();
                data.faunaResourceSessions=faunaPopulation->sessions();
            } else if(loadedSaveData) {
                data.faunaPopulation=loadedSaveData->faunaPopulation;
                data.faunaResourceSessions=loadedSaveData->faunaResourceSessions;
            }
            const auto& cameraState = viewController.getState();
            data.cameraState = stalthos::persistence::CameraStateSnapshot{
                static_cast<std::uint8_t>(cameraState.mode), static_cast<std::uint8_t>(cameraState.focus),
                cameraState.detachedAnchor, cameraState.orbit.azimuth, cameraState.orbit.elevation,
                cameraState.orbit.distance};
            if(faunaPopulation) {
                const auto previous=loadedHostSave?loadedHostSave->generation:0;
                if(previous==std::numeric_limits<uint64_t>::max()) throw std::runtime_error("native save generation exhausted");
                stalthos::persistence::host_save::Candidate candidate{data,faunaPopulation->continuation(),hostConfiguration,previous+1};
                stalthos::persistence::host_save::write(saveState,candidate,faunaPopulation->registry(),hostConfiguration);
            } else {
                if(loadedHostSave && loadedHostSave->regional) throw std::runtime_error("refusing regional save without registry");
                stalthos::persistence::writeSave(saveState,data);
            }
            if(successorId) {
                const auto saved=stalthos::persistence::host_save::read(saveState,hostConfiguration).host;
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
            last.signed_displacement_m = (last.phase_index == 1) ? last.displacement_m : (last.phase_index == 3) ? -last.displacement_m : 0.0;
            last.end_animation_phase_s = creaturePhase;
            auto* idleAnim = sceneIdleAnimEntity != wi::ecs::INVALID_ENTITY ? scene.animations.GetComponent(sceneIdleAnimEntity) : nullptr;
            last.end_animation_timer_s = idleAnim ? idleAnim->timer : 0.0f;
            last.bone_pose_changed = (last.local_bone_delta > 1e-5f);
        }
        if(connectedWorld)WriteJson(output/"carnivore-awareness.json",{{"probe_mode",carnivoreProbe},{"synthetic_body_startup",carnivoreProbe!=0},{"synthetic_reverse_input",carnivoreProbe==1 || carnivoreProbe==2},{"synthetic_camera_cancel",carnivoreProbe==2},{"watched_actor",carnivoreWatched},{"notice_m",stalthos::fauna::CarnivoreAwareness::noticeMeters},{"release_m",stalthos::fauna::CarnivoreAwareness::releaseMeters},{"samples",carnivoreSamples},{"captures",carnivoreCaptures},{"scope","actual physical adult Longstride proximity only; no attack intent, damage or hunting AI; no camera sensing"}});
        if(connectedWorld) WriteJson(output/"wildlife-awareness.json",{{"probe_mode",wildlifeProbe},{"synthetic_hero_startup_and_input",wildlifeProbe!=0},{"watched_actor",wildlifeWatched},{"notice_m",wildlifeTuning.noticeMeters},{"comfort_m",wildlifeTuning.comfortMeters},{"avoid_speed_m_s",wildlifeTuning.avoidanceSpeedMetersPerSecond},{"samples",wildlifeSamples},{"captures",wildlifeCaptures},{"source","actual hero motion root only; camera never context"},{"mid_edge_policy","finish existing graph hop; ordinary behavior resumes on next arrival"}});
        if(heroAutonomyEnabled) WriteJson(output/"hero-rest.json",{{"synthetic_fatigue",restProbe!=0},{"probe_mode",restProbe},{"direct_override_interval",restProbe==2},{"samples",restSamples},{"captures",restCaptures},{"cue_changes",restCueChanges},{"exit_fatigue",stalthos::domain::HeroRestAutonomy::recoveredFatigue},{"recovery_driver","existing NeedsCore physicalSeconds only"},{"scope","idle rest activity; no sleeping pose or anatomical claim"}});
        if(heroAutonomyEnabled) WriteJson(output/"hero-autonomy.json",{{"enabled",true},{"probe_mode",autonomyProbe},{"foraging_probe",foragingProbe},{"synthetic_need",autonomyProbe!=0 || foragingProbe!=0},{"synthetic_direct_interval",autonomyProbe==2 || foragingProbe==2},{"synthetic_camera_initialization",autonomyProbe==3 || foragingProbe==3},{"samples",autonomySamples},{"contacts",autonomyContacts},{"captures",autonomyCaptures},{"ledger_authority","existing native regional registry"},{"scope","ordinary local drink/browse; no pathfinding or anatomical reach acceptance"}});
        if(rustleReady){rustleMaxCursor=std::max(rustleMaxCursor,wi::audio::GetTotalSamplesPlayed(&rustleInstance));wi::audio::Pause(&rustleInstance);}const bool nativeAudioCaptured=audioProbe?audioCapture.finish(output/"native-output.wav"):false;
        if(connectedWorld)WriteJson(output/"body-audio.json",{{"ready",rustleReady},{"probe_mode",audioProbe},{"synthetic_body_or_camera_input",audioProbe!=0},{"events",rustleEvents},{"requests_total",rustleRequests},{"events_dropped",rustleEventsDropped},{"event_capacity",256},{"max_voice_samples_played",rustleMaxCursor},{"capture_state",audioCapture.state},{"capture_hresult",int64_t(audioCapture.result)},{"process_capture_activation_hresult",int64_t(audioCapture.processActivationResult)},{"output_endpoint",audioCapture.endpointId},{"native_output_captured",nativeAudioCaptured},{"global_audio_settings_modified",false},{"scope","generated mono ground rustle, bounded actual supported living body motion, simple camera-distance gain only; no anatomical foot contact, directional sound or acoustic acceptance"}});
        if(connectedWorld)WriteJson(output/"sapling-disturbance.json",{{"probe_mode",saplingProbe},{"tree_id",sapling.tree},{"region",sapling.region},{"available",saplingBase.has_value()},{"synthetic_start_input_noon",saplingProbe!=0},{"events",saplingEvents},{"samples",saplingSamples},{"captures",saplingCaptures},{"scope","one existing sapling, authoritative StandingDamaged exception; per-object tint/root lean only; no shared geometry/material changes, anatomy/destruction or connected save claim"}});
        if(connectedWorld)WriteJson(output/"hero-exploration.json",{{"enabled",explorationEnabled},{"probe_mode",explorationProbe},{"synthetic_start_input_need_destination_noon",explorationProbe!=0},{"samples",explorationSamples},{"events",explorationEvents},{"captures",explorationCaptures},{"scope","bounded supported dry local steps and cue-only pauses, no head animation, pathfinding, remote knowledge or connected saves"}});
        if(connectedWorld)WriteJson(output/"death-next-life.json",{{"synthetic_age_thirst_input_noon",deathNextLifeProbe},{"events",nextLifeEvents},{"samples",nextLifeSamples},{"captures",nextLifeCaptures},{"scope","explicit new individual in retained running world; reused presentation at existing supported position, no corpse, kinship or connected save support"}});
        if(reedLife)WriteJson(output/"reedcrest.json",{{"species","Reedcrest"},{"probe_mode",reedProbe},{"synthetic_body_route_needs_input_noon",reedProbe!=0},{"fixture_peer",reedFixturePeer},{"actor_identities_species_positions_preserved",true},{"peer",peerCompanion.peer()},{"samples",reedSamples},{"captures",reedCaptures},{"scope","one locally learned session reciprocal peer; actual NPC supported graph motion and hero contextual following; no kindergarten origin, broader cohort, teleport, connected persistence or NPC species art claim"}});
        if(brackenLife)WriteJson(output/"brackenback.json",{{"species","Brackenback"},{"probe_mode",protectionProbe},{"synthetic_adult_body_and_existing_actor_age_condition",protectionProbe!=0},{"synthetic_existing_actor_edge_position",protectionProbe!=0},{"existing_actor_positions_preserved_at_fixture",protectionProbe==0},{"npc_defense_integrated",false},{"distress_model","actual critical hunger/thirst >=.85, visible local condition cue"},{"threat_model","actual nearby live adult Longstride body; no attack intent inferred"},{"events",defenseEvents},{"samples",defenseSamples},{"captures",defenseCaptures},{"scope","third procedural playable hero; bounded adult interpose/brace through existing contextual owner; no attack, adoption, panic AI or NPC defensive movement"}});
        if(birdLife)WriteJson(output/"thistletail.json",{{"species","Thistletail"},{"hero_presentation","replaceable procedural feather-layered biped; imported Smalltrotter asset only NPCs"},{"probe_mode",birdProbe},{"synthetic_start_hunger_input_noon_death",birdProbe!=0},{"food_resource",microforage.resource},{"initial_food",microforage.quantity},{"remaining_food",getResourceRemaining(microforage.resource)},{"samples",birdSamples},{"contacts",autonomyContacts},{"captures",birdCaptures},{"scope","connected bird life and finite litter microforage; no individual insects, authored plumage animation, bird parenting or connected save"}});
        if(connectedWorld)WriteJson(output/"nesting.json",{{"synthetic_adult_start_input_noon",nestProbe},{"session_only",true},{"clutch_limit",1},{"eggs",nesting.clutch()?(nesting.offspring()?2:3):0},{"closed_shells",nesting.offspring()?1:0},{"incubation_calendar_seconds",stalthos::domain::Nesting::incubationSeconds},{"hatching_implemented",true},{"offspring_admitted",nesting.offspring()?1:0},{"hatch_probe",hatchProbe},{"birth_calendar",bornCalendar},{"child_session",childSession},{"hatch_events",hatchEvents},{"events",nestEvents},{"samples",nestSamples},{"captures",nestCaptures},{"scope","Bounded one independent hatchling from live clutch; authoritative local path/identity/session. Two eggs remain unmodelled. No mating, clutch mortality or connected save."}});
        if(connectedWorld)WriteJson(output/"life-stage.json",{{"synthetic_start_age_fatigue_input_and_noon",lifeStageProbe},{"initial_age",initialHeroLifecycleAge},{"ordinary_start_or_saved_age_modified",false},{"clock_scale_modified",false},{"milestones",lifeStageMilestones},{"samples",lifeStageSamples},{"captures",lifeStageCaptures},{"scope","actual calendar Lifecycle; proposed horizontal bulk only, existing vertical datum and locomotion pace preserved; no anatomical stage or full growth acceptance"}});
        if(connectedWorld)WriteJson(output/"exterior-shore.json",{{"probe_mode",shoreProbe},{"authored_cove_preview",true},{"synthetic_local_start_input_noon",shoreProbe!=0},{"synthetic_forward_input",shoreProbe==1 || shoreProbe==2},{"sea_level",exteriorShoreRoute.shore().layout().seaLevel},{"preserved_meadow_through_x",exteriorShoreRoute.shore().layout().startX},{"profile_end_x",exteriorShoreRoute.shore().layout().endX},{"rock_instances",exteriorShoreRocks},{"static_water_sheet_bounds",{200,-400,800,448}},{"trajectory",shoreTrajectory},{"captures",shoreCaptures},{"scope","bounded authored coastal cove; no global archipelago shoreline/hydrology/swimming or full watershed-to-coast animal route claim"}});
        if(connectedWorld)WriteJson(output/"npc-peer-association.json",{{"events",npcPeerEvents},{"probe_mode",npcPeerProbe},{"synthetic_actor_body_need_noon_fixture",npcPeerProbe!=0},{"samples",npcPeerSamples},{"captures",npcPeerCaptures},{"scope","one session-only retained conspecific pair; graph approach/wait and needs priority; no kinship or connected save"}});
        if(connectedWorld)WriteJson(output/"peer-companionship.json",{{"probe_mode",peerProbe},{"synthetic_start_satisfied_npc_input_and_noon",peerProbe!=0},{"peer",peerCompanion.peer()},{"events",peerEvents},{"samples",peerSamples},{"captures",peerCaptures},{"scope","one existing conspecific peer, session-only relationship and graph approach/wait; no kinship or connected save support"}});
        if(connectedWorld)WriteJson(output/"day-night.json",{{"probe_mode",dayNightProbe},{"synthetic_renderer_phase",nestProbe || shoreProbe!=0 || npcPeerProbe!=0 || dayNightProbe!=0 || woodlandProbe || peerProbe || lifeStageProbe || deathNextLifeProbe || explorationProbe || saplingProbe},{"woodland_entry_probe",woodlandProbe},{"renderer_modifies_clock_or_needs",false},{"synthetic_initial_fatigue_and_body_input",dayNightProbe==2},{"samples",dayNightSamples},{"captures",dayNightCaptures},{"scope","proposed calendar lighting plus readable night fill, not astronomy or elapsed24h proof"}});
        if(connectedWorld)WriteJson(output/"exterior-water.json",{{"resource",209},{"quantity",exteriorWater.quantity},{"water_height",exteriorWaterHeight},{"bank_x",exteriorWater.bankX},{"bank_z",exteriorWater.bankZ},{"probe_mode",exteriorWaterProbe},{"synthetic_initial_thirst_and_pose",exteriorWaterProbe!=0},{"samples",exteriorWaterSamples},{"npc_contacts",exteriorWaterNpcContacts},{"hero_contacts",autonomyContacts},{"captures",exteriorWaterCaptures},{"scope","static rock-lined seep, dry supported bank; no fluid solver or connected saves"}});
        if(exteriorBrowseEnabled)WriteJson(output/"exterior-browse.json",{{"resource",208},{"initial_quantity",exteriorBrowse.quantity},{"patch_x",exteriorBrowse.x},{"patch_z",exteriorBrowse.z},{"probe_mode",exteriorBrowseProbe},{"synthetic_initial_hunger_and_pose",exteriorBrowseProbe!=0},{"samples",exteriorBrowseSamples},{"npc_contacts",exteriorBrowseNpcContacts},{"hero_contacts",autonomyContacts},{"captures",exteriorBrowseCaptures},{"scope","one existing-asset shared finite exterior food patch; no connected saves or exterior hydrology"}});
        if(connectedWorld) WriteJson(output/"hero-destination.json",{{"probe_mode",destinationProbe},{"synthetic_native_mouse_event",destinationProbe!=0},{"physical_human_mouse_tested",false},{"events",destinationEvents},{"samples",destinationSamples},{"captures",destinationCaptures},{"scope","nearby12m dry supported straight-line destination; no general pathfinding"}});
        if(connectedWorld) WriteJson(output/"environmental-exposure.json",{{"synthetic_adverse_weather",exposureProbe},{"startup_fixture_only",exposureProbe},{"body_motion","shared forward intent and ordinary integrator"},{"fatigue_ceiling",.65},{"samples",exposureSamples},{"captures",exposureCaptures},{"scope","comfort proxy; coarse rock lee; no thermodynamics or waterproof roof"}});
        if(connectedWorld) WriteJson(output/"weather-response.json",{{"native_rain_emitter_disabled",true},{"visible_rainfall_claimed",false},{"normal_conditions_driver","existing calendar clock"},{"wetness_driver","physical time"},{"diagnostic_synthetic_weather",weatherPreview},{"shelter_model","approximate lee of visible outcrop; wind only, not waterproof roof"},{"samples",weatherResponseSamples},{"captures",weatherResponseCaptures},{"needs_or_lifecycle_coupled",true},{"lifecycle_coupled",false}});
        if(populatedTerrain) {
            auto streaming=populatedTerrain->receipt();
            streaming["diagnostic_traversal"]=populatedTraversal;
            streaming["imported_body_exterior_startup_fixture"]=connectedEmbodimentProbe;
            streaming["proxy_bounds_metric_applicable"]=!creatureLoaded;
            streaming["full_animal_route_crossing_proven"]=false;
            streaming["continuous_forward_intent_proof"]=connectedRouteWalk;
            streaming["connected_route_max_sampled_grade"]=connectedMaxGrade;
            streaming["connected_route_max_vertical_step_m"]=connectedMaxStep;
            streaming["connected_route_body_reached_meadow"]=connectedRouteWalk && x>=connectedLayout.meadowX;
            streaming["connected_preview_save_supported"]=false;
            if(connectedRouteWalk) WriteJson(output/"connected-route-trajectory.json",connectedTrajectory);
            streaming["max_body_x"]=populatedMaxX;
            streaming["max_root_ground_error_m"]=populatedRootError;
            streaming["max_proxy_bottom_center_error_m"]=populatedProxyError;
            streaming["max_rendered_bounds_center_error_m"]=populatedBoundsError;
            streaming["support_samples"]=populatedSupportSamples;
            streaming["rendered_support_fallbacks"]=renderedSupportFallbackCount;
            streaming["capture_diagnostics"]=populatedCaptures;
            streaming["final_body_x"]=x;streaming["final_body_z"]=z;
            streaming["route_returned"]=populatedTraversal && std::abs(x-24)<.01;
            streaming["population_count_retained"]=faunaPopulation->actors().size();
            streaming["population_activation_implemented"]=true;
            streaming["limitations"]="synchronous terrain; body-driven watershed dormancy only; no regional habitat populations or final art/FPS acceptance";
            WriteJson(output/"populated-terrain.json",streaming);
        }
        if(faunaPopulation) {
            auto regions=json::parse(stalthos::engine::regionConditionsJson(regionalConditions));
            regions["calendar_seconds"]=clock.calendarSeconds();
            regions["condition_updates"]=conditionsUpdates;
            regions["remote_population_is_coarse_estimate"]=true;
            regions["terrain_streaming_implemented"]=true;
            regions["detailed_patch_span_m"]=192;
            WriteJson(output/"world-regions.json",regions);
            json sites=json::array();
            for(const auto& site:faunaSites) sites.push_back({{"x",site.position.x},{"y",site.position.y},{"z",site.position.z},
                {"browse",site.browse},{"freshwater",site.freshwater},{"neighbors",site.neighbors}});
            if(!faunaPhysical) endFaunaDormancy(false);
            WriteJson(output/"world-population.json",{{"schema_version",2},{"region",1001},{"world_seed",world.worldSeed()},
                {"actors_initial",faunaInitial},{"actors_final",actorJson(faunaPopulation->actors())},{"habitat_sites",sites},
                {"snapshot_initial",faunaFullInitial},{"snapshot_final",populationJson(faunaPopulation->snapshot())},
                {"activation_driver","body"},{"activation_radius_m",192.0},{"physical_active_initial",faunaPhysicalInitial},
                {"physical_active_final",faunaPhysical},{"activation_transitions",faunaTransitions},
                {"dormant_intervals",faunaDormantIntervals},{"dormant_frames",faunaDormantFrames},
                {"dormant_physical_seconds",faunaDormantPhysicalSeconds},{"dormant_calendar_seconds",faunaDormantCalendarSeconds},
                {"dormant_model","retained route and pose; elapsed age/needs only; no resource contact"},
                {"activity_frames_rest_travel_browse_drink",faunaActivityFrames},{"simulation_frames",faunaSimulationFrames},
                {"pose_matrix_checks",faunaScene.poseChecks()},{"skin_matrix_checks",faunaScene.boneChecks()},
                {"hidden_object_checks",faunaScene.hiddenObjectChecks()},{"visible_object_checks",faunaScene.visibleObjectChecks()},
                {"grounded_actor_updates",faunaScene.groundedUpdates()},{"camera_drives_simulation",false},
                {"registry_owns_population",true},{"registry_owns_shared_ledger",true},{"native_single_region_transaction_supported",true},{"native_multiregion_gameplay_supported",false},
                {"registry_capacity",{{"regions",faunaPopulation->capacity().regions},{"individuals",faunaPopulation->capacity().individuals},{"resources_reserved",faunaPopulation->capacity().resources},{"history_reserved",faunaPopulation->capacity().historyKeys}}},
                {"resource_ledger_initial",faunaLedgerInitial},{"resource_ledger_final",ledgerJson(faunaPopulation->ledger())},
                {"resource_sessions_initial",faunaSessionsInitial},{"resource_sessions_final",faunaPopulation->sessions()},
                {"resource_contact_events",faunaResourceEvents},{"food_consumed",faunaPopulation->consumedFood()},
                {"water_consumed",faunaPopulation->consumedWater()},{"resources_use_shared_persisted_ledger",true},
                {"resource_history_used",resourceHistoryCapacity().used},{"resource_history_limit",resourceHistoryCapacity().limit},
                {"resource_history_available",resourceHistoryCapacity().available},
                {"resource_history_rejections",faunaPopulation->historyRejections()},{"needs_relief_requires_accepted_contact",true},
                {"population_persistence_supported",true},{"population_persisted",!saveState.empty()},
                {"population_restored",faunaPopulationRestored},
                {"meshes_best_effort",true},{"frame_time_measured",false}});
        }
        json transitionPhasesJson = json::array();
        json transitionCapturesJson = json::object();
        for (const auto& p : transitionPhases) {
            json telemetryJson = json::array();
            for (const auto& sample : p.blend_frame_telemetry) {
                telemetryJson.push_back({
                    {"frame_index", sample.frame_index},
                    {"elapsed_s", sample.elapsed_s},
                    {"logical_idle_weight", sample.logical_idle_weight},
                    {"logical_walk_weight", sample.logical_walk_weight},
                    {"applied_first_amount", sample.applied_first_amount},
                    {"applied_second_amount", sample.applied_second_amount},
                    {"first_evaluated_clip", sample.first_evaluated_clip},
                    {"second_evaluated_clip", sample.second_evaluated_clip},
                    {"max_bone_translation_delta_m", sample.max_bone_translation_delta_m},
                    {"max_bone_rotation_delta_rad", sample.max_bone_rotation_delta_rad},
                    {"deltas_finite", sample.deltas_finite}
                });
            }
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
                {"start_active_clip_amount", p.start_active_clip_amount},
                {"start_inactive_clip_amount", p.start_inactive_clip_amount},
                {"start_idle_weight", p.start_idle_weight},
                {"end_idle_weight", p.end_idle_weight},
                {"start_walk_weight", p.start_walk_weight},
                {"end_walk_weight", p.end_walk_weight},
                {"blend_frame_count", p.blend_frame_count},
                {"settled_frame_count", p.settled_frame_count},
                {"start_animation_phase_s", p.start_animation_phase_s},
                {"end_animation_phase_s", p.end_animation_phase_s},
                {"start_animation_timer_s", p.start_animation_timer_s},
                {"end_animation_timer_s", p.end_animation_timer_s},
                {"local_bone_delta", p.local_bone_delta},
                {"bone_pose_changed", p.bone_pose_changed},
                {"inactive_clip_paused", p.inactive_clip_paused},
                {"inactive_clip_amount_zero", p.inactive_clip_amount_zero},
                {"post_blend_inactive_clip_zero", p.post_blend_inactive_clip_zero},
                {"capture_filename", p.capture_filename},
                {"capture_saved", p.capture_saved},
                {"interior_capture_filename", p.interior_capture_filename},
                {"interior_capture_saved", p.interior_capture_saved},
                {"max_frame_translation_delta_m", p.max_frame_translation_delta_m},
                {"max_frame_rotation_delta_rad", p.max_frame_rotation_delta_rad},
                {"all_blend_deltas_finite", p.all_blend_deltas_finite},
                {"blend_frame_telemetry", telemetryJson}
            });
            if (p.capture_saved) {
                transitionCapturesJson[p.phase_name] = p.capture_filename;
            }
            if (p.interior_capture_saved) {
                transitionCapturesJson[p.phase_name + "_interior_blend"] = p.interior_capture_filename;
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
        json woodlandPlacedJson = json::array();
        for (const auto& t : woodlandReport.placedTrees) {
            woodlandPlacedJson.push_back({
                {"id", t.id},
                {"stage", t.stage},
                {"x", t.x},
                {"y", t.y},
                {"z", t.z},
                {"analytical_y", t.analyticalY},
                {"ground_delta_m", t.groundDeltaM},
                {"heading_radians", t.headingRadians},
                {"scale", t.scale},
                {"rendered_ground_sampled", t.renderedGroundSampled}
            });
        }
        json woodlandSkippedJson = json::array();
        for (const auto& s : woodlandReport.skippedTrees) {
            woodlandSkippedJson.push_back({
                {"id", s.id},
                {"stage", s.stage},
                {"x", s.x},
                {"z", s.z},
                {"reason", s.reason}
            });
        }
        json sequentialEvaluationJson = {
            {"evaluated", creatureLoaded && sceneWalkAnimEntity != wi::ecs::INVALID_ENTITY && sceneIdleAnimEntity != wi::ecs::INVALID_ENTITY}
        };
        if (creatureLoaded && sceneWalkAnimEntity != wi::ecs::INVALID_ENTITY && sceneIdleAnimEntity != wi::ecs::INVALID_ENTITY) {
            const auto order = stalthos::engine::determineEvaluationOrder(scene, sceneWalkAnimEntity, sceneWalkAnimName, sceneIdleAnimEntity, sceneIdleAnimName);
            const auto& topology = crossfadeTopology;
            sequentialEvaluationJson["engine_evaluation_order_verified"] = true;
            sequentialEvaluationJson["first_evaluated_clip"] = order.firstName;
            sequentialEvaluationJson["first_evaluated_index"] = order.firstIndex;
            sequentialEvaluationJson["second_evaluated_clip"] = order.secondName;
            sequentialEvaluationJson["second_evaluated_index"] = order.secondIndex;
            sequentialEvaluationJson["full_shared_tracks_verified"] = topology.has_full_shared_tracks;
            sequentialEvaluationJson["shared_channel_count"] = topology.shared_channel_count;
            sequentialEvaluationJson["first_only_channel_count"] = order.firstIsWalk ? topology.clip_a_only_channel_count : topology.clip_b_only_channel_count;
            sequentialEvaluationJson["second_only_channel_count"] = order.firstIsWalk ? topology.clip_b_only_channel_count : topology.clip_a_only_channel_count;
            sequentialEvaluationJson["topology_analysis_count"] = crossfadeTopologyAnalysisCount;
            sequentialEvaluationJson["first_clip_amount_policy"] = topology.has_full_shared_tracks ? "overwrite_1_0_during_blend" : "nominal_weight_fallback";
            sequentialEvaluationJson["second_clip_amount_policy"] = "nominal_weight_during_blend";
            sequentialEvaluationJson["settled_inactive_clip_skipped"] = inactiveClipWritesPrevented;
            sequentialEvaluationJson["paused_walk_clip_forced_evaluation"] = creatureClip.has_value();
        }
        WriteJson(output/"runtime-receipt.json",{{"schema_version",1},{"ok",runCalls>0 && screenshot},{"stage","engine-host feasibility"},
            {"populated_world",populatedWorld},{"populated_resource_probe",populatedResourceProbe},{"autonomous_population_count",faunaPopulation ? faunaPopulation->actors().size() : 0},
            {"engine_revision","df44c3db4c4927492bc9c791eac715d98d7ed091"},{"process_id",GetCurrentProcessId()},
            {"render_size",{app.canvas.GetPhysicalWidth(),app.canvas.GetPhysicalHeight()}},{"run_calls",runCalls},
            {"scenery_objects",sceneryCounts.objectCount},{"scenery_vertices",sceneryCounts.vertexCount},{"scenery_triangles",sceneryCounts.triangleCount},
            {"scenery_logs",sceneryCounts.logCount},{"scenery_pebbles",sceneryCounts.pebbleCount},{"scenery_bank_clumps",sceneryCounts.bankClumpCount},{"scenery_bluff_accents",sceneryCounts.bluffAccentCount},
            {"screenshot_saved",screenshot},{"plant_import_objects",importedObjects},{"user_closed",userClosed},
            {"plant_import_instances",importedInstances},{"landscape_extent_meters",192},
            {"landscape_candidate_placements",landscape.placements().size()},
            {"calendar_seconds",clock.calendarSeconds()},{"domain_world_seed",world.worldSeed()},
            {"physical_seconds",clock.physicalSeconds()},{"loaded_calendar_seconds",loadedCalendar},{"loaded_physical_seconds",loadedPhysical},
            {"native_saved_format",saveState.empty()?"none":(faunaPopulation?"STL9":"STL8")},{"individual_admission_provenance",loadedSaveData && loadedSaveData->faunaPopulation?"validated_save_declared":"generated"},{"resource_admission_provenance",loadedSaveData && loadedSaveData->resourceLedger?"validated_save_declared":"native_defaults"},{"historical_identity_authenticated",false},{"native_save_loaded_generation",loadedHostSave?loadedHostSave->generation:0},{"native_save_saved_generation",!saveState.empty() && faunaPopulation?(loadedHostSave?loadedHostSave->generation:0)+1:0},{"state_loaded",!loadState.empty()},{"state_saved",!saveState.empty()},{"camera_state_loaded",loadedSaveData && loadedSaveData->cameraState.has_value()},{"camera_state_saved",!saveState.empty()},
            {"needs_snapshot_loaded",loadedNeeds.has_value()},{"needs_snapshot_saved",!saveState.empty() && world.snapshot().hero.has_value()},
            {"diagnostic_walk",diagnosticWalk},{"diagnostic_needs",diagnosticNeeds},{"foot_terrain_diagnostic",footDiagnosticJson},{"native_sole_diagnostic",nativeSoleDiagnosticJson},{"diagnostic_transition",diagnosticTransition},{"calendar_scale",calendarScale},
            {"diagnostic_transition_phases", transitionPhasesJson},
            {"transition_captures", transitionCapturesJson},
            {"animation_crossfade_duration_s", kCrossfadeDurationSeconds},
            {"animation_crossfade_blend_frames_total", crossfadeBlendFramesTotal},
            {"both_clips_nonzero_weights_only_during_blend", crossfadeWeightsNonzeroOnlyDuringBlend},
            {"animation_blend_final_idle_weight", stalthos::domain::AnimationBlend::resolve(blendState).idleWeight},
            {"animation_blend_final_walk_weight", stalthos::domain::AnimationBlend::resolve(blendState).walkWeight},
            {"inactive_clip_state_guarded", inactiveClipWritesPrevented},
            {"walk_phase_follows_signed_distance", walkPhaseFollowsSignedDistance},
            {"idle_crossfade_settled", (blendState.targetClip == stalthos::domain::AnimationBlend::ClipKind::Idle && blendState.transitionProgress == 1.0)},
            {"idle_reset_to_neutral_retired", "explicit neutral snap retired in favor of continuous crossfade with sequential evaluation correction"},
            {"clip_selection_policy", "bounded explicit-duration crossfade with sequential evaluation amount correction (first clip overwrites amount 1.0, second clip nominal weight); inactive clip paused and skipped after blend"},
            {"sequential_evaluation_crossfade", sequentialEvaluationJson},
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
            {"connected_embodiment_mode",creatureLoaded?"imported_reviewed_draft":"proxy"},{"imported_wet_material_response_supported",false},{"near_water_creature_startup_fixture",autonomyProbe!=0 && autonomyProbe!=3 && creatureLoaded},{"creature_loaded",creatureLoaded},{"creature_source_front_axis","+X"},{"creature_host_forward_axis","+Z"},{"creature_parent_yaw_offset_radians",creatureLoaded?-pi*.5f:0.f},{"creature_sole_offset_m",creatureLoaded?creatureSoleOffset:0.0},{"creature_tail_ground_clearance_accepted",false},{"creature_path",creature.u8string()},{"creature_mesh_objects",creatureMeshes},{"creature_skin_count",creatureSkins},{"creature_animation_count",creatureAnimations},{"creature_transform_channels",creatureChannels},
            {"locomotion_pace_policy", "proposed one cycle per second, maximum one meter per second; proxy fallback two meters per second"},{"walk_duration_s",creatureClipDuration},{"walk_stride_length_m",creatureStrideLength},{"imported_animation_length_s",engineClipLength},{"actor_root_translation_channels_removed",removedActorRootTranslationChannels},{"actor_root_translation_target_name",actorRootTranslationTargetName},{"actor_root_translation_target_is_skin_bone",false},{"body_signed_distance_last_m",lastSignedBodyDistance},{"body_distance_accumulated_signed_m",creatureDistanceAccumulated},{"animation_phase_expected_s",lastExpectedPhase},{"animation_phase_actual_s",creaturePhase},
            {"animation_timer_min",std::isfinite(minimumAnimationTimer)?minimumAnimationTimer:0},{"animation_timer_max",maximumAnimationTimer},{"animation_playing_frames",animationPlayingFrames},{"animated_target_channel_count",channelPoseBaseline.size()},
            {"animated_channel_local_pose_delta",localBoneDelta},{"animated_channel_pose_changed",localBoneDelta>1e-5f},{"motion_early_saved",earlyScreenshot},{"motion_late_saved",lateScreenshot},
            {"save_scope","foundation state, clocks, diagnostic pose, current hero lifecycle and needs; populated watershed uses one STL9 host/registry transaction, other modes retain STL8; bonds not connected; camera mode, focus, anchor and orbit stored when present; tether configuration and travel intent not persisted"},
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
            {"pending_raycast", "connected right-click nearby supported ground implemented"},
            {"supportmode", latestSupportMode},
            {"support_mode", latestSupportMode},
            {"slope", latestSlopeRadians},
            {"support_slope_radians", latestSlopeRadians},
            {"normal", {{"x", latestNormal.x}, {"y", latestNormal.y}, {"z", latestNormal.z}}},
            {"support_normal", {{"x", latestNormal.x}, {"y", latestNormal.y}, {"z", latestNormal.z}}},
            {"supportfailure", supportFailureCount},
            {"support_failure_count", supportFailureCount},
            {"disable_terrain_support", disableTerrainSupport},
            {"terrain_support_longitudinal_halfspan_m", kCreatureLongitudinalHalfspan*heroStageBulk},
            {"terrain_support_lateral_halfspan_m", kCreatureLateralHalfspan*heroStageBulk},
            {"terrain_support_policy", "proposed approximate visual body plane alignment only; per-foot IK and terrain foot locking not claimed"},
            {"rendered_ground_support_active", latestGroundSampleSource == "rendered_triangles"},
            {"rendered_ground_support_samples", renderedSupportSampleCount},
            {"rendered_ground_support_fallbacks", renderedSupportFallbackCount},
            {"rendered_ground_support_fallback_reason", latestRenderedSupportFallbackReason},
            {"rendered_ground_sample_source", latestGroundSampleSource},
            {"creature_root_ground_source", latestCreatureRootSource},
            {"creature_root_ground_y", latestCreatureRootGroundY},
            {"creature_root_analytical_ground_y", latestCreatureRootAnalyticalGroundY},
            {"creature_root_rendered_ground_y", latestCreatureRootRenderedGroundY},
            {"creature_root_ground_delta_m", latestCreatureRootGroundDeltaM},
            {"woodland_active", woodlandReport.active},
            {"woodland_proposals_total", woodlandReport.proposalsTotal},
            {"woodland_proposals_placed", woodlandReport.placedCount},
            {"woodland_proposals_skipped", woodlandReport.skippedCount},
            {"woodland_candidate_attempts", woodlandReport.candidateAttempts},
            {"woodland_cluster_attempts", woodlandReport.clusterAttempts},
            {"woodland_budget_exhausted", woodlandReport.candidateBudgetExhausted},
            {"woodland_hard_cap", woodlandReport.hardCap},
            {"woodland_max_trees_limit", woodlandReport.requestedMaxTrees},
            {"woodland_adult_asset_path", woodlandReport.adultMetrics.path},
            {"woodland_adult_loaded", woodlandReport.adultMetrics.loaded},
            {"woodland_adult_placed", woodlandReport.adultPlacedCount},
            {"woodland_adult_skipped", woodlandReport.adultSkippedCount},
            {"woodland_adult_actual_whole_height_m", woodlandReport.adultMetrics.actualWholeHeightM},
            {"woodland_adult_actual_trunk_height_m", woodlandReport.adultMetrics.actualTrunkHeightM},
            {"woodland_adult_actual_crown_width_m", woodlandReport.adultMetrics.actualCrownWidthM},
            {"woodland_adult_authored_target_height_m", woodlandReport.adultMetrics.authoredTargetHeightM},
            {"woodland_adult_authored_target_crown_width_m", woodlandReport.adultMetrics.authoredTargetCrownWidthM},
            {"woodland_adult_authored_mismatch", woodlandReport.adultMetrics.authoredMismatch},
            {"woodland_adult_authored_mismatch_description", woodlandReport.adultMetrics.authoredMismatchDescription},
            {"woodland_adult_stage_correct", woodlandReport.adultMetrics.stageCorrect},
            {"woodland_adult_quality_label", woodlandReport.adultMetrics.qualityLabel},
            {"woodland_sapling_asset_path", woodlandReport.saplingMetrics.path},
            {"woodland_sapling_loaded", woodlandReport.saplingMetrics.loaded},
            {"woodland_sapling_placed", woodlandReport.saplingPlacedCount},
            {"woodland_sapling_skipped", woodlandReport.saplingSkippedCount},
            {"woodland_sapling_actual_whole_height_m", woodlandReport.saplingMetrics.actualWholeHeightM},
            {"woodland_sapling_actual_trunk_height_m", woodlandReport.saplingMetrics.actualTrunkHeightM},
            {"woodland_sapling_actual_crown_width_m", woodlandReport.saplingMetrics.actualCrownWidthM},
            {"woodland_sapling_authored_target_height_m", woodlandReport.saplingMetrics.authoredTargetHeightM},
            {"woodland_sapling_stage_correct", woodlandReport.saplingMetrics.stageCorrect},
            {"woodland_ground_cache_available", woodlandReport.groundCacheAvailable},
            {"woodland_ground_sample_source", woodlandReport.groundSampleSource},
            {"woodland_ground_fallbacks", woodlandReport.groundFallbacks},
            {"woodland_unique_objects", woodlandReport.adultMetrics.uniqueObjects + woodlandReport.saplingMetrics.uniqueObjects},
            {"woodland_unique_vertices", woodlandReport.totalUniqueVertices},
            {"woodland_unique_triangles", woodlandReport.totalUniqueTriangles},
            {"woodland_drawn_triangles", woodlandReport.totalDrawnTriangles},
            {"woodland_placed_instances", woodlandPlacedJson},
            {"woodland_skipped_instances", woodlandSkippedJson},
            {"frame_time_measured",false},{"full_draft_complete",false},{"visual_accepted",false},
            {"limits",{"proxy fallback remains diagnostic; imported creature playback is feasibility only, foot contact not accepted because low-tail clearance and foot skate during blend remain unresolved","no external frame-time measurement; 30 FPS floor not accepted","visual acceptance and full draft completion strictly unaccepted","resource ledger capped at 256 interaction sessions; cap exhaustion rejects further consumption without history eviction; production game requires unbounded or tiered epoch persistence","camera mode/focus/anchor/orbit persisted; tether configuration/travel intent, audio pending; bounded connected right-click ground destination implemented","bounded crossfade blend with sequential evaluation amount correction replaces exclusive binary clip selection for idle/walk/reverse transitions; unblended one-frame pose snap eliminated across transition boundaries; foot sliding during linear crossfade and low-tail clearance remain unresolved limitations","woodland imported geometry and placement counts describe only the supplied assets; biological stage and visual quality remain provisional; plant LOD quality is unverified; missing or rejected stages are reported by actual loaded and skipped fields"}}});
        wi::jobsystem::ShutDown(); DestroyWindow(window);
        return runCalls>0 && screenshot?0:3;
    } catch(const std::exception& error) {
        if(argv) LocalFree(argv);
        try { std::filesystem::create_directories(output); WriteJson(output/"failure.json",{{"ok",false},{"error",error.what()}}); } catch(...) {}
        wi::jobsystem::ShutDown(); return 1;
    }
}
