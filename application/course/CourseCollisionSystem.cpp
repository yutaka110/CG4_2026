#include "CourseCollisionSystem.h"
#include "../AppLogFile.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace {
bool Finite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float LengthSquared(const Vector3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
    const float lengthSquared = LengthSquared(value);
    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) {
        return fallback;
    }
    return Scale(value, 1.0f / std::sqrt(lengthSquared));
}

float Abs(float value) {
    return std::fabs(value);
}

float ActorDistance(const CourseEnemyActor& enemy) {
    return enemy.desc.spawnDistance + enemy.desc.distanceOffset;
}

float BulletDistance(const CourseBulletActor& bullet) {
    return bullet.spawnDistance + bullet.distanceOffset;
}

float ObstacleDistance(const CourseObstacleActor& obstacle) {
    return obstacle.desc.spawnDistance + obstacle.desc.distanceOffset;
}

bool SphereOverlapRailLocal(
    float aDistance,
    float aLateral,
    float aVertical,
    float aRadius,
    float bDistance,
    float bLateral,
    float bVertical,
    float bRadius) {
    const float dd = aDistance - bDistance;
    const float dl = aLateral - bLateral;
    const float dv = aVertical - bVertical;
    const float radius = aRadius + bRadius;
    return dd * dd + dl * dl + dv * dv <= radius * radius;
}

bool PlayerOverlapsObstacle(
    const CourseCollisionPlayerState& player,
    const CourseObstacleActor& obstacle) {
    const float distance = ObstacleDistance(obstacle);
    return Abs(player.distance - distance) <= obstacle.desc.halfExtents.z + player.radius &&
        Abs(player.lateralOffset - obstacle.desc.lateralOffset) <= obstacle.desc.halfExtents.x + player.radius &&
        Abs(player.verticalOffset - obstacle.desc.verticalOffset) <= obstacle.desc.halfExtents.y + player.radius;
}

bool PlayerOverlapsTerrainPlacement(
    const CourseCollisionPlayerState& player,
    const CourseTerrainPlacement& placement) {
    if (placement.layer != CourseTerrainLayer::GameplayCollision ||
        placement.collisionMode == CourseTerrainCollisionMode::None) {
        return false;
    }

    const float distance = placement.distance + placement.forwardOffset;
    return Abs(player.distance - distance) <= placement.scale.z + player.radius &&
        Abs(player.lateralOffset - placement.lateralOffset) <= placement.scale.x + player.radius &&
        Abs(player.verticalOffset - placement.verticalOffset) <= placement.scale.y + player.radius;
}

void SpawnImpactCue(
    CourseSpawnRuntime& runtime,
    const char* id,
    const char* effectName,
    float distance,
    float lateral,
    float vertical,
    float radius,
    const Vector4& color,
    float lifetime = 1.2f) {
    CourseVfxCueDesc cue{};
    cue.id = id;
    cue.effectName = effectName;
    cue.spawnDistance = distance;
    cue.distanceOffset = 0.0f;
    cue.lateralOffset = lateral;
    cue.verticalOffset = vertical;
    cue.radius = radius;
    cue.lifetime = lifetime;
    cue.color = color;
    runtime.SpawnVfxCue(std::move(cue));
}

} // namespace

CourseCollisionSystem::CourseCollisionSystem() {
    Reset();
}

void CourseCollisionSystem::Reset() {
    player_ = {};
    weapon_ = {};
    lastFrameStats_ = {};
    lastShotDistance_ = 0.0f;
    lastShotLateralOffset_ = 0.0f;
    lastShotVerticalOffset_ = 0.0f;
    lastShotWorldPoint_ = {};
    lastShotWorldNormal_ = {};
    lastShotHitKind_ = RailAimHitKind::None;
    lastShotHitActorId_ = 0;
    lastShotHasWorldPoint_ = false;
    lastShotHasWorldHit_ = false;
    lastShotVisible_ = false;
    lastWeaponHitRequest_ = {};
    lastDamageResult_ = {};
    lastWeaponFeedbackResult_ = {};
    lastWeaponFireResult_ = {};
    weaponFireSystem_.Reset();
    weaponDefinitionRegistry_.SynchronizeFireSystem(weaponFireSystem_);
    weaponFeedbackSystem_.Reset();
    damageReceiver_.Reset();
}

void CourseCollisionSystem::SynchronizePlayerHitPoints(float hitPoints) {
    if (std::isfinite(hitPoints)) {
        player_.hitPoints = (std::max)(0.0f, hitPoints);
    }
}

