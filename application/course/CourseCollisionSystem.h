#pragma once

#include <cstdint>

#include "../diagnostics/DebugDrawSystem.h"
#include "../terrain/RailPath.h"
#include "CourseSpawnRuntime.h"

struct CourseCollisionPlayerState {
    float distance = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 4.0f;
    float radius = 1.6f;
    float hitPoints = 100.0f;
    float invulnerabilityTime = 0.0f;
};

struct CourseCollisionWeaponState {
    bool enabled = true;
    float shotTimer = 0.0f;
    float shotInterval = 0.12f;
    float range = 96.0f;
    float radius = 2.2f;
    float damage = 18.0f;
};

struct CourseCollisionFrameInput {
    float deltaTime = 0.016f;
    CourseCollisionPlayerState player;
    CourseCollisionWeaponState weapon;
};

struct CourseCollisionFrameStats {
    uint32_t enemyBulletHits = 0;
    uint32_t obstacleHits = 0;
    uint32_t playerShotsFired = 0;
    uint32_t playerShotEnemyHits = 0;
    uint32_t playerShotObstacleHits = 0;
    float playerDamage = 0.0f;
};

class CourseCollisionSystem {
public:
    void Reset();
    CourseCollisionFrameStats Update(CourseSpawnRuntime& runtime, const CourseCollisionFrameInput& input);
    void AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw, const RailPath& railPath) const;

    const CourseCollisionPlayerState& Player() const { return player_; }
    const CourseCollisionWeaponState& Weapon() const { return weapon_; }
    const CourseCollisionFrameStats& LastFrameStats() const { return lastFrameStats_; }

private:
    void FirePlayerShot(CourseSpawnRuntime& runtime, const CourseCollisionFrameInput& input);
    void LogFrameStats(const CourseCollisionFrameStats& stats) const;

    CourseCollisionPlayerState player_{};
    CourseCollisionWeaponState weapon_{};
    CourseCollisionFrameStats lastFrameStats_{};
    float lastShotDistance_ = 0.0f;
    float lastShotLateralOffset_ = 0.0f;
    float lastShotVerticalOffset_ = 0.0f;
    bool lastShotVisible_ = false;
};
