#pragma once

#include <cstdint>
#include <string>

#include "RailVehicleMovementSystem.h"

enum class RailVehicleMountedEvasionPhase : uint8_t {
    Ready,
    Evading,
    Recovering,
    Cooldown,
};

struct RailVehicleMountedEvasionDefinition final {
    float evadeDurationSeconds = 0.16f;
    float recoveryDurationSeconds = 0.20f;
    float cooldownSeconds = 0.70f;
    float invulnerabilityDurationSeconds = 0.0f;
    float lateralDistance = 1.45f;
    float verticalDistance = 0.72f;
    float minimumDirectionalInput = 0.20f;
    float maximumSubstepSeconds = 1.0f / 60.0f;

    static RailVehicleMountedEvasionDefinition MineCartDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleMountedEvasionRuntimeState final {
    RailVehicleMountedEvasionPhase phase =
        RailVehicleMountedEvasionPhase::Ready;
    float phaseElapsedSeconds = 0.0f;
    float cooldownRemainingSeconds = 0.0f;
    float invulnerabilityRemainingSeconds = 0.0f;
    float directionX = 0.0f;
    float directionY = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float distanceScale = 1.0f;
    uint64_t evasionCount = 0;
    uint64_t eventSequence = 0;
    uint64_t revision = 0;
};

struct RailVehicleMountedEvasionInput final {
    float deltaTime = 0.0f;
    const RailVehicleDefinition* vehicleDefinition = nullptr;
    const RailVehicleRuntimeState* vehicleState = nullptr;
    bool occupantMounted = true;
    bool inputEnabled = true;
    bool evadePressed = false;
    float directionX = 0.0f;
    float directionY = 0.0f;
    // Scene-query authority may limit the authored evade distance. A value of
    // zero rejects movement; the system never expands an in-progress evade.
    float maximumDistanceScale = 1.0f;
};

struct RailVehicleMountedEvasionFrame final {
    RailVehicleMountedEvasionRuntimeState state{};
    Vector3 occupantWorldPosition{};
    float railLateralOffset = 0.0f;
    float railVerticalOffset = 0.0f;
    float normalizedStrength = 0.0f;
    float bankNormalized = 0.0f;
    bool active = false;
    bool invulnerable = false;
    bool startedThisFrame = false;
    bool endedThisFrame = false;
    bool becameReadyThisFrame = false;
    bool mounted = false;
    uint64_t sourceVehicleRevision = 0;
};

// Moves only the mounted occupant in vehicle-local space. The vehicle remains
// rail-authoritative, so evasion cannot derail or change course progression.
class RailVehicleMountedEvasionSystem final {
public:
    RailVehicleMountedEvasionSystem();

    bool Initialize(
        const RailVehicleMountedEvasionDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    bool RestoreState(
        const RailVehicleMountedEvasionRuntimeState& state,
        std::string* errorMessage = nullptr);
    const RailVehicleMountedEvasionFrame& Update(
        const RailVehicleMountedEvasionInput& input);

    bool CanEvade() const noexcept;
    const RailVehicleMountedEvasionDefinition& Definition() const noexcept {
        return definition_;
    }
    const RailVehicleMountedEvasionRuntimeState& State() const noexcept {
        return state_;
    }
    const RailVehicleMountedEvasionFrame& Frame() const noexcept {
        return frame_;
    }

private:
    void BeginEvasion(const RailVehicleMountedEvasionInput& input);
    void Advance(float deltaTime);
    void BuildFrame(const RailVehicleMountedEvasionInput& input);

    RailVehicleMountedEvasionDefinition definition_{};
    RailVehicleMountedEvasionRuntimeState state_{};
    RailVehicleMountedEvasionFrame frame_{};
    bool initialized_ = false;
};

const char* ToString(RailVehicleMountedEvasionPhase phase) noexcept;
