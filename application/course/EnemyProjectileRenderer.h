#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "EnemyProjectilePresentationBridge.h"
#include "utils/math/Vector.h"

namespace ge3::debug { class DebugDrawSystem; }

struct EnemyProjectileRendererSettings final {
    bool enabled = true;
    size_t maximumDrawProjectiles = 256;
    float maximumDrawDistance = 900.0f;
    float radiusScale = 1.35f;
    float minimumAngularRadius = 0.0032f;
    float maximumWorldRadius = 3.2f;
    float trailLengthInRadii = 7.0f;
};

struct EnemyProjectileRenderProxy final {
    uint64_t projectileId = 0;
    EnemyProjectileTrajectory trajectory = EnemyProjectileTrajectory::Direct;
    Vector3 worldPosition{};
    Vector3 trailStart{};
    Vector3 cameraRight{1.0f, 0.0f, 0.0f};
    Vector3 cameraUp{0.0f, 1.0f, 0.0f};
    Vector4 coreColor{1.0f, 0.3f, 0.12f, 1.0f};
    Vector4 trailColor{1.0f, 0.18f, 0.08f, 0.45f};
    float displayRadius = 0.34f;
    float pulse = 1.0f;
    float distanceFromCamera = 0.0f;
    bool threat = false;
};

struct EnemyProjectileRenderFrame final {
    std::vector<EnemyProjectileRenderProxy> proxies;
    uint32_t culledByDistance = 0;
    uint32_t droppedByBudget = 0;
    uint64_t sourcePresentationRevision = 0;
    uint64_t revision = 0;
};

struct EnemyProjectileRenderInput final {
    const EnemyProjectilePresentationFrame* presentation = nullptr;
    Vector3 cameraWorldPosition{};
    Vector3 cameraRight{1.0f, 0.0f, 0.0f};
    Vector3 cameraUp{0.0f, 1.0f, 0.0f};
    float elapsedTime = 0.0f;
    EnemyProjectileRendererSettings settings{};
};

// Builds bounded, camera-readable projectile primitives. The minimum angular
// radius keeps hostile fire readable on long rail segments without changing
// its collision radius.
class EnemyProjectileRenderer final {
public:
    void Reset();
    void Update(const EnemyProjectileRenderInput& input);
    void AppendWorldPrimitives(ge3::debug::DebugDrawSystem& debugDraw) const;

    const EnemyProjectileRenderFrame& Frame() const noexcept { return frame_; }

private:
    EnemyProjectileRenderFrame frame_{};
    uint64_t revision_ = 0;
};
