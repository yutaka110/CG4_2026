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

void CourseCollisionSystem::Reset() {
    player_ = {};
    weapon_ = {};
    lastFrameStats_ = {};
    lastShotDistance_ = 0.0f;
    lastShotLateralOffset_ = 0.0f;
    lastShotVerticalOffset_ = 0.0f;
    lastShotVisible_ = false;
}

CourseCollisionFrameStats CourseCollisionSystem::Update(
    CourseSpawnRuntime& runtime,
    const CourseCollisionFrameInput& input) {
    const float dt = (std::max)(0.0f, input.deltaTime);
    lastFrameStats_ = {};
    lastShotVisible_ = false;

    player_.distance = input.player.distance;
    player_.lateralOffset = input.player.lateralOffset;
    player_.verticalOffset = input.player.verticalOffset;
    player_.radius = (std::max)(0.1f, input.player.radius);
    if (player_.hitPoints <= 0.0f && input.player.hitPoints > 0.0f) {
        player_.hitPoints = input.player.hitPoints;
    }
    player_.invulnerabilityTime =
        (std::max)(0.0f, player_.invulnerabilityTime - dt);

    weapon_.enabled = input.weapon.enabled;
    weapon_.triggerHeld = input.weapon.triggerHeld;
    weapon_.triggerPressed = input.weapon.triggerPressed;
    weapon_.triggerReleased = input.weapon.triggerReleased;
    weapon_.shotInterval = (std::max)(0.03f, input.weapon.shotInterval);
    weapon_.range = (std::max)(1.0f, input.weapon.range);
    weapon_.radius = (std::max)(0.1f, input.weapon.radius);
    weapon_.damage = (std::max)(0.0f, input.weapon.damage);
    weapon_.assistEnabled = input.weapon.assistEnabled;
    weapon_.assistLateralOffset = input.weapon.assistLateralOffset;
    weapon_.assistVerticalOffset = input.weapon.assistVerticalOffset;
    weapon_.muzzleForwardOffset = (std::max)(0.0f, input.weapon.muzzleForwardOffset);
    weapon_.tracerForwardDistance = (std::max)(4.0f, input.weapon.tracerForwardDistance);
    weapon_.muzzleRadius = (std::max)(0.05f, input.weapon.muzzleRadius);
    weapon_.tracerRadius = (std::max)(0.05f, input.weapon.tracerRadius);
    weapon_.shotTimer -= dt;
    if (weapon_.triggerPressed) {
        weapon_.shotTimer = (std::min)(weapon_.shotTimer, 0.0f);
    }
    if (!weapon_.triggerHeld) {
        weapon_.shotTimer = (std::min)(weapon_.shotTimer, 0.0f);
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

    while (weapon_.enabled && weapon_.triggerHeld && weapon_.shotTimer <= 0.0f) {
        lastFrameStats_.playerShotsFired++;
        FirePlayerShot(runtime, input);
        weapon_.shotTimer += weapon_.shotInterval;
        break;
    }

    runtime.PruneDestroyedActors();
    LogFrameStats(lastFrameStats_);
    return lastFrameStats_;
}

void CourseCollisionSystem::FirePlayerShot(
    CourseSpawnRuntime& runtime,
    const CourseCollisionFrameInput& input) {
    const float minDistance = input.player.distance + 4.0f;
    const float maxDistance = input.player.distance + weapon_.range;
    const float aimLateral = input.weapon.assistEnabled
        ? input.weapon.assistLateralOffset
        : input.player.lateralOffset;
    const float aimVertical = input.weapon.assistEnabled
        ? input.weapon.assistVerticalOffset
        : input.player.verticalOffset;
    float bestDistance = (std::numeric_limits<float>::max)();
    CourseEnemyActor* bestEnemy = nullptr;
    CourseObstacleActor* bestObstacle = nullptr;

    for (CourseEnemyActor& enemy : runtime.MutableEnemies()) {
        const float distance = ActorDistance(enemy);
        if (distance < minDistance || distance > maxDistance) {
            continue;
        }
        const float allowed = weapon_.radius + enemy.desc.radius;
        if (Abs(enemy.desc.lateralOffset - aimLateral) > allowed ||
            Abs(enemy.desc.verticalOffset - aimVertical) > allowed) {
            continue;
        }
        if (distance < bestDistance) {
            bestDistance = distance;
            bestEnemy = &enemy;
            bestObstacle = nullptr;
        }
    }

    for (CourseObstacleActor& obstacle : runtime.MutableObstacles()) {
        if (!obstacle.desc.breakable) {
            continue;
        }
        const float distance = ObstacleDistance(obstacle);
        if (distance < minDistance || distance > maxDistance) {
            continue;
        }
        if (Abs(obstacle.desc.lateralOffset - aimLateral) >
                obstacle.desc.halfExtents.x + weapon_.radius ||
            Abs(obstacle.desc.verticalOffset - aimVertical) >
                obstacle.desc.halfExtents.y + weapon_.radius) {
            continue;
        }
        if (distance < bestDistance) {
            bestDistance = distance;
            bestEnemy = nullptr;
            bestObstacle = &obstacle;
        }
    }

    if (bestEnemy != nullptr) {
        const float hitDistance = ActorDistance(*bestEnemy);
        bestEnemy->desc.hitPoints -= weapon_.damage;
        lastFrameStats_.playerShotEnemyHits++;
        lastShotDistance_ = hitDistance;
        lastShotLateralOffset_ = bestEnemy->desc.lateralOffset;
        lastShotVerticalOffset_ = bestEnemy->desc.verticalOffset;
        lastShotVisible_ = true;
        SpawnImpactCue(
            runtime,
            "player_enemy_shot",
            "hit_ring",
            lastShotDistance_,
            lastShotLateralOffset_,
            lastShotVerticalOffset_,
            0.65f,
            {0.56f, 0.90f, 1.0f, 0.82f},
            0.42f);
        return;
    }

    if (bestObstacle != nullptr) {
        const float hitDistance = ObstacleDistance(*bestObstacle);
        bestObstacle->desc.hitPoints -= weapon_.damage;
        lastFrameStats_.playerShotObstacleHits++;
        lastShotDistance_ = hitDistance;
        lastShotLateralOffset_ = bestObstacle->desc.lateralOffset;
        lastShotVerticalOffset_ = bestObstacle->desc.verticalOffset;
        lastShotVisible_ = true;
        SpawnImpactCue(
            runtime,
            "player_obstacle_shot",
            "hit_plane_burst",
            lastShotDistance_,
            lastShotLateralOffset_,
            lastShotVerticalOffset_,
            0.85f,
            {1.0f, 0.72f, 0.24f, 0.82f},
            0.45f);
        return;
    }

    const float tracerDistance =
        (std::min)(input.player.distance + weapon_.range, input.player.distance + weapon_.tracerForwardDistance);
    lastShotDistance_ = tracerDistance;
    lastShotLateralOffset_ = aimLateral;
    lastShotVerticalOffset_ = aimVertical;
    lastShotVisible_ = true;
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
         << " enemyShotHits=" << stats.playerShotEnemyHits
         << " obstacleShotHits=" << stats.playerShotObstacleHits
         << "\n";
    OutputDebugStringA(line.str().c_str());
    std::ofstream log = app::OpenRotatingLog("logs/course_collision.log");
    if (log) {
        log << line.str();
    }
}
