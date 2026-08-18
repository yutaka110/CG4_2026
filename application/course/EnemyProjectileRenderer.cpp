#include "EnemyProjectileRenderer.h"

#include "../diagnostics/DebugDrawSystem.h"

#include <algorithm>
#include <cmath>

namespace {

Vector3 Add(Vector3 a, Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(Vector3 a, Vector3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(Vector3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Length(Vector3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector4 TrajectoryColor(EnemyProjectileTrajectory trajectory, Vector4 base) noexcept {
    switch (trajectory) {
    case EnemyProjectileTrajectory::Direct:
        return {1.0f, (std::max)(base.y, 0.18f), 0.08f, base.w};
    case EnemyProjectileTrajectory::Predictive:
        return {1.0f, 0.72f, 0.12f, base.w};
    case EnemyProjectileTrajectory::Homing:
        return {1.0f, 0.18f, 0.72f, base.w};
    case EnemyProjectileTrajectory::Arc:
        return {0.72f, 0.30f, 1.0f, base.w};
    }
    return base;
}

} // namespace

void EnemyProjectileRenderer::Reset() {
    frame_ = {};
    revision_ = 0;
}

void EnemyProjectileRenderer::Update(const EnemyProjectileRenderInput& input) {
    frame_ = {};
    if (!input.settings.enabled || input.presentation == nullptr) {
        frame_.revision = ++revision_;
        return;
    }
    const float maximumDistance = (std::max)(
        1.0f,
        input.settings.maximumDrawDistance);
    const size_t budget = (std::max)(
        static_cast<size_t>(1),
        input.settings.maximumDrawProjectiles);
    const Vector3 cameraRight = Length(input.cameraRight) > 0.00001f
        ? Scale(input.cameraRight, 1.0f / Length(input.cameraRight))
        : Vector3{1.0f, 0.0f, 0.0f};
    const Vector3 cameraUp = Length(input.cameraUp) > 0.00001f
        ? Scale(input.cameraUp, 1.0f / Length(input.cameraUp))
        : Vector3{0.0f, 1.0f, 0.0f};

    frame_.proxies.reserve((std::min)(
        budget,
        input.presentation->projectiles.size()));
    for (const EnemyProjectilePresentation& visual :
         input.presentation->projectiles) {
        const float cameraDistance = Length(Subtract(
            visual.worldPosition,
            input.cameraWorldPosition));
        if (!std::isfinite(cameraDistance) || cameraDistance > maximumDistance) {
            ++frame_.culledByDistance;
            continue;
        }
        if (frame_.proxies.size() >= budget) {
            ++frame_.droppedByBudget;
            continue;
        }
        EnemyProjectileRenderProxy proxy{};
        proxy.projectileId = visual.projectileId;
        proxy.trajectory = visual.trajectory;
        proxy.worldPosition = visual.worldPosition;
        proxy.cameraRight = cameraRight;
        proxy.cameraUp = cameraUp;
        proxy.coreColor = TrajectoryColor(visual.trajectory, visual.color);
        proxy.trailColor = proxy.coreColor;
        proxy.trailColor.w *= 0.42f;
        proxy.distanceFromCamera = cameraDistance;
        proxy.threat = visual.threat;
        const float physicalRadius = visual.collisionRadius *
            (std::max)(0.1f, input.settings.radiusScale);
        const float readableRadius = cameraDistance *
            (std::max)(0.0001f, input.settings.minimumAngularRadius);
        proxy.displayRadius = (std::clamp)(
            (std::max)(physicalRadius, readableRadius),
            0.03f,
            (std::max)(0.1f, input.settings.maximumWorldRadius));
        proxy.pulse = 1.0f + 0.12f * std::sin(
            input.elapsedTime * 9.0f +
            static_cast<float>(visual.projectileId % 17u) * 0.31f);
        const float trailLength = proxy.displayRadius *
            (std::max)(1.0f, input.settings.trailLengthInRadii);
        const float runtimeMotion = Length(Subtract(
            visual.worldPosition,
            visual.previousWorldPosition));
        proxy.trailStart = runtimeMotion > proxy.displayRadius * 0.35f
            ? visual.previousWorldPosition
            : Add(
                visual.worldPosition,
                Scale(visual.motionDirection, -trailLength));
        frame_.proxies.push_back(proxy);
    }
    frame_.sourcePresentationRevision = input.presentation->revision;
    frame_.revision = ++revision_;
}

void EnemyProjectileRenderer::AppendWorldPrimitives(
    ge3::debug::DebugDrawSystem& debugDraw) const {
    for (const EnemyProjectileRenderProxy& proxy : frame_.proxies) {
        const float radius = proxy.displayRadius * proxy.pulse;
        debugDraw.AddLine(
            proxy.trailStart,
            proxy.worldPosition,
            proxy.trailColor,
            proxy.coreColor);
        debugDraw.AddPoint(
            proxy.worldPosition,
            radius * (proxy.threat ? 0.78f : 0.58f),
            proxy.coreColor);
        debugDraw.AddCircle(
            proxy.worldPosition,
            proxy.cameraRight,
            proxy.cameraUp,
            radius,
            proxy.coreColor,
            16);
        if (proxy.trajectory == EnemyProjectileTrajectory::Homing) {
            debugDraw.AddCircle(
                proxy.worldPosition,
                proxy.cameraRight,
                proxy.cameraUp,
                radius * 1.55f,
                proxy.trailColor,
                16);
        } else if (proxy.trajectory == EnemyProjectileTrajectory::Predictive) {
            const Vector3 right = Scale(proxy.cameraRight, radius * 1.35f);
            const Vector3 up = Scale(proxy.cameraUp, radius * 1.35f);
            debugDraw.AddLine(
                Add(proxy.worldPosition, right),
                Add(proxy.worldPosition, up),
                proxy.coreColor);
            debugDraw.AddLine(
                Add(proxy.worldPosition, up),
                Subtract(proxy.worldPosition, right),
                proxy.coreColor);
            debugDraw.AddLine(
                Subtract(proxy.worldPosition, right),
                Subtract(proxy.worldPosition, up),
                proxy.coreColor);
            debugDraw.AddLine(
                Subtract(proxy.worldPosition, up),
                Add(proxy.worldPosition, right),
                proxy.coreColor);
        }
    }
}
