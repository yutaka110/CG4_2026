#include "CourseSpawnRuntime.h"

#include "../EffectRuntime.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector3 ResolveRailLocal(
    const RailPath& railPath,
    float spawnDistance,
    float distanceOffset,
    float lateralOffset,
    float verticalOffset) {
    const RailPathSample sample = railPath.Evaluate(spawnDistance + distanceOffset);
    return Add(
        Add(sample.position, Scale(sample.right, lateralOffset)),
        Scale(sample.up, verticalOffset));
}

Vector4 FadeColor(Vector4 color, float age, float lifetime, float floorAlpha = 0.20f) {
    const float t = lifetime > 0.0f ? (std::clamp)(age / lifetime, 0.0f, 1.0f) : 1.0f;
    color.w *= (std::max)(floorAlpha, 1.0f - t);
    return color;
}

bool RoleContains(const std::string& role, const char* token) {
    return role.find(token) != std::string::npos;
}

CourseEnemyFirePattern PatternForRole(const std::string& role) {
    if (RoleContains(role, "boss") || RoleContains(role, "gatekeeper")) {
        return CourseEnemyFirePattern::BossArc;
    }
    if (RoleContains(role, "turret") || RoleContains(role, "crossfire")) {
        return CourseEnemyFirePattern::Spread;
    }
    if (RoleContains(role, "chase") || RoleContains(role, "pursuit")) {
        return CourseEnemyFirePattern::Twin;
    }
    return CourseEnemyFirePattern::Single;
}
} // namespace

void CourseSpawnRuntime::Reset() {
    enemies_.clear();
    bullets_.clear();
    obstacles_.clear();
    vfxCues_.clear();
    fireSafetyStats_ = {};
    enemyCombatSystem_.Reset();
    enemyBehaviorSystem_.Reset();
    enemyAttackCoordinator_.Reset();
    enemyAttackExecutionSystem_.Reset();
    enemyTargetingSystem_.Reset();
    enemyProjectileSystem_.Reset();
    enemyFormationSystem_.Reset();
    enemyEntranceExitDirector_.Reset();
    nextActorId_ = 1;
}

CourseSpawnRuntimeCheckpoint CourseSpawnRuntime::CaptureCheckpoint() const {
    CourseSpawnRuntimeCheckpoint checkpoint{};
    checkpoint.enemies = enemies_;
    checkpoint.bullets = bullets_;
    checkpoint.obstacles = obstacles_;
    checkpoint.nextActorId = nextActorId_;
    return checkpoint;
}

void CourseSpawnRuntime::RestoreCheckpoint(
    const CourseSpawnRuntimeCheckpoint& checkpoint,
    bool restoreProjectiles) {
    enemies_ = checkpoint.enemies;
    bullets_ = restoreProjectiles
        ? checkpoint.bullets
        : std::vector<CourseBulletActor>{};
    obstacles_ = checkpoint.obstacles;
    vfxCues_.clear();
    fireSafetyStats_ = {};
    enemyCombatSystem_.Reset();
    enemyBehaviorSystem_.Reset();
    enemyAttackCoordinator_.Reset();
    enemyAttackCoordinator_.RebuildFromRuntime(*this);
    enemyAttackExecutionSystem_.Reset();
    enemyTargetingSystem_.Reset();
    enemyProjectileSystem_.Reset();
    enemyProjectileSystem_.RebuildFromProjectiles(bullets_);
    enemyFormationSystem_.Reset();
    enemyEntranceExitDirector_.Reset();
    nextActorId_ = (std::max)(1u, checkpoint.nextActorId);
}

void CourseSpawnRuntime::Update(float deltaTime) {
    CourseEnemyFireSafetyFrameInput safetyInput{};
    safetyInput.deltaTime = deltaTime;
    Update(deltaTime, safetyInput);
}

