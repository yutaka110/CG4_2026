#pragma once

#include <cstdint>
#include <string>

#include "EnemyProjectileSystem.h"

enum class PlayerProjectileContactKind : uint8_t {
    None,
    NearMiss,
    Hit,
};

struct PlayerHitboxDefinition final {
    std::string definitionId = "player.mine_cart_occupant";
    float hurtRadius = 0.82f;
    float nearMissOuterRadius = 3.25f;
    float dodgeHurtRadiusScale = 0.56f;
    float minimumHurtRadius = 0.34f;
    float forwardOffset = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float motionHistoryResetDistance = 24.0f;

    static PlayerHitboxDefinition RailVehicleOccupantDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
};

// Serializable hitbox authority. Coordinates remain rail-local so CCD stays
// deterministic and independent of render interpolation or camera motion.
struct PlayerHitboxRuntimeState final {
    float distance = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 4.0f;
    float previousDistance = 0.0f;
    float previousLateralOffset = 0.0f;
    float previousVerticalOffset = 4.0f;
    float hurtRadius = 0.82f;
    float nearMissOuterRadius = 3.25f;
    bool dodgeActive = false;
    bool invulnerable = false;
    bool motionHistoryResetThisFrame = false;
    bool initialized = false;
    uint64_t frameIndex = 0;
    uint64_t revision = 0;
};

struct PlayerHitboxFrameInput final {
    float distance = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 4.0f;
    bool dodgeActive = false;
    bool invulnerable = false;
    float hurtRadiusScale = 1.0f;
    bool resetMotionHistory = false;
};

struct PlayerProjectileContact final {
    PlayerProjectileContactKind kind = PlayerProjectileContactKind::None;
    uint64_t projectileId = 0;
    float closestTime = 0.0f;
    float closestCenterDistance = 0.0f;
    float surfaceSeparation = 0.0f;
    float nearMissCloseness = 0.0f;
    float closestRailDistance = 0.0f;
    float closestLateralOffset = 0.0f;
    float closestVerticalOffset = 0.0f;
};

class PlayerHitboxSystem final {
public:
    PlayerHitboxSystem();

    bool Initialize(
        const PlayerHitboxDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    bool RestoreState(
        const PlayerHitboxRuntimeState& state,
        std::string* errorMessage = nullptr);
    const PlayerHitboxRuntimeState& Update(
        const PlayerHitboxFrameInput& input);
    PlayerProjectileContact EvaluateProjectile(
        const EnemyProjectileRuntimeState& projectile) const noexcept;

    const PlayerHitboxDefinition& Definition() const noexcept {
        return definition_;
    }
    const PlayerHitboxRuntimeState& State() const noexcept { return state_; }

private:
    PlayerHitboxDefinition definition_{};
    PlayerHitboxRuntimeState state_{};
};

const char* ToString(PlayerProjectileContactKind kind) noexcept;
