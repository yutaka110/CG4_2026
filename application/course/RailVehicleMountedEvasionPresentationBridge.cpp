#include "RailVehicleMountedEvasionPresentationBridge.h"

#include <algorithm>
#include <cmath>

namespace {

bool Finite(float value) noexcept { return std::isfinite(value); }

void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

Vector3 Add(Vector3 a, Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(Vector3 value, float amount) noexcept {
    return {value.x * amount, value.y * amount, value.z * amount};
}

RailVehicleOccupantPosePhase PosePhase(
    RailVehicleMountedEvasionPhase phase) noexcept {
    switch (phase) {
    case RailVehicleMountedEvasionPhase::Ready:
        return RailVehicleOccupantPosePhase::MountedIdle;
    case RailVehicleMountedEvasionPhase::Evading:
        return RailVehicleOccupantPosePhase::Evading;
    case RailVehicleMountedEvasionPhase::Recovering:
        return RailVehicleOccupantPosePhase::Recovering;
    case RailVehicleMountedEvasionPhase::Cooldown:
        return RailVehicleOccupantPosePhase::Cooldown;
    }
    return RailVehicleOccupantPosePhase::MountedIdle;
}

} // namespace

bool RailVehicleMountedEvasionPresentationSettings::Validate(
    std::string* errorMessage) const {
    if (!Finite(maximumLateralLeanDegrees) ||
        !Finite(maximumVerticalLeanDegrees) ||
        !Finite(maximumCounterYawDegrees) || !Finite(suspensionFollow) ||
        !Finite(poseResponse) || !Finite(afterimageStrength) ||
        maximumLateralLeanDegrees < 0.0f ||
        maximumLateralLeanDegrees > 60.0f ||
        maximumVerticalLeanDegrees < 0.0f ||
        maximumVerticalLeanDegrees > 45.0f ||
        maximumCounterYawDegrees < 0.0f ||
        maximumCounterYawDegrees > 30.0f || suspensionFollow < 0.0f ||
        suspensionFollow > 1.0f || poseResponse <= 0.0f ||
        poseResponse > 120.0f || afterimageStrength < 0.0f ||
        afterimageStrength > 1.0f) {
        SetError(errorMessage, "Mounted evasion presentation settings are invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleMountedEvasionPresentationBridge::Reset() {
    frame_ = {};
    smoothedPoseWeight_ = 0.0f;
    revision_ = 0;
}

void RailVehicleMountedEvasionPresentationBridge::Update(
    const RailVehicleMountedEvasionPresentationInput& input) {
    frame_ = {};
    if (!input.settings.enabled || !input.gameplayVisible ||
        !input.settings.Validate(nullptr) || input.vehicleState == nullptr ||
        input.vehiclePresentation == nullptr || input.evasion == nullptr ||
        input.combatMount == nullptr || !input.vehicleState->initialized ||
        !input.vehiclePresentation->visible || !input.evasion->mounted ||
        !input.combatMount->valid ||
        input.combatMount->sourceVehicleRevision !=
            input.vehicleState->revision) {
        smoothedPoseWeight_ = 0.0f;
        return;
    }

    const float dt = Finite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    const float targetWeight = input.evasion->normalizedStrength;
    const float response = dt > 0.0f
        ? 1.0f - std::exp(-input.settings.poseResponse * dt)
        : 1.0f;
    smoothedPoseWeight_ +=
        (targetWeight - smoothedPoseWeight_) * response;
    smoothedPoseWeight_ = (std::clamp)(smoothedPoseWeight_, 0.0f, 1.0f);

    const RailVehicleRuntimeState& vehicle = *input.vehicleState;
    const float directionX = input.evasion->state.directionX;
    const float directionY = input.evasion->state.directionY;
    const float suspension = input.vehiclePresentation->suspensionOffset *
        input.settings.suspensionFollow;

    frame_.visible = true;
    frame_.posePhase = PosePhase(input.evasion->state.phase);
    frame_.position = Add(
        input.combatMount->playerWorldPosition,
        Scale(vehicle.up, suspension));
    frame_.weaponWorldPosition = Add(
        input.combatMount->weaponWorldPosition,
        Scale(vehicle.up, suspension));
    frame_.damageVfxWorldPosition = input.combatMount->damageVfxWorldPosition;
    frame_.forward = vehicle.forward;
    frame_.right = vehicle.right;
    frame_.up = vehicle.up;
    frame_.poseWeight = smoothedPoseWeight_;
    frame_.lateralLeanDegrees =
        -directionX * input.settings.maximumLateralLeanDegrees *
        smoothedPoseWeight_;
    frame_.verticalLeanDegrees =
        -directionY * input.settings.maximumVerticalLeanDegrees *
        smoothedPoseWeight_;
    frame_.counterYawDegrees =
        -directionX * input.settings.maximumCounterYawDegrees *
        smoothedPoseWeight_;
    frame_.afterimageAlpha = input.evasion->active
        ? input.settings.afterimageStrength * smoothedPoseWeight_
        : 0.0f;
    frame_.invulnerable = input.evasion->invulnerable;
    frame_.sourceVehicleRevision = vehicle.revision;
    frame_.sourceEvasionRevision = input.evasion->state.revision;
    frame_.sourceCombatMountRevision = input.combatMount->revision;
    frame_.revision = ++revision_;
}

const char* ToString(RailVehicleOccupantPosePhase phase) noexcept {
    switch (phase) {
    case RailVehicleOccupantPosePhase::MountedIdle: return "MountedIdle";
    case RailVehicleOccupantPosePhase::Evading: return "Evading";
    case RailVehicleOccupantPosePhase::Recovering: return "Recovering";
    case RailVehicleOccupantPosePhase::Cooldown: return "Cooldown";
    }
    return "Unknown";
}
