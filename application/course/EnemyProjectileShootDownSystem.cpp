#include "EnemyProjectileShootDownSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
float Dot(Vector3 a, Vector3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vector3 Add(Vector3 a, Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vector3 Subtract(Vector3 a, Vector3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vector3 Scale(Vector3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}
Vector3 NormalizeOr(Vector3 value, Vector3 fallback) noexcept {
    const float lengthSquared = Dot(value, value);
    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) return fallback;
    return Scale(value, 1.0f / std::sqrt(lengthSquared));
}
bool Finite(Vector3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}
}

void EnemyProjectileShootDownSystem::Reset() {
    lastResult_ = {};
    frame_ = {};
    revision_ = 0;
}

void EnemyProjectileShootDownSystem::BeginFrame() {
    frame_ = {};
    frame_.revision = revision_;
}

EnemyProjectileShootDownResult EnemyProjectileShootDownSystem::Submit(
    std::vector<EnemyProjectileRuntimeState>& projectiles,
    const RailPath& railPath,
    const EnemyProjectileShootDownRequest& request) {
    lastResult_ = {};
    lastResult_.shotId = request.shotId;
    ++frame_.shotsEvaluated;
    if (request.shotId == 0 || railPath.Length() <= 0.0f ||
        !Finite(request.rayOrigin) || !Finite(request.rayDirection) ||
        !std::isfinite(request.maximumDistance) ||
        request.maximumDistance <= 0.0f || !std::isfinite(request.damage) ||
        request.damage < 0.0f) {
        lastResult_.rejectReason =
            EnemyProjectileShootDownRejectReason::InvalidShot;
        frame_.results.push_back(lastResult_);
        frame_.revision = ++revision_;
        return lastResult_;
    }
    const Vector3 direction = NormalizeOr(
        request.rayDirection, {0.0f, 0.0f, 1.0f});
    float distanceLimit = request.maximumDistance;
    if (std::isfinite(request.maximumWorldHitDistance) &&
        request.maximumWorldHitDistance > 0.0f) {
        distanceLimit = (std::min)(distanceLimit,
            request.maximumWorldHitDistance);
    }

    EnemyProjectileRuntimeState* best = nullptr;
    Vector3 bestCenter{};
    float bestDistance = std::numeric_limits<float>::max();
    for (EnemyProjectileRuntimeState& projectile : projectiles) {
        if (!projectile.active || projectile.hitConsumed ||
            projectile.age >= projectile.lifetime ||
            !projectile.shootDownEnabled ||
            projectile.shootDownHitPoints <= 0.0f) {
            continue;
        }
        const float railDistance = projectile.spawnDistance +
            projectile.distanceOffset;
        const RailPathSample sample = railPath.Evaluate(railDistance);
        const Vector3 center = Add(
            Add(sample.position,
                Scale(sample.right, projectile.lateralOffset)),
            Scale(sample.up, projectile.verticalOffset));
        const Vector3 relative = Subtract(center, request.rayOrigin);
        const float projection = Dot(relative, direction);
        if (projection < 0.0f || projection > distanceLimit) continue;
        const Vector3 closest = Add(request.rayOrigin,
            Scale(direction, projection));
        const Vector3 delta = Subtract(center, closest);
        const float radius = (std::max)(0.05f,
            projectile.radius * projectile.shootDownRadiusScale);
        if (Dot(delta, delta) > radius * radius) continue;
        const float perpendicularSquared = Dot(delta, delta);
        const float entry = projection - std::sqrt((std::max)(
            0.0f, radius * radius - perpendicularSquared));
        const float hitDistance = (std::max)(0.0f, entry);
        if (hitDistance < bestDistance) {
            best = &projectile;
            bestCenter = center;
            bestDistance = hitDistance;
        }
    }
    if (best == nullptr) {
        lastResult_.rejectReason =
            EnemyProjectileShootDownRejectReason::NoTarget;
        frame_.results.push_back(lastResult_);
        frame_.revision = ++revision_;
        return lastResult_;
    }

    lastResult_.targetResolved = true;
    lastResult_.accepted = true;
    lastResult_.projectileId = best->projectileId;
    lastResult_.ownerActorId = best->ownerActorId;
    lastResult_.hitDistance = bestDistance;
    lastResult_.worldHitPoint = Add(request.rayOrigin,
        Scale(direction, bestDistance));
    lastResult_.worldHitNormal = NormalizeOr(
        Subtract(lastResult_.worldHitPoint, bestCenter),
        Scale(direction, -1.0f));
    lastResult_.appliedDamage = (std::min)(
        request.damage, best->shootDownHitPoints);
    best->shootDownHitPoints = (std::max)(
        0.0f, best->shootDownHitPoints - lastResult_.appliedDamage);
    lastResult_.remainingHitPoints = best->shootDownHitPoints;
    lastResult_.destroyed = best->shootDownHitPoints <= 0.0f;
    ++frame_.projectilesHit;
    if (lastResult_.destroyed) {
        best->active = false;
        best->hitConsumed = true;
        best->age = best->lifetime;
        ++frame_.projectilesDestroyed;
    }
    frame_.results.push_back(lastResult_);
    frame_.revision = ++revision_;
    return lastResult_;
}

const char* ToString(EnemyProjectileShootDownRejectReason reason) noexcept {
    switch (reason) {
    case EnemyProjectileShootDownRejectReason::None: return "None";
    case EnemyProjectileShootDownRejectReason::InvalidShot: return "InvalidShot";
    case EnemyProjectileShootDownRejectReason::NoTarget: return "NoTarget";
    case EnemyProjectileShootDownRejectReason::ProjectileNotShootable:
        return "ProjectileNotShootable";
    }
    return "Unknown";
}
