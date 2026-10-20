#pragma once

#include "WickedEngine.h"
#include "domain/woodland_stands.hpp"
#include "domain/reference_landscape.hpp"
#include "domain/rendered_ground_surface.hpp"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>
#include <functional>

void ImportModel_GLTF(const std::string&, wi::scene::Scene&);

namespace stalthos::engine {

struct WoodlandTreeInstanceRecord {
    std::uint64_t id = 0;
    std::string stage; // "adult" or "sapling"
    double x = 0.0;
    double y = 0.0; // sampled rendered ground elevation
    double z = 0.0;
    double analyticalY = 0.0;
    double groundDeltaM = 0.0;
    double headingRadians = 0.0;
    double scale = 1.0;
    wi::ecs::Entity rootEntity = wi::ecs::INVALID_ENTITY;
    bool renderedGroundSampled = true;
};

struct WoodlandTreeSkippedRecord {
    std::uint64_t id = 0;
    std::string stage;
    double x = 0.0;
    double z = 0.0;
    std::string reason;
};

struct WoodlandAssetMetrics {
    std::string path;
    bool loaded = false;
    double actualWholeHeightM = 0.0;
    double actualTrunkHeightM = 0.0;
    double actualCrownWidthM = 0.0;
    double authoredTargetHeightM = 0.0;
    double authoredTargetCrownWidthM = 0.0;
    double baseDatumCorrectionM = 0.0;
    bool authoredMismatch = false;
    std::string authoredMismatchDescription;
    bool stageCorrect = false;
    size_t uniqueObjects = 0;
    size_t uniqueVertices = 0;
    size_t uniqueTriangles = 0;
    std::string qualityLabel = "provisional_unverified";
};

struct WoodlandSceneReport {
    bool active = false;
    std::uint64_t worldSeed = 42;
    std::size_t hardCap = domain::WoodlandStands::kHardCapTrees;
    std::size_t requestedMaxTrees = 20;
    std::size_t candidateAttempts = 0;
    std::size_t clusterAttempts = 0;
    bool candidateBudgetExhausted = false;

    std::size_t proposalsTotal = 0;
    std::size_t placedCount = 0;
    std::size_t skippedCount = 0;

    std::size_t adultPlacedCount = 0;
    std::size_t adultSkippedCount = 0;
    std::size_t saplingPlacedCount = 0;
    std::size_t saplingSkippedCount = 0;

    bool groundCacheAvailable = false;
    std::string groundSampleSource = "rendered_triangles";
    std::size_t groundFallbacks = 0;

    WoodlandAssetMetrics adultMetrics;
    WoodlandAssetMetrics saplingMetrics;

    std::size_t totalUniqueVertices = 0;
    std::size_t totalUniqueTriangles = 0;
    std::size_t totalDrawnTriangles = 0;

