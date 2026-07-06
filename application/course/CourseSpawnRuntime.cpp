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
    nextActorId_ = 1;
}

void CourseSpawnRuntime::Update(float deltaTime) {
    const float dt = (std::max)(0.0f, deltaTime);

    for (CourseEnemyActor& enemy : enemies_) {
        enemy.age += dt;
        enemy.desc.distanceOffset += enemy.desc.forwardSpeed * dt;
        enemy.fireTimer -= dt;
        while (enemy.fireTimer <= 0.0f && enemy.age < enemy.desc.lifetime) {
            EmitEnemyBullets(enemy);
            enemy.fireTimer += (std::max)(0.08f, enemy.desc.fireInterval);
        }
    }

    for (CourseBulletActor& bullet : bullets_) {
        bullet.age += dt;
        bullet.distanceOffset += bullet.forwardSpeed * dt;
        bullet.lateralOffset += bullet.lateralSpeed * dt;
        bullet.verticalOffset += bullet.verticalSpeed * dt;
    }

    for (CourseObstacleActor& obstacle : obstacles_) {
        obstacle.age += dt;
        obstacle.desc.distanceOffset += obstacle.desc.forwardSpeed * dt;
    }

    for (CourseVfxCue& cue : vfxCues_) {
        cue.age += dt;
    }

    PruneDestroyedActors();
}

void CourseSpawnRuntime::PruneDestroyedActors() {
    enemies_.erase(
        std::remove_if(
            enemies_.begin(),
            enemies_.end(),
            [](const CourseEnemyActor& enemy) {
                return enemy.age >= enemy.desc.lifetime || enemy.desc.hitPoints <= 0.0f;
            }),
        enemies_.end());
    bullets_.erase(
        std::remove_if(
            bullets_.begin(),
            bullets_.end(),
            [](const CourseBulletActor& bullet) {
                return bullet.age >= bullet.lifetime;
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
    enemies_.push_back(std::move(actor));
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

        const Vector3 center = ResolveRailLocal(
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

void CourseSpawnRuntime::EmitEnemyBullets(const CourseEnemyActor& enemy) {
    const int bulletCount = (std::max)(1, enemy.desc.bulletCount);
    const float center = static_cast<float>(bulletCount - 1) * 0.5f;

    for (int i = 0; i < bulletCount; ++i) {
        const float lane = static_cast<float>(i) - center;
        CourseBulletActor bullet{};
        bullet.ownerActorId = enemy.actorId;
        bullet.sourceRole = enemy.desc.role;
        bullet.spawnDistance = enemy.desc.spawnDistance;
        bullet.distanceOffset = enemy.desc.distanceOffset - enemy.desc.radius * 1.5f;
        bullet.lateralOffset = enemy.desc.lateralOffset + lane * enemy.desc.radius * 0.7f;
        bullet.verticalOffset = enemy.desc.verticalOffset;
        bullet.forwardSpeed = -enemy.desc.bulletSpeed;
        bullet.lateralSpeed = lane * enemy.desc.bulletLateralSpreadSpeed;
        bullet.verticalSpeed = std::abs(lane) * enemy.desc.bulletVerticalSpreadSpeed;
        bullet.radius = enemy.desc.bulletRadius;
        bullet.lifetime = enemy.desc.bulletLifetime;
        bullet.damage = enemy.desc.bulletDamage;
        bullet.color = enemy.desc.bulletColor;
        bullets_.push_back(std::move(bullet));
    }
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

    for (const CourseBulletActor& bullet : bullets_) {
        const RailPathSample sample = railPath.Evaluate(bullet.spawnDistance + bullet.distanceOffset);
        const Vector3 center = ResolveRailLocal(
            railPath,
            bullet.spawnDistance,
            bullet.distanceOffset,
            bullet.lateralOffset,
            bullet.verticalOffset);
        const Vector4 color = FadeColor(bullet.color, bullet.age, bullet.lifetime, 0.35f);
        debugDraw.AddPoint(center, bullet.radius, color);
        debugDraw.AddCircle(center, sample.right, sample.up, bullet.radius * 2.0f, color, 12);
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
        const Vector3 center = ResolveRailLocal(
            railPath,
            cue.desc.spawnDistance,
            cue.desc.distanceOffset,
            cue.desc.lateralOffset,
            cue.desc.verticalOffset);
        const Vector4 color = FadeColor(cue.desc.color, cue.age, cue.desc.lifetime);
        debugDraw.AddCircle(center, sample.right, sample.up, cue.desc.radius, color, 32);
        debugDraw.AddLine(center, Add(center, Scale(sample.up, cue.desc.radius * 1.4f)), color);
    }
}