void CourseSpawnRuntime::Update(float deltaTime, const CourseEnemyFireSafetyFrameInput& safetyInput) {
    const float dt = (std::max)(0.0f, deltaTime);
    fireSafetyStats_ = {};

    // Additive staging is removed in reverse order before Behavior writes the
    // new base pose. This prevents cumulative drift in long-lived formations.
    enemyEntranceExitDirector_.BeginFrame(*this);
    enemyFormationSystem_.BeginFrame(*this);

    EnemyCombatFrameInput combatInput{};
    combatInput.deltaTime = dt;
    combatInput.playerDistance = safetyInput.playerDistance;
    enemyCombatSystem_.Update(*this, combatInput);
    EnemyBehaviorFrameInput behaviorInput{};
    behaviorInput.deltaTime = dt;
    behaviorInput.playerDistance = safetyInput.playerDistance;
    enemyBehaviorSystem_.Update(*this, behaviorInput);
    enemyFormationSystem_.Update(*this, dt);
    enemyEntranceExitDirector_.Update(*this, dt);

    for (CourseEnemyActor& enemy : enemies_) {
        ++fireSafetyStats_.activeEnemies;
        enemy.bulletsEmittedThisFrame = 0;
        enemy.age += dt;
        const bool behaviorDriven = enemy.behaviorState.initialized &&
            enemy.behaviorDefinition.commercialBehavior;
        if (!behaviorDriven) {
            enemy.desc.distanceOffset += enemy.desc.forwardSpeed * dt;
            enemy.fireTimer -= dt;
        }
        const bool canFire = CanEnemyFire(enemy, safetyInput, dt);
        while (!behaviorDriven && enemy.fireTimer <= 0.0f &&
               enemy.age < enemy.desc.lifetime) {
            if (!canFire) {
                enemy.fireTimer = (std::max)(enemy.fireTimer, fireSafetySettings_.blockedRetryDelay);
                break;
            }
            const uint32_t emitted = EmitEnemyBullets(enemy);
            fireSafetyStats_.bulletsEmitted += emitted;
            enemy.bulletsEmittedThisFrame += emitted;
            if (emitted > 0) {
                ++enemy.fireSequence;
            }
            enemy.fireTimer += (std::max)(0.08f, enemy.desc.fireInterval);
        }
    }

    enemyAttackCoordinator_.Update(*this, enemyBehaviorSystem_.Frame(), dt);
    EnemyTargetingFrameInput targetingInput{};
    targetingInput.deltaTime = dt;
    targetingInput.playerDistance = safetyInput.playerDistance;
    targetingInput.playerLateralOffset = safetyInput.playerLateralOffset;
    targetingInput.playerVerticalOffset = safetyInput.playerVerticalOffset;
    enemyTargetingSystem_.Update(*this, targetingInput);
    enemyAttackExecutionSystem_.Update(
        *this, enemyAttackCoordinator_, enemyBehaviorSystem_);
    fireSafetyStats_.bulletsEmitted +=
        enemyAttackExecutionSystem_.Frame().emittedProjectiles;

    EnemyProjectileFrameInput projectileInput{};
    projectileInput.deltaTime = dt;
    projectileInput.playerDistance = safetyInput.playerDistance;
    projectileInput.playerLateralOffset = safetyInput.playerLateralOffset;
    projectileInput.playerVerticalOffset = safetyInput.playerVerticalOffset;
    enemyProjectileSystem_.Update(bullets_, projectileInput);

    for (CourseObstacleActor& obstacle : obstacles_) {
        obstacle.age += dt;
        obstacle.desc.distanceOffset += obstacle.desc.forwardSpeed * dt;
    }

    for (CourseVfxCue& cue : vfxCues_) {
        cue.age += dt;
    }

    PruneDestroyedActors();
}

