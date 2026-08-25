#pragma once

#include <cstdint>
#include <string>

#include "PlayerHitboxSystem.h"
#include "RailVehicleMovementSystem.h"
#include "RailVehicleOccupantClearanceSystem.h"

// One source of truth for the mounted vehicle body, occupant hurt volume,
// near-miss shell and evasion clearance. Query and damage budgets live beside
// the shape so a preset cannot hot-reload mutually incompatible dimensions.
struct RailVehicleHitboxProfile final {
    std::string profileId = "rail_vehicle.mine_cart.hitbox";
    Vector3 bodyHalfExtents{2.4f, 1.4f, 4.0f};
    float occupantHurtRadius = 0.82f;
    float occupantNearMissOuterRadius = 3.25f;
    float evasionHurtRadiusScale = 0.56f;
    float minimumHurtRadius = 0.34f;
    float occupantForwardOffset = 0.0f;
    float occupantLateralOffset = 0.0f;
    float occupantVerticalOffset = 0.0f;
    float clearanceSafetyMargin = 0.12f;
    float motionHistoryResetDistance = 24.0f;
    float obstacleContactDamage = 24.0f;
    float terrainContactDamage = 20.0f;
    float proceduralTerrainContactDamage = 20.0f;
    float postContactInvulnerabilitySeconds = 0.80f;
    float repeatedContactIntervalSeconds = 0.80f;
    float minimumDamageImpactSpeed = 1.0f;
    uint32_t maximumObstacleCandidates = 128;
    uint32_t maximumTerrainCandidates = 256;
    uint32_t proceduralSweepSamples = 12;
    uint32_t proceduralRefinementSteps = 5;
    bool includeDynamicObstacles = true;
    bool includeTerrainPlacements = true;
    bool includeProceduralTerrain = true;

    static RailVehicleHitboxProfile MineCartDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
    PlayerHitboxDefinition BuildPlayerHitboxDefinition() const;
    void ApplyAuthoritativeShapes(
        RailVehicleDefinition& vehicle,
        RailVehicleOccupantClearanceDefinition& clearance) const;
};
