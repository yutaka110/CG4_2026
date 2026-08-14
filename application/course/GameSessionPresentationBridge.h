#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GameSessionSystem.h"

enum class GameSessionPresentationCueKind : uint8_t {
    SessionStarted,
    IntroCompleted,
    Paused,
    Resumed,
    Checkpoint,
    PlayerDamaged,
    PlayerRecovered,
    Victory,
    Defeat,
    Result,
    Retry,
    Restart,
};

struct GameSessionPresentationColor final {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct GameSessionPresentationSettings final {
    bool enabled = true;
    bool showPlayingHud = true;
    bool audioEnabled = true;
    bool cameraEnabled = true;
    bool hapticsEnabled = true;
    float bannerDurationSeconds = 1.35f;
    float damageFlashDurationSeconds = 0.22f;
    float outcomeFlashDurationSeconds = 0.65f;
    uint32_t maximumCuesPerFrame = 8;
};

struct GameSessionHudView final {
    bool visible = false;
    GameSessionPhase phase = GameSessionPhase::Uninitialized;
    float healthNormalized = 0.0f;
    float courseProgressNormalized = 0.0f;
    uint64_t score = 0;
    uint32_t combo = 0;
    uint32_t retriesRemaining = 0;
    bool showBanner = false;
    bool showRetryPrompt = false;
    float bannerAlpha = 0.0f;
    GameSessionPresentationColor bannerColor{};
    std::string headline;
    std::string detail;
};

struct GameSessionPresentationCue final {
    GameSessionPresentationCueKind kind =
        GameSessionPresentationCueKind::SessionStarted;
    uint64_t sourceEventSequence = 0;
    float audioVolume = 0.0f;
    float audioPitch = 1.0f;
    float cameraShake = 0.0f;
    float cameraPitchImpulse = 0.0f;
    float cameraYawImpulse = 0.0f;
    float hapticLow = 0.0f;
    float hapticHigh = 0.0f;
    float hapticDurationSeconds = 0.0f;
};

struct GameSessionPresentationFrame final {
    GameSessionHudView hud{};
    std::vector<GameSessionPresentationCue> cues;
    GameSessionPresentationColor screenFlashColor{};
    float screenFlashIntensity = 0.0f;
    float hapticLow = 0.0f;
    float hapticHigh = 0.0f;
    bool hapticsActive = false;
    uint64_t lastConsumedEventSequence = 0;
    uint64_t revision = 0;
};

struct GameSessionPresentationInput final {
    const GameSessionDefinition* definition = nullptr;
    const GameSessionRuntimeState* state = nullptr;
    const std::vector<GameSessionEvent>* eventHistory = nullptr;
    float deltaTime = 0.0f;
    GameSessionPresentationSettings settings{};
};

// Converts authoritative session state/events into a renderer- and platform-
// independent HUD view plus bounded audio/camera/haptic commands.
class GameSessionPresentationBridge final {
public:
    void Reset(uint64_t consumedThroughSequence = 0);
    void Update(const GameSessionPresentationInput& input);

    const GameSessionPresentationFrame& Frame() const noexcept { return frame_; }

private:
    void ConsumeEvent(
        const GameSessionEvent& event,
        const GameSessionPresentationSettings& settings);
    void SetBanner(
        std::string headline,
        std::string detail,
        GameSessionPresentationColor color,
        float duration);

    GameSessionPresentationFrame frame_{};
    uint64_t lastConsumedEventSequence_ = 0;
    uint64_t revision_ = 0;
    float bannerRemaining_ = 0.0f;
    float bannerDuration_ = 0.0f;
    std::string bannerHeadline_;
    std::string bannerDetail_;
    GameSessionPresentationColor bannerColor_{};
    float flashRemaining_ = 0.0f;
    float flashDuration_ = 0.0f;
    GameSessionPresentationColor flashColor_{};
    float hapticRemaining_ = 0.0f;
    float hapticDuration_ = 0.0f;
    float hapticLowPeak_ = 0.0f;
    float hapticHighPeak_ = 0.0f;
};

const char* ToString(GameSessionPresentationCueKind kind);
