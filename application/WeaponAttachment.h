#pragma once

#include <cstdint>
#include <string>

#include "BoneSocket.h"

struct WeaponAttachmentSettings {
    bool enabled = false;
    std::string jointName = "mixamorig:RightHand";
    QuaternionTransform socketOffset{
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
    };
    BoneSocketScaleMode scaleMode = BoneSocketScaleMode::RemoveJointScale;
};

enum class WeaponAttachmentStatus : uint8_t {
    Disabled,
    MissingSkeleton,
    SocketUnavailable,
    InvalidWorldTransform,
    ModelUnavailable,
    Active,
};

struct WeaponAttachmentTelemetry {
    WeaponAttachmentStatus status = WeaponAttachmentStatus::Disabled;
    BoneSocketStatus socketStatus = BoneSocketStatus::Unresolved;
    uint32_t modelIndex = UINT32_MAX;
    Matrix4x4 worldMatrix = MakeIdentity4x4();
    Vector3 worldPosition{};
    Vector3 sourceJointScale{1.0f, 1.0f, 1.0f};
    Vector3 worldScale{1.0f, 1.0f, 1.0f};
    bool jointScaleRemoved = false;
};

// Evaluates a reusable Bone Socket binding for a weapon. GPU resources remain
// owned by the scene; this service only publishes the validated attachment pose.
class WeaponAttachment {
public:
    void Update(
        const WeaponAttachmentSettings& settings,
        const Skeleton* skeleton,
        const Matrix4x4& ownerWorldMatrix,
        uint32_t modelIndex,
        bool modelAvailable);
    void Reset() noexcept;

    [[nodiscard]] const WeaponAttachmentTelemetry& Telemetry() const noexcept {
        return telemetry_;
    }
    [[nodiscard]] const BoneSocketBinding& SocketBinding() const noexcept {
        return socketBinding_;
    }

private:
    BoneSocketBinding socketBinding_{};
    WeaponAttachmentTelemetry telemetry_{};
};

[[nodiscard]] const char* WeaponAttachmentStatusName(
    WeaponAttachmentStatus status) noexcept;
