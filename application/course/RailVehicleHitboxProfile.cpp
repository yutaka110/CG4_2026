#include "RailVehicleHitboxProfile.h"

#include <cmath>

namespace {

bool Finite(float value) noexcept { return std::isfinite(value); }

bool FiniteVector(Vector3 value) noexcept {
    return Finite(value.x) && Finite(value.y) && Finite(value.z);
}

void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

} // namespace

RailVehicleHitboxProfile RailVehicleHitboxProfile::MineCartDefaults() {
    return {};
}

bool RailVehicleHitboxProfile::Validate(std::string* errorMessage) const {
    if (profileId.empty() || profileId.size() > 96 ||
        !FiniteVector(bodyHalfExtents) || bodyHalfExtents.x <= 0.0f ||
        bodyHalfExtents.y <= 0.0f || bodyHalfExtents.z <= 0.0f ||
        !Finite(occupantHurtRadius) || occupantHurtRadius <= 0.0f ||
        !Finite(occupantNearMissOuterRadius) ||
        occupantNearMissOuterRadius <= occupantHurtRadius ||
        !Finite(evasionHurtRadiusScale) || evasionHurtRadiusScale <= 0.0f ||
        evasionHurtRadiusScale > 1.0f || !Finite(minimumHurtRadius) ||
        minimumHurtRadius <= 0.0f || minimumHurtRadius > occupantHurtRadius ||
        !Finite(occupantForwardOffset) || !Finite(occupantLateralOffset) ||
        !Finite(occupantVerticalOffset) || !Finite(clearanceSafetyMargin) ||
        clearanceSafetyMargin < 0.0f ||
        !Finite(motionHistoryResetDistance) ||
        motionHistoryResetDistance <= 0.0f) {
        SetError(errorMessage, "Rail vehicle hitbox geometry is invalid.");
        return false;
    }
    if (!Finite(obstacleContactDamage) || obstacleContactDamage < 0.0f ||
        !Finite(terrainContactDamage) || terrainContactDamage < 0.0f ||
        !Finite(proceduralTerrainContactDamage) ||
        proceduralTerrainContactDamage < 0.0f ||
        !Finite(postContactInvulnerabilitySeconds) ||
        postContactInvulnerabilitySeconds < 0.0f ||
        postContactInvulnerabilitySeconds > 5.0f ||
        !Finite(repeatedContactIntervalSeconds) ||
        repeatedContactIntervalSeconds < 0.01f ||
        repeatedContactIntervalSeconds > 10.0f ||
        !Finite(minimumDamageImpactSpeed) || minimumDamageImpactSpeed < 0.0f ||
        maximumObstacleCandidates == 0 || maximumTerrainCandidates == 0 ||
        proceduralSweepSamples < 2 || proceduralSweepSamples > 128 ||
        proceduralRefinementSteps > 16) {
        SetError(errorMessage, "Rail vehicle collision or damage budget is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

PlayerHitboxDefinition
RailVehicleHitboxProfile::BuildPlayerHitboxDefinition() const {
    PlayerHitboxDefinition definition{};
    definition.definitionId = profileId + ".occupant";
    definition.hurtRadius = occupantHurtRadius;
    definition.nearMissOuterRadius = occupantNearMissOuterRadius;
    definition.dodgeHurtRadiusScale = evasionHurtRadiusScale;
    definition.minimumHurtRadius = minimumHurtRadius;
    definition.forwardOffset = occupantForwardOffset;
    definition.lateralOffset = occupantLateralOffset;
    definition.verticalOffset = occupantVerticalOffset;
    definition.motionHistoryResetDistance = motionHistoryResetDistance;
    return definition;
}

void RailVehicleHitboxProfile::ApplyAuthoritativeShapes(
    RailVehicleDefinition& vehicle,
    RailVehicleOccupantClearanceDefinition& clearance) const {
    vehicle.collisionHalfExtents = bodyHalfExtents;
    clearance.occupantRadius = occupantHurtRadius;
    clearance.safetyMargin = clearanceSafetyMargin;
}
