#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../diagnostics/DebugDrawSystem.h"
#include "../terrain/RailPath.h"
#include "EnemyAttackCoordinator.h"
#include "EnemyAttackExecutionSystem.h"
#include "EnemyCombatSystem.h"
#include "EnemyBehaviorSystem.h"
#include "EnemyProjectileDefinitionAsset.h"
#include "EnemyProjectileSystem.h"
#include "EnemyTargetingSystem.h"
#include "utils/math/Vector.h"

class EffectRuntime;

enum class CourseEnemyFirePattern {
    Single,
    Twin,
    Spread,
    BossArc,
};

struct CourseEnemyFireSafetySettings {
    bool enabled = true;
    bool requireCameraAllowsFire = true;
    float minForwardDistance = 6.0f;
    float maxForwardDistance = 150.0f;
    float minVisibleBeforeFire = 0.22f;
    float blockedRetryDelay = 0.05f;
};

struct CourseEnemyFireSafetyFrameInput {
    bool cameraAllowsEnemyFire = true;
    bool cameraStableForAiming = true;
    bool cameraHardTransition = false;
    float playerDistance = 0.0f;
    float playerLateralOffset = 0.0f;
    float playerVerticalOffset = 4.0f;
    float deltaTime = 0.016f;
    std::string cameraReason = "stable";
};

struct CourseEnemyFireSafetyStats {
    uint32_t activeEnemies = 0;
    uint32_t allowedEnemies = 0;
    uint32_t blockedByCamera = 0;
    uint32_t blockedByRange = 0;
    uint32_t blockedByVisibilityTime = 0;
    uint32_t bulletsEmitted = 0;
    std::string lastBlockedReason = "-";
    std::string lastAllowedReason = "-";
};

struct CourseEnemyActorDesc {
    std::string waveId;
    // Stable schema-v7 placement identity. Empty for legacy/event-spawned actors.
    std::string sourcePlacementGuid;
    std::string actorAssetId;
    std::string meshId = "ball";
    std::string bulletPatternId = "single_red";
    std::string projectileDefinitionId;
    std::string role = "drone";
    float spawnDistance = 0.0f;
    float distanceOffset = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float forwardSpeed = 0.0f;
    float radius = 1.2f;
    float lifetime = 8.0f;
    float hitPoints = 30.0f;
    float fireInterval = 0.8f;
    float firstShotDelay = 0.35f;
    float bulletSpeed = 48.0f;
    int bulletCount = 1;
    float bulletLateralSpreadSpeed = 0.0f;
    float bulletVerticalSpreadSpeed = 0.0f;
    float bulletRadius = 0.34f;
    float bulletLifetime = 4.0f;
    float bulletDamage = 8.0f;
    Vector4 bulletColor{1.0f, 0.18f, 0.08f, 1.0f};
    CourseEnemyFirePattern firePattern = CourseEnemyFirePattern::Single;
    Vector4 color{1.0f, 0.25f, 0.18f, 1.0f};
    Vector3 localRotation{};
    Vector3 localScale{1.0f, 1.0f, 1.0f};
    // Empty definitionId selects commercial defaults for production
    // ActorAssets and legacy-compatible timing for anonymous/event actors.
    EnemyCombatDefinition combatDefinition{};
    EnemyBehaviorDefinition behaviorDefinition{};
    EnemyProjectileDefinitionAsset projectileDefinition{};
    bool previewOnly = false;
    bool suppressFire = false;
};

struct CourseObstacleActorDesc {
    std::string id;
    std::string meshId = "rock_gate";
    std::string vfxCueId;
    std::string payload;
    float spawnDistance = 0.0f;
    float distanceOffset = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float forwardSpeed = 0.0f;
    float lifetime = 12.0f;
    float hitPoints = 80.0f;
    bool breakable = true;
    Vector3 halfExtents{3.5f, 3.0f, 3.5f};
    Vector4 color{1.0f, 0.62f, 0.12f, 1.0f};
};

struct CourseVfxCueDesc {
    std::string id;
    std::string effectName = "hit_ring";
    std::string payload;
    float spawnDistance = 0.0f;
    float distanceOffset = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float radius = 2.5f;
    float lifetime = 4.0f;
    Vector4 color{0.30f, 0.82f, 1.0f, 1.0f};
    Vector3 worldPosition{};
    bool hasWorldPosition = false;
};

struct CourseEnemyActor {
    CourseEnemyActorDesc desc;
    EnemyCombatDefinition combatDefinition{};
    EnemyCombatRuntimeState combatState{};
    EnemyBehaviorDefinition behaviorDefinition{};
    EnemyBehaviorRuntimeState behaviorState{};
    EnemyAttackRuntimeState attackState{};
    EnemyTargetingRuntimeState targetingState{};
    float age = 0.0f;
    float fireTimer = 0.0f;
    float fireVisibleTime = 0.0f;
    uint64_t fireSequence = 0;
    uint32_t bulletsEmittedThisFrame = 0;
    bool fireSafetyAllowed = false;
    std::string fireSafetyReason = "not evaluated";
    uint32_t actorId = 0;
};

