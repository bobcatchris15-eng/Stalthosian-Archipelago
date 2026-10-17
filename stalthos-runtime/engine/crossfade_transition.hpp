#pragma once

#include "domain/animation_blend.hpp"
#include "WickedEngine.h"
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace stalthos::engine {

struct AnimationTrackKey {
    wi::ecs::Entity target = wi::ecs::INVALID_ENTITY;
    wi::scene::AnimationComponent::AnimationChannel::Path path = wi::scene::AnimationComponent::AnimationChannel::Path::TRANSLATION;

    bool operator<(const AnimationTrackKey& other) const {
        if (target != other.target) return target < other.target;
        return static_cast<int>(path) < static_cast<int>(other.path);
    }
    bool operator==(const AnimationTrackKey& other) const {
        return target == other.target && path == other.path;
    }
};

struct AnimationTopologyAnalysis {
    bool has_full_shared_tracks = false;
    size_t shared_channel_count = 0;
    size_t clip_a_only_channel_count = 0;
    size_t clip_b_only_channel_count = 0;
    std::string clip_a_name;
    std::string clip_b_name;
};

inline AnimationTopologyAnalysis analyzeAnimationTopology(
    const wi::scene::AnimationComponent& animA,
    const std::string& nameA,
    const wi::scene::AnimationComponent& animB,
    const std::string& nameB)
{
    AnimationTopologyAnalysis analysis;
    analysis.clip_a_name = nameA;
    analysis.clip_b_name = nameB;

    std::set<AnimationTrackKey> tracksA;
    for (const auto& ch : animA.channels) {
        tracksA.insert({ch.target, ch.path});
    }

    std::set<AnimationTrackKey> tracksB;
    for (const auto& ch : animB.channels) {
        tracksB.insert({ch.target, ch.path});
    }

    for (const auto& track : tracksA) {
        if (tracksB.find(track) != tracksB.end()) {
            analysis.shared_channel_count++;
        } else {
            analysis.clip_a_only_channel_count++;
        }
    }

    for (const auto& track : tracksB) {
        if (tracksA.find(track) == tracksA.end()) {
            analysis.clip_b_only_channel_count++;
        }
    }

    analysis.has_full_shared_tracks = (analysis.shared_channel_count > 0 &&
                                       analysis.clip_a_only_channel_count == 0 &&
                                       analysis.clip_b_only_channel_count == 0);
    return analysis;
}

struct EngineCrossfadeOrder {
    wi::scene::AnimationComponent* firstAnim = nullptr;
    wi::scene::AnimationComponent* secondAnim = nullptr;
    wi::ecs::Entity firstEntity = wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity secondEntity = wi::ecs::INVALID_ENTITY;
    size_t firstIndex = 0;
    size_t secondIndex = 0;
    std::string firstName;
    std::string secondName;
    bool firstIsWalk = false;
};

inline EngineCrossfadeOrder determineEvaluationOrder(
    wi::scene::Scene& scene,
    wi::ecs::Entity walkEntity,
    const std::string& walkName,
    wi::ecs::Entity idleEntity,
    const std::string& idleName)
{
    EngineCrossfadeOrder order;
    auto* walkAnim = scene.animations.GetComponent(walkEntity);
    auto* idleAnim = scene.animations.GetComponent(idleEntity);
    const size_t walkIdx = scene.animations.GetIndex(walkEntity);
    const size_t idleIdx = scene.animations.GetIndex(idleEntity);

    if (walkIdx < idleIdx) {
        order.firstAnim = walkAnim;
        order.firstEntity = walkEntity;
        order.firstIndex = walkIdx;
        order.firstName = walkName;
        order.firstIsWalk = true;

        order.secondAnim = idleAnim;
        order.secondEntity = idleEntity;
        order.secondIndex = idleIdx;
        order.secondName = idleName;
    } else {
        order.firstAnim = idleAnim;
        order.firstEntity = idleEntity;
        order.firstIndex = idleIdx;
        order.firstName = idleName;
        order.firstIsWalk = false;

        order.secondAnim = walkAnim;
        order.secondEntity = walkEntity;
        order.secondIndex = walkIdx;
        order.secondName = walkName;
    }
    return order;
}

struct BlendFrameTelemetrySample {
    uint64_t frame_index = 0;
    double elapsed_s = 0.0;
    double logical_idle_weight = 0.0;
    double logical_walk_weight = 0.0;
    float applied_first_amount = 0.0f;
    float applied_second_amount = 0.0f;
    std::string first_evaluated_clip;
    std::string second_evaluated_clip;
    float max_bone_translation_delta_m = 0.0f;
    float max_bone_rotation_delta_rad = 0.0f;
    bool deltas_finite = true;
};

} // namespace stalthos::engine
