#pragma once

#include <cstdint>
#include <string>

#include "RailVehicleCombatMountBridge.h"
#include "RailVehiclePresentationBridge.h"

enum class RailVehicleOccupantPosePhase : uint8_t {
    MountedIdle,
    Evading,
    Recovering,
    Cooldown,
};

struct RailVehicleMountedEvasionPresentationSettings final {
    bool enabled = true;
    float maximumLateralLeanDegrees = 28.0f;
    float maximumVerticalLeanDegrees = 14.0f;
    float maximumCounterYawDegrees = 7.0f;
    float suspensionFollow = 0.72f;
    float poseResponse = 20.0f;
    float afterimageStrength = 0.58f;

    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleMountedEvasionPresentationInput final {
    const RailVehicleRuntimeState* vehicleState = nullptr;
    const RailVehiclePresentationFrame* vehiclePresentation = nullptr;
    const RailVehicleMountedEvasionFrame* evasion = nullptr;
    const RailVehicleCombatMountFrame* combatMount = nullptr;
    float deltaTime = 0.0f;
    bool gameplayVisible = true;
    RailVehicleMountedEvasionPresentationSettings settings{};
};

// Presentation-only view of the mounted occupant. Position follows the combat
// mount exactly; suspension and pose are visual additions and never feed back
// into collision, weapon or camera authority.
struct RailVehicleMountedEvasionPresentationFrame final {
    bool visible = false;
    RailVehicleOccupantPosePhase posePhase =
        RailVehicleOccupantPosePhase::MountedIdle;
    Vector3 position{};
    Vector3 weaponWorldPosition{};
    Vector3 damageVfxWorldPosition{};
    Vector3 forward{0.0f, 0.0f, 1.0f};
    Vector3 right{1.0f, 0.0f, 0.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
    float poseWeight = 0.0f;
    float lateralLeanDegrees = 0.0f;
    float verticalLeanDegrees = 0.0f;
    float counterYawDegrees = 0.0f;
    float afterimageAlpha = 0.0f;
    bool invulnerable = false;
    uint64_t sourceVehicleRevision = 0;
    uint64_t sourceEvasionRevision = 0;
    uint64_t sourceCombatMountRevision = 0;
    uint64_t revision = 0;
};

class RailVehicleMountedEvasionPresentationBridge final {
public:
    void Reset();
    void Update(const RailVehicleMountedEvasionPresentationInput& input);

    const RailVehicleMountedEvasionPresentationFrame& Frame() const noexcept {
        return frame_;
    }

private:
    RailVehicleMountedEvasionPresentationFrame frame_{};
    float smoothedPoseWeight_ = 0.0f;
    uint64_t revision_ = 0;
};

const char* ToString(RailVehicleOccupantPosePhase phase) noexcept;