    std::vector<WoodlandTreeInstanceRecord> placedTrees;
    std::vector<WoodlandTreeSkippedRecord> skippedTrees;
};

namespace detail {

inline std::string toLowerString(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

struct PrototypeModel {
    bool valid = false;
    std::string errorMessage;
    WoodlandAssetMetrics metrics;

    struct Part {
        std::string name;
        wi::ecs::Entity protoObjectEntity = wi::ecs::INVALID_ENTITY;
        wi::ecs::Entity meshID = wi::ecs::INVALID_ENTITY;
        DirectX::XMFLOAT4X4 renderedTransform{};
    };
    std::vector<Part> parts;
};

inline PrototypeModel loadWoodlandPrototype(
    const std::filesystem::path& path,
    wi::scene::Scene& mainScene,
    bool isSaplingStage
) {
    PrototypeModel model;
    model.metrics.path = path.u8string();
    model.metrics.authoredTargetHeightM = isSaplingStage ? 4.2 : 14.0;
    model.metrics.authoredTargetCrownWidthM = isSaplingStage ? 2.4 : 6.5;
    model.metrics.qualityLabel = "provisional_unverified";

    if (path.empty()) {
        model.valid = false;
        model.errorMessage = "no asset path provided";
        return model;
    }

    if (!std::filesystem::exists(path)) {
        model.valid = false;
        model.errorMessage = "asset path does not exist: " + path.u8string();
        return model;
    }

    wi::scene::Scene tempScene;
    try {
        ImportModel_GLTF(path.u8string(), tempScene);
    } catch (const std::exception& e) {
        model.valid = false;
        model.errorMessage = std::string("GLTF import exception: ") + e.what();
        return model;
    }

    if (tempScene.objects.GetCount() == 0 || tempScene.meshes.GetCount() == 0) {
        model.valid = false;
        model.errorMessage = "GLTF import produced 0 objects or meshes";
        return model;
    }

    // Resolve renderer object matrices, including imported GLB ancestor transforms.
    tempScene.Update(0.0f);

    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();

    float structMinY = std::numeric_limits<float>::infinity();
    float structMaxY = -std::numeric_limits<float>::infinity();
    bool hasStructureMesh = false;

    size_t totalVertices = 0;
    size_t totalTriangles = 0;

    bool finiteGeometry = true;
    for (size_t i = 0; i < tempScene.objects.GetCount(); ++i) {
        auto entity = tempScene.objects.GetEntity(i);
        const auto& obj = tempScene.objects[i];
        const size_t objectIndex = tempScene.objects.GetIndex(entity);
        if (obj.meshID == wi::ecs::INVALID_ENTITY) continue;
        if (objectIndex >= tempScene.matrix_objects.size()) {
            model.errorMessage = "GLTF rendered object instance matrix unavailable after scene update";
            return model;
        }
        const auto* meshPtr = tempScene.meshes.GetComponent(obj.meshID);
        if (!meshPtr) continue;
        const auto& mesh = *meshPtr;
        const auto* nameComp = tempScene.names.GetComponent(entity);
        std::string meshName = nameComp ? nameComp->name : "";
        std::string lowerMeshName = toLowerString(meshName);
        bool isStructure = (lowerMeshName.find("structure") != std::string::npos ||
                            lowerMeshName.find("trunk") != std::string::npos ||
                            lowerMeshName.find("bark") != std::string::npos ||
                            lowerMeshName.find("wood") != std::string::npos ||
                            lowerMeshName.find("stem") != std::string::npos);

        totalVertices += mesh.vertex_positions.size();
        totalTriangles += mesh.indices.size() / 3;

        const auto world = XMLoadFloat4x4(&tempScene.matrix_objects[objectIndex]);
        for (const auto& pos : mesh.vertex_positions) {
            const auto transformed = XMVector3TransformCoord(XMLoadFloat3(&pos), world);
            XMFLOAT3 p{}; XMStoreFloat3(&p, transformed);
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) { finiteGeometry = false; continue; }
            minX = std::min(minX, p.x); minY = std::min(minY, p.y); minZ = std::min(minZ, p.z);
            maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y); maxZ = std::max(maxZ, p.z);

            if (isStructure) {
                hasStructureMesh = true;
                structMinY = std::min(structMinY, p.y);
                structMaxY = std::max(structMaxY, p.y);
            }
        }
    }

    if (!finiteGeometry || !std::isfinite(minY) || !std::isfinite(maxY) || maxY < minY) {
        model.errorMessage = "GLTF geometry has no finite rendered object bounds";
        return model;
    }
    model.metrics.actualWholeHeightM = static_cast<double>(maxY - minY);
    double spanX = (maxX >= minX) ? static_cast<double>(maxX - minX) : 0.0;
    double spanZ = (maxZ >= minZ) ? static_cast<double>(maxZ - minZ) : 0.0;
    model.metrics.actualCrownWidthM = std::max(spanX, spanZ);
    model.metrics.actualTrunkHeightM = hasStructureMesh ? static_cast<double>(structMaxY - structMinY) : 0.0;
    model.metrics.baseDatumCorrectionM = -static_cast<double>(minY);
    model.metrics.uniqueObjects = tempScene.objects.GetCount();
    model.metrics.uniqueVertices = totalVertices;
    model.metrics.uniqueTriangles = totalTriangles;