struct CourseObstacleActor {
    CourseObstacleActorDesc desc;
    float age = 0.0f;
    uint32_t actorId = 0;
};

struct CourseVfxCue {
    CourseVfxCueDesc desc;
    float age = 0.0f;
    uint32_t effectInstanceId = 0;
    bool submitted = false;
};

// Retry-safe snapshot of gameplay actors. Transient VFX are deliberately not
// captured; hostile projectiles are retained in the snapshot only so a mode
// may explicitly opt into restoring them. Rail-shooter retries clear them by
// default to guarantee a readable recovery frame.
struct CourseSpawnRuntimeCheckpoint final {
    std::vector<CourseEnemyActor> enemies;
    std::vector<CourseBulletActor> bullets;
    std::vector<CourseObstacleActor> obstacles;
    uint32_t nextActorId = 1;
};

class CourseSpawnRuntime {
public:
    void Reset();
    CourseSpawnRuntimeCheckpoint CaptureCheckpoint() const;
    void RestoreCheckpoint(
        const CourseSpawnRuntimeCheckpoint& checkpoint,
        bool restoreProjectiles = false);
    void Update(float deltaTime);
    void Update(float deltaTime, const CourseEnemyFireSafetyFrameInput& safetyInput);

    void SpawnEnemyActor(CourseEnemyActorDesc desc);
    void SpawnObstacle(CourseObstacleActorDesc desc);
    void SpawnVfxCue(CourseVfxCueDesc desc);
    void SubmitPendingVfx(EffectRuntime& effectRuntime, const RailPath& railPath);
    void AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw, const RailPath& railPath) const;

    size_t ActiveEnemyCount() const { return enemies_.size(); }
    size_t ActiveBulletCount() const { return bullets_.size(); }
    size_t ActiveObstacleCount() const { return obstacles_.size(); }
    size_t ActiveVfxCueCount() const { return vfxCues_.size(); }
    const std::vector<CourseEnemyActor>& Enemies() const { return enemies_; }
    std::vector<CourseEnemyActor>& MutableEnemies() { return enemies_; }
    const std::vector<CourseBulletActor>& Bullets() const { return bullets_; }
    std::vector<CourseBulletActor>& MutableBullets() { return bullets_; }
    const std::vector<CourseObstacleActor>& Obstacles() const { return obstacles_; }
    std::vector<CourseObstacleActor>& MutableObstacles() { return obstacles_; }
    const std::vector<CourseVfxCue>& VfxCues() const { return vfxCues_; }
    void PruneDestroyedActors();
    const CourseEnemyFireSafetySettings& FireSafetySettings() const { return fireSafetySettings_; }
    CourseEnemyFireSafetySettings& MutableFireSafetySettings() { return fireSafetySettings_; }
    const CourseEnemyFireSafetyStats& LastFireSafetyStats() const { return fireSafetyStats_; }
    const EnemyCombatSystem& EnemyCombat() const noexcept { return enemyCombatSystem_; }
    EnemyCombatSystem& EnemyCombat() noexcept { return enemyCombatSystem_; }
    const EnemyBehaviorSystem& EnemyBehavior() const noexcept { return enemyBehaviorSystem_; }
    EnemyBehaviorSystem& EnemyBehavior() noexcept { return enemyBehaviorSystem_; }
    const EnemyAttackCoordinator& EnemyAttacks() const noexcept {
        return enemyAttackCoordinator_;
    }
    EnemyAttackCoordinator& EnemyAttacks() noexcept {
        return enemyAttackCoordinator_;
    }
    const EnemyAttackExecutionSystem& EnemyAttackExecution() const noexcept {
        return enemyAttackExecutionSystem_;
    }
    const EnemyTargetingSystem& EnemyTargeting() const noexcept {
        return enemyTargetingSystem_;
    }
    const EnemyProjectileSystem& EnemyProjectiles() const noexcept {
        return enemyProjectileSystem_;
    }
    bool MarkEnemyAttackTelegraphPresented(
        uint32_t actorId,
        uint64_t attackIntentSequence);

private:
    friend class EnemyAttackExecutionSystem;
    bool CanEnemyFire(CourseEnemyActor& enemy, const CourseEnemyFireSafetyFrameInput& safetyInput, float dt);
    uint32_t EmitEnemyBullets(const CourseEnemyActor& enemy);

    std::vector<CourseEnemyActor> enemies_;
    std::vector<CourseBulletActor> bullets_;
    std::vector<CourseObstacleActor> obstacles_;
    std::vector<CourseVfxCue> vfxCues_;
    CourseEnemyFireSafetySettings fireSafetySettings_{};
    CourseEnemyFireSafetyStats fireSafetyStats_{};
    EnemyCombatSystem enemyCombatSystem_{};
    EnemyBehaviorSystem enemyBehaviorSystem_{};
    EnemyAttackCoordinator enemyAttackCoordinator_{};
    EnemyAttackExecutionSystem enemyAttackExecutionSystem_{};
    EnemyTargetingSystem enemyTargetingSystem_{};
    EnemyProjectileSystem enemyProjectileSystem_{};
    uint32_t nextActorId_ = 1;
};
