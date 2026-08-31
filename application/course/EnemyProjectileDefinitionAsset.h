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

enum class EnemyAttackDefenseResponse : uint32_t {
    None = 0,
    ShootDown = 1u << 0u,
    Interrupt = 1u << 1u,
    LeanLeft = 1u << 2u,
    LeanRight = 1u << 3u,
    Duck = 1u << 4u,
};

constexpr EnemyAttackDefenseResponse operator|(
    EnemyAttackDefenseResponse left,
    EnemyAttackDefenseResponse right) noexcept {
    return static_cast<EnemyAttackDefenseResponse>(
        static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

constexpr bool HasDefenseResponse(
    EnemyAttackDefenseResponse responses,
    EnemyAttackDefenseResponse response) noexcept {
    return (static_cast<uint32_t>(responses) &
        static_cast<uint32_t>(response)) != 0u;
}

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
    EnemyAttackDefenseResponse defenseResponses =
        EnemyAttackDefenseResponse::ShootDown |
        EnemyAttackDefenseResponse::Interrupt |
        EnemyAttackDefenseResponse::LeanLeft |
        EnemyAttackDefenseResponse::LeanRight;
    float shootDownHitPoints = 1.0f;
    float shootDownRadiusScale = 1.65f;

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
bool TryParseEnemyAttackDefenseResponses(
    const std::string& text,
    EnemyAttackDefenseResponse& responses) noexcept;
