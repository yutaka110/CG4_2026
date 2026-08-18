#pragma once

#include <cstdint>

#include "RailPlayerMovementSystem.h"
#include "RailVehicleMovementSystem.h"

struct RailPlayerVehicleMountInput final {
    const RailVehicleDefinition* vehicleDefinition = nullptr;
    const RailVehicleRuntimeState* vehicleState = nullptr;
    RailPlayerMovementSystem* playerMovement = nullptr;
    RailPlayerMovementInput movementInput{};
    bool occupantMounted = true;
};

// Derived, checkpoint-safe mount state. It records ownership transitions but
// never duplicates the vehicle or player movement authority.
struct RailPlayerVehicleMountRuntimeState final {
    bool active = false;
    bool occupantMounted = false;
    bool playerMovementSuppressed = false;
    RailVehicleMountedMovementMode movementMode =
        RailVehicleMountedMovementMode::FreeOffset;
    uint64_t transitionIndex = 0;
    uint64_t frameIndex = 0;
    uint64_t revision = 0;
};

struct RailPlayerVehicleMountFrame final {
    RailPlayerVehicleMountRuntimeState state{};
    RailPlayerMovementFrame playerMovementFrame{};
    Vector3 playerWorldPosition{};
    float railLateralOffset = 0.0f;
    float railVerticalOffset = 0.0f;
    bool playerMovementUpdated = false;
    bool ownershipChangedThisFrame = false;
    uint64_t sourceVehicleRevision = 0;
    uint64_t sourcePlayerMovementRevision = 0;
};

// Owns the boundary between vehicle motion and player-local motion. In
// VehicleMounted mode the player movement system is not ticked; the vehicle's
// player mount becomes the authoritative world/rail-local position.
class RailPlayerVehicleMountSystem final {
public:
    void Reset();
    const RailPlayerVehicleMountFrame& Update(
        const RailPlayerVehicleMountInput& input);

    const RailPlayerVehicleMountRuntimeState& State() const noexcept {
        return state_;
    }
    const RailPlayerVehicleMountFrame& Frame() const noexcept { return frame_; }

private:
    RailPlayerVehicleMountRuntimeState state_{};
    RailPlayerVehicleMountFrame frame_{};
};

