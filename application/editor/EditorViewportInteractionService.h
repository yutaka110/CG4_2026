#pragma once

#include <cstdint>

#include "EditorPanelLayoutService.h"

namespace editor {

enum class EditorViewportPointerOwner {
    None,
    ViewportSurface,
    ViewportCamera,
    InteractiveTool,
    EditorUi,
    PopupOrModal,
};

const char* ToString(EditorViewportPointerOwner owner);

struct EditorViewportInteractionInput {
    EditorPanelRect viewportRect{};
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool mouseAvailable = false;
    bool imguiWantsMouse = false;
    bool developerToolsVisible = false;
    bool viewportFocusMode = false;
    bool documentEditable = false;
    bool authoringMutationAllowed = true;
    bool playSessionActive = false;
    bool viewportOwnsMouse = false;
    bool viewportUiBlocked = false;
    bool popupOrModalActive = false;
    bool interactiveToolActive = false;
    bool primaryPressed = false;
    bool primaryDown = false;
    bool primaryReleased = false;
    bool cameraCapturePressed = false;
    bool cameraCaptureDown = false;
    bool cameraCaptureReleased = false;
    bool cameraCaptureCancelRequested = false;
    bool applicationFocused = true;
};

struct EditorViewportInteractionState {
    EditorPanelRect viewportRect{};
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    float mouseViewportX = 0.0f;
    float mouseViewportY = 0.0f;
    bool mouseAvailable = false;
    bool mouseInsideViewport = false;
    bool imguiWantsMouse = false;
    bool developerToolsVisible = false;
    bool viewportFocusMode = false;
    bool documentEditable = false;
    bool authoringMutationAllowed = true;
    bool playSessionActive = false;
    bool viewportOwnsMouse = false;
    bool viewportUiBlocked = false;
    bool popupOrModalActive = false;
    bool interactiveToolActive = false;
    bool viewportPrimaryPressed = false;
    bool viewportPrimaryDown = false;
    bool viewportPrimaryReleased = false;
    bool primaryCaptureCancelled = false;
    bool viewportCameraCaptureStarted = false;
    bool viewportCameraCaptureDown = false;
    bool viewportCameraCaptureReleased = false;
    bool viewportCameraCaptureCancelled = false;
    EditorViewportPointerOwner pointerOwner = EditorViewportPointerOwner::None;
    EditorViewportPointerOwner captureOwner = EditorViewportPointerOwner::None;
    uint32_t revision = 0;
};

class EditorViewportInteractionService {
public:
    void Update(const EditorViewportInteractionInput& input);

    const EditorViewportInteractionState& State() const { return state_; }
    uint32_t Revision() const { return state_.revision; }

    bool ViewportAvailable() const { return state_.viewportRect.Valid(); }
    bool MouseInsideViewport() const { return state_.mouseInsideViewport; }
    bool CanMutateAuthoring() const;
    bool CanUseViewportInput() const;
    bool CanUseInteractiveToolInput() const;
    bool CanUseViewportCameraInput() const;
    bool CanUseSceneInput() const;
    bool HasPrimaryCapture() const {
        return primaryCaptureOwner_ != EditorViewportPointerOwner::None;
    }
    bool HasViewportCameraCapture() const { return viewportCameraCaptureActive_; }
    bool HasAnyCapture() const {
        return HasPrimaryCapture() || HasViewportCameraCapture();
    }
    bool AuthoringInputLocked() const { return !CanMutateAuthoring(); }

    const char* BoundaryLabel() const;
    const char* AuthoringLabel() const;
    const char* ViewportInputLabel() const;
    const char* DisabledReason() const;

private:
    static bool Contains(const EditorPanelRect& rect, float x, float y);

    EditorViewportInteractionState state_{};
    EditorViewportPointerOwner primaryCaptureOwner_ = EditorViewportPointerOwner::None;
    bool viewportCameraCaptureActive_ = false;
};

} // namespace editor
