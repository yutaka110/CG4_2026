#include "EnemyAttackDefenseOutcomeFeedbackBridge.h"

#include <algorithm>
#include <cmath>

namespace {
std::string Headline(const EnemyAttackDefenseResult& result) {
    if (result.outcome == EnemyAttackDefenseOutcome::Failed) {
        return "DEFENSE FAILED";
    }
    if (result.grade == EnemyAttackDefenseGrade::Perfect) {
        return "PERFECT DEFENSE";
    }
    switch (result.method) {
    case EnemyAttackDefenseMethod::Interrupt: return "ATTACK INTERRUPTED";
    case EnemyAttackDefenseMethod::ShootDown: return "PROJECTILE DESTROYED";
    case EnemyAttackDefenseMethod::LeanLeft:
    case EnemyAttackDefenseMethod::LeanRight:
    case EnemyAttackDefenseMethod::Duck: return "EVASION SUCCESS";
    case EnemyAttackDefenseMethod::None: break;
    }
    return "DEFENSE SUCCESS";
}

std::string Detail(const EnemyAttackDefenseResult& result) {
    if (result.outcome == EnemyAttackDefenseOutcome::Failed) {
        return "HIT TAKEN - DEFENSE CHAIN LOST";
    }
    std::string detail = ToString(result.grade);
    detail += "  +" + std::to_string(result.scoreAwarded);
    if (result.chainAfter > 1) {
        detail += "  CHAIN x" + std::to_string(result.chainAfter);
    }
    return detail;
}
}

void EnemyAttackDefenseOutcomeFeedbackBridge::Reset() {
    lastConsumedSequence_ = 0;
    displayRemainingSeconds_ = 0.0f;
    displayDurationSeconds_ = 0.0f;
    hapticRemainingSeconds_ = 0.0f;
    frame_ = {};
    revision_ = 0;
}

void EnemyAttackDefenseOutcomeFeedbackBridge::Update(
    const EnemyAttackDefenseOutcomeFeedbackInput& input) {
    const float dt = std::isfinite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f) : 0.0f;
    EnemyAttackDefenseOutcomeFeedbackFrame next = frame_;
    next.audioCues.clear();
    next.cameraShake = 0.0f;
    next.cameraPitchImpulse = 0.0f;
    next.cameraYawImpulse = 0.0f;
    displayRemainingSeconds_ = (std::max)(0.0f, displayRemainingSeconds_ - dt);
    hapticRemainingSeconds_ = (std::max)(0.0f, hapticRemainingSeconds_ - dt);

    if (input.settings.enabled && input.gameplayActive) {
        for (const EnemyAttackDefenseResult& result : input.results) {
            if (!result.accepted || result.sequence <= lastConsumedSequence_) continue;
            lastConsumedSequence_ = result.sequence;
            next.resultSequence = result.sequence;
            next.method = result.method;
            next.outcome = result.outcome;
            next.grade = result.grade;
            next.headline = Headline(result);
            next.detail = Detail(result);
            next.scoreAwarded = result.scoreAwarded;
            next.chain = result.chainAfter;
            const bool success = result.outcome == EnemyAttackDefenseOutcome::Success;
            const bool perfect = result.grade == EnemyAttackDefenseGrade::Perfect;
            next.color = success
                ? (perfect ? Vector4{1.0f, 0.82f, 0.24f, 1.0f}
                           : Vector4{0.24f, 1.0f, 0.64f, 1.0f})
                : Vector4{1.0f, 0.24f, 0.30f, 1.0f};
            displayDurationSeconds_ = success
                ? input.settings.displayDurationSeconds
                : input.settings.failureDisplayDurationSeconds;
            displayRemainingSeconds_ = displayDurationSeconds_;
            hapticRemainingSeconds_ = input.settings.hapticDurationSeconds;
            next.hapticLow = success ? (perfect ? 0.42f : 0.28f) : 0.55f;
            next.hapticHigh = success ? (perfect ? 0.66f : 0.48f) : 0.32f;
            next.cameraShake = success ? (perfect ? 0.075f : 0.045f) : 0.11f;
            next.cameraPitchImpulse = success ? -0.010f : 0.018f;
            if (next.audioCues.size() < input.settings.maximumAudioCuesPerFrame) {
                next.audioCues.push_back({
                    result.sequence,
                    result.outcome,
                    result.grade,
                    success ? (perfect ? 0.48f : 0.38f) : 0.30f,
                    success ? (perfect ? 1.22f : 1.0f) : 0.78f});
            }
        }
    }

    next.visible = input.settings.enabled && displayRemainingSeconds_ > 0.0f &&
        !next.headline.empty();
    next.alpha = next.visible && displayDurationSeconds_ > 0.0f
        ? (std::clamp)(displayRemainingSeconds_ /
            (std::min)(0.18f, displayDurationSeconds_), 0.0f, 1.0f)
        : 0.0f;
    next.hapticRemainingSeconds = hapticRemainingSeconds_;
    if (hapticRemainingSeconds_ <= 0.0f) {
        next.hapticLow = 0.0f;
        next.hapticHigh = 0.0f;
    }
    next.revision = ++revision_;
    frame_ = std::move(next);
}
