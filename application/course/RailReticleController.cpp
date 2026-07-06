#include "RailReticleController.h"

#include <algorithm>
#include <cmath>

namespace {
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
} // namespace

void RailReticleController::Reset() {
    state_ = {};
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

    Vector2 next = state_.currentScreenPosition;
    Vector2 cursor{};
    if (CursorInClient(input.hwnd, input.viewportWidth, input.viewportHeight, cursor)) {
        next = cursor;
    }

    const float xAxis = AxisValue(KeyDown('A') || KeyDown(VK_LEFT), KeyDown('D') || KeyDown(VK_RIGHT));
    const float yAxis = AxisValue(KeyDown('W') || KeyDown(VK_UP), KeyDown('S') || KeyDown(VK_DOWN));
    const float axisLength = std::sqrt(xAxis * xAxis + yAxis * yAxis);
    if (axisLength > 0.0001f) {
        const float invLength = 1.0f / axisLength;
        const float move = input.settings.reticleKeyboardSpeed * (std::max)(0.0f, input.deltaTime);
        next.x += xAxis * invLength * move;
        next.y += yAxis * invLength * move;
    }

    next = ClampToViewport(next, input.viewportWidth, input.viewportHeight);
    const float dt = (std::max)(input.deltaTime, 0.0001f);
    state_.velocity = {
        (next.x - state_.currentScreenPosition.x) / dt,
        (next.y - state_.currentScreenPosition.y) / dt,
    };
    state_.currentScreenPosition = next;

    state_.lockHeld = KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT) || KeyDown(VK_RBUTTON);
    state_.lockPressed = state_.lockHeld && !wasHeld;
    state_.lockReleased = !state_.lockHeld && wasHeld;
}

