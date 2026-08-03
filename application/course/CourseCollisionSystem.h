#pragma once

#include <cstdint>

#include "../diagnostics/DebugDrawSystem.h"
#include "../terrain/RailPath.h"
#include "CourseAsset.h"
#include "CourseSpawnRuntime.h"
#include "RailAimState.h"
#include "WeaponDamageSystem.h"
#include "WeaponDefinitionRegistry.h"
#include "WeaponFeedbackSystem.h"
#include "WeaponFireSystem.h"

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
    bool triggerHeld = false;
    bool triggerPressed = false;
    bool triggerReleased = false;
    float shotTimer = 0.0f;
    float shotInterval = 0.12f;
    float range = 96.0f;
    float radius = 2.2f;
    float damage = 18.0f;
    float damageMultiplier = 1.0f;
    bool assistEnabled = false;
    float assistLateralOffset = 0.0f;
    float assistVerticalOffset = 4.0f;
    float muzzleForwardOffset = 3.2f;
    float tracerForwardDistance = 28.0f;
    float muzzleRadius = 0.9f;
    float tracerRadius = 1.0f;
};

struct CourseCollisionFrameInput {
    float deltaTime = 0.016f;
    const CourseAsset* course = nullptr;
    const RailAimState* worldAim = nullptr;
    CourseCollisionPlayerState player;
    CourseCollisionWeaponState weapon;
};

struct CourseCollisionFrameStats {
    uint32_t enemyBulletHits = 0;
    uint32_t obstacleHits = 0;
    uint32_t playerShotsFired = 0;
    uint32_t playerShotWorldHits = 0;
    uint32_t playerShotEnemyHits = 0;
    uint32_t playerShotObstacleHits = 0;
    uint32_t playerShotTerrainHits = 0;
    uint32_t playerShotStaleHits = 0;
    float playerDamage = 0.0f;
};

class CourseCollisionSystem {
public:
    CourseCollisionSystem();
    void Reset();
    CourseCollisionFrameStats Update(CourseSpawnRuntime& runtime, const CourseCollisionFrameInput& input);
    void AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw, const RailPath& railPath) const;

    const CourseCollisionPlayerState& Player() const { return player_; }
    const CourseCollisionWeaponState& Weapon() const { return weapon_; }
    const CourseCollisionFrameStats& LastFrameStats() const { return lastFrameStats_; }
    bool LastShotVisible() const { return lastShotVisible_; }
    float LastShotDistance() const { return lastShotDistance_; }
    float LastShotLateralOffset() const { return lastShotLateralOffset_; }
    float LastShotVerticalOffset() const { return lastShotVerticalOffset_; }
    const Vector3& LastShotWorldPoint() const { return lastShotWorldPoint_; }
    const Vector3& LastShotWorldNormal() const { return lastShotWorldNormal_; }
    RailAimHitKind LastShotHitKind() const { return lastShotHitKind_; }
    uint32_t LastShotHitActorId() const { return lastShotHitActorId_; }
    bool LastShotHasWorldPoint() const { return lastShotHasWorldPoint_; }
    bool LastShotHasWorldHit() const { return lastShotHasWorldHit_; }
    const WeaponHitRequest& LastWeaponHitRequest() const { return lastWeaponHitRequest_; }
    const DamageResult& LastDamageResult() const { return lastDamageResult_; }
    const WeaponFeedbackDispatchResult& LastWeaponFeedbackResult() const {
        return lastWeaponFeedbackResult_;
    }
    const WeaponFeedbackSystem& WeaponFeedback() const { return weaponFeedbackSystem_; }
    const WeaponFireResult& LastWeaponFireResult() const { return lastWeaponFireResult_; }
    WeaponFireSystem& WeaponFire() { return weaponFireSystem_; }
    const WeaponFireSystem& WeaponFire() const { return weaponFireSystem_; }
    bool LoadWeaponDefinitions(
        const std::filesystem::path& directory,
        std::string* errorMessage = nullptr);
    WeaponDefinitionReloadReport ReloadChangedWeaponDefinitions();
    const WeaponDefinitionAsset* FindWeaponDefinition(const std::string& weaponId) const {
        return weaponDefinitionRegistry_.Find(weaponId);
    }
    const WeaponDefinitionRegistry& WeaponDefinitions() const {
        return weaponDefinitionRegistry_;
    }
    WeaponFireResult UpdateWeaponFire(const WeaponFireInput& input);
    DamageResult ApplyWeaponHit(
        CourseSpawnRuntime& runtime,
        const CourseAsset* course,
        const WeaponHitRequest& request);

private:
    void FirePlayerShot(
        CourseSpawnRuntime& runtime,
        const CourseCollisionFrameInput& input,
        const WeaponShot& shot);
    void LogFrameStats(const CourseCollisionFrameStats& stats) const;

    CourseCollisionPlayerState player_{};
    CourseCollisionWeaponState weapon_{};
    CourseCollisionFrameStats lastFrameStats_{};
    float lastShotDistance_ = 0.0f;
    float lastShotLateralOffset_ = 0.0f;
    float lastShotVerticalOffset_ = 0.0f;
    Vector3 lastShotWorldPoint_{};
    Vector3 lastShotWorldNormal_{};
    RailAimHitKind lastShotHitKind_ = RailAimHitKind::None;
    uint32_t lastShotHitActorId_ = 0;
    bool lastShotHasWorldPoint_ = false;
    bool lastShotHasWorldHit_ = false;
    bool lastShotVisible_ = false;
    WeaponHitRequest lastWeaponHitRequest_{};
    DamageResult lastDamageResult_{};
    WeaponFeedbackDispatchResult lastWeaponFeedbackResult_{};
    WeaponFireResult lastWeaponFireResult_{};
    WeaponDefinitionRegistry weaponDefinitionRegistry_{};
    WeaponFireSystem weaponFireSystem_{};
    WeaponFeedbackSystem weaponFeedbackSystem_{};
    CourseActorDamageReceiver damageReceiver_{};
};
