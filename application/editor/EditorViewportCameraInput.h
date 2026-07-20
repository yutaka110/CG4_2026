#pragma once

namespace editor {

// Frame-local, ownership-filtered input consumed by the editor viewport camera.
// Raw platform input must be routed through EditorViewportInteractionService
// before populating this contract.
struct EditorViewportCameraInput {
    float deltaTime = 0.0f;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;
    float wheelDelta = 0.0f;

    bool captureStarted = false;
    bool captureActive = false;
    bool captureReleased = false;
    bool captureCancelled = false;

    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool moveUp = false;
    bool moveDown = false;

    bool orbitModifier = false;
    bool fastModifier = false;
    bool slowModifier = false;
    bool focusSelectionPressed = false;
    bool cancelPressed = false;
};

} // namespace editor
