#include "RailTargetRegistry.h"

#include "CourseSpawnRuntime.h"
#include "../terrain/RailPath.h"

#include <algorithm>

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

float EnemyDistance(const CourseEnemyActor& enemy) {
    return enemy.desc.spawnDistance + enemy.desc.distanceOffset;
}

float ObstacleDistance(const CourseObstacleActor& obstacle) {
    return obstacle.desc.spawnDistance + obstacle.desc.distanceOffset;
}
} // namespace

void RailTargetRegistry::Build(const RailTargetRegistryFrameInput& input) {
    anchors_.clear();
    if (input.spawnRuntime == nullptr || input.railPath == nullptr || input.railPath->Length() <= 0.0f) {
        return;
    }

    for (const CourseEnemyActor& enemy : input.spawnRuntime->Enemies()) {
        const float distance = EnemyDistance(enemy);
        RailLockAnchor anchor{};
        anchor.target.kind = RailLockTargetKind::Enemy;
        anchor.target.actorId = enemy.actorId;
        anchor.target.generationId = enemy.actorId;
        anchor.anchorId = 0;
        anchor.label = enemy.desc.role.empty() ? enemy.desc.waveId : enemy.desc.role;
        anchor.worldPosition = ResolveRailLocal(
            *input.railPath,
            enemy.desc.spawnDistance,
            enemy.desc.distanceOffset,
            enemy.desc.lateralOffset,
            enemy.desc.verticalOffset);
        anchor.screenRadius = (std::max)(input.settings.enemyScreenRadius, enemy.desc.radius * 12.0f);
        anchor.forwardDistance = distance - input.playerDistance;
        anchor.priority = 1.0f;
        anchor.maxStack = 1;
        anchors_.push_back(std::move(anchor));
    }

    for (const CourseObstacleActor& obstacle : input.spawnRuntime->Obstacles()) {
        if (!obstacle.desc.breakable) {
            continue;
        }
        const float distance = ObstacleDistance(obstacle);
        RailLockAnchor anchor{};
        anchor.target.kind = RailLockTargetKind::Obstacle;
        anchor.target.actorId = obstacle.actorId;
        anchor.target.generationId = obstacle.actorId;
        anchor.anchorId = 0;
        anchor.label = obstacle.desc.id;
        anchor.worldPosition = ResolveRailLocal(
            *input.railPath,
            obstacle.desc.spawnDistance,
            obstacle.desc.distanceOffset,
            obstacle.desc.lateralOffset,
            obstacle.desc.verticalOffset);
        const float size = (std::max)(obstacle.desc.halfExtents.x, obstacle.desc.halfExtents.y);
        anchor.screenRadius = (std::max)(input.settings.obstacleScreenRadius, size * 8.0f);
        anchor.forwardDistance = distance - input.playerDistance;
        anchor.priority = 0.8f;
        anchor.maxStack = 1;
        anchors_.push_back(std::move(anchor));
    }
}

