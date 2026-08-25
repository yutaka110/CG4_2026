#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "RailVehicleCombatMountBridge.h"

enum class RailVehicleEvasionAudioCueKind : uint8_t {
    Start,
    Recover,
    Ready,
};

struct RailVehicleEvasionFeedbackSettings final {
    bool enabled = true;
    bool audioEnabled = true;
    bool vfxEnabled = true;
    bool hapticsEnabled = true;
    float masterVolume = 0.68f;
    float afterimageIntervalSeconds = 0.055f;
    float startHapticDurationSeconds = 0.13f;
    uint32_t maximumVfxCommandsPerFrame = 3;

    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleEvasionAudioCue final {
    RailVehicleEvasionAudioCueKind kind =
        RailVehicleEvasionAudioCueKind::Start;
    float volume = 0.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
};

struct RailVehicleEvasionVfxCommand final {
    std::string effectId = "hit_ring";
    float railDistance = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float radius = 0.8f;
    float lifetime = 0.20f;
    Vector4 color{0.20f, 0.88f, 1.0f, 0.70f};
    uint64_t eventSequence = 0;
};

struct RailVehicleEvasionHapticFrame final {
    float lowFrequencyMotor = 0.0f;
    float highFrequencyMotor = 0.0f;
    float remainingSeconds = 0.0f;
    bool active = false;
};

struct RailVehicleEvasionFeedbackFrame final {
    static constexpr size_t kMaximumAudioCues = 3;
    static constexpr size_t kMaximumVfxCommands = 4;

    std::array<RailVehicleEvasionAudioCue, kMaximumAudioCues> audioCues{};
    std::array<RailVehicleEvasionVfxCommand, kMaximumVfxCommands> vfxCommands{};
    size_t audioCueCount = 0;
    size_t vfxCommandCount = 0;
    RailVehicleEvasionHapticFrame haptics{};
    uint32_t suppressedVfxCommands = 0;
    uint64_t sourceEvasionRevision = 0;
    uint64_t revision = 0;
};

struct RailVehicleEvasionFeedbackInput final {
    const RailVehicleMountedEvasionFrame* evasion = nullptr;
    const RailVehicleCombatMountFrame* combatMount = nullptr;
    float deltaTime = 0.0f;
    bool gameplayActive = true;
    RailVehicleEvasionFeedbackSettings settings{};
};

// Converts mounted-evasion transitions into bounded one-frame presentation
// commands. Audio, VFX and haptics are emitted once per authoritative event.
class RailVehicleEvasionFeedbackBridge final {
public:
    void Reset();
    void Update(const RailVehicleEvasionFeedbackInput& input);

    const RailVehicleEvasionFeedbackFrame& Frame() const noexcept {
        return frame_;
    }

private:
    void PushAudio(
        RailVehicleEvasionAudioCueKind kind,
        float volume,
        float pitch,
        float pan);
    void PushVfx(
        const RailVehicleEvasionFeedbackInput& input,
        float radius,
        float lifetime,
        Vector4 color);

    RailVehicleEvasionFeedbackFrame frame_{};
    float afterimageAccumulator_ = 0.0f;
    float hapticRemaining_ = 0.0f;
    float hapticDuration_ = 0.0f;
    uint64_t lastEventSequence_ = 0;
    uint64_t revision_ = 0;
};