    // Validate stage dimensions
    if (isSaplingStage) {
        // Sapling stage strictly rejects mature-sized assets (> 10m)
        if (model.metrics.actualWholeHeightM > 10.0) {
            model.valid = false;
            model.errorMessage = "Sapling candidate rejected: height " + std::to_string(model.metrics.actualWholeHeightM) +
                                 "m exceeds 10m; mature GLB cannot masquerade as sapling (authored target 4.2m).";
            model.metrics.authoredMismatch = true;
            model.metrics.authoredMismatchDescription = model.errorMessage;
            model.metrics.stageCorrect = false;
            return model;
        }
        // Valid sapling size
        model.metrics.stageCorrect = false;
        model.metrics.authoredMismatch = false;
        model.metrics.authoredMismatchDescription = "Geometry measured; biological stage suitability remains provisional and is not inferred from height tolerance.";
    } else {
        // Adult stage: detect provisional mature prototype mismatch (e.g., 28m Splitcrown)
        if (model.metrics.actualWholeHeightM > 18.0) {
            model.metrics.authoredMismatch = true;
            model.metrics.stageCorrect = false;
            model.metrics.authoredMismatchDescription =
                "Imported adult candidate exceeds the provisional 18m whole-height envelope (" +
                std::to_string(model.metrics.actualWholeHeightM) + "m whole, " +
                std::to_string(model.metrics.actualTrunkHeightM) +
                "m named structure). Authored young-adult structure target is 14m; biological stage suitability is not inferred from dimensions.";
        } else {
            model.metrics.stageCorrect = false;
            model.metrics.authoredMismatch = false;
            model.metrics.authoredMismatchDescription = "Geometry measured; biological stage suitability remains provisional and is not inferred from height tolerance.";
        }
    }

    model.metrics.authoredMismatchDescription += " Whole height measured: " + std::to_string(model.metrics.actualWholeHeightM) +
        " m; named structure height: " + (model.metrics.actualTrunkHeightM > 0.0 ? std::to_string(model.metrics.actualTrunkHeightM) + " m" : "unavailable") +
        ". Derived base datum correction applied to instance roots: " +
        std::to_string(model.metrics.baseDatumCorrectionM) + " m (source geometry minimum Y aligned to rendered ground sample).";

    // Capture parts before merging
    for (size_t i = 0; i < tempScene.objects.GetCount(); ++i) {
        auto ent = tempScene.objects.GetEntity(i);
        const auto* obj = &tempScene.objects[i];
        const auto* nameComp = tempScene.names.GetComponent(ent);
        std::string name = nameComp ? nameComp->name : ("part_" + std::to_string(i));
        PrototypeModel::Part part;
        part.name = name;
        part.protoObjectEntity = ent;
        part.meshID = obj->meshID;
        const size_t objectIndex = tempScene.objects.GetIndex(ent);
        if (objectIndex >= tempScene.matrix_objects.size()) {
            model.errorMessage = "GLTF object matrix unavailable after scene update";
            return model;
        }
        part.renderedTransform = tempScene.matrix_objects[objectIndex];
        model.parts.push_back(part);
    }

    // Merge resources into main scene
    mainScene.Merge(tempScene);

    // Disable rendering on the prototype entities at origin
    for (const auto& part : model.parts) {
        auto* obj = mainScene.objects.GetComponent(part.protoObjectEntity);
        if (obj) {
            obj->SetRenderable(false);
            obj->SetCastShadow(false);
        }
    }

    model.valid = true;
    model.metrics.loaded = true;
    return model;
}

} // namespace detail

