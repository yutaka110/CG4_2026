#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "CourseCollisionSystem.h"
#include "GrazeScoreSystem.h"

class CourseSpawnRuntime;

struct PlayerCombatFeelFrameInput {
    float deltaTime = 0.016f;
    float playerDistance = 0.0f;
    float playerLateralOffset = 0.0f;
    float playerVerticalOffset = 4.0f;
    CourseCollisionWeaponState baseWeapon;
    const CourseSpawnRuntime* spawnRuntime = nullptr;
    bool allowAimAssist = true;
    bool hasReticleAim = false;
    float reticleAimLateralOffset = 0.0f;
    float reticleAimVerticalOffset = 4.0f;
};

struct PlayerCombatFeelStats {
    uint32_t score = 0;
    uint32_t combo = 0;
    uint32_t maxCombo = 0;
    uint32_t lastLockScore = 0;
    uint32_t lastLockTokenCount = 0;
    uint32_t lastLockHitCount = 0;
    uint32_t nearMissCount = 0;
    uint32_t nearMissScore = 0;
    float comboTimer = 0.0f;
    float hitFlash = 0.0f;
    float damageFlash = 0.0f;
    float hitStopTime = 0.0f;
    float cameraShake = 0.0f;
    bool lockOnActive = false;
    bool lastLockWasMax = false;
    bool lastLockWasEarly = false;
    std::string lockOnTarget;
    float lockOnDistance = 0.0f;
};

class PlayerCombatFeelSystem {
public:
    void Reset();
    CourseCollisionWeaponState BuildWeaponState(const PlayerCombatFeelFrameInput& input);
    void ApplyCollisionStats(const CourseCollisionFrameStats& stats);
    void ApplyPlayerDamageResults(
        std::span<const PlayerDamageResult> results);
    void ApplyGrazeScoreResults(
        std::span<const GrazeScoreResult> results);
    void ApplyLockOnRelease(uint32_t tokenCount, uint32_t hitCount, uint32_t maxLockCount);
    void Update(float deltaTime);

    const PlayerCombatFeelStats& LastStats() const { return stats_; }

private:
    bool TryResolveLockOn(
        const PlayerCombatFeelFrameInput& input,
        float& lateralOffset,
        float& verticalOffset,
        float& targetDistance,
        std::string& targetName) const;

    PlayerCombatFeelStats stats_{};
};
