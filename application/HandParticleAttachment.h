#pragma once

#include <cstdint>
#include <string>

#include "BoneSocket.h"

class EffectRuntime;

struct HandParticleAttachmentSettings {
    bool enabled = false;
    std::string jointName = "mixamorig:RightHand";
    QuaternionTransform socketOffset{
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
    };
    BoneSocketScaleMode scaleMode = BoneSocketScaleMode::RemoveJointScale;
    std::string effectName = "hand_socket_particle";
    Vector4 color = {0.35f, 0.85f, 1.0f, 1.0f};
    Vector3 effectScale = {1.0f, 1.0f, 1.0f};
};

enum class HandParticleAttachmentStatus : uint8_t {
    Disabled,
    MissingSkeleton,
    SocketUnavailable,
    InvalidWorldTransform,
    EffectUnavailable,
    Active,
};

struct HandParticleAttachmentTelemetry {
    HandParticleAttachmentStatus status = HandParticleAttachmentStatus::Disabled;
    BoneSocketStatus socketStatus = BoneSocketStatus::Unresolved;
    uint32_t effectInstanceId = 0;
    Vector3 worldPosition{};
    Vector3 sourceJointScale{1.0f, 1.0f, 1.0f};
    bool jointScaleRemoved = false;
};

class HandParticleAttachment {
public:
    void Update(
        const HandParticleAttachmentSettings& settings,
        const Skeleton* skeleton,
        const Matrix4x4& ownerWorldMatrix,
        EffectRuntime& effectRuntime);
    void Stop(EffectRuntime& effectRuntime);

    [[nodiscard]] const HandParticleAttachmentTelemetry& Telemetry() const noexcept {
        return telemetry_;
    }
    [[nodiscard]] const BoneSocketBinding& SocketBinding() const noexcept {
        return socketBinding_;
    }

private:
    void StopInstance(EffectRuntime& effectRuntime);

    BoneSocketBinding socketBinding_{};
    HandParticleAttachmentTelemetry telemetry_{};
    uint32_t effectInstanceId_ = 0;
    std::string activeEffectName_;
};

[[nodiscard]] const char* HandParticleAttachmentStatusName(
    HandParticleAttachmentStatus status) noexcept;
