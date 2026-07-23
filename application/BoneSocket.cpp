#include "BoneSocket.h"

#include <cmath>
#include <cstddef>
#include <utility>

namespace {

bool IsMatchingJoint(
    const Skeleton& skeleton,
    int32_t jointIndex,
    const std::string& jointName) {
    if (jointIndex < 0 ||
        static_cast<size_t>(jointIndex) >= skeleton.joints.size()) {
        return false;
    }
    return skeleton.joints[static_cast<size_t>(jointIndex)].name == jointName;
}

float Dot(const Vector3& lhs, const Vector3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vector3 Scale(const Vector3& value, float scalar) {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float Length(const Vector3& value) {
    return std::sqrt(Dot(value, value));
}

bool NormalizeChecked(Vector3& value) {
    constexpr float kMinimumBasisLength = 1.0e-7f;
    const float length = Length(value);
    if (!std::isfinite(length) || length <= kMinimumBasisLength) {
        return false;
    }
    value = Scale(value, 1.0f / length);
    return true;
}

Vector3 ExtractBasisScale(const Matrix4x4& matrix) {
    return {
        Length({matrix.m[0][0], matrix.m[0][1], matrix.m[0][2]}),
        Length({matrix.m[1][0], matrix.m[1][1], matrix.m[1][2]}),
        Length({matrix.m[2][0], matrix.m[2][1], matrix.m[2][2]}),
    };
}

bool TryRemoveScalePreserveTranslation(
    const Matrix4x4& source,
    Matrix4x4& normalized) {
    Vector3 x{source.m[0][0], source.m[0][1], source.m[0][2]};
    Vector3 y{source.m[1][0], source.m[1][1], source.m[1][2]};
    const Vector3 sourceZ{source.m[2][0], source.m[2][1], source.m[2][2]};
    if (!NormalizeChecked(x)) {
        return false;
    }

    // Gram-Schmidt also removes shear accumulated through non-uniform parent
    // scales while preserving the animated X/Y orientation.
    y = Subtract(y, Scale(x, Dot(y, x)));
    if (!NormalizeChecked(y)) {
        return false;
    }
    Vector3 sourceZDirection = sourceZ;
    Vector3 z = Cross(x, y);
    if (!NormalizeChecked(z) || !NormalizeChecked(sourceZDirection)) {
        return false;
    }
    // Preserve mirrored authoring transforms instead of silently changing
    // their handedness.
    if (Dot(z, sourceZDirection) < 0.0f) {
        z = Scale(z, -1.0f);
    }

    normalized = MakeIdentity4x4();
    normalized.m[0][0] = x.x;
    normalized.m[0][1] = x.y;
    normalized.m[0][2] = x.z;
    normalized.m[1][0] = y.x;
    normalized.m[1][1] = y.y;
    normalized.m[1][2] = y.z;
    normalized.m[2][0] = z.x;
    normalized.m[2][1] = z.y;
    normalized.m[2][2] = z.z;
    normalized.m[3][0] = source.m[3][0];
    normalized.m[3][1] = source.m[3][1];
    normalized.m[3][2] = source.m[3][2];
    return std::isfinite(normalized.m[3][0]) &&
        std::isfinite(normalized.m[3][1]) &&
        std::isfinite(normalized.m[3][2]);
}

} // namespace

void SetBoneSocketJoint(BoneSocketBinding& binding, std::string jointName) {
    binding.jointName = std::move(jointName);
    InvalidateBoneSocket(binding);
}

void InvalidateBoneSocket(BoneSocketBinding& binding) noexcept {
    binding.cachedJointIndex = -1;
}

BoneSocketStatus ResolveBoneSocket(
    BoneSocketBinding& binding,
    const Skeleton& skeleton) {
    if (!binding.enabled) {
        return BoneSocketStatus::Disabled;
    }
    if (binding.jointName.empty()) {
        InvalidateBoneSocket(binding);
        return BoneSocketStatus::EmptyJointName;
    }
    if (IsMatchingJoint(
            skeleton,
            binding.cachedJointIndex,
            binding.jointName)) {
        return BoneSocketStatus::Resolved;
    }

    InvalidateBoneSocket(binding);
    const auto found = skeleton.jointMap.find(binding.jointName);
    if (found == skeleton.jointMap.end()) {
        return BoneSocketStatus::JointNotFound;
    }
    if (!IsMatchingJoint(skeleton, found->second, binding.jointName)) {
        return BoneSocketStatus::SkeletonMismatch;
    }

    binding.cachedJointIndex = found->second;
    return BoneSocketStatus::Resolved;
}

BoneSocketPose EvaluateBoneSocket(
    BoneSocketBinding& binding,
    const Skeleton& skeleton,
    const Matrix4x4& ownerWorldMatrix) {
    BoneSocketPose pose{};
    pose.status = ResolveBoneSocket(binding, skeleton);
    if (pose.status != BoneSocketStatus::Resolved) {
        return pose;
    }

    pose.jointIndex = binding.cachedJointIndex;
    const Matrix4x4 localOffsetMatrix = MakeAffineMatrix(
        binding.localOffset.scale,
        binding.localOffset.rotate,
        binding.localOffset.translate);
    const Matrix4x4& jointMatrix =
        skeleton.joints[static_cast<size_t>(pose.jointIndex)].skeletonSpaceMatrix;
    pose.sourceJointScale = ExtractBasisScale(jointMatrix);
    Matrix4x4 socketJointMatrix = jointMatrix;
    if (binding.scaleMode == BoneSocketScaleMode::RemoveJointScale) {
        if (!TryRemoveScalePreserveTranslation(jointMatrix, socketJointMatrix)) {
            pose.status = BoneSocketStatus::InvalidJointTransform;
            pose.jointIndex = -1;
            return pose;
        }
        pose.jointScaleRemoved = true;
    }
    pose.worldMatrix = Multiply(
        Multiply(localOffsetMatrix, socketJointMatrix),
        ownerWorldMatrix);
    return pose;
}

const char* BoneSocketStatusName(BoneSocketStatus status) noexcept {
    switch (status) {
    case BoneSocketStatus::Unresolved:
        return "unresolved";
    case BoneSocketStatus::Resolved:
        return "resolved";
    case BoneSocketStatus::Disabled:
        return "disabled";
    case BoneSocketStatus::EmptyJointName:
        return "empty_joint_name";
    case BoneSocketStatus::JointNotFound:
        return "joint_not_found";
    case BoneSocketStatus::SkeletonMismatch:
        return "skeleton_mismatch";
    case BoneSocketStatus::InvalidJointTransform:
        return "invalid_joint_transform";
    }
    return "unknown";
}
