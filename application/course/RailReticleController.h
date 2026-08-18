#pragma once

#include <Windows.h>
#include <cstdint>

#include "AimInputDeviceRouter.h"
#include "RailLockOnTypes.h"

struct RailReticleFrameInput {
    HWND hwnd = nullptr;
    float deltaTime = 0.016f;
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    bool hasCursorPosition = false;
    Vector2 cursorPosition{};
    // Optional deterministic lock input for automated tests and non-Win32 hosts.
    // Runtime Win32 input remains the fallback when this is false.
    bool hasLockHeldOverride = false;
    bool lockHeldOverride = false;
    // Input ownership flags allow the host game to reserve keyboard movement
    // keys and Shift for player flight without changing reusable editor/tests.
    bool keyboardDirectionalAimEnabled = true;
    bool hasKeyboardAimOverride = false;
    Vector2 keyboardAimOverride{};
    bool shiftLockEnabled = true;
    bool gamepadConnected = false;
    Vector2 gamepadAim{};
    float aimFrictionScale = 1.0f;
    const std::vector<RailLockAnchor>* anchors = nullptr;
    const Matrix4x4* gameplayViewProjection = nullptr;
    Vector3 gameplayCameraPosition{};
    float aimRayMaxDistance = 120.0f;
    RailLockSettings settings{};
};

class RailReticleController {
public:
    void Reset();
    void Update(const RailReticleFrameInput& input);
    void SetAim(const RailAimState& aim) { state_.aim = aim; }
    void ApplyAimHit(const RailAimHit& hit) { ApplyRailAimHit(state_.aim, hit); }

    const RailReticleState& State() const { return state_; }
    const AimInputDeviceRouterState& InputDeviceState() const {
        return inputDeviceRouter_.State();
    }

private:
    RailReticleState state_{};
    AimInputDeviceRouter inputDeviceRouter_{};
    Vector2 previousRawPointerPosition_{};
    bool rawPointerInitialized_ = false;
};

