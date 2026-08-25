#include "RailVehicleOccupantActor.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDegreesToRadians = 0.01745329251994329577f;

void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

bool FiniteVector(Vector3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

float Distance(Vector3 a, Vector3 b) noexcept {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return std::sqrt(x * x + y * y + z * z);
}

Vector3 Add(Vector3 a, Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(Vector3 value, float amount) noexcept {
    return {value.x * amount, value.y * amount, value.z * amount};
}

} // namespace

bool RailVehicleOccupantActorDefinition::Validate(
    std::string* errorMessage) const {
    if (actorId == 0 || meshId.empty() || fallbackMeshId.empty() ||
        !FiniteVector(visualScale) || !std::isfinite(visualHeightOffset) ||
        !std::isfinite(maximumDrawDistance) || visualScale.x <= 0.0f ||
        visualScale.y <= 0.0f || visualScale.z <= 0.0f ||
        maximumDrawDistance <= 0.0f) {
        SetError(errorMessage, "Rail vehicle occupant actor definition is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailVehicleOccupantActor::RailVehicleOccupantActor() {
    (void)Initialize({}, nullptr);
}

bool RailVehicleOccupantActor::Initialize(
    const RailVehicleOccupantActorDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleOccupantActor::Reset() {
    frame_ = {};
    revision_ = 0;
}

void RailVehicleOccupantActor::Update(
    const RailVehicleOccupantActorInput& input) {
    frame_ = {};
    if (input.presentation == nullptr || !input.presentation->visible ||
        !definition_.visible) {
        return;
    }
    const RailVehicleMountedEvasionPresentationFrame& presentation =
        *input.presentation;
    const Vector3 position = Add(
        presentation.position,
        Scale(presentation.up, definition_.visualHeightOffset));
    const float cameraDistance = Distance(position, input.cameraWorldPosition);
    if (!std::isfinite(cameraDistance) ||
        cameraDistance > definition_.maximumDrawDistance) {
        return;
    }

    const float yaw = std::atan2(presentation.forward.x, presentation.forward.z);
    const float railPitch = std::asin((std::clamp)(
        -presentation.forward.y, -1.0f, 1.0f));
    const Vector3 rotation{
        railPitch + presentation.verticalLeanDegrees * kDegreesToRadians,
        yaw + presentation.counterYawDegrees * kDegreesToRadians,
        presentation.lateralLeanDegrees * kDegreesToRadians};
    const float crouch = 1.0f - presentation.poseWeight * 0.08f;
    const Vector3 scale{
        definition_.visualScale.x * (1.0f + presentation.poseWeight * 0.05f),
        definition_.visualScale.y * crouch,
        definition_.visualScale.z * (1.0f + presentation.poseWeight * 0.04f)};

    frame_.active = true;
    frame_.visible = true;
    frame_.actorId = definition_.actorId;
    frame_.meshId = definition_.meshId;
    frame_.fallbackMeshId = definition_.fallbackMeshId;
    frame_.worldMatrix = MakeAffineMatrix(scale, rotation, position);
    frame_.position = position;
    frame_.posePhase = presentation.posePhase;
    frame_.poseWeight = presentation.poseWeight;
    frame_.afterimageAlpha = presentation.afterimageAlpha;
    frame_.distanceFromCamera = cameraDistance;
    frame_.invulnerable = presentation.invulnerable;
    frame_.sourcePresentationRevision = presentation.revision;
    frame_.revision = ++revision_;
}
