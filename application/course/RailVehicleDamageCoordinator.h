#pragma once

#include <cstdint>
#include <string>

#include "CourseCollisionSystem.h"
#include "RailVehicleBodyCollisionSystem.h"

struct RailVehicleDamageRuntimeState final {
    float repeatedContactCooldownSeconds = 0.0f;
    uint64_t lastBodyContactSequence = 0;
    uint64_t lastAcceptedDamageSequence = 0;
    uint64_t revision = 0;
    bool initialized = false;
};

struct RailVehicleDamageCoordinatorInput final {
    float deltaTime = 0.0f;
    const RailVehicleBodyCollisionFrame* bodyCollision = nullptr;
    CourseCollisionSystem* collisionSystem = nullptr;
    RailVehicleMovementSystem* vehicleMovement = nullptr;
    bool gameplayActive = true;
};

struct RailVehicleDamageCoordinatorFrame final {
    RailVehicleDamageRuntimeState state{};
    PlayerDamageResult damageResult{};
    float playerHealthNormalized = 1.0f;
    float vehicleHitPointsBefore = 0.0f;
    float vehicleHitPointsAfter = 0.0f;
    bool submittedContactDamage = false;
    bool acceptedContactDamage = false;
    bool vehicleHealthSynchronized = false;
    bool lethal = false;
    uint64_t sourceBodyCollisionRevision = 0;
};

// Converts authoritative body contacts into PlayerHitRequest and mirrors the
// resulting PlayerDamageSystem health ratio into vehicle presentation state.
// It never subtracts HP directly and consumes no render-only collision data.
class RailVehicleDamageCoordinator final {
public:
    RailVehicleDamageCoordinator();

    bool Initialize(
        const RailVehicleHitboxProfile& profile,
        std::string* errorMessage = nullptr);
    void Reset();
    bool RestoreState(
        const RailVehicleDamageRuntimeState& state,
        std::string* errorMessage = nullptr);
    const RailVehicleDamageCoordinatorFrame& Update(
        const RailVehicleDamageCoordinatorInput& input);

    const RailVehicleHitboxProfile& Profile() const noexcept {
        return profile_;
    }
    const RailVehicleDamageRuntimeState& State() const noexcept {
        return state_;
    }
    const RailVehicleDamageCoordinatorFrame& Frame() const noexcept {
        return frame_;
    }

private:
    float DamageFor(RailVehicleBodyContactKind kind) const noexcept;

    RailVehicleHitboxProfile profile_{};
    RailVehicleDamageRuntimeState state_{};
    RailVehicleDamageCoordinatorFrame frame_{};
    bool initialized_ = false;
};