bool CourseCollisionSystem::LoadWeaponDefinitions(
    const std::filesystem::path& directory,
    std::string* errorMessage) {
    return weaponDefinitionRegistry_.LoadDirectory(
        directory,
        &weaponFireSystem_,
        errorMessage);
}

WeaponDefinitionReloadReport CourseCollisionSystem::ReloadChangedWeaponDefinitions() {
    return weaponDefinitionRegistry_.ReloadChangedAssets(&weaponFireSystem_);
}

DamageResult CourseCollisionSystem::ApplyWeaponHit(
    CourseSpawnRuntime& runtime,
    const CourseAsset* course,
    const WeaponHitRequest& request) {
    lastWeaponHitRequest_ = request;
    lastDamageResult_ = damageReceiver_.Apply(runtime, course, request);
    lastWeaponFeedbackResult_ =
        weaponFeedbackSystem_.Submit(runtime, request, lastDamageResult_);
    return lastDamageResult_;
}

WeaponFireResult CourseCollisionSystem::UpdateWeaponFire(const WeaponFireInput& input) {
    lastWeaponFireResult_ = weaponFireSystem_.Update(input);
    return lastWeaponFireResult_;
}

CourseCollisionFrameStats CourseCollisionSystem::Update(
    CourseSpawnRuntime& runtime,
    const CourseCollisionFrameInput& input) {
    const float dt = (std::max)(0.0f, input.deltaTime);
    weaponFeedbackSystem_.Update(dt);
    lastFrameStats_ = {};
    lastShotVisible_ = false;

    player_.distance = input.player.distance;
    player_.lateralOffset = input.player.lateralOffset;
    player_.verticalOffset = input.player.verticalOffset;
    player_.radius = (std::max)(0.1f, input.player.radius);
    if (player_.hitPoints <= 0.0f && input.player.hitPoints > 0.0f) {
        player_.hitPoints = input.player.hitPoints;
    }
    player_.invulnerabilityTime = (std::max)(
        (std::max)(0.0f, player_.invulnerabilityTime - dt),
        (std::max)(0.0f, input.player.invulnerabilityTime));

    weapon_.enabled = input.weapon.enabled;
    weapon_.triggerHeld = input.weapon.triggerHeld;
    weapon_.triggerPressed = input.weapon.triggerPressed;
    weapon_.triggerReleased = input.weapon.triggerReleased;
    weapon_.shotInterval = (std::max)(0.03f, input.weapon.shotInterval);
    weapon_.range = (std::max)(1.0f, input.weapon.range);
    weapon_.radius = (std::max)(0.1f, input.weapon.radius);
    weapon_.damage = (std::max)(0.0f, input.weapon.damage);
    weapon_.damageMultiplier =
        std::isfinite(input.weapon.damageMultiplier)
        ? (std::max)(0.0f, input.weapon.damageMultiplier)
        : 1.0f;
    weapon_.assistEnabled = input.weapon.assistEnabled;
    weapon_.assistLateralOffset = input.weapon.assistLateralOffset;
    weapon_.assistVerticalOffset = input.weapon.assistVerticalOffset;
    weapon_.muzzleForwardOffset = (std::max)(0.0f, input.weapon.muzzleForwardOffset);
    weapon_.tracerForwardDistance = (std::max)(4.0f, input.weapon.tracerForwardDistance);
    weapon_.muzzleRadius = (std::max)(0.05f, input.weapon.muzzleRadius);
    weapon_.tracerRadius = (std::max)(0.05f, input.weapon.tracerRadius);
    if (const WeaponDefinitionAsset* pulseCannon =
            weaponDefinitionRegistry_.Find(RailWeaponIds::PulseCannon)) {
        weapon_.shotInterval = pulseCannon->definition.shotInterval;
        weapon_.range = pulseCannon->definition.range;
        weapon_.damage = pulseCannon->definition.baseDamage;
        weapon_.radius = pulseCannon->projectileRadius;
        weapon_.muzzleForwardOffset = pulseCannon->muzzleForwardOffset;
        weapon_.tracerForwardDistance = pulseCannon->tracerForwardDistance;
        weapon_.muzzleRadius = pulseCannon->muzzleRadius;
        weapon_.tracerRadius = pulseCannon->tracerRadius;
    }

    for (CourseBulletActor& bullet : runtime.MutableBullets()) {
        if (bullet.age >= bullet.lifetime || player_.invulnerabilityTime > 0.0f) {
            continue;
        }
        if (!SphereOverlapRailLocal(
                player_.distance,
                player_.lateralOffset,
                player_.verticalOffset,
                player_.radius,
                BulletDistance(bullet),
                bullet.lateralOffset,
                bullet.verticalOffset,
                bullet.radius)) {
            continue;
        }

        bullet.age = bullet.lifetime;
        player_.hitPoints = (std::max)(0.0f, player_.hitPoints - bullet.damage);
        player_.invulnerabilityTime = 0.65f;
        lastFrameStats_.enemyBulletHits++;
        lastFrameStats_.playerDamage += bullet.damage;
        SpawnImpactCue(
            runtime,
            "player_bullet_hit",
            "ice_impact",
            player_.distance,
            player_.lateralOffset,
            player_.verticalOffset,
            1.6f,
            {0.6f, 0.9f, 1.0f, 1.0f});
    }

    for (const CourseObstacleActor& obstacle : runtime.Obstacles()) {
        if (player_.invulnerabilityTime > 0.0f || !PlayerOverlapsObstacle(player_, obstacle)) {
            continue;
        }

        constexpr float kObstacleContactDamage = 24.0f;
        player_.hitPoints = (std::max)(0.0f, player_.hitPoints - kObstacleContactDamage);
        player_.invulnerabilityTime = 0.80f;
        lastFrameStats_.obstacleHits++;
        lastFrameStats_.playerDamage += kObstacleContactDamage;
        SpawnImpactCue(
            runtime,
            "player_obstacle_hit",
            "hit_ring",
            player_.distance,
            player_.lateralOffset,
            player_.verticalOffset,
            2.0f,
            {1.0f, 0.62f, 0.12f, 1.0f});
        break;
    }

    if (input.course != nullptr) {
        for (const CourseTerrainPlacement& placement : input.course->terrainPlacements) {
            if (player_.invulnerabilityTime > 0.0f ||
                !PlayerOverlapsTerrainPlacement(player_, placement)) {
                continue;
            }

            constexpr float kTerrainContactDamage = 20.0f;
            player_.hitPoints = (std::max)(0.0f, player_.hitPoints - kTerrainContactDamage);
            player_.invulnerabilityTime = 0.80f;
            lastFrameStats_.obstacleHits++;
            lastFrameStats_.playerDamage += kTerrainContactDamage;
            SpawnImpactCue(
                runtime,
                "player_terrain_hit",
                "hit_ring",
                player_.distance,
                player_.lateralOffset,
                player_.verticalOffset,
                2.0f,
                {1.0f, 0.54f, 0.18f, 1.0f});
            break;
        }
    }

    WeaponFireInput fireInput{};
    fireInput.weaponId = RailWeaponIds::PulseCannon;
    fireInput.deltaTime = dt;
    fireInput.enabled = weapon_.enabled;
    fireInput.triggerHeld = weapon_.triggerHeld;
    fireInput.triggerPressed = weapon_.triggerPressed;
    fireInput.triggerReleased = weapon_.triggerReleased;
    fireInput.damageMultiplier = weapon_.damageMultiplier;
    lastWeaponFireResult_ = UpdateWeaponFire(fireInput);
    weapon_.shotTimer = lastWeaponFireResult_.cooldownRemaining;
    lastFrameStats_.playerShotsFired +=
        static_cast<uint32_t>(lastWeaponFireResult_.shots.size());
    for (const WeaponShot& shot : lastWeaponFireResult_.shots) {
        FirePlayerShot(runtime, input, shot);
    }

    runtime.PruneDestroyedActors();
    LogFrameStats(lastFrameStats_);
    return lastFrameStats_;
}

