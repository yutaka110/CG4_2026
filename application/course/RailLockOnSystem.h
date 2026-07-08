#pragma once

#include "RailLockResolver.h"
#include "RailReticleController.h"
#include "RailTargetRegistry.h"

namespace ge3::debug {
class DebugDrawSystem;
}

struct RailLockOnFrameInput {
    HWND hwnd = nullptr;
    float deltaTime = 0.016f;
    float playerDistance = 0.0f;
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    bool hasCursorPosition = false;
    Vector2 cursorPosition{};
    const Matrix4x4* viewProjection = nullptr;
    const RailPath* railPath = nullptr;
    const CourseSpawnRuntime* spawnRuntime = nullptr;
    Vector3 cameraPosition{};
};

class RailLockOnSystem {
public:
    void Reset();
    void Update(const RailLockOnFrameInput& input);
    void AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw) const;

    const RailReticleState& Reticle() const { return reticle_.State(); }
    const std::vector<RailLockToken>& Tokens() const { return resolver_.Tokens(); }
    const RailLockRelease& LastRelease() const { return lastRelease_; }
    const RailLockDebugFrame& DebugFrame() const { return debugFrame_; }
    const RailLockSettings& Settings() const { return settings_; }
    RailLockSettings& MutableSettings() { return settings_; }

private:
    RailLockSettings settings_{};
    RailReticleController reticle_;
    RailTargetRegistry registry_;
    RailLockResolver resolver_;
    RailLockDebugFrame debugFrame_{};
    RailLockRelease lastRelease_{};
    float elapsedTime_ = 0.0f;
};
