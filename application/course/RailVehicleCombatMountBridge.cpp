#include "RailVehicleCombatMountBridge.h"

namespace {

Vector3 Add(Vector3 lhs, Vector3 rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vector3 Scale(Vector3 value, float amount) noexcept {
    return {value.x * amount, value.y * amount, value.z * amount};
}

Vector3 Offset(Vector3 base, Vector3 right, Vector3 up, float x, float y) {
    return Add(Add(base, Scale(right, x)), Scale(up, y));
}

} // namespace

void RailVehicleCombatMountBridge::Reset() {
    frame_ = {};
    revision_ = 0;
}

void RailVehicleCombatMountBridge::Update(
    const RailVehicleCombatMountInput& input) {
    frame_ = {};
    if (!input.settings.enabled || input.vehicleDefinition == nullptr ||
        input.vehicleState == nullptr || !input.vehicleState->initialized ||
        input.vehicleDefinition->mountedMovementMode !=
            RailVehicleMountedMovementMode::VehicleMounted) {
        return;
    }
    const RailVehicleDefinition& definition = *input.vehicleDefinition;
    const RailVehicleRuntimeState& vehicle = *input.vehicleState;
    const bool evasionValid = input.evasion != nullptr &&
        input.evasion->mounted &&
        input.evasion->sourceVehicleRevision == vehicle.revision;
    const float evasionX = evasionValid
        ? input.evasion->state.lateralOffset : 0.0f;
    const float evasionY = evasionValid
        ? input.evasion->state.verticalOffset : 0.0f;
    const float weaponX = input.settings.weaponFollowsOccupantEvasion
        ? evasionX : 0.0f;
    const float weaponY = input.settings.weaponFollowsOccupantEvasion
        ? evasionY : 0.0f;
    const float damageX = input.settings.damageVfxFollowsOccupantEvasion
        ? evasionX : 0.0f;
    const float damageY = input.settings.damageVfxFollowsOccupantEvasion
        ? evasionY : 0.0f;

    frame_.valid = true;
    frame_.playerDistance = vehicle.distance + definition.mounts.player.z;
    frame_.playerLateralOffset = definition.mounts.player.x + evasionX;
    frame_.playerVerticalOffset = definition.bodyVerticalOffset +
        definition.mounts.player.y + evasionY;
    frame_.weaponDistance = vehicle.distance + definition.mounts.weapon.z;
    frame_.weaponLateralOffset = definition.mounts.weapon.x + weaponX;
    frame_.weaponVerticalOffset = definition.bodyVerticalOffset +
        definition.mounts.weapon.y + weaponY;
    frame_.playerWorldPosition = Offset(
        vehicle.playerMountPosition, vehicle.right, vehicle.up,
        evasionX, evasionY);
    frame_.weaponWorldPosition = Offset(
        vehicle.weaponMountPosition, vehicle.right, vehicle.up,
        weaponX, weaponY);
    frame_.damageVfxWorldPosition = Offset(
        vehicle.damageVfxMountPosition, vehicle.right, vehicle.up,
        damageX, damageY);
    frame_.forward = vehicle.forward;
    frame_.right = vehicle.right;
    frame_.up = vehicle.up;
    frame_.evasionActive = evasionValid && input.evasion->active;
    frame_.invulnerable = evasionValid && input.evasion->invulnerable;
    frame_.sourceVehicleRevision = vehicle.revision;
    frame_.sourceEvasionRevision = evasionValid
        ? input.evasion->state.revision : 0;
    frame_.revision = ++revision_;
}
