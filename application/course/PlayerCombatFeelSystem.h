#pragma once

#include <cstdint>
#include <string>

#include "CourseCollisionSystem.h"

class CourseSpawnRuntime;

struct PlayerCombatFeelFrameInput {
    float deltaTime = 0.016f;
    float playerDistance = 0.0f;
    float playerLateralOffset = 0.0f;
    float playerVerticalOffset = 4.0f;
    CourseCollisionWeaponState baseWeapon;
    const CourseSpawnRuntime* spawnRuntime = nullptr;
};

struct PlayerCombatFeelStats {
    uint32_t score = 0;
    uint32_t combo = 0;
    uint32_t maxCombo = 0;
    float comboTimer = 0.0f;
    float hitFlash = 0.0f;
    float damageFlash = 0.0f;
    float hitStopTime = 0.0f;
    float cameraShake = 0.0f;
    bool lockOnActive = false;
    std::string lockOnTarget;
    float lockOnDistance = 0.0f;
};

class PlayerCombatFeelSystem {
public:
    void Reset();
    CourseCollisionWeaponState BuildWeaponState(const PlayerCombatFeelFrameInput& input);
    void ApplyCollisionStats(const CourseCollisionFrameStats& stats);
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
