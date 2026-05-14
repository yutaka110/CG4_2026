#include "AnimationClip.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

namespace {

std::string JoinPath(const std::string& directoryPath, const std::string& filename) {
    if (directoryPath.empty()) {
        return filename;
    }
    const char tail = directoryPath.back();
    if (tail == '/' || tail == '\\') {
        return directoryPath + filename;
    }
    return directoryPath + "/" + filename;
}

float NormalizeTicksPerSecond(double ticksPerSecond) {
    if (ticksPerSecond <= 0.0) {
        return 25.0f;
    }
    return static_cast<float>(ticksPerSecond);
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

template <typename TValue>
void SortKeyframes(AnimationCurve<TValue>& curve) {
    std::sort(
        curve.keyframes.begin(),
        curve.keyframes.end(),
        [](const Keyframe<TValue>& lhs, const Keyframe<TValue>& rhs) {
            return lhs.time < rhs.time;
        });
}

} // namespace

void Animator::Update(float deltaTime, float duration) {
    if (!playing || duration <= 0.0f) {
        return;
    }

    time += deltaTime * speed;
    if (loop) {
        time = std::fmod(time, duration);
        if (time < 0.0f) {
            time += duration;
        }
    } else {
        time = (std::clamp)(time, 0.0f, duration);
    }
}

AnimationClip LoadAnimationFile(
    const std::string& directoryPath,
    const std::string& filename) {
    AnimationClip animation{};

    Assimp::Importer importer;
    const std::string filePath = JoinPath(directoryPath, filename);
    const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
    if (scene == nullptr || scene->mNumAnimations == 0 || scene->mAnimations[0] == nullptr) {
        return animation;
    }

    const aiAnimation* animationAssimp = scene->mAnimations[0];
    const float ticksPerSecond = NormalizeTicksPerSecond(animationAssimp->mTicksPerSecond);
    animation.duration = static_cast<float>(animationAssimp->mDuration) / ticksPerSecond;

    for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
        const aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
        if (nodeAnimationAssimp == nullptr) {
            continue;
        }

        NodeAnimation& nodeAnimation =
            animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
            const aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
            Keyframe<Vector3> key{};
            key.time = static_cast<float>(keyAssimp.mTime) / ticksPerSecond;
            key.value = {
                keyAssimp.mValue.x,
                keyAssimp.mValue.y,
                keyAssimp.mValue.z,
            };
            nodeAnimation.translate.keyframes.push_back(key);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
            const aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
            Keyframe<Quaternion> key{};
            key.time = static_cast<float>(keyAssimp.mTime) / ticksPerSecond;
            key.value = Normalize(Quaternion{
                keyAssimp.mValue.x,
                keyAssimp.mValue.y,
                keyAssimp.mValue.z,
                keyAssimp.mValue.w,
            });
            nodeAnimation.rotate.keyframes.push_back(key);
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
            const aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
            Keyframe<Vector3> key{};
            key.time = static_cast<float>(keyAssimp.mTime) / ticksPerSecond;
            key.value = {
                keyAssimp.mValue.x,
                keyAssimp.mValue.y,
                keyAssimp.mValue.z,
            };
            nodeAnimation.scale.keyframes.push_back(key);
        }

        SortKeyframes(nodeAnimation.translate);
        SortKeyframes(nodeAnimation.rotate);
        SortKeyframes(nodeAnimation.scale);
    }

    return animation;
}

Vector3 CalculateValue(
    const AnimationCurve<Vector3>& curve,
    float time,
    const Vector3& fallback) {
    if (curve.keyframes.empty()) {
        return fallback;
    }
    if (curve.keyframes.size() == 1 || time <= curve.keyframes.front().time) {
        return curve.keyframes.front().value;
    }

    for (size_t index = 0; index + 1 < curve.keyframes.size(); ++index) {
        const Keyframe<Vector3>& current = curve.keyframes[index];
        const Keyframe<Vector3>& next = curve.keyframes[index + 1];
        if (current.time <= time && time <= next.time) {
            const float span = next.time - current.time;
            const float t = span > 0.0f ? (time - current.time) / span : 0.0f;
            return Lerp(current.value, next.value, t);
        }
    }

    return curve.keyframes.back().value;
}

Quaternion CalculateValue(
    const AnimationCurve<Quaternion>& curve,
    float time,
    const Quaternion& fallback) {
    if (curve.keyframes.empty()) {
        return fallback;
    }
    if (curve.keyframes.size() == 1 || time <= curve.keyframes.front().time) {
        return curve.keyframes.front().value;
    }

    for (size_t index = 0; index + 1 < curve.keyframes.size(); ++index) {
        const Keyframe<Quaternion>& current = curve.keyframes[index];
        const Keyframe<Quaternion>& next = curve.keyframes[index + 1];
        if (current.time <= time && time <= next.time) {
            const float span = next.time - current.time;
            const float t = span > 0.0f ? (time - current.time) / span : 0.0f;
            return Slerp(current.value, next.value, t);
        }
    }

    return curve.keyframes.back().value;
}

Matrix4x4 MakeNodeAnimationMatrix(
    const NodeAnimation& nodeAnimation,
    float time) {
    const Vector3 translate = CalculateValue(nodeAnimation.translate, time, {0.0f, 0.0f, 0.0f});
    const Quaternion rotate = CalculateValue(nodeAnimation.rotate, time, {0.0f, 0.0f, 0.0f, 1.0f});
    const Vector3 scale = CalculateValue(nodeAnimation.scale, time, {1.0f, 1.0f, 1.0f});
    return MakeAffineMatrix(scale, rotate, translate);
}
