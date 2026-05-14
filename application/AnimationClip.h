#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "utils/math/MathUtils.h"

template <typename TValue>
struct Keyframe {
    float time = 0.0f;
    TValue value{};
};

template <typename TValue>
struct AnimationCurve {
    std::vector<Keyframe<TValue>> keyframes;
};

struct NodeAnimation {
    AnimationCurve<Vector3> translate;
    AnimationCurve<Quaternion> rotate;
    AnimationCurve<Vector3> scale;
};

struct AnimationClip {
    float duration = 0.0f;
    std::unordered_map<std::string, NodeAnimation> nodeAnimations;
};

struct Animator {
    float time = 0.0f;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;

    void Update(float deltaTime, float duration);
};

AnimationClip LoadAnimationFile(
    const std::string& directoryPath,
    const std::string& filename);

Vector3 CalculateValue(
    const AnimationCurve<Vector3>& curve,
    float time,
    const Vector3& fallback);

Quaternion CalculateValue(
    const AnimationCurve<Quaternion>& curve,
    float time,
    const Quaternion& fallback);

Matrix4x4 MakeNodeAnimationMatrix(
    const NodeAnimation& nodeAnimation,
    float time);