bool CourseSpawnRuntime::CanEnemyFire(
    CourseEnemyActor& enemy,
    const CourseEnemyFireSafetyFrameInput& safetyInput,
    float dt) {
    if (enemy.desc.suppressFire || enemy.entranceExitState.attackSuppressed ||
        (enemy.combatState.initialized && !enemy.combatState.canFire)) {
        enemy.fireSafetyAllowed = false;
        enemy.fireSafetyReason = enemy.desc.suppressFire
            ? "actor fire suppressed"
            : (enemy.entranceExitState.attackSuppressed
                ? "entrance/exit staging gate"
                : "combat phase: " + std::string(ToString(enemy.combatState.phase)));
        fireSafetyStats_.lastBlockedReason = enemy.fireSafetyReason;
        return false;
    }
    if (enemy.behaviorState.initialized &&
        enemy.behaviorDefinition.commercialBehavior &&
        !enemyBehaviorSystem_.CanCommitAttack(enemy)) {
        enemy.fireSafetyAllowed = false;
        enemy.fireSafetyReason = enemy.behaviorState.attackIntentActive
            ? enemy.behaviorState.telegraphPresented
                ? "behavior attack countdown"
                : "behavior waiting for telegraph presentation"
            : "behavior has no attack intent";
        fireSafetyStats_.lastBlockedReason = enemy.fireSafetyReason;
        return false;
    }
    if (enemy.screenPresenceEvaluated &&
        !enemy.screenPresenceAttackAllowed) {
        enemy.fireSafetyAllowed = false;
        enemy.fireSafetyReason = "screen presence exposure gate";
        ++fireSafetyStats_.blockedByVisibilityTime;
        fireSafetyStats_.lastBlockedReason = enemy.fireSafetyReason;
        return false;
    }
    if (enemy.encounterPacingEvaluated &&
        !enemy.encounterPacingAttackAllowed) {
        enemy.fireSafetyAllowed = false;
        enemy.fireSafetyReason = "encounter pacing phase gate";
        ++fireSafetyStats_.blockedByVisibilityTime;
        fireSafetyStats_.lastBlockedReason = enemy.fireSafetyReason;
        return false;
    }
    if (!fireSafetySettings_.enabled) {
        enemy.fireSafetyAllowed = true;
        enemy.fireSafetyReason = "safety disabled";
        ++fireSafetyStats_.allowedEnemies;
        fireSafetyStats_.lastAllowedReason = enemy.fireSafetyReason;
        return true;
    }

    const float actorDistance = enemy.desc.spawnDistance + enemy.desc.distanceOffset;
    const float forwardDistance = actorDistance - safetyInput.playerDistance;
    const bool cameraBlocks =
        fireSafetySettings_.requireCameraAllowsFire &&
        (!safetyInput.cameraAllowsEnemyFire ||
            !safetyInput.cameraStableForAiming ||
            safetyInput.cameraHardTransition);
    const bool inFireRange =
        forwardDistance >= fireSafetySettings_.minForwardDistance &&
        forwardDistance <= fireSafetySettings_.maxForwardDistance;

    if (!cameraBlocks && inFireRange) {
        enemy.fireVisibleTime += dt;
    } else {
        enemy.fireVisibleTime = 0.0f;
    }

    if (cameraBlocks) {
        enemy.fireSafetyAllowed = false;
        enemy.fireSafetyReason = "camera: " + safetyInput.cameraReason;
        ++fireSafetyStats_.blockedByCamera;
        fireSafetyStats_.lastBlockedReason = enemy.fireSafetyReason;
        return false;
    }
    if (!inFireRange) {
        enemy.fireSafetyAllowed = false;
        enemy.fireSafetyReason = forwardDistance < fireSafetySettings_.minForwardDistance
            ? "too close / behind safety window"
            : "too far for readable fire";
        ++fireSafetyStats_.blockedByRange;
        fireSafetyStats_.lastBlockedReason = enemy.fireSafetyReason;
        return false;
    }
    if (enemy.fireVisibleTime < fireSafetySettings_.minVisibleBeforeFire) {
        enemy.fireSafetyAllowed = false;
        enemy.fireSafetyReason = "visible time warming";
        ++fireSafetyStats_.blockedByVisibilityTime;
        fireSafetyStats_.lastBlockedReason = enemy.fireSafetyReason;
        return false;
    }

    enemy.fireSafetyAllowed = true;
    enemy.fireSafetyReason = "camera safe";
    ++fireSafetyStats_.allowedEnemies;
    fireSafetyStats_.lastAllowedReason = enemy.fireSafetyReason;
    return true;
}

void CourseSpawnRuntime::PruneDestroyedActors() {
    enemies_.erase(
        std::remove_if(
            enemies_.begin(),
            enemies_.end(),
            [](const CourseEnemyActor& enemy) {
                if (enemy.age >= enemy.desc.lifetime) {
                    return true;
                }
                if (enemy.entranceExitState.exitComplete) {
                    return true;
                }
                if (!enemy.combatState.initialized) {
                    return enemy.desc.hitPoints <= 0.0f;
                }
                return enemy.combatState.phase == EnemyCombatPhase::Retired;
            }),
        enemies_.end());
    bullets_.erase(
        std::remove_if(
            bullets_.begin(),
            bullets_.end(),
            [](const CourseBulletActor& bullet) {
                return !bullet.active || bullet.age >= bullet.lifetime;
            }),
        bullets_.end());
    obstacles_.erase(
        std::remove_if(
            obstacles_.begin(),
            obstacles_.end(),
            [](const CourseObstacleActor& obstacle) {
                return obstacle.age >= obstacle.desc.lifetime ||
                    (obstacle.desc.breakable && obstacle.desc.hitPoints <= 0.0f);
            }),
        obstacles_.end());
    vfxCues_.erase(
        std::remove_if(
            vfxCues_.begin(),
            vfxCues_.end(),
            [](const CourseVfxCue& cue) {
                return cue.age >= cue.desc.lifetime;
            }),
        vfxCues_.end());
}

