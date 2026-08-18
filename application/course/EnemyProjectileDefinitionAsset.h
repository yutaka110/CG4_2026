#pragma once

#include <cstdint>
#include <string>

#include "utils/math/Vector.h"

enum class EnemyProjectileTrajectory : uint8_t {
    Direct,
    Predictive,
    Homing,
    Arc,
};

// Immutable projectile contract resolved during Wave compilation/cook.
struct EnemyProjectileDefinitionAsset final {
    std::string id;
    std::string displayName;
    EnemyProjectileTrajectory trajectory = EnemyProjectileTrajectory::Direct;
    float initialSpeed = 48.0f;
    float acceleration = 0.0f;
    float maximumSpeed = 48.0f;
    float homingTurnRateRadians = 0.0f;
    float predictionScale = 1.0f;
    float maximumPredictionSeconds = 1.2f;
    float arcGravity = 0.0f;
    float radius = 0.34f;
    float lifetime = 4.0f;
    float damage = 8.0f;
    Vector4 color{1.0f, 0.18f, 0.08f, 1.0f};
    std::string trailEffectId = "enemy_projectile_trail";
    std::string impactEffectId = "ice_impact";

    bool LoadFromFile(
        const std::string& path,
        std::string* errorMessage = nullptr);
    bool Validate(std::string* errorMessage = nullptr) const;

    static EnemyProjectileDefinitionAsset LegacyDirect();
};

bool TryParseEnemyProjectileTrajectory(
    const std::string& text,
    EnemyProjectileTrajectory& trajectory) noexcept;
const char* ToString(EnemyProjectileTrajectory trajectory) noexcept;
