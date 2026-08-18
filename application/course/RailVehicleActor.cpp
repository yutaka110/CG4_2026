#include "RailVehicleActor.h"

#include <algorithm>
#include <cmath>

namespace {

void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

bool FiniteVector(Vector3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

} // namespace

bool RailVehicleActorDefinition::Validate(std::string* errorMessage) const {
    if (actorId == 0) {
        SetError(errorMessage, "RailVehicleActorDefinition.actorId must be non-zero.");
        return false;
    }
    if (meshId.empty() || fallbackMeshId.empty()) {
        SetError(errorMessage, "Rail vehicle actor mesh bindings must not be empty.");
        return false;
    }
    if (!FiniteVector(visualScale) || visualScale.x <= 0.0f ||
        visualScale.y <= 0.0f || visualScale.z <= 0.0f) {
        SetError(errorMessage, "Rail vehicle actor visual scale is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailVehicleActor::RailVehicleActor() {
    (void)Initialize({}, nullptr);
}

bool RailVehicleActor::Initialize(
    const RailVehicleActorDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleActor::Reset() {
    frame_ = {};
    revision_ = 0;
}

void RailVehicleActor::Update(const RailVehicleActorInput& input) {
    frame_ = {};
    if (input.vehicleDefinition == nullptr || input.vehicleState == nullptr ||
        input.presentation == nullptr || !input.vehicleState->initialized ||
        !input.presentation->visible ||
        input.vehicleState->vehicleId != input.vehicleDefinition->vehicleId) {
        return;
    }

    const RailVehicleDefinition& vehicleDefinition = *input.vehicleDefinition;
    const RailVehicleRuntimeState& state = *input.vehicleState;
    const RailVehiclePresentationFrame& presentation = *input.presentation;
    frame_.active = true;
    frame_.visible = definition_.visible;
    frame_.actorId = definition_.actorId;
    frame_.vehicleId = state.vehicleId;
    frame_.meshId = definition_.meshId;
    frame_.fallbackMeshId = definition_.fallbackMeshId;
    frame_.visualScale = definition_.visualScale;
    frame_.position = presentation.visualPosition;
    frame_.forward = presentation.forward;
    frame_.up = presentation.up;
    frame_.right = presentation.right;
    frame_.playerMountPosition = state.playerMountPosition;
    frame_.weaponMountPosition = state.weaponMountPosition;
    frame_.cameraMountPosition = state.cameraMountPosition;
    frame_.damageVfxMountPosition = state.damageVfxMountPosition;
    frame_.visualBankDegrees = presentation.visualBankDegrees;
    frame_.visualPitchDegrees = presentation.visualPitchDegrees;
    frame_.wheelRotationRadians = presentation.wheelRotationRadians;
    frame_.healthNormalized = (std::clamp)(
        state.hitPoints / (std::max)(0.001f, vehicleDefinition.maximumHitPoints),
        0.0f,
        1.0f);
    frame_.speedNormalized = presentation.speedNormalized;
    frame_.brakeSparksActive = presentation.brakeSparksActive;
    frame_.sourceVehicleRevision = state.revision;
    frame_.sourcePresentationRevision = presentation.revision;
    frame_.revision = ++revision_;
}