inline WoodlandSceneReport populateWoodlandScene(
    wi::scene::Scene& scene,
    const domain::ReferenceLandscape& landscape,
    const std::filesystem::path& adultTreePath,
    const std::filesystem::path& saplingTreePath,
    std::uint64_t worldSeed = 42,
    std::size_t maxTrees = 20,
    const std::vector<domain::TreePlacement>* exterior = nullptr,
    const std::function<double(double,double)>& exteriorGround = {}
) {
    using namespace DirectX;
    WoodlandSceneReport report;
    report.worldSeed = worldSeed;
    report.hardCap = domain::WoodlandStands::kHardCapTrees;
    std::size_t clampedMax = std::min(maxTrees, report.hardCap);
    report.requestedMaxTrees = clampedMax;

    // 1. Build and verify RenderedGroundSurface from the cached ground mesh in scene
    wi::ecs::Entity groundEntity = wi::ecs::INVALID_ENTITY;
    for (size_t i = 0; i < scene.objects.GetCount(); ++i) {
        auto ent = scene.objects.GetEntity(i);
        const auto* nameComp = scene.names.GetComponent(ent);
        if (nameComp && nameComp->name == "draft reference ground") {
            groundEntity = ent;
            break;
        }
    }

    domain::RenderedGroundSurface groundSurface;
    std::string groundIssue;
    if (groundEntity == wi::ecs::INVALID_ENTITY) {
        groundIssue = "'draft reference ground' not found in scene";
    } else {
        const auto* groundObj = scene.objects.GetComponent(groundEntity);
        if (!groundObj || groundObj->meshID == wi::ecs::INVALID_ENTITY) {
            groundIssue = "ground object has no valid meshID";
        } else {
            const auto* groundMesh = scene.meshes.GetComponent(groundObj->meshID);
            size_t groundIndex = scene.objects.GetIndex(groundEntity);
            if (!groundMesh) {
                groundIssue = "ground mesh component missing";
            } else {
                domain::Matrix4x4 groundMat = domain::Matrix4x4::identity();
                if (groundIndex < scene.matrix_objects.size()) {
                    groundMat = domain::Matrix4x4::fromRowMajor(reinterpret_cast<const float*>(&scene.matrix_objects[groundIndex]));
                } else { groundIssue = "ground rendered object instance matrix unavailable"; }
                if (groundIndex < scene.matrix_objects.size() && groundSurface.build(groundMesh->vertex_positions.data(), groundMesh->vertex_positions.size(),
                                        groundMesh->indices.data(), groundMesh->indices.size(), groundMat)) {
                    report.groundCacheAvailable = true;
                    report.groundSampleSource = "rendered_triangles";
                } else if (groundIndex < scene.matrix_objects.size()) {
                    groundIssue = groundSurface.errorMessage();
                }
            }
        }
    }

    // 2. Generate candidate stands proposals via reviewed WoodlandStands domain helper
    domain::WoodlandStands stands(landscape, worldSeed, clampedMax);
    report.candidateAttempts = stands.candidateAttempts();
    report.clusterAttempts = stands.clusterAttempts();
    report.candidateBudgetExhausted = stands.isCandidateBudgetExhausted();
    report.proposalsTotal = exterior ? exterior->size() : stands.placements().size();
    if(exterior){if(!exteriorGround || exterior->size()>report.hardCap)throw std::invalid_argument("invalid exterior woodland inputs");report.groundCacheAvailable=true;report.groundSampleSource="resident_streamed_rendered_triangles";report.candidateAttempts=0;report.clusterAttempts=0;report.candidateBudgetExhausted=false;}

    // 3. Load prototypes (if paths supplied)
    detail::PrototypeModel adultModel;
    if (!adultTreePath.empty()) {
        adultModel = detail::loadWoodlandPrototype(adultTreePath, scene, false);
        report.adultMetrics = adultModel.metrics;
    } else {
        report.adultMetrics.path = "";
        report.adultMetrics.loaded = false;
        report.adultMetrics.authoredTargetHeightM = 14.0;
        report.adultMetrics.authoredTargetCrownWidthM = 6.5;
        report.adultMetrics.qualityLabel = "provisional_unverified";
    }

    detail::PrototypeModel saplingModel;
    if (!saplingTreePath.empty()) {
        // Enforce: no one GLB masquerading both ages
        std::error_code equivalentError;
        const bool sameAsset = !adultTreePath.empty() && std::filesystem::equivalent(adultTreePath, saplingTreePath, equivalentError);
        if (equivalentError) {
            saplingModel.valid = false;
            saplingModel.errorMessage = "Unable to compare adult and sapling asset paths: " + equivalentError.message();
            report.saplingMetrics.path = saplingTreePath.u8string();
            report.saplingMetrics.loaded = false;
            report.saplingMetrics.authoredTargetHeightM = 4.2;
            report.saplingMetrics.authoredTargetCrownWidthM = 2.4;
            report.saplingMetrics.authoredMismatch = true;
            report.saplingMetrics.authoredMismatchDescription = saplingModel.errorMessage;
        } else if (sameAsset) {
            saplingModel.valid = false;
            saplingModel.errorMessage = "Sapling path identical to adult path; mature asset cannot masquerade as sapling.";
            report.saplingMetrics.path = saplingTreePath.u8string();
            report.saplingMetrics.loaded = false;
            report.saplingMetrics.authoredTargetHeightM = 4.2;
            report.saplingMetrics.authoredTargetCrownWidthM = 2.4;
            report.saplingMetrics.authoredMismatch = true;
            report.saplingMetrics.authoredMismatchDescription = saplingModel.errorMessage;
        } else {
            saplingModel = detail::loadWoodlandPrototype(saplingTreePath, scene, true);
            report.saplingMetrics = saplingModel.metrics;
        }
    } else {
        report.saplingMetrics.path = "";
        report.saplingMetrics.loaded = false;
        report.saplingMetrics.authoredTargetHeightM = 4.2;
        report.saplingMetrics.authoredTargetCrownWidthM = 2.4;
        report.saplingMetrics.qualityLabel = "provisional_unverified";
    }

    // Calculate truthful unique geometry
    if (adultModel.valid) {
        report.totalUniqueVertices += adultModel.metrics.uniqueVertices;
        report.totalUniqueTriangles += adultModel.metrics.uniqueTriangles;
    }
    if (saplingModel.valid) {
        report.totalUniqueVertices += saplingModel.metrics.uniqueVertices;
        report.totalUniqueTriangles += saplingModel.metrics.uniqueTriangles;
    }

    // 4. Instantiate proposals using shared mesh/material resources
    for (const auto& tp : (exterior ? *exterior : stands.placements())) {
        std::string stageName = tp.isSapling ? "sapling" : "adult";

        // Check if corresponding stage asset prototype is loaded
        const detail::PrototypeModel* activeModel = tp.isSapling ? &saplingModel : &adultModel;
        if (!activeModel->valid) {
            std::string reason = tp.isSapling
                ? (saplingTreePath.empty() ? "missing_stage_asset: sapling" : ("invalid_sapling_asset: " + saplingModel.errorMessage))
                : (adultTreePath.empty() ? "missing_stage_asset: adult" : ("invalid_adult_asset: " + adultModel.errorMessage));

            WoodlandTreeSkippedRecord skip;
            skip.id = tp.id;
            skip.stage = stageName;
            skip.x = tp.x;
            skip.z = tp.z;
            skip.reason = reason;
            report.skippedTrees.push_back(skip);
            if (tp.isSapling) report.saplingSkippedCount++; else report.adultSkippedCount++;
            report.skippedCount++;
            continue;
        }

        // Verify ground cache availability
        if (!report.groundCacheAvailable) {
            WoodlandTreeSkippedRecord skip;
            skip.id = tp.id;
            skip.stage = stageName;
            skip.x = tp.x;
            skip.z = tp.z;
            skip.reason = "rendered_ground_unavailable: " + groundIssue;
            report.skippedTrees.push_back(skip);
            if (tp.isSapling) report.saplingSkippedCount++; else report.adultSkippedCount++;
            report.skippedCount++;
            report.groundFallbacks++;
            continue;
        }

        // Sample actual rendered ground triangles at proposal XZ
        if (!exterior && !groundSurface.inBounds(tp.x, tp.z)) {
            WoodlandTreeSkippedRecord skip;
            skip.id = tp.id;
            skip.stage = stageName;
            skip.x = tp.x;
            skip.z = tp.z;
            skip.reason = "proposal_out_of_rendered_ground_bounds";
            report.skippedTrees.push_back(skip);
            if (tp.isSapling) report.saplingSkippedCount++; else report.adultSkippedCount++;
            report.skippedCount++;
            report.groundFallbacks++;
            continue;
        }

        double sampledGroundY = exterior ? exteriorGround(tp.x,tp.z) : groundSurface.sampleHeight(tp.x, tp.z);
        if(!std::isfinite(sampledGroundY))throw std::runtime_error("nonfinite woodland ground");
        double deltaM = sampledGroundY - tp.y;

        // Create root transform entity for the tree instance
        wi::ecs::Entity instanceRoot = scene.Entity_CreateTransform("woodland_" + stageName + "_" + std::to_string(tp.id));
        auto* rootTrans = scene.transforms.GetComponent(instanceRoot);
        if (rootTrans) {
            rootTrans->Translate(XMFLOAT3(static_cast<float>(tp.x), static_cast<float>(sampledGroundY + activeModel->metrics.baseDatumCorrectionM * tp.scale), static_cast<float>(tp.z)));
            rootTrans->RotateRollPitchYaw(XMFLOAT3(0.0f, static_cast<float>(tp.headingRadians), 0.0f));
            rootTrans->Scale(XMFLOAT3(static_cast<float>(tp.scale), static_cast<float>(tp.scale), static_cast<float>(tp.scale)));
            rootTrans->UpdateTransform();
        }

        // Attach child objects referencing shared prototype meshes
        for (const auto& part : activeModel->parts) {
            wi::ecs::Entity childObj = scene.Entity_CreateObject("woodland_" + stageName + "_" + std::to_string(tp.id) + "_" + part.name);
            auto* objComp = scene.objects.GetComponent(childObj);
            if (objComp) {
                objComp->meshID = part.meshID;
                objComp->SetCastShadow(true);
                objComp->SetRenderable(true);
            }

            auto* childTrans = scene.transforms.GetComponent(childObj);
            if (childTrans) {
                childTrans->MatrixTransform(part.renderedTransform);
                childTrans->UpdateTransform();
            }

            scene.Component_Attach(childObj, instanceRoot, true);
        }

        WoodlandTreeInstanceRecord placed;
        placed.id = tp.id;
        placed.stage = stageName;
        placed.x = tp.x;
        placed.y = sampledGroundY;
        placed.z = tp.z;
        placed.analyticalY = tp.y;
        placed.groundDeltaM = deltaM;
        placed.headingRadians = tp.headingRadians;
        placed.scale = tp.scale;
        placed.rootEntity = instanceRoot;
        placed.renderedGroundSampled = true;

        report.placedTrees.push_back(placed);
        if (tp.isSapling) {
            report.saplingPlacedCount++;
            report.totalDrawnTriangles += saplingModel.metrics.uniqueTriangles;
        } else {
            report.adultPlacedCount++;
            report.totalDrawnTriangles += adultModel.metrics.uniqueTriangles;
        }
        report.placedCount++;
    }

    report.active = (report.placedCount > 0);
    return report;
}

} // namespace stalthos::engine
