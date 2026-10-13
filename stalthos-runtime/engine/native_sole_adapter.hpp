#pragma once

#include "WickedEngine.h"
#include "foot_terrain_diagnostic.hpp"
#include "../domain/reference_landscape.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace stalthos::engine {

struct NativeSoleSample {
    FootId foot = FootId::FrontLeft;
    bool valid = false;
    uint32_t selectedVertexCount = 0;
    double minClearanceMeters = 0.0;
    double maxClearanceMeters = 0.0;
    double meanClearanceMeters = 0.0;
    DirectX::XMFLOAT3 lowestVertexWorldPos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 highestVertexWorldPos{0.0f, 0.0f, 0.0f};
    std::string issue;
};

struct NativeSoleReport {
    bool allValid = false;
    std::string method = "native_body_proxy_skinned_surface";
    bool footContactAccepted = false; // Strictly false: diagnostic metric only
    std::array<NativeSoleSample, kFootCount> feet{};
    std::vector<std::string> limitations;
};

class NativeSoleAdapter {
public:
    struct FootConfig {
        double minRestY = 0.000;
        double maxRestY = 0.048;
        float minToeWeight = 0.35f;
        size_t maxVertices = 4096;
    };

    bool initialize(wi::scene::Scene& scene, wi::ecs::Entity heroMotionRoot, const FootConfig& config = FootConfig{}) {
        m_initialized = false;
        if (!std::isfinite(config.minRestY) || !std::isfinite(config.maxRestY) || config.minRestY > config.maxRestY ||
            !std::isfinite(config.minToeWeight) || config.minToeWeight <= 0 || config.minToeWeight > 1 || config.maxVertices == 0 || config.maxVertices > 4096) {
            m_errorMessage = "invalid bounded sole selector configuration";
            return false;
        }
        m_errorMessage.clear();
        for (auto& list : m_cachedIndices) list.clear();

        // 1. Locate the body_proxy object and mesh
        wi::ecs::Entity bodyProxyEntity = wi::ecs::INVALID_ENTITY;
        for (size_t i = 0; i < scene.objects.GetCount(); ++i) {
            const auto entity = scene.objects.GetEntity(i);
            const auto* name = scene.names.GetComponent(entity);
            if (name && name->name == "body_proxy" && scene.Entity_IsDescendant(entity, heroMotionRoot)) {
                bodyProxyEntity = entity; break;
            }
        }
        m_objectEntity = bodyProxyEntity;
        if (bodyProxyEntity == wi::ecs::INVALID_ENTITY) {
            m_errorMessage = "body_proxy object not found in scene";
            return false;
        }

        const auto* object = scene.objects.GetComponent(bodyProxyEntity);
        if (!object || object->meshID == wi::ecs::INVALID_ENTITY) {
            m_errorMessage = "body_proxy object has no valid meshID";
            return false;
        }
        m_meshEntity = object->meshID;
        const auto* mesh = scene.meshes.GetComponent(m_meshEntity);
        if (!mesh) {
            m_errorMessage = "body_proxy mesh component not found";
            return false;
        }

        // 2. Locate the ArmatureComponent
        m_armatureEntity = mesh->armatureID;
        if (m_armatureEntity == wi::ecs::INVALID_ENTITY) {
            m_errorMessage = "body_proxy mesh has no authoritative armature";
            return false;
        }
        const auto* armature = scene.armatures.GetComponent(m_armatureEntity);
        if (!armature) {
            m_errorMessage = "armature component not found";
            return false;
        }

        // 3. Identify toe, calf, and tail joint indices in armature->boneCollection
        std::array<int32_t, kFootCount> toeBoneIndices = {-1, -1, -1, -1};
        std::vector<uint32_t> calfBoneIndices;
        std::vector<uint32_t> tailBoneIndices;

        const size_t boneCount = armature->boneCollection.size();
        for (uint32_t b = 0; b < boneCount; ++b) {
            wi::ecs::Entity boneEntity = armature->boneCollection[b];
            const auto* nameComp = scene.names.GetComponent(boneEntity);
            if (!nameComp) continue;
            const std::string& name = nameComp->name;

            if (name == "toe_front_L") toeBoneIndices[static_cast<size_t>(FootId::FrontLeft)] = static_cast<int32_t>(b);
            else if (name == "toe_front_R") toeBoneIndices[static_cast<size_t>(FootId::FrontRight)] = static_cast<int32_t>(b);
            else if (name == "toe_rear_L") toeBoneIndices[static_cast<size_t>(FootId::RearLeft)] = static_cast<int32_t>(b);
            else if (name == "toe_rear_R") toeBoneIndices[static_cast<size_t>(FootId::RearRight)] = static_cast<int32_t>(b);

            if (name.find("tail") != std::string::npos) {
                tailBoneIndices.push_back(b);
            }
            if ((name.find("leg_") != std::string::npos && name.find("_01") != std::string::npos) ||
                name.find("calf") != std::string::npos) {
                calfBoneIndices.push_back(b);
            }
        }

        for (size_t f = 0; f < kFootCount; ++f) {
            if (toeBoneIndices[f] < 0) {
                m_errorMessage = "missing toe bone in armature: " + std::string(kWickedFootLandmarks[f].boneName);
                return false;
            }
        }

        // 4. Validate bone indices and cache rest vertices per foot
        const size_t vertexCount = mesh->vertex_positions.size();
        const uint32_t influence_div4 = mesh->GetBoneInfluenceDiv4();
        if (influence_div4 == 0 || influence_div4 > 2) { m_errorMessage = "unsupported bone influence count"; return false; }
        if (mesh->vertex_boneindices.size() < vertexCount || mesh->vertex_boneweights.size() < vertexCount) {
            m_errorMessage = "mesh missing bone indices or weights";
            return false;
        }
        if (influence_div4 > 1 && (mesh->vertex_boneindices2.size() < vertexCount || mesh->vertex_boneweights2.size() < vertexCount)) {
            m_errorMessage = "mesh missing secondary bone indices or weights";
            return false;
        }

        std::array<std::vector<uint32_t>, kFootCount> discoveredIndices;

        for (uint32_t i = 0; i < static_cast<uint32_t>(vertexCount); ++i) {
            const DirectX::XMFLOAT3& pos = mesh->vertex_positions[i];
            if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) continue;

            // Rest elevation filter (sole floor is ~0.004 m, pad cushion lobes extend to ~0.048 m)
            if (pos.y < config.minRestY || pos.y > config.maxRestY) continue;

            // Validate all bone indices including zero-weight lanes because SkinVertex dereferences all 4
            const auto& ind1 = mesh->vertex_boneindices[i];
            if (ind1.x >= boneCount || ind1.y >= boneCount || ind1.z >= boneCount || ind1.w >= boneCount) {
                m_errorMessage = "mesh vertex " + std::to_string(i) + " contains out-of-bounds bone index";
                return false;
            }
            if (influence_div4 > 1) {
                const auto& ind2 = mesh->vertex_boneindices2[i];
                if (ind2.x >= boneCount || ind2.y >= boneCount || ind2.z >= boneCount || ind2.w >= boneCount) {
                    m_errorMessage = "mesh vertex " + std::to_string(i) + " contains out-of-bounds bone index in secondary lane";
                    return false;
                }
            }

            const auto& wei1 = mesh->vertex_boneweights[i];
            auto validWeights = [](const DirectX::XMFLOAT4& w) { return std::isfinite(w.x) && std::isfinite(w.y) && std::isfinite(w.z) && std::isfinite(w.w) && w.x >= 0 && w.y >= 0 && w.z >= 0 && w.w >= 0; };
            if (!validWeights(wei1) || (influence_div4 > 1 && !validWeights(mesh->vertex_boneweights2[i]))) { m_errorMessage = "invalid sole skin weights"; return false; }
            double totalWeight = wei1.x + wei1.y + wei1.z + wei1.w;
            if (influence_div4 > 1) { const auto& w = mesh->vertex_boneweights2[i]; totalWeight += w.x + w.y + w.z + w.w; }
            if (std::abs(totalWeight - 1.0) > 0.002) { m_errorMessage = "sole weights are not normalized"; return false; }
            auto getToeWeight = [&](uint32_t toeJoint) -> float {
                float w = 0.0f;
                if (ind1.x == toeJoint) w += wei1.x;
                if (ind1.y == toeJoint) w += wei1.y;
                if (ind1.z == toeJoint) w += wei1.z;
                if (ind1.w == toeJoint) w += wei1.w;
                if (influence_div4 > 1) {
                    const auto& wei2 = mesh->vertex_boneweights2[i];
                    const auto& ind2 = mesh->vertex_boneindices2[i];
                    if (ind2.x == toeJoint) w += wei2.x;
                    if (ind2.y == toeJoint) w += wei2.y;
                    if (ind2.z == toeJoint) w += wei2.z;
                    if (ind2.w == toeJoint) w += wei2.w;
                }
                return w;
            };

            auto getGroupWeight = [&](const std::vector<uint32_t>& group) -> float {
                float sum = 0.0f;
                for (uint32_t j : group) {
                    if (ind1.x == j) sum += wei1.x;
                    if (ind1.y == j) sum += wei1.y;
                    if (ind1.z == j) sum += wei1.z;
                    if (ind1.w == j) sum += wei1.w;
                    if (influence_div4 > 1) {
                        const auto& wei2 = mesh->vertex_boneweights2[i];
                        const auto& ind2 = mesh->vertex_boneindices2[i];
                        if (ind2.x == j) sum += wei2.x;
                        if (ind2.y == j) sum += wei2.y;
                        if (ind2.z == j) sum += wei2.z;
                        if (ind2.w == j) sum += wei2.w;
                    }
                }
                return sum;
            };

            float calfWeight = getGroupWeight(calfBoneIndices);
            float tailWeight = getGroupWeight(tailBoneIndices);

            // Exclude vertices influenced by tail
            if (tailWeight > 1e-4f) continue;

            for (size_t f = 0; f < kFootCount; ++f) {
                uint32_t toeJoint = static_cast<uint32_t>(toeBoneIndices[f]);
                float toeWeight = getToeWeight(toeJoint);

                // Dominant toe weight over calves, strictly >= minToeWeight
                if (toeWeight >= config.minToeWeight && toeWeight > calfWeight) {
                    discoveredIndices[f].push_back(i);
                }
            }
        }