void CourseSpawnRuntime::SpawnEnemyActor(CourseEnemyActorDesc desc) {
    desc.lifetime = (std::max)(0.1f, desc.lifetime);
    desc.radius = (std::max)(0.05f, desc.radius);
    desc.hitPoints = (std::max)(1.0f, desc.hitPoints);
    desc.fireInterval = (std::max)(0.08f, desc.fireInterval);
    desc.firstShotDelay = (std::max)(0.02f, desc.firstShotDelay);
    desc.bulletSpeed = (std::max)(1.0f, desc.bulletSpeed);
    if (desc.firePattern == CourseEnemyFirePattern::Single) {
        desc.firePattern = PatternForRole(desc.role);
    }
    if (desc.bulletCount <= 1) {
        desc.bulletCount =
            desc.firePattern == CourseEnemyFirePattern::BossArc ? 5 :
            desc.firePattern == CourseEnemyFirePattern::Spread ? 3 :
            desc.firePattern == CourseEnemyFirePattern::Twin ? 2 :
            1;
    }
    if (desc.bulletLateralSpreadSpeed <= 0.0f) {
        desc.bulletLateralSpreadSpeed =
            desc.firePattern == CourseEnemyFirePattern::BossArc ? 3.2f :
            desc.firePattern == CourseEnemyFirePattern::Spread ? 2.4f :
            desc.firePattern == CourseEnemyFirePattern::Twin ? 1.3f :
            0.0f;
    }
    if (desc.bulletVerticalSpreadSpeed <= 0.0f &&
        desc.firePattern == CourseEnemyFirePattern::BossArc) {
        desc.bulletVerticalSpreadSpeed = 0.55f;
    }
    desc.bulletCount = (std::max)(1, desc.bulletCount);
    desc.bulletRadius = (std::max)(0.05f, desc.bulletRadius);
    desc.bulletLifetime = (std::max)(0.1f, desc.bulletLifetime);
    desc.bulletDamage = (std::max)(0.0f, desc.bulletDamage);

    CourseEnemyActor actor{};
    actor.desc = std::move(desc);
    actor.fireTimer = actor.desc.firstShotDelay;
    actor.actorId = nextActorId_++;
    enemyCombatSystem_.InitializeActor(actor);
    enemyBehaviorSystem_.InitializeActor(actor);
    if (actor.desc.projectileDefinition.id.empty()) {
        actor.desc.projectileDefinition =
            EnemyProjectileDefinitionAsset::LegacyDirect();
        actor.desc.projectileDefinition.id = actor.desc.projectileDefinitionId.empty()
            ? "runtime_" + actor.desc.bulletPatternId
            : actor.desc.projectileDefinitionId;
        actor.desc.projectileDefinition.displayName =
            actor.desc.projectileDefinition.id;
        actor.desc.projectileDefinition.trajectory =
            actor.behaviorDefinition.commercialBehavior
                ? EnemyProjectileTrajectory::Predictive
                : EnemyProjectileTrajectory::Direct;
        actor.desc.projectileDefinition.initialSpeed = actor.desc.bulletSpeed;
        actor.desc.projectileDefinition.maximumSpeed = actor.desc.bulletSpeed;
        actor.desc.projectileDefinition.radius = actor.desc.bulletRadius;
        actor.desc.projectileDefinition.lifetime = actor.desc.bulletLifetime;
        actor.desc.projectileDefinition.damage = actor.desc.bulletDamage;
        actor.desc.projectileDefinition.color = actor.desc.bulletColor;
    }
    actor.desc.projectileDefinitionId = actor.desc.projectileDefinition.id;
    enemyAttackCoordinator_.InitializeActor(actor);
    enemies_.push_back(std::move(actor));
}

bool CourseSpawnRuntime::MarkEnemyAttackTelegraphPresented(
    uint32_t actorId,
    uint64_t attackIntentSequence) {
    if (!enemyAttackCoordinator_.MarkTelegraphPresented(
            *this, actorId, attackIntentSequence)) {
        return false;
    }
    return enemyBehaviorSystem_.MarkTelegraphPresented(
        *this, actorId, attackIntentSequence);
}

void CourseSpawnRuntime::SpawnObstacle(CourseObstacleActorDesc desc) {
    desc.lifetime = (std::max)(0.1f, desc.lifetime);
    desc.halfExtents.x = (std::max)(0.25f, desc.halfExtents.x);
    desc.halfExtents.y = (std::max)(0.25f, desc.halfExtents.y);
    desc.halfExtents.z = (std::max)(0.25f, desc.halfExtents.z);

    CourseObstacleActor actor{};
    actor.desc = std::move(desc);
    actor.actorId = nextActorId_++;
    obstacles_.push_back(std::move(actor));
}

