#include "RailReticleController.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
struct ProjectedPoint {
    Vector2 screen{};
    float depth = 0.0f;
    bool behind = false;
};

bool KeyDown(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

float AxisValue(bool negative, bool positive) {
    return (positive ? 1.0f : 0.0f) - (negative ? 1.0f : 0.0f);
}

Vector2 ClampToViewport(Vector2 value, uint32_t width, uint32_t height) {
    const float maxX = width > 0 ? static_cast<float>(width - 1u) : 0.0f;
    const float maxY = height > 0 ? static_cast<float>(height - 1u) : 0.0f;
    value.x = (std::clamp)(value.x, 0.0f, maxX);
    value.y = (std::clamp)(value.y, 0.0f, maxY);
    return value;
}

bool CursorInClient(HWND hwnd, uint32_t width, uint32_t height, Vector2& outPosition) {
    if (hwnd == nullptr || width == 0 || height == 0) {
        return false;
    }
    POINT cursor{};
    if (!GetCursorPos(&cursor) || !ScreenToClient(hwnd, &cursor)) {
        return false;
    }
    if (cursor.x < 0 || cursor.y < 0 ||
        cursor.x >= static_cast<LONG>(width) ||
        cursor.y >= static_cast<LONG>(height)) {
        return false;
    }
    outPosition = {static_cast<float>(cursor.x), static_cast<float>(cursor.y)};
    return true;
}

ProjectedPoint ProjectToScreen(
    const Vector3& point,
    const Matrix4x4& matrix,
    uint32_t width,
    uint32_t height) {
    const float x =
        point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0];
    const float y =
        point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1];
    const float z =
        point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2];
    const float w =
        point.x * matrix.m[0][3] + point.y * matrix.m[1][3] + point.z * matrix.m[2][3] + matrix.m[3][3];

    ProjectedPoint result{};
    if (w <= 0.00001f) {
        result.behind = true;
        return result;
    }
    const float ndcX = x / w;
    const float ndcY = y / w;
    result.depth = z / w;
    result.screen = {
        (ndcX * 0.5f + 0.5f) * static_cast<float>(width),
        (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height),
    };
    return result;
}

float Clamp01(float value) {
    return (std::clamp)(value, 0.0f, 1.0f);
}

float Distance(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

float SafeRange01(float value, float minValue, float maxValue) {
    const float range = maxValue - minValue;
    if (range <= 0.0001f) {
        return 0.0f;
    }
    return Clamp01((value - minValue) / range);
}

Vector2 PullToward(Vector2 current, Vector2 target, float pullPixels) {
    const float dx = target.x - current.x;
    const float dy = target.y - current.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= 0.0001f || pullPixels <= 0.0f) {
        return current;
    }
    const float amount = (std::min)(pullPixels, distance);
    const float invDistance = 1.0f / distance;
    return {
        current.x + dx * invDistance * amount,
        current.y + dy * invDistance * amount,
    };
}
} // namespace

void RailReticleController::Reset() {
    state_ = {};
    inputDeviceRouter_.Reset();
    previousRawPointerPosition_ = {};
    rawPointerInitialized_ = false;
}

