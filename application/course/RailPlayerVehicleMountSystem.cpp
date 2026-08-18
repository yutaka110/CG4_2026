#include "RailPlayerVehicleMountSystem.h"

namespace {

Vector3 Add(Vector3 lhs, Vector3 rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vector3 Scale(Vector3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

} // namespace

void RailPlayerVehicleMountSystem::Reset() {
    state_ = {};
    frame_ = {};
}

const RailPlayerVehicleMountFrame& RailPlayerVehicleMountSystem::Update(
    const RailPlayerVehicleMountInput& input) {
    frame_ = {};
    if (input.vehicleDefinition == nullptr || input.vehicleState == nullptr ||
        input.playerMovement == nullptr || !input.vehicleState->initialized ||
        !input.playerMovement->IsInitialized()) {
        state_.active = false;
        state_.occupantMounted = false;
        state_.playerMovementSuppressed = false;
        ++state_.frameIndex;
        ++state_.revision;
        frame_.state = state_;
        return frame_;
    }

    const RailVehicleDefinition& definition = *input.vehicleDefinition;
    const RailVehicleRuntimeState& vehicle = *input.vehicleState;
    const bool suppressMovement = input.occupantMounted &&
        definition.mountedMovementMode ==
            RailVehicleMountedMovementMode::VehicleMounted;
    const bool ownershipChanged =
        !state_.active || state_.occupantMounted != input.occupantMounted ||
        state_.movementMode != definition.mountedMovementMode ||
        state_.playerMovementSuppressed != suppressMovement;

    state_.active = true;
    state_.occupantMounted = input.occupantMounted;
    state_.movementMode = definition.mountedMovementMode;
    state_.playerMovementSuppressed = suppressMovement;
    if (ownershipChanged) ++state_.transitionIndex;

    if (!suppressMovement) {
        frame_.playerMovementFrame =
            input.playerMovement->Update(input.movementInput);
        frame_.playerMovementUpdated = true;
        const RailPlayerMovementRuntimeState& player =
            frame_.playerMovementFrame.state;
        frame_.railLateralOffset = definition.mounts.player.x +
            player.lateralOffset;
        frame_.railVerticalOffset = definition.bodyVerticalOffset +
            definition.mounts.player.y + player.verticalOffset;
        frame_.playerWorldPosition = vehicle.playerMountPosition;
        frame_.playerWorldPosition = Add(
            frame_.playerWorldPosition,
            Scale(vehicle.right, player.lateralOffset));
        frame_.playerWorldPosition = Add(
            frame_.playerWorldPosition,
            Scale(vehicle.up, player.verticalOffset));
    } else {
        // Do not call RailPlayerMovementSystem::Update here. This preserves the
        // parked free-flight state for a future unmount while guaranteeing that
        // no input, dodge displacement or stale velocity can move the occupant.
        frame_.playerMovementFrame = input.playerMovement->Frame();
        frame_.railLateralOffset = definition.mounts.player.x;
        frame_.railVerticalOffset = definition.bodyVerticalOffset +
            definition.mounts.player.y;
        frame_.playerWorldPosition = vehicle.playerMountPosition;
    }

    ++state_.frameIndex;
    ++state_.revision;
    frame_.ownershipChangedThisFrame = ownershipChanged;
    frame_.sourceVehicleRevision = vehicle.revision;
    frame_.sourcePlayerMovementRevision =
        input.playerMovement->State().revision;
    frame_.state = state_;
    return frame_;
}

