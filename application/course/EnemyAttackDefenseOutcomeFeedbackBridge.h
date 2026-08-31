#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "EnemyAttackDefenseResult.h"
#include "utils/math/Vector.h"

struct EnemyAttackDefenseOutcomeFeedbackSettings final {
    bool enabled = true;
    float displayDurationSeconds = 0.75f;
    float failureDisplayDurationSeconds = 0.42f;
    float hapticDurationSeconds = 0.14f;
    size_t maximumAudioCuesPerFrame = 4;
};

struct EnemyAttackDefenseOutcomeAudioCue final {
    uint64_t resultSequence = 0;
    EnemyAttackDefenseOutcome outcome = EnemyAttackDefenseOutcome::Failed;
    EnemyAttackDefenseGrade grade = EnemyAttackDefenseGrade::None;
    float volume = 0.0f;
    float pitch = 1.0f;
};

struct EnemyAttackDefenseOutcomeFeedbackFrame final {
    std::vector<EnemyAttackDefenseOutcomeAudioCue> audioCues;
    uint64_t resultSequence = 0;
    EnemyAttackDefenseMethod method = EnemyAttackDefenseMethod::None;
    EnemyAttackDefenseOutcome outcome = EnemyAttackDefenseOutcome::Failed;
    EnemyAttackDefenseGrade grade = EnemyAttackDefenseGrade::None;
    std::string headline;
    std::string detail;
    Vector4 color{0.25f, 0.92f, 1.0f, 1.0f};
    uint32_t scoreAwarded = 0;
    uint32_t chain = 0;
    float alpha = 0.0f;
    float cameraShake = 0.0f;
    float cameraPitchImpulse = 0.0f;
    float cameraYawImpulse = 0.0f;
    float hapticLow = 0.0f;
    float hapticHigh = 0.0f;
    float hapticRemainingSeconds = 0.0f;
    bool visible = false;
    uint64_t revision = 0;
};

struct EnemyAttackDefenseOutcomeFeedbackInput final {
    std::span<const EnemyAttackDefenseResult> results{};
    float deltaTime = 0.016f;
    bool gameplayActive = true;
    EnemyAttackDefenseOutcomeFeedbackSettings settings{};
};

// Presentation-only consumer of authoritative defense results. One-shot audio,
// camera and haptics are emitted once per result sequence; the HUD outcome is
// retained independently for a short readable duration.
class EnemyAttackDefenseOutcomeFeedbackBridge final {
public:
    void Reset();
    void Update(const EnemyAttackDefenseOutcomeFeedbackInput& input);
    const EnemyAttackDefenseOutcomeFeedbackFrame& Frame() const noexcept {
        return frame_;
    }

private:
    uint64_t lastConsumedSequence_ = 0;
    float displayRemainingSeconds_ = 0.0f;
    float displayDurationSeconds_ = 0.0f;
    float hapticRemainingSeconds_ = 0.0f;
    EnemyAttackDefenseOutcomeFeedbackFrame frame_{};
    uint64_t revision_ = 0;
};
