#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "EnemyAttackTelegraphSystem.h"

struct EnemyAttackTelegraphFeedbackSettings {
    bool enabled = true;
    bool audioEnabled = true;
    bool hapticsEnabled = true;
    float masterVolume = 0.72f;
    float stereoPanStrength = 0.82f;
    float offscreenVolumeBoost = 0.12f;
    float acquiredCooldownSeconds = 0.10f;
    float imminentCooldownSeconds = 0.08f;
    float firedCooldownSeconds = 0.06f;
    uint32_t maximumAudioCommandsPerFrame = 2;
};

struct EnemyAttackTelegraphAudioCommand {
    EnemyAttackTelegraphEventKind kind =
        EnemyAttackTelegraphEventKind::Acquired;
    uint32_t actorId = 0;
    uint64_t fireSequence = 0;
    float volume = 0.0f;
    float pan = 0.0f;
    float pitch = 1.0f;
    float priority = 0.0f;
};

struct EnemyAttackTelegraphHapticCommand {
    float lowFrequencyMotor = 0.0f;
    float highFrequencyMotor = 0.0f;
    bool active = false;
};

struct EnemyAttackTelegraphFeedbackStats {
    uint32_t inputEvents = 0;
    uint32_t audioCommands = 0;
    uint32_t hapticEvents = 0;
    uint32_t suppressedDisabled = 0;
    uint32_t suppressedInactive = 0;
    uint32_t suppressedCooldown = 0;
    uint32_t suppressedBudget = 0;
};

struct EnemyAttackTelegraphFeedbackFrame {
    std::vector<EnemyAttackTelegraphAudioCommand> audioCommands;
    EnemyAttackTelegraphHapticCommand haptics{};
    EnemyAttackTelegraphFeedbackStats stats{};
    uint64_t revision = 0;
};

struct EnemyAttackTelegraphFeedbackInput {
    const EnemyAttackTelegraphFrame* telegraphFrame = nullptr;
    float deltaTime = 0.016f;
    bool gameplayActive = true;
    EnemyAttackTelegraphFeedbackSettings settings{};
};

// Converts the authoritative telegraph event frame into bounded audio and
// haptic commands. The bridge is platform independent; AppRunLoop is the thin
// adapter that sends these commands to AudioSystem and XInput.
class EnemyAttackTelegraphFeedbackBridge {
public:
    void Reset();
    void Update(const EnemyAttackTelegraphFeedbackInput& input);

    const EnemyAttackTelegraphFeedbackFrame& Frame() const { return frame_; }

private:
    std::array<float, 3> audioCooldownRemaining_{};
    float hapticRemaining_ = 0.0f;
    float hapticDuration_ = 0.0f;
    float hapticLowPeak_ = 0.0f;
    float hapticHighPeak_ = 0.0f;
    EnemyAttackTelegraphFeedbackFrame frame_{};
    uint64_t revision_ = 0;
};

