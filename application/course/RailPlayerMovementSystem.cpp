#include "RailPlayerMovementSystem.h"

#include <algorithm>
#include <cmath>

namespace {

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

bool Finite(float value) noexcept {
    return std::isfinite(value);
}

float Approach(float current, float target, float maximumDelta) noexcept {
    if (current < target) return (std::min)(current + maximumDelta, target);
    return (std::max)(current - maximumDelta, target);
}

float ShapeAxis(float value, float deadZone, float exponent) noexcept {
    if (!Finite(value)) return 0.0f;
    value = (std::clamp)(value, -1.0f, 1.0f);
    const float magnitude = std::abs(value);
    if (magnitude <= deadZone) return 0.0f;
    const float normalized = (magnitude - deadZone) / (1.0f - deadZone);
    return std::copysign(std::pow(normalized, exponent), value);
}

float NormalizeRange(float value, float minimum, float maximum) noexcept {
    const float span = maximum - minimum;
    if (span <= 0.00001f) return 0.0f;
    return (std::clamp)(((value - minimum) / span) * 2.0f - 1.0f, -1.0f, 1.0f);
}

} // namespace

RailPlayerMovementDefinition RailPlayerMovementDefinition::RailShooterDefaults() {
    return {};
}

bool RailPlayerMovementDefinition::Validate(std::string* errorMessage) const {
    const bool finite =
        Finite(initialLateralOffset) && Finite(initialVerticalOffset) &&
        Finite(minimumLateralOffset) && Finite(maximumLateralOffset) &&
        Finite(minimumVerticalOffset) && Finite(maximumVerticalOffset) &&
        Finite(maximumLateralSpeed) && Finite(maximumVerticalSpeed) &&
        Finite(acceleration) && Finite(deceleration) &&
        Finite(boundaryBrakeAcceleration) && Finite(boundarySoftZone) &&
        Finite(inputDeadZone) && Finite(inputExponent) &&
        Finite(maximumBankDegrees) && Finite(maximumPitchDegrees) &&
        Finite(attitudeResponse) && Finite(maximumSubstepSeconds);
    if (!finite) {
        SetError(errorMessage, "RailPlayerMovementDefinition contains a non-finite value.");
        return false;
    }
    if (minimumLateralOffset >= maximumLateralOffset ||
        minimumVerticalOffset >= maximumVerticalOffset) {
        SetError(errorMessage, "Rail player movement bounds must have a positive extent.");
        return false;
    }
    if (initialLateralOffset < minimumLateralOffset ||
        initialLateralOffset > maximumLateralOffset ||
        initialVerticalOffset < minimumVerticalOffset ||
        initialVerticalOffset > maximumVerticalOffset) {
        SetError(errorMessage, "Rail player initial position must be inside movement bounds.");
        return false;
    }
    if (maximumLateralSpeed <= 0.0f || maximumVerticalSpeed <= 0.0f ||
        acceleration <= 0.0f || deceleration <= 0.0f ||
        boundaryBrakeAcceleration <= 0.0f || boundarySoftZone < 0.0f ||
        inputDeadZone < 0.0f || inputDeadZone >= 1.0f || inputExponent <= 0.0f ||
        maximumBankDegrees < 0.0f || maximumPitchDegrees < 0.0f ||
        attitudeResponse <= 0.0f || maximumSubstepSeconds <= 0.0f) {
        SetError(errorMessage, "Rail player movement tuning contains an invalid range.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailPlayerMovementSystem::RailPlayerMovementSystem() {
    (void)Initialize(RailPlayerMovementDefinition::RailShooterDefaults(), nullptr);
}

bool RailPlayerMovementSystem::Initialize(
    const RailPlayerMovementDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    initialized_ = true;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailPlayerMovementSystem::Reset() {
    Reset(definition_.initialLateralOffset, definition_.initialVerticalOffset);
}

void RailPlayerMovementSystem::Reset(float lateralOffset, float verticalOffset) {
    state_ = {};
    state_.lateralOffset = (std::clamp)(
        Finite(lateralOffset) ? lateralOffset : definition_.initialLateralOffset,
        definition_.minimumLateralOffset,
        definition_.maximumLateralOffset);
    state_.verticalOffset = (std::clamp)(
        Finite(verticalOffset) ? verticalOffset : definition_.initialVerticalOffset,
        definition_.minimumVerticalOffset,
        definition_.maximumVerticalOffset);
    state_.lateralNormalized = NormalizeRange(
        state_.lateralOffset,
        definition_.minimumLateralOffset,
        definition_.maximumLateralOffset);
    state_.verticalNormalized = NormalizeRange(
        state_.verticalOffset,
        definition_.minimumVerticalOffset,
        definition_.maximumVerticalOffset);
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
}

bool RailPlayerMovementSystem::RestoreState(
    const RailPlayerMovementRuntimeState& restored,
    std::string* errorMessage) {
    if (!initialized_) {
        SetError(errorMessage, "Rail player movement system is not initialized.");
        return false;
    }
    if (!Finite(restored.lateralOffset) || !Finite(restored.verticalOffset) ||
        !Finite(restored.lateralVelocity) || !Finite(restored.verticalVelocity) ||
        restored.lateralOffset < definition_.minimumLateralOffset ||
        restored.lateralOffset > definition_.maximumLateralOffset ||
        restored.verticalOffset < definition_.minimumVerticalOffset ||
        restored.verticalOffset > definition_.maximumVerticalOffset) {
        SetError(errorMessage, "Rail player movement checkpoint is invalid for the active definition.");
        return false;
    }
    state_ = restored;
    state_.lateralVelocity = (std::clamp)(
        state_.lateralVelocity,
        -definition_.maximumLateralSpeed,
        definition_.maximumLateralSpeed);
    state_.verticalVelocity = (std::clamp)(
        state_.verticalVelocity,
        -definition_.maximumVerticalSpeed,
        definition_.maximumVerticalSpeed);
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const RailPlayerMovementFrame& RailPlayerMovementSystem::Update(
    const RailPlayerMovementInput& input) {
    frame_ = {};
    if (!initialized_) return frame_;

    RailPlayerMovementInput sanitized = input;
    sanitized.deltaTime = Finite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    sanitized.movementScale = Finite(input.movementScale)
        ? (std::clamp)(input.movementScale, 0.0f, 1.0f)
        : 0.0f;
    sanitized.externalLateralDisplacement =
        Finite(input.externalLateralDisplacement)
        ? input.externalLateralDisplacement
        : 0.0f;
    sanitized.externalVerticalDisplacement =
        Finite(input.externalVerticalDisplacement)
        ? input.externalVerticalDisplacement
        : 0.0f;
    sanitized.externalBankNormalized = Finite(input.externalBankNormalized)
        ? (std::clamp)(input.externalBankNormalized, -1.0f, 1.0f)
        : 0.0f;

    state_.inputEnabled = sanitized.inputEnabled;
    state_.shapedInputX = sanitized.inputEnabled
        ? ShapeAxis(input.moveX, definition_.inputDeadZone, definition_.inputExponent)
        : 0.0f;
    state_.shapedInputY = sanitized.inputEnabled
        ? ShapeAxis(input.moveY, definition_.inputDeadZone, definition_.inputExponent)
        : 0.0f;

    float remaining = sanitized.deltaTime;
    while (remaining > 0.000001f) {
        const float step = (std::min)(remaining, definition_.maximumSubstepSeconds);
        SimulateStep(step, sanitized);
        remaining -= step;
    }

    const float beforeExternalX = state_.lateralOffset;
    const float beforeExternalY = state_.verticalOffset;
    state_.lateralOffset += sanitized.externalLateralDisplacement;
    state_.verticalOffset += sanitized.externalVerticalDisplacement;
    state_.lateralOffset = (std::clamp)(
        state_.lateralOffset,
        definition_.minimumLateralOffset,
        definition_.maximumLateralOffset);
    state_.verticalOffset = (std::clamp)(
        state_.verticalOffset,
        definition_.minimumVerticalOffset,
        definition_.maximumVerticalOffset);
    frame_.appliedLateralDisplacement += state_.lateralOffset - beforeExternalX;
    frame_.appliedVerticalDisplacement += state_.verticalOffset - beforeExternalY;
    frame_.clampedByBoundary =
        std::abs((state_.lateralOffset - beforeExternalX) -
                 sanitized.externalLateralDisplacement) > 0.0001f ||
        std::abs((state_.verticalOffset - beforeExternalY) -
                 sanitized.externalVerticalDisplacement) > 0.0001f;

    RefreshDerivedState(sanitized.deltaTime, sanitized.externalBankNormalized);
    ++state_.revision;
    frame_.state = state_;
    return frame_;
}

void RailPlayerMovementSystem::SimulateStep(
    float deltaTime,
    const RailPlayerMovementInput& input) {
    const float targetX = state_.shapedInputX * definition_.maximumLateralSpeed *
        input.movementScale;
    const float targetY = state_.shapedInputY * definition_.maximumVerticalSpeed *
        input.movementScale;
    const auto accelerationFor = [this](float current, float target) {
        const bool accelerating =
            current * target >= 0.0f && std::abs(target) > std::abs(current);
        return accelerating ? definition_.acceleration : definition_.deceleration;
    };
    state_.lateralVelocity = Approach(
        state_.lateralVelocity,
        targetX,
        accelerationFor(state_.lateralVelocity, targetX) * deltaTime);
    state_.verticalVelocity = Approach(
        state_.verticalVelocity,
        targetY,
        accelerationFor(state_.verticalVelocity, targetY) * deltaTime);

    const float lateralSoftMin =
        definition_.minimumLateralOffset + definition_.boundarySoftZone;
    const float lateralSoftMax =
        definition_.maximumLateralOffset - definition_.boundarySoftZone;
    const float verticalSoftMin =
        definition_.minimumVerticalOffset + definition_.boundarySoftZone;
    const float verticalSoftMax =
        definition_.maximumVerticalOffset - definition_.boundarySoftZone;
    if ((state_.lateralOffset <= lateralSoftMin && state_.lateralVelocity < 0.0f) ||
        (state_.lateralOffset >= lateralSoftMax && state_.lateralVelocity > 0.0f)) {
        state_.lateralVelocity = Approach(
            state_.lateralVelocity,
            0.0f,
            definition_.boundaryBrakeAcceleration * deltaTime);
    }
    if ((state_.verticalOffset <= verticalSoftMin && state_.verticalVelocity < 0.0f) ||
        (state_.verticalOffset >= verticalSoftMax && state_.verticalVelocity > 0.0f)) {
        state_.verticalVelocity = Approach(
            state_.verticalVelocity,
            0.0f,
            definition_.boundaryBrakeAcceleration * deltaTime);
    }

    const float beforeX = state_.lateralOffset;
    const float beforeY = state_.verticalOffset;
    state_.lateralOffset = (std::clamp)(
        state_.lateralOffset + state_.lateralVelocity * deltaTime,
        definition_.minimumLateralOffset,
        definition_.maximumLateralOffset);
    state_.verticalOffset = (std::clamp)(
        state_.verticalOffset + state_.verticalVelocity * deltaTime,
        definition_.minimumVerticalOffset,
        definition_.maximumVerticalOffset);
    if ((state_.lateralOffset <= definition_.minimumLateralOffset &&
         state_.lateralVelocity < 0.0f) ||
        (state_.lateralOffset >= definition_.maximumLateralOffset &&
         state_.lateralVelocity > 0.0f)) {
        state_.lateralVelocity = 0.0f;
        frame_.clampedByBoundary = true;
    }
    if ((state_.verticalOffset <= definition_.minimumVerticalOffset &&
         state_.verticalVelocity < 0.0f) ||
        (state_.verticalOffset >= definition_.maximumVerticalOffset &&
         state_.verticalVelocity > 0.0f)) {
        state_.verticalVelocity = 0.0f;
        frame_.clampedByBoundary = true;
    }
    frame_.appliedLateralDisplacement += state_.lateralOffset - beforeX;
    frame_.appliedVerticalDisplacement += state_.verticalOffset - beforeY;
}

void RailPlayerMovementSystem::RefreshDerivedState(
    float deltaTime,
    float externalBankNormalized) {
    state_.lateralNormalized = NormalizeRange(
        state_.lateralOffset,
        definition_.minimumLateralOffset,
        definition_.maximumLateralOffset);
    state_.verticalNormalized = NormalizeRange(
        state_.verticalOffset,
        definition_.minimumVerticalOffset,
        definition_.maximumVerticalOffset);
    state_.touchingLateralBoundary =
        state_.lateralOffset <= definition_.minimumLateralOffset + 0.0001f ||
        state_.lateralOffset >= definition_.maximumLateralOffset - 0.0001f;
    state_.touchingVerticalBoundary =
        state_.verticalOffset <= definition_.minimumVerticalOffset + 0.0001f ||
        state_.verticalOffset >= definition_.maximumVerticalOffset - 0.0001f;
    state_.moving =
        std::abs(state_.lateralVelocity) > 0.02f ||
        std::abs(state_.verticalVelocity) > 0.02f;

    const float velocityBank = definition_.maximumLateralSpeed > 0.0f
        ? -state_.lateralVelocity / definition_.maximumLateralSpeed
        : 0.0f;
    const float bankNormalized = (std::clamp)(
        velocityBank + externalBankNormalized,
        -1.0f,
        1.0f);
    const float pitchNormalized = definition_.maximumVerticalSpeed > 0.0f
        ? (std::clamp)(
            state_.verticalVelocity / definition_.maximumVerticalSpeed,
            -1.0f,
            1.0f)
        : 0.0f;
    const float response = 1.0f - std::exp(
        -definition_.attitudeResponse * (std::max)(0.0f, deltaTime));
    state_.bankDegrees +=
        (bankNormalized * definition_.maximumBankDegrees - state_.bankDegrees) * response;
    state_.pitchDegrees +=
        (pitchNormalized * definition_.maximumPitchDegrees - state_.pitchDegrees) * response;
}

