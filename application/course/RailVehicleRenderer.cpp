#include "RailVehicleRenderer.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDegreesToRadians = 0.01745329251994329577f;

float Distance(Vector3 a, Vector3 b) noexcept {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

void RailVehicleRenderer::Reset() {
    frame_ = {};
    revision_ = 0;
}

void RailVehicleRenderer::Update(const RailVehicleRenderInput& input) {
    frame_ = {};
    if (!input.settings.enabled || input.actor == nullptr ||
        !input.actor->active || !input.actor->visible) {
        return;
    }

    const RailVehicleActorFrame& actor = *input.actor;
    const float drawDistance = (std::max)(1.0f, input.settings.maximumDrawDistance);
    const float cameraDistance = Distance(actor.position, input.cameraWorldPosition);
    if (!std::isfinite(cameraDistance) || cameraDistance > drawDistance) return;

    const float yaw = std::atan2(actor.forward.x, actor.forward.z);
    const float railPitch = std::asin((std::clamp)(
        -actor.forward.y, -1.0f, 1.0f));
    const Vector3 rotation{
        railPitch + actor.visualPitchDegrees * kDegreesToRadians,
        yaw + actor.visualYawDegrees * kDegreesToRadians,
        -actor.visualBankDegrees * kDegreesToRadians,
    };

    frame_.visible = true;
    frame_.actorId = actor.actorId;
    frame_.meshId = actor.meshId;
    frame_.fallbackMeshId = actor.fallbackMeshId;
    frame_.worldMatrix = MakeAffineMatrix(
        actor.visualScale,
        rotation,
        actor.position);
    frame_.distanceFromCamera = cameraDistance;
    frame_.wheelRotationRadians = actor.wheelRotationRadians;
    frame_.healthNormalized = actor.healthNormalized;
    frame_.sourceActorRevision = actor.revision;
    frame_.revision = ++revision_;
}
