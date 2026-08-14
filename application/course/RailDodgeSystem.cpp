#include "RailDodgeSystem.h"

#include <algorithm>
#include <cmath>

namespace {

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

bool Finite(float value) noexcept {
    return std::isfinite(value);
}

float EaseOutCubic(float value) noexcept {
    value = (std::clamp)(value, 0.0f, 1.0f);
    const float inverse = 1.0f - value;
    return 1.0f - inverse * inverse * inverse;
}

} // namespace

RailDodgeDefinition RailDodgeDefinition::RailShooterDefaults() {
    return {};
}

bool RailDodgeDefinition::Validate(std::string* errorMessage) const {
    const bool finite =
        Finite(activeDurationSeconds) && Finite(recoveryDurationSeconds) &&
        Finite(cooldownSeconds) && Finite(invulnerabilityDurationSeconds) &&
        Finite(lateralDistance) && Finite(verticalDistance) &&
        Finite(minimumDirectionalInput) && Finite(activeMovementScale) &&
        Finite(recoveryMovementScale) && Finite(bankImpulseNormalized);
    if (!finite) {
        SetError(errorMessage, "RailDodgeDefinition contains a non-finite value.");
        return false;
    }
    if (activeDurationSeconds <= 0.0f || recoveryDurationSeconds < 0.0f ||
        cooldownSeconds < activeDurationSeconds ||
        invulnerabilityDurationSeconds < 0.0f ||
        invulnerabilityDurationSeconds > cooldownSeconds ||
        lateralDistance < 0.0f || verticalDistance < 0.0f ||
        (lateralDistance <= 0.0f && verticalDistance <= 0.0f) ||
        minimumDirectionalInput < 0.0f || minimumDirectionalInput > 1.0f ||
        activeMovementScale < 0.0f || activeMovementScale > 1.0f ||
        recoveryMovementScale < 0.0f || recoveryMovementScale > 1.0f ||
        bankImpulseNormalized < 0.0f || bankImpulseNormalized > 1.0f) {
        SetError(errorMessage, "Rail dodge tuning contains an invalid range.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailDodgeSystem::RailDodgeSystem() {
    (void)Initialize(RailDodgeDefinition::RailShooterDefaults(), nullptr);
}

bool RailDodgeSystem::Initialize(
    const RailDodgeDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    initialized_ = true;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailDodgeSystem::Reset() {
    state_ = {};
    state_.phase = RailDodgePhase::Ready;
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
}

bool RailDodgeSystem::RestoreState(
    const RailDodgeRuntimeState& restored,
    std::string* errorMessage) {
    if (!initialized_) {
        SetError(errorMessage, "Rail dodge system is not initialized.");
        return false;
    }
    if (!Finite(restored.phaseElapsedSeconds) ||
        !Finite(restored.cooldownRemainingSeconds) ||
        !Finite(restored.invulnerabilityRemainingSeconds) ||
        !Finite(restored.directionX) || !Finite(restored.directionY) ||
        restored.phaseElapsedSeconds < 0.0f ||
        restored.cooldownRemainingSeconds < 0.0f ||
        restored.cooldownRemainingSeconds > definition_.cooldownSeconds + 0.001f ||
        restored.invulnerabilityRemainingSeconds < 0.0f ||
        restored.invulnerabilityRemainingSeconds >
            definition_.invulnerabilityDurationSeconds + 0.001f ||
        restored.phase > RailDodgePhase::Cooldown) {
        SetError(errorMessage, "Rail dodge checkpoint is invalid for the active definition.");
        return false;
    }
    state_ = restored;
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
    frame_.invulnerable = state_.invulnerabilityRemainingSeconds > 0.0f;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool RailDodgeSystem::CanDodge() const noexcept {
    return initialized_ && state_.phase == RailDodgePhase::Ready &&
        state_.cooldownRemainingSeconds <= 0.0f;
}

const RailDodgeFrame& RailDodgeSystem::Update(const RailDodgeInput& input) {
    frame_ = {};
    if (!initialized_) return frame_;
    const float deltaTime = Finite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;

    state_.cooldownRemainingSeconds = (std::max)(
        0.0f,
        state_.cooldownRemainingSeconds - deltaTime);
    state_.invulnerabilityRemainingSeconds = (std::max)(
        0.0f,
        state_.invulnerabilityRemainingSeconds - deltaTime);

    if (input.inputEnabled && input.dodgePressed && CanDodge()) {
        BeginDodge(input);
        frame_.startedThisFrame = true;
    }

    if (state_.phase == RailDodgePhase::Active) {
        AdvanceActive(deltaTime);
    } else if (state_.phase == RailDodgePhase::Recovery) {
        state_.phaseElapsedSeconds += deltaTime;
        if (state_.phaseElapsedSeconds >= definition_.recoveryDurationSeconds) {
            state_.phase = state_.cooldownRemainingSeconds > 0.0f
                ? RailDodgePhase::Cooldown
                : RailDodgePhase::Ready;
            state_.phaseElapsedSeconds = 0.0f;
            frame_.becameReadyThisFrame = state_.phase == RailDodgePhase::Ready;
        }
    } else if (state_.phase == RailDodgePhase::Cooldown &&
               state_.cooldownRemainingSeconds <= 0.0f) {
        state_.phase = RailDodgePhase::Ready;
        state_.phaseElapsedSeconds = 0.0f;
        frame_.becameReadyThisFrame = true;
    }

    switch (state_.phase) {
    case RailDodgePhase::Active:
        frame_.movementScale = definition_.activeMovementScale;
        frame_.bankNormalized =
            -state_.directionX * definition_.bankImpulseNormalized;
        break;
    case RailDodgePhase::Recovery:
        frame_.movementScale = definition_.recoveryMovementScale;
        frame_.bankNormalized =
            -state_.directionX * definition_.bankImpulseNormalized * 0.35f;
        break;
    case RailDodgePhase::Ready:
    case RailDodgePhase::Cooldown:
        frame_.movementScale = 1.0f;
        break;
    }
    frame_.invulnerable = state_.invulnerabilityRemainingSeconds > 0.0f;
    ++state_.revision;
    frame_.state = state_;
    return frame_;
}

void RailDodgeSystem::BeginDodge(const RailDodgeInput& input) {
    float directionX = Finite(input.directionX) ? input.directionX : 0.0f;
    float directionY = Finite(input.directionY) ? input.directionY : 0.0f;
    float magnitude = std::sqrt(directionX * directionX + directionY * directionY);
    if (magnitude < definition_.minimumDirectionalInput) {
        directionX = Finite(input.movementVelocityX) ? input.movementVelocityX : 0.0f;
        directionY = Finite(input.movementVelocityY) ? input.movementVelocityY : 0.0f;
        magnitude = std::sqrt(directionX * directionX + directionY * directionY);
    }
    if (magnitude < 0.0001f) {
        // Alternate the fallback side so repeated neutral dodges do not always
        // bias the player toward one course boundary.
        directionX = (state_.dodgeCount % 2u) == 0u ? 1.0f : -1.0f;
        directionY = 0.0f;
        magnitude = 1.0f;
    }
    state_.directionX = directionX / magnitude;
    state_.directionY = directionY / magnitude;
    state_.phase = RailDodgePhase::Active;
    state_.phaseElapsedSeconds = 0.0f;
    state_.cooldownRemainingSeconds = definition_.cooldownSeconds;
    state_.invulnerabilityRemainingSeconds =
        definition_.invulnerabilityDurationSeconds;
    ++state_.dodgeCount;
    ++state_.eventSequence;
}

void RailDodgeSystem::AdvanceActive(float deltaTime) {
    const float beforeProgress = (std::clamp)(
        state_.phaseElapsedSeconds / definition_.activeDurationSeconds,
        0.0f,
        1.0f);
    state_.phaseElapsedSeconds += deltaTime;
    const float afterProgress = (std::clamp)(
        state_.phaseElapsedSeconds / definition_.activeDurationSeconds,
        0.0f,
        1.0f);
    const float deltaCurve = EaseOutCubic(afterProgress) - EaseOutCubic(beforeProgress);
    frame_.lateralDisplacement =
        state_.directionX * definition_.lateralDistance * deltaCurve;
    frame_.verticalDisplacement =
        state_.directionY * definition_.verticalDistance * deltaCurve;
    if (afterProgress >= 1.0f) {
        state_.phase = definition_.recoveryDurationSeconds > 0.0f
            ? RailDodgePhase::Recovery
            : (state_.cooldownRemainingSeconds > 0.0f
                ? RailDodgePhase::Cooldown
                : RailDodgePhase::Ready);
        state_.phaseElapsedSeconds = 0.0f;
        frame_.endedThisFrame = true;
    }
}

const char* ToString(RailDodgePhase phase) {
    switch (phase) {
    case RailDodgePhase::Ready: return "Ready";
    case RailDodgePhase::Active: return "Active";
    case RailDodgePhase::Recovery: return "Recovery";
    case RailDodgePhase::Cooldown: return "Cooldown";
    }
    return "Unknown";
}