        std::vector<unsigned char> seen(vertexCount, 0);
        for (const auto& group : discoveredIndices) for (auto index : group) {
            if (seen[index]++) { m_errorMessage = "ambiguous overlapping foot selectors"; return false; }
        }
        // 5. Validate selections per foot
        for (size_t f = 0; f < kFootCount; ++f) {
            if (discoveredIndices[f].empty()) {
                m_errorMessage = "empty sole vertex selection for " + std::string(footIdToString(kAllFootIds[f]));
                return false;
            }
            if (discoveredIndices[f].size() > config.maxVertices) {
                m_errorMessage = "sole vertex selection exceeds " + std::to_string(config.maxVertices) + " vertices for " + std::string(footIdToString(kAllFootIds[f]));
                return false;
            }
        }

        m_cachedIndices = std::move(discoveredIndices);
        m_initialized = true;
        return true;
    }

    NativeSoleReport evaluate(const wi::scene::Scene& scene, const domain::ReferenceLandscape& landscape) const {
        NativeSoleReport report;
        report.method = "native_body_proxy_skinned_surface";
        report.footContactAccepted = false;
        report.limitations = {
            "Evaluates actual CPU-skinned contact vertices from body_proxy via pinned wi::scene::SkinVertex against ReferenceLandscape.",
            "Front limbs (manus) are kinematic diagnostics only; authored contact schedules apply only to hind limbs (pes).",
            "Clearance is instantaneous signed geometric elevation; terrain foot-locking, substrate friction, and soft-ground penetration are unmodelled.",
            "Kinematic surface diagnostic only; foot contact acceptance is strictly false."
        };

        if (!m_initialized) {
            report.allValid = false;
            for (size_t f = 0; f < kFootCount; ++f) {
                report.feet[f].foot = kAllFootIds[f];
                report.feet[f].valid = false;
                report.feet[f].issue = "NativeSoleAdapter uninitialized: " + m_errorMessage;
            }
            return report;
        }

        const auto* mesh = scene.meshes.GetComponent(m_meshEntity);
        const auto* armature = scene.armatures.GetComponent(m_armatureEntity);
        const size_t objectIndex = scene.objects.GetIndex(m_objectEntity);
        const bool objectMatrixValid = objectIndex < scene.matrix_objects.size();

        if (!mesh || !armature || !objectMatrixValid || armature->boneData.size() < armature->boneCollection.size()) {
            report.allValid = false;
            for (size_t f = 0; f < kFootCount; ++f) {
                report.feet[f].foot = kAllFootIds[f];
                report.feet[f].valid = false;
                report.feet[f].issue = "mesh, armature, or armature transform missing from scene";
            }
            return report;
        }

        // Armature world transform matrix
        const DirectX::XMMATRIX armatureWorld = DirectX::XMLoadFloat4x4(&scene.matrix_objects[objectIndex]);

        bool allFeetValid = true;

        for (size_t f = 0; f < kFootCount; ++f) {
            auto& sample = report.feet[f];
            sample.foot = kAllFootIds[f];
            const auto& indices = m_cachedIndices[f];
            sample.selectedVertexCount = static_cast<uint32_t>(indices.size());

            if (indices.empty()) {
                sample.valid = false;
                sample.issue = "empty vertex selection";
                allFeetValid = false;
                continue;
            }

            double minClear = std::numeric_limits<double>::infinity();
            double maxClear = -std::numeric_limits<double>::infinity();
            double sumClear = 0.0;
            size_t validCount = 0;
            DirectX::XMFLOAT3 lowestWorldPos{0.0f, 0.0f, 0.0f};
            DirectX::XMFLOAT3 highestWorldPos{0.0f, 0.0f, 0.0f};

            for (uint32_t v_idx : indices) {
                // Pinned Wicked Engine CPU skinning: returns position in armature local space
                DirectX::XMVECTOR localPos = wi::scene::SkinVertex(*mesh, *armature, v_idx);
                // Transform from armature local space to world space (single world transform)
                DirectX::XMVECTOR worldPosVec = DirectX::XMVector3Transform(localPos, armatureWorld);
                DirectX::XMFLOAT3 worldPos;
                DirectX::XMStoreFloat3(&worldPos, worldPosVec);

                if (!std::isfinite(worldPos.x) || !std::isfinite(worldPos.y) || !std::isfinite(worldPos.z)) {
                    continue;
                }

                if (landscape.inBounds(worldPos.x, worldPos.z)) {
                    double terrainH = landscape.height(worldPos.x, worldPos.z);
                    if (std::isfinite(terrainH)) {
                        double clear = static_cast<double>(worldPos.y) - terrainH;
                        if (std::isfinite(clear)) {
                            ++validCount;
                            sumClear += clear;
                            if (clear < minClear) {
                                minClear = clear;
                                lowestWorldPos = worldPos;
                            }
                            if (clear > maxClear) {
                                maxClear = clear;
                                highestWorldPos = worldPos;
                            }
                        }
                    }
                }
            }

            if (validCount == indices.size() && validCount > 0) {
                sample.valid = true;
                sample.minClearanceMeters = minClear;
                sample.maxClearanceMeters = maxClear;
                sample.meanClearanceMeters = sumClear / static_cast<double>(validCount);
                sample.lowestVertexWorldPos = lowestWorldPos;
                sample.highestVertexWorldPos = highestWorldPos;
            } else {
                sample.valid = false;
                sample.issue = "incomplete or invalid sole clearance samples";
                allFeetValid = false;
            }
        }

        report.allValid = allFeetValid;
        return report;
    }

    bool isInitialized() const noexcept { return m_initialized; }
    const std::string& getErrorMessage() const noexcept { return m_errorMessage; }
    const std::array<std::vector<uint32_t>, kFootCount>& getCachedIndices() const noexcept { return m_cachedIndices; }

private:
    bool m_initialized = false;
    std::string m_errorMessage;
    wi::ecs::Entity m_objectEntity = wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity m_meshEntity = wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity m_armatureEntity = wi::ecs::INVALID_ENTITY;
    std::array<std::vector<uint32_t>, kFootCount> m_cachedIndices{};
};

} // namespace stalthos::engine
