#include "RailVehicleCameraMountBridge.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDegreesToRadians = 0.01745329251994329577f;

bool Finite(float value) noexcept { return std::isfinite(value); }

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

Vector3 Add(Vector3 lhs, Vector3 rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vector3 Scale(Vector3 value, float amount) noexcept {
    return {value.x * amount, value.y * amount, value.z * amount};
}

} // namespace

RailVehicleCameraMountDefinition
RailVehicleCameraMountDefinition::MineCartDefaults() {
    return {};
}

bool RailVehicleCameraMountDefinition::Validate(
    std::string* errorMessage) const {
    const bool valid = Finite(anchorBlend) &&
        Finite(maximumAnchorCorrection) && Finite(evasionLateralFollow) &&
        Finite(evasionVerticalFollow) && Finite(targetEvasionFollow) &&
        Finite(maximumEvasionRollDegrees) && anchorBlend >= 0.0f &&
        anchorBlend <= 1.0f && maximumAnchorCorrection >= 0.0f &&
        evasionLateralFollow >= 0.0f && evasionLateralFollow <= 1.0f &&
        evasionVerticalFollow >= 0.0f && evasionVerticalFollow <= 1.0f &&
        targetEvasionFollow >= 0.0f && targetEvasionFollow <= 1.0f &&
        maximumEvasionRollDegrees >= 0.0f &&
        maximumEvasionRollDegrees <= 30.0f;
    if (!valid) {
        SetError(errorMessage, "Vehicle camera mount definition is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailVehicleCameraMountBridge::RailVehicleCameraMountBridge() {
    (void)Initialize(RailVehicleCameraMountDefinition::MineCartDefaults());
}

bool RailVehicleCameraMountBridge::Initialize(
    const RailVehicleCameraMountDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    initialized_ = true;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleCameraMountBridge::Reset() {
    frame_ = {};
    revision_ = 0;
}

void RailVehicleCameraMountBridge::Update(
    const RailVehicleCameraMountInput& input) {
    frame_ = {};
    if (!initialized_ || !definition_.enabled || !input.gameplayActive ||
        input.vehicleDefinition == nullptr || input.vehicleState == nullptr ||
        !input.vehicleState->initialized ||
        input.vehicleDefinition->mountedMovementMode !=
            RailVehicleMountedMovementMode::VehicleMounted) {
        return;
    }
    const RailVehicleRuntimeState& vehicle = *input.vehicleState;
    const bool evasionValid = input.evasion != nullptr &&
        input.evasion->mounted &&
        input.evasion->sourceVehicleRevision == vehicle.revision;
    const float x = evasionValid ? input.evasion->state.lateralOffset : 0.0f;
    const float y = evasionValid ? input.evasion->state.verticalOffset : 0.0f;

    frame_.active = true;
    frame_.anchorWorldPosition = vehicle.cameraMountPosition;
    frame_.cameraOffset = Add(
        Scale(vehicle.right, x * definition_.evasionLateralFollow),
        Scale(vehicle.up, y * definition_.evasionVerticalFollow));
    frame_.targetOffset = Add(
        Scale(vehicle.right, x * definition_.targetEvasionFollow),
        Scale(vehicle.up, y * definition_.targetEvasionFollow));
    frame_.anchorBlend = definition_.anchorBlend;
    frame_.maximumAnchorCorrection = definition_.maximumAnchorCorrection;
    frame_.rollOffsetRadians = evasionValid
        ? -input.evasion->bankNormalized *
            definition_.maximumEvasionRollDegrees * kDegreesToRadians
        : 0.0f;
    frame_.sourceVehicleRevision = vehicle.revision;
    frame_.sourceEvasionRevision = evasionValid
        ? input.evasion->state.revision : 0;
    frame_.revision = ++revision_;
}