void CourseCollisionSystem::FirePlayerShot(
    CourseSpawnRuntime& runtime,
    const CourseCollisionFrameInput& input,
    const WeaponShot& shot) {
    const float aimLateral = input.weapon.assistEnabled
        ? input.weapon.assistLateralOffset
        : input.player.lateralOffset;
    const float aimVertical = input.weapon.assistEnabled
        ? input.weapon.assistVerticalOffset
        : input.player.verticalOffset;
    lastShotDistance_ = input.player.distance + shot.range;
    lastShotLateralOffset_ = aimLateral;
    lastShotVerticalOffset_ = aimVertical;
    lastShotWorldNormal_ = {};
    lastShotHitKind_ = RailAimHitKind::None;
    lastShotHitActorId_ = 0;
    lastShotHasWorldPoint_ = false;
    lastShotHasWorldHit_ = false;
    lastShotVisible_ = true;
    lastWeaponHitRequest_ = {};
    lastDamageResult_ = {};
    lastWeaponFeedbackResult_ = {};

    const RailAimState* aim = input.worldAim;
    if (aim == nullptr || !aim->valid || !Finite(aim->worldRayOrigin) ||
        !Finite(aim->worldRayDirection)) {
        return;
    }

    const Vector3 direction = NormalizeOr(aim->worldRayDirection, {0.0f, 0.0f, 1.0f});
    lastShotWorldPoint_ = Add(aim->worldRayOrigin, Scale(direction, shot.range));
    lastShotHasWorldPoint_ = true;
    if (!aim->hasWorldHit || aim->hitKind == RailAimHitKind::None ||
        !Finite(aim->worldAimPoint) || !Finite(aim->worldAimNormal) ||
        !std::isfinite(aim->aimDistance) || aim->aimDistance < 0.0f ||
        aim->aimDistance > shot.range + 0.001f) {
        return;
    }

    WeaponHitRequest request{};
    request.shotId = shot.shotId;
    request.targetActorId = aim->hitActorId;
    request.sourceIndex = aim->hitSourceIndex;
    request.hitKind = aim->hitKind;
    request.damageType = shot.damageType;
    request.rayOrigin = aim->worldRayOrigin;
    request.rayDirection = direction;
    request.hitPoint = aim->worldAimPoint;
    request.hitNormal = aim->worldAimNormal;
    request.hitDistance = aim->aimDistance;
    request.baseDamage = shot.damage;
    ApplyWeaponHit(runtime, input.course, request);

    if (!lastWeaponFeedbackResult_.accepted) {
        lastFrameStats_.playerShotStaleHits++;
        return;
    }

    if (aim->hitKind == RailAimHitKind::Enemy &&
        lastDamageResult_.damageApplied) {
        lastFrameStats_.playerShotEnemyHits++;
    } else if (aim->hitKind == RailAimHitKind::Obstacle &&
               lastDamageResult_.damageApplied) {
        lastFrameStats_.playerShotObstacleHits++;
    } else if (aim->hitKind == RailAimHitKind::TerrainPlacement ||
               aim->hitKind == RailAimHitKind::ProceduralTerrain) {
        lastFrameStats_.playerShotTerrainHits++;
    }

    lastFrameStats_.playerShotWorldHits++;
    const WeaponFeedbackEvent& feedback = lastWeaponFeedbackResult_.event;
    lastShotWorldPoint_ = feedback.worldPosition;
    lastShotWorldNormal_ = feedback.worldNormal;
    lastShotHitKind_ = feedback.hitKind;
    lastShotHitActorId_ = feedback.targetActorId;
    lastShotHasWorldHit_ = true;
    lastShotDistance_ = input.player.distance + aim->aimDistance;
}

