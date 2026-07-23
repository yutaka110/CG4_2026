#include "WeaponAttachment.h"

#include <cmath>

namespace {

bool IsFinite(const Matrix4x4& matrix) {
    for (const auto& row : matrix.m) {
        for (const float value : row) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
    }
    return true;
}

Vector3 ExtractWorldPosition(const Matrix4x4& matrix) {
    return {matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]};
}

float BasisLength(float x, float y, float z) {
    return std::sqrt(x * x + y * y + z * z);
}

Vector3 ExtractWorldScale(const Matrix4x4& matrix) {
    return {
        BasisLength(matrix.m[0][0], matrix.m[0][1], matrix.m[0][2]),
        BasisLength(matrix.m[1][0], matrix.m[1][1], matrix.m[1][2]),
        BasisLength(matrix.m[2][0], matrix.m[2][1], matrix.m[2][2]),
    };
}

} // namespace

void WeaponAttachment::Update(
    const WeaponAttachmentSettings& settings,
    const Skeleton* skeleton,
    const Matrix4x4& ownerWorldMatrix,
    uint32_t modelIndex,
    bool modelAvailable) {
    telemetry_ = {};
    telemetry_.modelIndex = modelIndex;
    if (!settings.enabled) {
        telemetry_.status = WeaponAttachmentStatus::Disabled;
        return;
    }
    if (skeleton == nullptr) {
        telemetry_.status = WeaponAttachmentStatus::MissingSkeleton;
        return;
    }

    if (socketBinding_.jointName != settings.jointName) {
        SetBoneSocketJoint(socketBinding_, settings.jointName);
    }
    socketBinding_.enabled = true;
    socketBinding_.scaleMode = settings.scaleMode;
    socketBinding_.localOffset = settings.socketOffset;
    socketBinding_.localOffset.rotate = Normalize(
        socketBinding_.localOffset.rotate);

    const BoneSocketPose socketPose = EvaluateBoneSocket(
        socketBinding_,
        *skeleton,
        ownerWorldMatrix);
    telemetry_.socketStatus = socketPose.status;
    telemetry_.sourceJointScale = socketPose.sourceJointScale;
    telemetry_.jointScaleRemoved = socketPose.jointScaleRemoved;
    if (!socketPose.IsValid()) {
        telemetry_.status = WeaponAttachmentStatus::SocketUnavailable;
        return;
    }
    if (!IsFinite(socketPose.worldMatrix)) {
        telemetry_.status = WeaponAttachmentStatus::InvalidWorldTransform;
        return;
    }
    if (!modelAvailable || modelIndex == UINT32_MAX) {
        telemetry_.status = WeaponAttachmentStatus::ModelUnavailable;
        return;
    }

    telemetry_.worldMatrix = socketPose.worldMatrix;
    telemetry_.worldPosition = ExtractWorldPosition(socketPose.worldMatrix);
    telemetry_.worldScale = ExtractWorldScale(socketPose.worldMatrix);
    telemetry_.status = WeaponAttachmentStatus::Active;
}

void WeaponAttachment::Reset() noexcept {
    socketBinding_ = {};
    telemetry_ = {};
}

const char* WeaponAttachmentStatusName(
    WeaponAttachmentStatus status) noexcept {
    switch (status) {
    case WeaponAttachmentStatus::Disabled:
        return "disabled";
    case WeaponAttachmentStatus::MissingSkeleton:
        return "missing_skeleton";
    case WeaponAttachmentStatus::SocketUnavailable:
        return "socket_unavailable";
    case WeaponAttachmentStatus::InvalidWorldTransform:
        return "invalid_world_transform";
    case WeaponAttachmentStatus::ModelUnavailable:
        return "model_unavailable";
    case WeaponAttachmentStatus::Active:
        return "active";
    }
    return "unknown";
}