void CourseSpawnRuntime::SpawnVfxCue(CourseVfxCueDesc desc) {
    desc.lifetime = (std::max)(0.1f, desc.lifetime);
    desc.radius = (std::max)(0.05f, desc.radius);
    if (desc.hasWorldPosition &&
        (!std::isfinite(desc.worldPosition.x) ||
         !std::isfinite(desc.worldPosition.y) ||
         !std::isfinite(desc.worldPosition.z))) {
        desc.hasWorldPosition = false;
    }

    CourseVfxCue cue{};
    cue.desc = std::move(desc);
    vfxCues_.push_back(std::move(cue));
}

void CourseSpawnRuntime::SubmitPendingVfx(EffectRuntime& effectRuntime, const RailPath& railPath) {
    if (railPath.Length() <= 0.0f || !effectRuntime.IsAttached()) {
        return;
    }

    for (CourseVfxCue& cue : vfxCues_) {
        if (cue.submitted) {
            continue;
        }

        const Vector3 center = cue.desc.hasWorldPosition
            ? cue.desc.worldPosition
            : ResolveRailLocal(
                railPath,
                cue.desc.spawnDistance,
                cue.desc.distanceOffset,
                cue.desc.lateralOffset,
                cue.desc.verticalOffset);
        cue.effectInstanceId = effectRuntime.PlayEffectWithParams(
            cue.desc.effectName,
            center,
            cue.desc.color,
            {cue.desc.radius, cue.desc.radius, cue.desc.radius});
        cue.submitted = cue.effectInstanceId != 0;
    }
}

uint32_t CourseSpawnRuntime::EmitEnemyBullets(const CourseEnemyActor& enemy) {
    return enemyProjectileSystem_.SpawnVolley(enemy, bullets_);
}

void CourseSpawnRuntime::AppendDebugDraw(
    ge3::debug::DebugDrawSystem& debugDraw,
    const RailPath& railPath) const {
    if (railPath.Length() <= 0.0f) {
        return;
    }

    for (const CourseEnemyActor& enemy : enemies_) {
        const RailPathSample sample = railPath.Evaluate(enemy.desc.spawnDistance + enemy.desc.distanceOffset);
        const Vector3 center = ResolveRailLocal(
            railPath,
            enemy.desc.spawnDistance,
            enemy.desc.distanceOffset,
            enemy.desc.lateralOffset,
            enemy.desc.verticalOffset);
        const Vector4 color = FadeColor(enemy.desc.color, enemy.age, enemy.desc.lifetime);
        debugDraw.AddPoint(center, enemy.desc.radius, color);
        debugDraw.AddCircle(center, sample.right, sample.up, enemy.desc.radius * 1.35f, color, 20);
        debugDraw.AddLine(center, Add(center, Scale(sample.tangent, -enemy.desc.radius * 2.0f)), color);
    }

    for (const CourseObstacleActor& obstacle : obstacles_) {
        const Vector3 center = ResolveRailLocal(
            railPath,
            obstacle.desc.spawnDistance,
            obstacle.desc.distanceOffset,
            obstacle.desc.lateralOffset,
            obstacle.desc.verticalOffset);
        const Vector3 extent = obstacle.desc.halfExtents;
        const Vector4 color = FadeColor(obstacle.desc.color, obstacle.age, obstacle.desc.lifetime);
        debugDraw.AddBox(
            {center.x - extent.x, center.y - extent.y, center.z - extent.z},
            {center.x + extent.x, center.y + extent.y, center.z + extent.z},
            color);
    }

    for (const CourseVfxCue& cue : vfxCues_) {
        const RailPathSample sample = railPath.Evaluate(cue.desc.spawnDistance + cue.desc.distanceOffset);
        const Vector3 center = cue.desc.hasWorldPosition
            ? cue.desc.worldPosition
            : ResolveRailLocal(
                railPath,
                cue.desc.spawnDistance,
                cue.desc.distanceOffset,
                cue.desc.lateralOffset,
                cue.desc.verticalOffset);
        const Vector3 axisU = cue.desc.hasWorldPosition
            ? Vector3{1.0f, 0.0f, 0.0f}
            : sample.right;
        const Vector3 axisV = cue.desc.hasWorldPosition
            ? Vector3{0.0f, 1.0f, 0.0f}
            : sample.up;
        const Vector4 color = FadeColor(cue.desc.color, cue.age, cue.desc.lifetime);
        debugDraw.AddCircle(center, axisU, axisV, cue.desc.radius, color, 32);
        debugDraw.AddLine(center, Add(center, Scale(axisV, cue.desc.radius * 1.4f)), color);
    }
}