void CourseCollisionSystem::AppendDebugDraw(
    ge3::debug::DebugDrawSystem& debugDraw,
    const RailPath& railPath) const {
    if (railPath.Length() <= 0.0f) {
        return;
    }

    const RailPathSample playerSample = railPath.Evaluate(player_.distance);
    const Vector3 playerCenter{
        playerSample.position.x + playerSample.right.x * player_.lateralOffset + playerSample.up.x * player_.verticalOffset,
        playerSample.position.y + playerSample.right.y * player_.lateralOffset + playerSample.up.y * player_.verticalOffset,
        playerSample.position.z + playerSample.right.z * player_.lateralOffset + playerSample.up.z * player_.verticalOffset,
    };
    const Vector4 playerColor =
        player_.invulnerabilityTime > 0.0f ?
            Vector4{1.0f, 0.35f, 0.15f, 1.0f} :
            Vector4{0.2f, 1.0f, 0.75f, 1.0f};
    debugDraw.AddCircle(playerCenter, playerSample.right, playerSample.up, player_.radius, playerColor, 24);
}

void CourseCollisionSystem::LogFrameStats(const CourseCollisionFrameStats& stats) const {
    if (stats.enemyBulletHits == 0 &&
        stats.obstacleHits == 0 &&
        stats.playerShotWorldHits == 0 &&
        stats.playerShotStaleHits == 0 &&
        stats.playerShotEnemyHits == 0 &&
        stats.playerShotObstacleHits == 0) {
        return;
    }

    std::ostringstream line;
    line << "[CourseCollision] playerHp=" << player_.hitPoints
         << " damage=" << stats.playerDamage
         << " enemyBulletHits=" << stats.enemyBulletHits
         << " obstacleHits=" << stats.obstacleHits
         << " shots=" << stats.playerShotsFired
         << " worldShotHits=" << stats.playerShotWorldHits
         << " enemyShotHits=" << stats.playerShotEnemyHits
         << " obstacleShotHits=" << stats.playerShotObstacleHits
         << " terrainShotHits=" << stats.playerShotTerrainHits
         << " staleShotHits=" << stats.playerShotStaleHits
         << "\n";
    OutputDebugStringA(line.str().c_str());
    std::ofstream log = app::OpenRotatingLog("logs/course_collision.log");
    if (log) {
        log << line.str();
    }
}
