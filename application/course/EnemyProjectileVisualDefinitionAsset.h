#pragma once

#include <string>

#include "EnemyProjectileDefinitionAsset.h"
#include "utils/math/Vector.h"

enum class EnemyProjectileVisualStyle : unsigned char {
    Bolt,
    Orb,
    Missile,
    Arc,
};

// Presentation-only contract for hostile projectile readability. Collision
// radius remains owned by EnemyProjectileDefinitionAsset; this asset may make
// distant fire larger without changing gameplay or near-miss evaluation.
struct EnemyProjectileVisualDefinitionAsset final {
    std::string id;
    std::string projectileDefinitionId;
    EnemyProjectileVisualStyle style = EnemyProjectileVisualStyle::Bolt;
    std::string coreEffectId = "enemy_projectile_core";
    std::string haloEffectId = "enemy_projectile_halo";
    Vector4 coreColor{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 haloColor{1.0f, 0.08f, 0.72f, 0.86f};
    Vector4 trailColor{1.0f, 0.05f, 0.58f, 0.62f};
    float coreRadiusScale = 1.65f;
    float haloRadiusScale = 2.35f;
    float minimumAngularRadius = 0.0045f;
    float maximumWorldRadius = 4.5f;
    float threatRadiusScale = 1.18f;
    float pulseAmplitude = 0.10f;
    float pulseFrequencyHz = 2.1f;
    float trailLengthInRadii = 9.0f;
    float trailWidthScale = 0.48f;
    bool enabled = true;

    bool LoadFromFile(
        const std::string& path,
        std::string* errorMessage = nullptr);
    bool Validate(std::string* errorMessage = nullptr) const;

    static EnemyProjectileVisualDefinitionAsset CommercialDefault(
        EnemyProjectileTrajectory trajectory);
};

bool TryParseEnemyProjectileVisualStyle(
    const std::string& text,
    EnemyProjectileVisualStyle& style) noexcept;
const char* ToString(EnemyProjectileVisualStyle style) noexcept;
