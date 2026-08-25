#include "RailVehicleMountedEvasionSystem.h"

#include <algorithm>
#include <cmath>

namespace {

bool Finite(float value) noexcept { return std::isfinite(value); }

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

float SmoothStep(float value) noexcept {
    const float x = (std::clamp)(value, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

Vector3 Add(Vector3 lhs, Vector3 rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vector3 Scale(Vector3 value, float amount) noexcept {
    return {value.x * amount, value.y * amount, value.z * amount};
}

} // namespace

RailVehicleMountedEvasionDefinition
RailVehicleMountedEvasionDefinition::MineCartDefaults() {
    return {};
}

bool RailVehicleMountedEvasionDefinition::Validate(
    std::string* errorMessage) const {
    const bool valid = Finite(evadeDurationSeconds) &&
        Finite(recoveryDurationSeconds) && Finite(cooldownSeconds) &&
        Finite(invulnerabilityDurationSeconds) && Finite(lateralDistance) &&
        Finite(verticalDistance) && Finite(minimumDirectionalInput) &&
        Finite(maximumSubstepSeconds) && evadeDurationSeconds > 0.0f &&
        recoveryDurationSeconds >= 0.0f &&
        cooldownSeconds >= evadeDurationSeconds &&
        invulnerabilityDurationSeconds >= 0.0f &&
        invulnerabilityDurationSeconds <= cooldownSeconds &&
        lateralDistance >= 0.0f && verticalDistance >= 0.0f &&
        lateralDistance + verticalDistance > 0.0f &&
        minimumDirectionalInput >= 0.0f && minimumDirectionalInput <= 1.0f &&
        maximumSubstepSeconds > 0.0f && maximumSubstepSeconds <= 0.05f;
    if (!valid) {
        SetError(errorMessage, "Mounted evasion definition is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailVehicleMountedEvasionSystem::RailVehicleMountedEvasionSystem() {
    (void)Initialize(
        RailVehicleMountedEvasionDefinition::MineCartDefaults(), nullptr);
}

bool RailVehicleMountedEvasionSystem::Initialize(
    const RailVehicleMountedEvasionDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    initialized_ = true;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleMountedEvasionSystem::Reset() {
    state_ = {};
    state_.phase = RailVehicleMountedEvasionPhase::Ready;
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
}

bool RailVehicleMountedEvasionSystem::RestoreState(
    const RailVehicleMountedEvasionRuntimeState& restored,
    std::string* errorMessage) {
    if (!initialized_ || restored.phase >
            RailVehicleMountedEvasionPhase::Cooldown ||
        !Finite(restored.phaseElapsedSeconds) ||
        !Finite(restored.cooldownRemainingSeconds) ||
        !Finite(restored.invulnerabilityRemainingSeconds) ||
        !Finite(restored.directionX) || !Finite(restored.directionY) ||
        !Finite(restored.lateralOffset) || !Finite(restored.verticalOffset) ||
        !Finite(restored.distanceScale) ||
        restored.phaseElapsedSeconds < 0.0f ||
        restored.cooldownRemainingSeconds < 0.0f ||
        restored.cooldownRemainingSeconds > definition_.cooldownSeconds + 0.001f ||
        restored.invulnerabilityRemainingSeconds < 0.0f ||
        restored.invulnerabilityRemainingSeconds >
            definition_.invulnerabilityDurationSeconds + 0.001f ||
        restored.distanceScale < 0.0f || restored.distanceScale > 1.0f ||
        std::abs(restored.lateralOffset) > definition_.lateralDistance + 0.001f ||
        std::abs(restored.verticalOffset) > definition_.verticalDistance + 0.001f) {
        SetError(errorMessage, "Mounted evasion checkpoint is invalid.");
        return false;
    }
    state_ = restored;
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool RailVehicleMountedEvasionSystem::CanEvade() const noexcept {
    return initialized_ &&
        state_.phase == RailVehicleMountedEvasionPhase::Ready &&
        state_.cooldownRemainingSeconds <= 0.0f;
}

const RailVehicleMountedEvasionFrame&
RailVehicleMountedEvasionSystem::Update(
    const RailVehicleMountedEvasionInput& input) {
    frame_ = {};
    if (!initialized_) return frame_;
    const bool mounted = input.occupantMounted &&
        input.vehicleDefinition != nullptr && input.vehicleState != nullptr &&
        input.vehicleState->initialized &&
        input.vehicleDefinition->mountedMovementMode ==
            RailVehicleMountedMovementMode::VehicleMounted;
    const float totalDelta = Finite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    if (!mounted) {
        if (state_.phase != RailVehicleMountedEvasionPhase::Ready ||
            state_.lateralOffset != 0.0f || state_.verticalOffset != 0.0f) {
            Reset();
        }
        BuildFrame(input);
        return frame_;
    }

    state_.cooldownRemainingSeconds = (std::max)(
        0.0f, state_.cooldownRemainingSeconds - totalDelta);
    state_.invulnerabilityRemainingSeconds = (std::max)(
        0.0f, state_.invulnerabilityRemainingSeconds - totalDelta);
    if (input.inputEnabled && input.evadePressed && CanEvade()) {
        BeginEvasion(input);
        frame_.startedThisFrame = true;
    }
    if (state_.phase == RailVehicleMountedEvasionPhase::Evading ||
        state_.phase == RailVehicleMountedEvasionPhase::Recovering) {
        const float maximumDistanceScale = Finite(input.maximumDistanceScale)
            ? (std::clamp)(input.maximumDistanceScale, 0.0f, 1.0f)
            : 0.0f;
        state_.distanceScale = (std::min)(
            state_.distanceScale, maximumDistanceScale);
    }

    float remaining = totalDelta;
    while (remaining > 0.000001f) {
        const float step = (std::min)(
            remaining, definition_.maximumSubstepSeconds);
        Advance(step);
        remaining -= step;
    }
    if (state_.phase == RailVehicleMountedEvasionPhase::Cooldown &&
        state_.cooldownRemainingSeconds <= 0.0f) {
        state_.phase = RailVehicleMountedEvasionPhase::Ready;
        state_.phaseElapsedSeconds = 0.0f;
        state_.distanceScale = 1.0f;
        frame_.becameReadyThisFrame = true;
    }
    ++state_.revision;
    BuildFrame(input);
    return frame_;
}

void RailVehicleMountedEvasionSystem::BeginEvasion(
    const RailVehicleMountedEvasionInput& input) {
    float x = Finite(input.directionX) ? input.directionX : 0.0f;
    float y = Finite(input.directionY) ? input.directionY : 0.0f;
    float length = std::sqrt(x * x + y * y);
    if (length < definition_.minimumDirectionalInput) {
        x = (state_.evasionCount % 2u) == 0u ? 1.0f : -1.0f;
        y = 0.0f;
        length = 1.0f;
    }
    state_.directionX = x / length;
    state_.directionY = y / length;
    state_.phase = RailVehicleMountedEvasionPhase::Evading;
    state_.phaseElapsedSeconds = 0.0f;
    state_.cooldownRemainingSeconds = definition_.cooldownSeconds;
    state_.invulnerabilityRemainingSeconds =
        definition_.invulnerabilityDurationSeconds;
    state_.distanceScale = Finite(input.maximumDistanceScale)
        ? (std::clamp)(input.maximumDistanceScale, 0.0f, 1.0f)
        : 0.0f;
    ++state_.evasionCount;
    ++state_.eventSequence;
}

void RailVehicleMountedEvasionSystem::Advance(float deltaTime) {
    if (state_.phase == RailVehicleMountedEvasionPhase::Evading) {
        state_.phaseElapsedSeconds += deltaTime;
        const float progress = (std::clamp)(
            state_.phaseElapsedSeconds / definition_.evadeDurationSeconds,
            0.0f, 1.0f);
        const float strength = SmoothStep(progress);
        state_.lateralOffset =
            state_.directionX * definition_.lateralDistance *
            state_.distanceScale * strength;
        state_.verticalOffset =
            state_.directionY * definition_.verticalDistance *
            state_.distanceScale * strength;
        if (progress >= 1.0f) {
            state_.phase = definition_.recoveryDurationSeconds > 0.0f
                ? RailVehicleMountedEvasionPhase::Recovering
                : RailVehicleMountedEvasionPhase::Cooldown;
            state_.phaseElapsedSeconds = 0.0f;
            frame_.endedThisFrame = true;
        }
    } else if (state_.phase == RailVehicleMountedEvasionPhase::Recovering) {
        state_.phaseElapsedSeconds += deltaTime;
        const float progress = (std::clamp)(
            state_.phaseElapsedSeconds /
                (std::max)(0.0001f, definition_.recoveryDurationSeconds),
            0.0f, 1.0f);
        const float strength = 1.0f - SmoothStep(progress);
        state_.lateralOffset =
            state_.directionX * definition_.lateralDistance *
            state_.distanceScale * strength;
        state_.verticalOffset =
            state_.directionY * definition_.verticalDistance *
            state_.distanceScale * strength;
        if (progress >= 1.0f) {
            state_.lateralOffset = 0.0f;
            state_.verticalOffset = 0.0f;
            state_.phase = state_.cooldownRemainingSeconds > 0.0f
                ? RailVehicleMountedEvasionPhase::Cooldown
                : RailVehicleMountedEvasionPhase::Ready;
            state_.phaseElapsedSeconds = 0.0f;
            frame_.becameReadyThisFrame =
                state_.phase == RailVehicleMountedEvasionPhase::Ready;
        }
    }
}

void RailVehicleMountedEvasionSystem::BuildFrame(
    const RailVehicleMountedEvasionInput& input) {
    frame_.state = state_;
    frame_.mounted = input.occupantMounted &&
        input.vehicleDefinition != nullptr && input.vehicleState != nullptr &&
        input.vehicleState->initialized &&
        input.vehicleDefinition->mountedMovementMode ==
            RailVehicleMountedMovementMode::VehicleMounted;
    // Only the outward evade compresses the combat hitbox. The occupant still
    // returns through the authored offset during Recovering, but that movement
    // must not silently extend the dodge window beyond its active phase.
    frame_.active = frame_.mounted &&
        state_.phase == RailVehicleMountedEvasionPhase::Evading;
    frame_.invulnerable = frame_.mounted &&
        state_.invulnerabilityRemainingSeconds > 0.0f;
    const float lateralDenominator = (std::max)(0.001f, definition_.lateralDistance);
    const float verticalDenominator = (std::max)(0.001f, definition_.verticalDistance);
    frame_.normalizedStrength = (std::clamp)((std::max)(
        std::abs(state_.lateralOffset) / lateralDenominator,
        std::abs(state_.verticalOffset) / verticalDenominator), 0.0f, 1.0f);
    frame_.bankNormalized = -state_.directionX * frame_.normalizedStrength;
    if (!frame_.mounted) return;
    const RailVehicleDefinition& definition = *input.vehicleDefinition;
    const RailVehicleRuntimeState& vehicle = *input.vehicleState;
    frame_.railLateralOffset = definition.mounts.player.x + state_.lateralOffset;
    frame_.railVerticalOffset = definition.bodyVerticalOffset +
        definition.mounts.player.y + state_.verticalOffset;
    frame_.occupantWorldPosition = Add(
        Add(vehicle.playerMountPosition,
            Scale(vehicle.right, state_.lateralOffset)),
        Scale(vehicle.up, state_.verticalOffset));
    frame_.sourceVehicleRevision = vehicle.revision;
}

const char* ToString(RailVehicleMountedEvasionPhase phase) noexcept {
    switch (phase) {
    case RailVehicleMountedEvasionPhase::Ready: return "Ready";
    case RailVehicleMountedEvasionPhase::Evading: return "Evading";
    case RailVehicleMountedEvasionPhase::Recovering: return "Recovering";
    case RailVehicleMountedEvasionPhase::Cooldown: return "Cooldown";
    }
    return "Unknown";
}
