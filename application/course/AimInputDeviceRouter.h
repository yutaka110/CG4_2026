#pragma once

#include <cstdint>

#include "RailAimAssistSystem.h"
#include "utils/math/Vector.h"

struct AimInputDeviceRouterSettings {
    float mouseMovementThresholdPixels = 0.35f;
    float mouseStrongTakeoverPixels = 1.5f;
    float gamepadActivityThreshold = 0.12f;
    float gamepadStrongTakeoverThreshold = 0.35f;
};

struct AimInputDeviceRouterInput {
    float deltaTime = 0.016f;
    Vector2 mouseDeltaPixels{};
    bool keyboardAimActive = false;
    bool gamepadConnected = false;
    Vector2 gamepadAim{};
    AimInputDeviceRouterSettings settings{};
};

struct AimInputDeviceRouterState {
    RailAimAssistInputDevice activeDevice =
        RailAimAssistInputDevice::MouseKeyboard;
    float mouseMagnitudePixels = 0.0f;
    float gamepadMagnitude = 0.0f;
    float activeDeviceSeconds = 0.0f;
    uint64_t switchRevision = 0;
    bool mouseKeyboardActive = false;
    bool gamepadActive = false;
    bool switchedThisFrame = false;
};

// Resolves the most recently intentional aim source without allowing mouse
// sensor noise or gamepad stick drift to flap the active device every frame.
class AimInputDeviceRouter {
public:
    void Reset(
        RailAimAssistInputDevice initialDevice =
            RailAimAssistInputDevice::MouseKeyboard);
    void Update(const AimInputDeviceRouterInput& input);

    const AimInputDeviceRouterState& State() const { return state_; }

private:
    AimInputDeviceRouterState state_{};
};
