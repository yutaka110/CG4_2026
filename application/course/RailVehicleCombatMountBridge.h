#pragma once

#include <cstdint>

#include "RailVehicleMountedEvasionSystem.h"

struct RailVehicleCombatMountSettings final {
    bool enabled = true;
    bool weaponFollowsOccupantEvasion = true;
    bool damageVfxFollowsOccupantEvasion = true;
};

struct RailVehicleCombatMountInput final {
    const RailVehicleDefinition* vehicleDefinition = nullptr;
    const RailVehicleRuntimeState* vehicleState = nullptr;
    const RailVehicleMountedEvasionFrame* evasion = nullptr;
    RailVehicleCombatMountSettings settings{};
};

// Immutable combat-space view of the vehicle mounts. Collision, weapon
// presentation and damage VFX consume this single frame instead of rebuilding
// slightly different rail-local transforms.
struct RailVehicleCombatMountFrame final {
    bool valid = false;
    float playerDistance = 0.0f;
    float playerLateralOffset = 0.0f;
    float playerVerticalOffset = 0.0f;
    float weaponDistance = 0.0f;
    float weaponLateralOffset = 0.0f;
    float weaponVerticalOffset = 0.0f;
    Vector3 playerWorldPosition{};
    Vector3 weaponWorldPosition{};
    Vector3 damageVfxWorldPosition{};
    Vector3 forward{0.0f, 0.0f, 1.0f};
    Vector3 right{1.0f, 0.0f, 0.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
    bool evasionActive = false;
    bool invulnerable = false;
    uint64_t sourceVehicleRevision = 0;
    uint64_t sourceEvasionRevision = 0;
    uint64_t revision = 0;
};

class RailVehicleCombatMountBridge final {
public:
    void Reset();
    void Update(const RailVehicleCombatMountInput& input);

    const RailVehicleCombatMountFrame& Frame() const noexcept { return frame_; }

private:
    RailVehicleCombatMountFrame frame_{};
    uint64_t revision_ = 0;
};
