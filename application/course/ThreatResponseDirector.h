#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "EnemyProjectilePresentationBridge.h"
#include "GrazeScoreSystem.h"
#include "PlayerDamageSystem.h"

enum class ThreatResponseBand : uint8_t {
    Calm,
    Alert,
    Critical,
};

enum class ThreatResponseCueKind : uint8_t {
    Graze,
    ChainMilestone,
    CriticalEntered,
    DangerCleared,
};

struct ThreatResponseCue final {
    ThreatResponseCueKind kind = ThreatResponseCueKind::Graze;
    uint64_t sourceSequence = 0;
    uint32_t chain = 0;
    uint32_t scoreAwarded = 0;
    float intensity = 0.0f;
    float pitch = 1.0f;
};

struct ThreatResponseSettings final {
    bool enabled = true;
    float awarenessDistance = 32.0f;
    float criticalDistance = 8.0f;
    float alertThreshold = 0.28f;
    float criticalThreshold = 0.68f;
    float hysteresis = 0.08f;
    float risePerSecond = 5.0f;
    float decayPerSecond = 1.8f;
    float grazePulseSeconds = 0.34f;
    float maximumCameraShake = 0.22f;
    float maximumVignette = 0.65f;
    size_t maximumCuesPerFrame = 8;
};

struct ThreatResponseFrameInput final {
    float deltaTime = 0.0f;
    const EnemyProjectilePresentationFrame* projectileFrame = nullptr;
    std::span<const GrazeScoreResult> grazeResults{};
    std::span<const PlayerDamageResult> damageResults{};
    uint32_t currentGrazeChain = 0;
    bool gameplayActive = true;
};

struct ThreatResponseFrame final {
    ThreatResponseBand band = ThreatResponseBand::Calm;
    float threatNormalized = 0.0f;
    float grazePulse = 0.0f;
    float vignetteIntensity = 0.0f;
    float cameraShake = 0.0f;
    float cameraPitchImpulse = 0.0f;
    float cameraYawImpulse = 0.0f;
    float hapticLow = 0.0f;
    float hapticHigh = 0.0f;
    float hapticRemainingSeconds = 0.0f;
    uint32_t nearbyThreats = 0;
    uint32_t scoreAwardedThisFrame = 0;
    uint32_t chain = 0;
    std::vector<ThreatResponseCue> cues;
    uint64_t revision = 0;
};

// Presentation-only director. It observes authoritative projectile, graze and
// damage results and never mutates collision, damage, projectile or score data.
class ThreatResponseDirector final {
public:
    void Reset();
    void Update(
        const ThreatResponseFrameInput& input,
        const ThreatResponseSettings& settings = {});

    const ThreatResponseFrame& Frame() const noexcept { return frame_; }

private:
    void PushCue(ThreatResponseCue cue, size_t maximumCues);

    ThreatResponseFrame frame_{};
    ThreatResponseBand previousBand_ = ThreatResponseBand::Calm;
    float grazePulseRemaining_ = 0.0f;
    uint64_t revision_ = 0;
};

const char* ToString(ThreatResponseBand band) noexcept;
const char* ToString(ThreatResponseCueKind kind) noexcept;
