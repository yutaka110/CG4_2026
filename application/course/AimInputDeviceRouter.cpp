#include "AimInputDeviceRouter.h"

#include <algorithm>
#include <cmath>

namespace {
float Magnitude(Vector2 value) {
    const float squared = value.x * value.x + value.y * value.y;
    return std::isfinite(squared) && squared > 0.0f ? std::sqrt(squared) : 0.0f;
}
} // namespace

void AimInputDeviceRouter::Reset(RailAimAssistInputDevice initialDevice) {
    state_ = {};
    state_.activeDevice = initialDevice;
}

void AimInputDeviceRouter::Update(const AimInputDeviceRouterInput& input) {
    state_.switchedThisFrame = false;
    state_.mouseMagnitudePixels = Magnitude(input.mouseDeltaPixels);
    state_.gamepadMagnitude = input.gamepadConnected
        ? Magnitude(input.gamepadAim)
        : 0.0f;
    state_.mouseKeyboardActive = input.keyboardAimActive ||
        state_.mouseMagnitudePixels >=
            (std::max)(0.0f, input.settings.mouseMovementThresholdPixels);
    state_.gamepadActive = input.gamepadConnected &&
        state_.gamepadMagnitude >=
            (std::max)(0.0f, input.settings.gamepadActivityThreshold);

    RailAimAssistInputDevice resolved = state_.activeDevice;
    if (!input.gamepadConnected &&
        resolved == RailAimAssistInputDevice::Gamepad) {
        resolved = RailAimAssistInputDevice::MouseKeyboard;
    } else if (state_.mouseKeyboardActive && !state_.gamepadActive) {
        resolved = RailAimAssistInputDevice::MouseKeyboard;
    } else if (state_.gamepadActive && !state_.mouseKeyboardActive) {
        resolved = RailAimAssistInputDevice::Gamepad;
    } else if (state_.mouseKeyboardActive && state_.gamepadActive) {
        // Preserve the current device for simultaneous low-level input. Only a
        // deliberate strong gesture from the other device takes ownership.
        if (resolved == RailAimAssistInputDevice::Gamepad &&
            (input.keyboardAimActive ||
             state_.mouseMagnitudePixels >= input.settings.mouseStrongTakeoverPixels)) {
            resolved = RailAimAssistInputDevice::MouseKeyboard;
        } else if (resolved == RailAimAssistInputDevice::MouseKeyboard &&
                   state_.gamepadMagnitude >=
                       input.settings.gamepadStrongTakeoverThreshold &&
                   !input.keyboardAimActive) {
            resolved = RailAimAssistInputDevice::Gamepad;
        }
    }

    if (resolved != state_.activeDevice) {
        state_.activeDevice = resolved;
        state_.activeDeviceSeconds = 0.0f;
        ++state_.switchRevision;
        state_.switchedThisFrame = true;
    } else {
        state_.activeDeviceSeconds +=
            (std::clamp)(input.deltaTime, 0.0f, 0.1f);
    }
}
