#include "PlayerHitboxSystem.h"

#include <algorithm>
#include <cmath>

namespace {

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

bool Finite(float value) noexcept {
    return std::isfinite(value);
}

float LengthSquared(float distance, float lateral, float vertical) noexcept {
    return distance * distance + lateral * lateral + vertical * vertical;
}

bool ValidState(const PlayerHitboxRuntimeState& state) noexcept {
    return Finite(state.distance) && Finite(state.lateralOffset) &&
        Finite(state.verticalOffset) && Finite(state.previousDistance) &&
        Finite(state.previousLateralOffset) &&
        Finite(state.previousVerticalOffset) && Finite(state.hurtRadius) &&
        Finite(state.nearMissOuterRadius) && state.hurtRadius > 0.0f &&
        state.nearMissOuterRadius > state.hurtRadius;
}

} // namespace

PlayerHitboxDefinition PlayerHitboxDefinition::RailVehicleOccupantDefaults() {
    return {};
}

bool PlayerHitboxDefinition::Validate(std::string* errorMessage) const {
    if (definitionId.empty() || !Finite(hurtRadius) ||
        !Finite(nearMissOuterRadius) || !Finite(dodgeHurtRadiusScale) ||
        !Finite(minimumHurtRadius) || !Finite(forwardOffset) ||
        !Finite(lateralOffset) || !Finite(verticalOffset) ||
        !Finite(motionHistoryResetDistance) || hurtRadius <= 0.0f ||
        nearMissOuterRadius <= hurtRadius || dodgeHurtRadiusScale <= 0.0f ||
        dodgeHurtRadiusScale > 1.0f || minimumHurtRadius <= 0.0f ||
        minimumHurtRadius > hurtRadius || motionHistoryResetDistance <= 0.0f) {
        SetError(errorMessage, "Player hitbox definition is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

PlayerHitboxSystem::PlayerHitboxSystem() {
    (void)Initialize(PlayerHitboxDefinition::RailVehicleOccupantDefaults());
}

bool PlayerHitboxSystem::Initialize(
    const PlayerHitboxDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void PlayerHitboxSystem::Reset() {
    state_ = {};
    state_.hurtRadius = definition_.hurtRadius;
    state_.nearMissOuterRadius = definition_.nearMissOuterRadius;
}

bool PlayerHitboxSystem::RestoreState(
    const PlayerHitboxRuntimeState& state,
    std::string* errorMessage) {
    if (!ValidState(state) ||
        state.hurtRadius > definition_.hurtRadius + 0.001f ||
        state.nearMissOuterRadius > definition_.nearMissOuterRadius + 0.001f) {
        SetError(errorMessage, "Player hitbox checkpoint is invalid.");
        return false;
    }
    state_ = state;
    state_.motionHistoryResetThisFrame = true;
    state_.previousDistance = state_.distance;
    state_.previousLateralOffset = state_.lateralOffset;
    state_.previousVerticalOffset = state_.verticalOffset;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const PlayerHitboxRuntimeState& PlayerHitboxSystem::Update(
    const PlayerHitboxFrameInput& input) {
    const float nextDistance = Finite(input.distance)
        ? input.distance + definition_.forwardOffset
        : state_.distance;
    const float nextLateral = Finite(input.lateralOffset)
        ? input.lateralOffset + definition_.lateralOffset
        : state_.lateralOffset;
    const float nextVertical = Finite(input.verticalOffset)
        ? input.verticalOffset + definition_.verticalOffset
        : state_.verticalOffset;
    const bool firstFrame = !state_.initialized;
    float previousDistance = firstFrame ? nextDistance : state_.distance;
    float previousLateral = firstFrame ? nextLateral : state_.lateralOffset;
    float previousVertical = firstFrame ? nextVertical : state_.verticalOffset;
    const float movementSquared = LengthSquared(
        nextDistance - previousDistance,
        nextLateral - previousLateral,
        nextVertical - previousVertical);
    const float resetDistance = definition_.motionHistoryResetDistance;
    const bool resetHistory = firstFrame || input.resetMotionHistory ||
        movementSquared > resetDistance * resetDistance;
    if (resetHistory) {
        previousDistance = nextDistance;
        previousLateral = nextLateral;
        previousVertical = nextVertical;
    }

    state_.previousDistance = previousDistance;
    state_.previousLateralOffset = previousLateral;
    state_.previousVerticalOffset = previousVertical;
    state_.distance = nextDistance;
    state_.lateralOffset = nextLateral;
    state_.verticalOffset = nextVertical;
    state_.dodgeActive = input.dodgeActive;
    state_.invulnerable = input.invulnerable;
    state_.hurtRadius = input.dodgeActive
        ? (std::max)(
            definition_.minimumHurtRadius,
            definition_.hurtRadius * definition_.dodgeHurtRadiusScale)
        : definition_.hurtRadius;
    state_.nearMissOuterRadius = definition_.nearMissOuterRadius;
    state_.motionHistoryResetThisFrame = resetHistory;
    state_.initialized = true;
    ++state_.frameIndex;
    ++state_.revision;
    return state_;
}

PlayerProjectileContact PlayerHitboxSystem::EvaluateProjectile(
    const EnemyProjectileRuntimeState& projectile) const noexcept {
    PlayerProjectileContact contact{};
    contact.projectileId = projectile.projectileId;
    if (!state_.initialized || !projectile.active ||
        projectile.age >= projectile.lifetime || !Finite(projectile.radius) ||
        projectile.radius <= 0.0f) {
        return contact;
    }

    const float projectileStartDistance =
        projectile.spawnDistance + projectile.previousDistanceOffset;
    const float projectileEndDistance =
        projectile.spawnDistance + projectile.distanceOffset;
    const float relativeStartDistance =
        projectileStartDistance - state_.previousDistance;
    const float relativeStartLateral =
        projectile.previousLateralOffset - state_.previousLateralOffset;
    const float relativeStartVertical =
        projectile.previousVerticalOffset - state_.previousVerticalOffset;
    const float relativeEndDistance = projectileEndDistance - state_.distance;
    const float relativeEndLateral =
        projectile.lateralOffset - state_.lateralOffset;
    const float relativeEndVertical =
        projectile.verticalOffset - state_.verticalOffset;
    const float segmentDistance =
        relativeEndDistance - relativeStartDistance;
    const float segmentLateral =
        relativeEndLateral - relativeStartLateral;
    const float segmentVertical =
        relativeEndVertical - relativeStartVertical;
    const float segmentLengthSquared = LengthSquared(
        segmentDistance,
        segmentLateral,
        segmentVertical);
    contact.closestTime = segmentLengthSquared > 0.000001f
        ? (std::clamp)(
            -(relativeStartDistance * segmentDistance +
              relativeStartLateral * segmentLateral +
              relativeStartVertical * segmentVertical) /
                segmentLengthSquared,
            0.0f,
            1.0f)
        : 0.0f;
    const float closestDistance =
        relativeStartDistance + segmentDistance * contact.closestTime;
    const float closestLateral =
        relativeStartLateral + segmentLateral * contact.closestTime;
    const float closestVertical =
        relativeStartVertical + segmentVertical * contact.closestTime;
    const float closestLengthSquared = LengthSquared(
        closestDistance,
        closestLateral,
        closestVertical);
    contact.closestCenterDistance = std::sqrt((std::max)(
        0.0f,
        closestLengthSquared));
    const float hitThreshold = state_.hurtRadius + projectile.radius;
    const float nearThreshold = state_.nearMissOuterRadius + projectile.radius;
    contact.surfaceSeparation = contact.closestCenterDistance - hitThreshold;
    contact.closestRailDistance =
        projectileStartDistance +
        (projectileEndDistance - projectileStartDistance) * contact.closestTime;
    contact.closestLateralOffset =
        projectile.previousLateralOffset +
        (projectile.lateralOffset - projectile.previousLateralOffset) *
            contact.closestTime;
    contact.closestVerticalOffset =
        projectile.previousVerticalOffset +
        (projectile.verticalOffset - projectile.previousVerticalOffset) *
            contact.closestTime;
    if (contact.closestCenterDistance <= hitThreshold) {
        contact.kind = PlayerProjectileContactKind::Hit;
        contact.nearMissCloseness = 1.0f;
    } else if (contact.closestCenterDistance <= nearThreshold) {
        contact.kind = PlayerProjectileContactKind::NearMiss;
        contact.nearMissCloseness = (std::clamp)(
            1.0f - contact.surfaceSeparation /
                (std::max)(0.001f, nearThreshold - hitThreshold),
            0.0f,
            1.0f);
    }
    return contact;
}

const char* ToString(PlayerProjectileContactKind kind) noexcept {
    switch (kind) {
    case PlayerProjectileContactKind::None: return "None";
    case PlayerProjectileContactKind::NearMiss: return "NearMiss";
    case PlayerProjectileContactKind::Hit: return "Hit";
    }
    return "Unknown";
}
