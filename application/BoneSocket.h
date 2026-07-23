#pragma once

#include <cstdint>
#include <string>

#include "Skeleton.h"

// Bone sockets use the engine's row-vector convention:
// socket local offset * joint skeleton-space pose * owner world transform.
enum class BoneSocketScaleMode : uint8_t {
    // Appropriate for deforming accessories that must inherit squash/stretch.
    InheritJointScale,
    // Appropriate for rigid attachments and imported skeletons whose root
    // scale is only a unit conversion (for example Mixamo's 0.01 Armature).
    RemoveJointScale,
};

struct BoneSocketBinding {
    std::string jointName;
    QuaternionTransform localOffset{
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
    };
    int32_t cachedJointIndex = -1;
    bool enabled = true;
    BoneSocketScaleMode scaleMode = BoneSocketScaleMode::RemoveJointScale;
};

enum class BoneSocketStatus : uint8_t {
    Unresolved,
    Resolved,
    Disabled,
    EmptyJointName,
    JointNotFound,
    SkeletonMismatch,
    InvalidJointTransform,
};

struct BoneSocketPose {
    Matrix4x4 worldMatrix = MakeIdentity4x4();
    Vector3 sourceJointScale{1.0f, 1.0f, 1.0f};
    int32_t jointIndex = -1;
    BoneSocketStatus status = BoneSocketStatus::Unresolved;
    bool jointScaleRemoved = false;

    [[nodiscard]] bool IsValid() const noexcept {
        return status == BoneSocketStatus::Resolved;
    }
};

// Changes the target joint and invalidates the cached index. This should be
// used by authoring and hot-reload code instead of writing jointName directly.
void SetBoneSocketJoint(BoneSocketBinding& binding, std::string jointName);
void InvalidateBoneSocket(BoneSocketBinding& binding) noexcept;

// Resolve validates cached indices against the joint name before reusing them,
// so a socket remains safe when its Skeleton asset is hot-reloaded.
[[nodiscard]] BoneSocketStatus ResolveBoneSocket(
    BoneSocketBinding& binding,
    const Skeleton& skeleton);

// Returns identity with a diagnostic status on failure. A consumer must check
// IsValid() before using the pose; stale transforms are never returned.
[[nodiscard]] BoneSocketPose EvaluateBoneSocket(
    BoneSocketBinding& binding,
    const Skeleton& skeleton,
    const Matrix4x4& ownerWorldMatrix);

[[nodiscard]] const char* BoneSocketStatusName(BoneSocketStatus status) noexcept;
