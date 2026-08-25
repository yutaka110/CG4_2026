#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "PlayerDamageSystem.h"
#include "RailVehicleMovementSystem.h"

struct RailVehicleCollisionFeedbackSettings final {
    bool enabled = true;
    bool audioEnabled = true;
    bool vfxEnabled = true;
    float bodyKickDistance = 0.22f;
    float maximumBankDegrees = 8.0f;
    float maximumPitchDegrees = 5.0f;
    float maximumYawDegrees = 4.0f;
    float responseDurationSeconds = 0.32f;
    float oscillationFrequencyHz = 13.0f;
    float cameraShake = 0.42f;
    float sparkRadius = 0.55f;
    float sparkLifetimeSeconds = 0.38f;
    float sparkSpread = 0.75f;
    uint32_t sparkBurstCount = 4;
    float impactVolume = 0.86f;
    float slowdownSpeedMultiplier = 0.55f;
    float slowdownDurationSeconds = 0.42f;

    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleCollisionAudioCue final {
    uint64_t resultSequence = 0;
    float volume = 0.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    bool lethal = false;
};

struct RailVehicleCollisionVfxCommand final {
    uint64_t resultSequence = 0;
    std::string effectId = "hit_ring";
    Vector3 worldPosition{};
    Vector3 impactNormalWorld{0.0f, 1.0f, 0.0f};
    Vector4 color{1.0f, 0.58f, 0.12f, 1.0f};
    float radius = 0.55f;
    float lifetime = 0.38f;
};

struct RailVehicleImpactSlowdownCommand final {
    uint64_t resultSequence = 0;
    float speedMultiplier = 1.0f;
    float durationSeconds = 0.0f;
    bool valid = false;
};

struct RailVehicleCollisionFeedbackInput final {
    std::span<const PlayerDamageResult> damageResults;
    const RailVehicleRuntimeState* vehicleState = nullptr;
    float deltaTime = 0.0f;
    bool gameplayActive = true;
    RailVehicleCollisionFeedbackSettings settings{};
};

struct RailVehicleCollisionFeedbackFrame final {
    static constexpr size_t kMaximumAudioCues = 2;
    static constexpr size_t kMaximumVfxCommands = 8;

    std::array<RailVehicleCollisionAudioCue, kMaximumAudioCues> audioCues{};
    std::array<RailVehicleCollisionVfxCommand, kMaximumVfxCommands> vfxCommands{};
    size_t audioCueCount = 0;
    size_t vfxCommandCount = 0;
    RailVehicleImpactSlowdownCommand slowdown{};
    Vector3 bodyTranslationWorld{};
    Vector3 impactWorldPosition{};
    Vector3 impactNormalWorld{0.0f, 1.0f, 0.0f};
    float bodyBankDegrees = 0.0f;
    float bodyPitchDegrees = 0.0f;
    float bodyYawDegrees = 0.0f;
    float cameraShake = 0.0f;
    float cameraPitchImpulse = 0.0f;
    float cameraYawImpulse = 0.0f;
    float activeResponseRemainingSeconds = 0.0f;
    uint64_t lastConsumedResultSequence = 0;
    uint64_t revision = 0;
};

// Presentation-only consumer of accepted PlayerDamageResult records. Collision
// counters and rejected hits can never trigger vehicle feel or speed changes.
class RailVehicleCollisionFeedbackBridge final {
public:
    void Reset();
    void Update(const RailVehicleCollisionFeedbackInput& input);

    const RailVehicleCollisionFeedbackFrame& Frame() const noexcept {
        return frame_;
    }

private:
    RailVehicleCollisionFeedbackFrame frame_{};
    Vector3 responseNormalWorld_{0.0f, 1.0f, 0.0f};
    Vector3 responseNormalRailLocal_{0.0f, 1.0f, 0.0f};
    Vector3 responseWorldPosition_{};
    float responseRemainingSeconds_ = 0.0f;
    float responseDurationSeconds_ = 0.0f;
    float responseIntensity_ = 0.0f;
    uint64_t lastConsumedResultSequence_ = 0;
    uint64_t revision_ = 0;
};
