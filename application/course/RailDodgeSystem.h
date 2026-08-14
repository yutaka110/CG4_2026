#pragma once

#include <cstdint>
#include <string>

enum class RailDodgePhase : uint8_t {
    Ready,
    Active,
    Recovery,
    Cooldown,
};

struct RailDodgeDefinition final {
    float activeDurationSeconds = 0.20f;
    float recoveryDurationSeconds = 0.16f;
    float cooldownSeconds = 0.72f;
    float invulnerabilityDurationSeconds = 0.30f;
    float lateralDistance = 7.0f;
    float verticalDistance = 5.0f;
    float minimumDirectionalInput = 0.22f;
    float activeMovementScale = 0.30f;
    float recoveryMovementScale = 0.72f;
    float bankImpulseNormalized = 0.85f;

    static RailDodgeDefinition RailShooterDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
};

// Serializable and safe to embed in a retry checkpoint.
struct RailDodgeRuntimeState final {
    RailDodgePhase phase = RailDodgePhase::Ready;
    float phaseElapsedSeconds = 0.0f;
    float cooldownRemainingSeconds = 0.0f;
    float invulnerabilityRemainingSeconds = 0.0f;
    float directionX = 0.0f;
    float directionY = 0.0f;
    uint64_t dodgeCount = 0;
    uint64_t eventSequence = 0;
    uint64_t revision = 0;
};

struct RailDodgeInput final {
    float deltaTime = 0.0f;
    bool inputEnabled = true;
    bool dodgePressed = false;
    float directionX = 0.0f;
    float directionY = 0.0f;
    float movementVelocityX = 0.0f;
    float movementVelocityY = 0.0f;
};

struct RailDodgeFrame final {
    RailDodgeRuntimeState state{};
    bool startedThisFrame = false;
    bool endedThisFrame = false;
    bool becameReadyThisFrame = false;
    bool invulnerable = false;
    float movementScale = 1.0f;
    float lateralDisplacement = 0.0f;
    float verticalDisplacement = 0.0f;
    float bankNormalized = 0.0f;
};

class RailDodgeSystem final {
public:
    RailDodgeSystem();

    bool Initialize(
        const RailDodgeDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    bool RestoreState(
        const RailDodgeRuntimeState& state,
        std::string* errorMessage = nullptr);
    const RailDodgeFrame& Update(const RailDodgeInput& input);

    bool IsInitialized() const noexcept { return initialized_; }
    bool CanDodge() const noexcept;
    const RailDodgeDefinition& Definition() const noexcept { return definition_; }
    const RailDodgeRuntimeState& State() const noexcept { return state_; }
    const RailDodgeFrame& Frame() const noexcept { return frame_; }

private:
    void BeginDodge(const RailDodgeInput& input);
    void AdvanceActive(float deltaTime);

    RailDodgeDefinition definition_{};
    RailDodgeRuntimeState state_{};
    RailDodgeFrame frame_{};
    bool initialized_ = false;
};

const char* ToString(RailDodgePhase phase);

