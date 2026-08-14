#pragma once

#include <cstdint>
#include <string>

// Immutable, authoring-friendly movement tuning. Coordinates are expressed in
// rail-local lateral/vertical space so gameplay never depends on render-camera
// orientation or frame rate.
struct RailPlayerMovementDefinition final {
    float initialLateralOffset = 0.0f;
    float initialVerticalOffset = 4.0f;
    float minimumLateralOffset = -18.0f;
    float maximumLateralOffset = 18.0f;
    float minimumVerticalOffset = 0.5f;
    float maximumVerticalOffset = 16.0f;
    float maximumLateralSpeed = 22.0f;
    float maximumVerticalSpeed = 18.0f;
    float acceleration = 72.0f;
    float deceleration = 58.0f;
    float boundaryBrakeAcceleration = 105.0f;
    float boundarySoftZone = 2.5f;
    float inputDeadZone = 0.08f;
    float inputExponent = 1.35f;
    float maximumBankDegrees = 28.0f;
    float maximumPitchDegrees = 14.0f;
    float attitudeResponse = 10.0f;
    float maximumSubstepSeconds = 1.0f / 60.0f;

    static RailPlayerMovementDefinition RailShooterDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
};

// Serializable/checkpoint-safe authoritative movement state.
struct RailPlayerMovementRuntimeState final {
    float lateralOffset = 0.0f;
    float verticalOffset = 4.0f;
    float lateralVelocity = 0.0f;
    float verticalVelocity = 0.0f;
    float shapedInputX = 0.0f;
    float shapedInputY = 0.0f;
    float bankDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float lateralNormalized = 0.0f;
    float verticalNormalized = 0.0f;
    bool inputEnabled = false;
    bool moving = false;
    bool touchingLateralBoundary = false;
    bool touchingVerticalBoundary = false;
    uint64_t revision = 0;
};

struct RailPlayerMovementInput final {
    float deltaTime = 0.0f;
    float moveX = 0.0f;
    float moveY = 0.0f;
    bool inputEnabled = true;
    float movementScale = 1.0f;
    float externalLateralDisplacement = 0.0f;
    float externalVerticalDisplacement = 0.0f;
    float externalBankNormalized = 0.0f;
};

struct RailPlayerMovementFrame final {
    RailPlayerMovementRuntimeState state{};
    float appliedLateralDisplacement = 0.0f;
    float appliedVerticalDisplacement = 0.0f;
    bool clampedByBoundary = false;
};

class RailPlayerMovementSystem final {
public:
    RailPlayerMovementSystem();

    bool Initialize(
        const RailPlayerMovementDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    void Reset(float lateralOffset, float verticalOffset);
    bool RestoreState(
        const RailPlayerMovementRuntimeState& state,
        std::string* errorMessage = nullptr);
    const RailPlayerMovementFrame& Update(const RailPlayerMovementInput& input);

    bool IsInitialized() const noexcept { return initialized_; }
    const RailPlayerMovementDefinition& Definition() const noexcept {
        return definition_;
    }
    const RailPlayerMovementRuntimeState& State() const noexcept {
        return state_;
    }
    const RailPlayerMovementFrame& Frame() const noexcept { return frame_; }

private:
    void SimulateStep(float deltaTime, const RailPlayerMovementInput& input);
    void RefreshDerivedState(float deltaTime, float externalBankNormalized);

    RailPlayerMovementDefinition definition_{};
    RailPlayerMovementRuntimeState state_{};
    RailPlayerMovementFrame frame_{};
    bool initialized_ = false;
};