void RailReticleController::Update(const RailReticleFrameInput& input) {
    const float width = static_cast<float>(input.viewportWidth);
    const float height = static_cast<float>(input.viewportHeight);
    if (!state_.initialized) {
        state_.currentScreenPosition = {width * 0.5f, height * 0.5f};
        state_.previousScreenPosition = state_.currentScreenPosition;
        state_.initialized = true;
    }

    const bool wasHeld = state_.lockHeld;
    state_.previousScreenPosition = state_.currentScreenPosition;
    state_.aimFeelActive = false;
    state_.aimFeelStrength = 0.0f;
    state_.aimFeelPullPixels = 0.0f;
    state_.aimFeelTargetScore = 0.0f;

    Vector2 next = state_.currentScreenPosition;
    Vector2 cursor{};
    bool hasPointerSample = false;
    if (input.hasCursorPosition) {
        cursor = input.cursorPosition;
        hasPointerSample = true;
    } else if (CursorInClient(input.hwnd, input.viewportWidth, input.viewportHeight, cursor)) {
        hasPointerSample = true;
    }

    const float xAxis = AxisValue(KeyDown('A') || KeyDown(VK_LEFT), KeyDown('D') || KeyDown(VK_RIGHT));
    const float yAxis = AxisValue(KeyDown('W') || KeyDown(VK_UP), KeyDown('S') || KeyDown(VK_DOWN));
    const float axisLength = std::sqrt(xAxis * xAxis + yAxis * yAxis);
    Vector2 mouseDelta{};
    bool initialPointerSample = false;
    if (hasPointerSample) {
        cursor = ClampToViewport(cursor, input.viewportWidth, input.viewportHeight);
        if (rawPointerInitialized_) {
            mouseDelta = {
                cursor.x - previousRawPointerPosition_.x,
                cursor.y - previousRawPointerPosition_.y};
        } else {
            previousRawPointerPosition_ = cursor;
            rawPointerInitialized_ = true;
            initialPointerSample = true;
        }
        previousRawPointerPosition_ = cursor;
    }

    AimInputDeviceRouterInput routerInput{};
    routerInput.deltaTime = input.deltaTime;
    routerInput.mouseDeltaPixels = mouseDelta;
    routerInput.keyboardAimActive = axisLength > 0.0001f;
    routerInput.gamepadConnected = input.gamepadConnected;
    routerInput.gamepadAim = input.gamepadAim;
    inputDeviceRouter_.Update(routerInput);

    const bool currentLockHeld = input.hasLockHeldOverride
        ? input.lockHeldOverride
        : (KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT) || KeyDown(VK_RBUTTON));
    const float frictionScale = currentLockHeld
        ? 1.0f
        : (std::clamp)(input.aimFrictionScale, 0.15f, 1.0f);
    state_.appliedAimFrictionScale = frictionScale;
    const RailAimAssistInputDevice activeDevice =
        inputDeviceRouter_.State().activeDevice;
    if (initialPointerSample &&
        activeDevice == RailAimAssistInputDevice::MouseKeyboard) {
        next = cursor;
    } else if (activeDevice == RailAimAssistInputDevice::MouseKeyboard) {
        next.x += mouseDelta.x * frictionScale;
        next.y += mouseDelta.y * frictionScale;
    }

    if (axisLength > 0.0001f &&
        activeDevice == RailAimAssistInputDevice::MouseKeyboard) {
        const float invLength = 1.0f / axisLength;
        const float move = input.settings.reticleKeyboardSpeed *
            (std::max)(0.0f, input.deltaTime) * frictionScale;
        next.x += xAxis * invLength * move;
        next.y += yAxis * invLength * move;
    }
    if (activeDevice == RailAimAssistInputDevice::Gamepad &&
        input.gamepadConnected) {
        const float move = input.settings.reticleGamepadSpeed *
            (std::max)(0.0f, input.deltaTime) * frictionScale;
        next.x += input.gamepadAim.x * move;
        next.y -= input.gamepadAim.y * move;
    }

    // Mouse input is an absolute on-screen authority while locking. Positional
    // magnetism must not move the gameplay reticle away from the visible OS
    // cursor; gamepad remains free to use target pull because it has no cursor.
    if (currentLockHeld &&
        activeDevice == RailAimAssistInputDevice::MouseKeyboard &&
        hasPointerSample) {
        next = cursor;
    }

    next = ClampToViewport(next, input.viewportWidth, input.viewportHeight);
    state_.lockHeld = currentLockHeld;
    state_.lockPressed = state_.lockHeld && !wasHeld;
    state_.lockReleased = !state_.lockHeld && wasHeld;

    if (state_.lockHeld &&
        activeDevice == RailAimAssistInputDevice::Gamepad &&
        input.settings.lockAimFeelEnabled &&
        input.anchors != nullptr &&
        input.gameplayViewProjection != nullptr &&
        input.viewportWidth > 0 &&
        input.viewportHeight > 0) {
        const Vector2 screenCenter{
            static_cast<float>(input.viewportWidth) * 0.5f,
            static_cast<float>(input.viewportHeight) * 0.5f};
        const float halfDiagonal = (std::max)(
            1.0f,
            std::sqrt(screenCenter.x * screenCenter.x + screenCenter.y * screenCenter.y));
        const float dt = (std::max)(input.deltaTime, 0.0f);
        const float intentTotal = (std::max)(
            0.0001f,
            input.settings.lockAimReticleIntentWeight +
                input.settings.lockAimCenterIntentWeight +
                input.settings.lockAimForwardIntentWeight +
                input.settings.lockPriorityAnchorWeight * 0.20f);

        Vector2 bestTarget{};
        float bestDistance = 0.0f;
        float bestScore = -(std::numeric_limits<float>::max)();
        bool hasBest = false;

        for (const RailLockAnchor& anchor : *input.anchors) {
            if (anchor.forwardDistance < input.settings.minForwardDistance ||
                anchor.forwardDistance > input.settings.maxForwardDistance) {
                continue;
            }

            const ProjectedPoint projected =
                ProjectToScreen(
                    anchor.worldPosition,
                    *input.gameplayViewProjection,
                    input.viewportWidth,
                    input.viewportHeight);
            if (projected.behind || projected.depth < 0.0f || projected.depth > 1.0f) {
                continue;
            }

            const float magnetRadius =
                (std::max)(1.0f, anchor.screenRadius + input.settings.assistRadius + input.settings.lockAimMagnetRadius);
            const float reticleDistance = Distance(next, projected.screen);
            if (reticleDistance > magnetRadius) {
                continue;
            }

            const float reticleScore = 1.0f - Clamp01(reticleDistance / magnetRadius);
            const float centerScore = 1.0f - Clamp01(Distance(projected.screen, screenCenter) / halfDiagonal);
            const float forwardScore =
                1.0f - SafeRange01(anchor.forwardDistance, input.settings.minForwardDistance, input.settings.maxForwardDistance);
            const float anchorScore = Clamp01(anchor.priority);
            const float score =
                (reticleScore * input.settings.lockAimReticleIntentWeight +
                 centerScore * input.settings.lockAimCenterIntentWeight +
                 forwardScore * input.settings.lockAimForwardIntentWeight +
                 anchorScore * input.settings.lockPriorityAnchorWeight * 0.20f) /
                intentTotal;

            if (!hasBest || score > bestScore) {
                hasBest = true;
                bestScore = score;
                bestTarget = projected.screen;
                bestDistance = reticleDistance;
            }
        }

        if (hasBest && bestDistance > input.settings.lockAimDeadZone) {
            const float normalizedScore = Clamp01(bestScore);
            const float strength = normalizedScore * input.settings.lockAimMagnetStrength;
            const float pullByFeel =
                (bestDistance - input.settings.lockAimDeadZone) *
                strength *
                input.settings.lockAimTargetBlend;
            const float pullBySpeed = input.settings.lockAimMaxPullSpeed * dt;
            const float pullPixels = (std::max)(0.0f, (std::min)(pullByFeel, pullBySpeed));
            next = PullToward(next, bestTarget, pullPixels);
            next = ClampToViewport(next, input.viewportWidth, input.viewportHeight);
            state_.aimFeelActive = pullPixels > 0.0001f;
            state_.aimFeelTargetScreenPosition = bestTarget;
            state_.aimFeelStrength = strength;
            state_.aimFeelPullPixels = pullPixels;
            state_.aimFeelTargetScore = normalizedScore;
        }
    }

    const float dt = (std::max)(input.deltaTime, 0.0001f);
    state_.velocity = {
        (next.x - state_.currentScreenPosition.x) / dt,
        (next.y - state_.currentScreenPosition.y) / dt,
    };
    state_.currentScreenPosition = next;

    state_.aim = {};
    if (input.gameplayViewProjection != nullptr) {
        RailAimRayBuildInput aimInput{};
        aimInput.pixelPosition = state_.currentScreenPosition;
        aimInput.viewportWidth = input.viewportWidth;
        aimInput.viewportHeight = input.viewportHeight;
        aimInput.gameplayViewProjection = *input.gameplayViewProjection;
        aimInput.gameplayCameraPosition = input.gameplayCameraPosition;
        aimInput.maxDistance = input.aimRayMaxDistance;
        state_.aim = BuildRailAimState(aimInput);
    }
}

