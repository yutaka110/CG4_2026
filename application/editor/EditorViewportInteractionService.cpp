#include "EditorViewportInteractionService.h"

#include "EditorViewportCoordinateService.h"

namespace editor {

const char* ToString(EditorViewportPointerOwner owner) {
    switch (owner) {
    case EditorViewportPointerOwner::None: return "None";
    case EditorViewportPointerOwner::ViewportSurface: return "Viewport Surface";
    case EditorViewportPointerOwner::ViewportCamera: return "Viewport Camera";
    case EditorViewportPointerOwner::InteractiveTool: return "Interactive Tool";
    case EditorViewportPointerOwner::EditorUi: return "Editor UI";
    case EditorViewportPointerOwner::PopupOrModal: return "Popup / Modal";
    }
    return "Unknown";
}

void EditorViewportInteractionService::Update(const EditorViewportInteractionInput& input) {
    const uint32_t nextRevision = state_.revision + 1;
    state_.viewportRect = input.viewportRect;
    state_.renderWidth = input.renderWidth;
    state_.renderHeight = input.renderHeight;
    state_.mouseX = input.mouseX;
    state_.mouseY = input.mouseY;
    state_.mouseAvailable = input.mouseAvailable;
    state_.mouseInsideViewport =
        input.mouseAvailable && Contains(input.viewportRect, input.mouseX, input.mouseY);
    state_.mouseViewportX = 0.0f;
    state_.mouseViewportY = 0.0f;
    if (state_.mouseInsideViewport && input.renderWidth > 0 && input.renderHeight > 0) {
        EditorViewportCoordinateService coordinates;
        coordinates.Update(EditorViewportCoordinateContext{
            input.viewportRect,
            input.renderWidth,
            input.renderHeight});
        const EditorViewportCoordinatePoint renderPoint =
            coordinates.DisplayToRender(input.mouseX, input.mouseY);
        if (renderPoint.valid) {
            state_.mouseViewportX = renderPoint.x;
            state_.mouseViewportY = renderPoint.y;
        }
    }
    state_.imguiWantsMouse = input.imguiWantsMouse;
    state_.developerToolsVisible = input.developerToolsVisible;
    state_.viewportFocusMode = input.viewportFocusMode;
    state_.documentEditable = input.documentEditable;
    state_.authoringMutationAllowed = input.authoringMutationAllowed;
    state_.playSessionActive = input.playSessionActive;
    state_.viewportOwnsMouse = input.viewportOwnsMouse;
    state_.viewportUiBlocked = input.viewportUiBlocked;
    state_.popupOrModalActive = input.popupOrModalActive;
    state_.interactiveToolActive = input.interactiveToolActive;
    state_.viewportPrimaryPressed = false;
    state_.viewportPrimaryDown = false;
    state_.viewportPrimaryReleased = false;
    state_.primaryCaptureCancelled = false;
    state_.viewportCameraCaptureStarted = false;
    state_.viewportCameraCaptureDown = false;
    state_.viewportCameraCaptureReleased = false;
    state_.viewportCameraCaptureCancelled = false;

    if (primaryCaptureOwner_ == EditorViewportPointerOwner::InteractiveTool &&
        !input.interactiveToolActive) {
        primaryCaptureOwner_ = EditorViewportPointerOwner::None;
    }

    const bool routingAvailable =
        input.applicationFocused && CanMutateAuthoring() &&
        ViewportAvailable() && state_.mouseAvailable;
    if (primaryCaptureOwner_ != EditorViewportPointerOwner::None &&
        (!routingAvailable || state_.popupOrModalActive)) {
        state_.primaryCaptureCancelled = true;
        primaryCaptureOwner_ = EditorViewportPointerOwner::None;
    }

    const bool cameraRoutingAvailable =
        input.applicationFocused && ViewportAvailable() && state_.mouseAvailable;
    const bool cameraCaptureLost =
        viewportCameraCaptureActive_ &&
        !input.cameraCaptureDown && !input.cameraCaptureReleased;
    if (viewportCameraCaptureActive_ &&
        (!cameraRoutingAvailable || state_.popupOrModalActive ||
            input.cameraCaptureCancelRequested || cameraCaptureLost)) {
        state_.viewportCameraCaptureCancelled = true;
        viewportCameraCaptureActive_ = false;
    }

    const bool surfaceCanOwnPointer =
        routingAvailable && state_.mouseInsideViewport &&
        !state_.viewportUiBlocked && !state_.popupOrModalActive &&
        (!state_.imguiWantsMouse || state_.viewportOwnsMouse);
    const bool cameraSurfaceCanOwnPointer =
        cameraRoutingAvailable && state_.mouseInsideViewport &&
        !state_.viewportUiBlocked && !state_.popupOrModalActive &&
        (!state_.imguiWantsMouse || state_.viewportOwnsMouse);
    EditorViewportPointerOwner hoverOwner = EditorViewportPointerOwner::None;
    if (state_.popupOrModalActive) {
        hoverOwner = EditorViewportPointerOwner::PopupOrModal;
    } else if (state_.viewportUiBlocked ||
        (state_.imguiWantsMouse && !state_.viewportOwnsMouse)) {
        hoverOwner = EditorViewportPointerOwner::EditorUi;
    } else if (surfaceCanOwnPointer) {
        hoverOwner = input.interactiveToolActive
            ? EditorViewportPointerOwner::InteractiveTool
            : EditorViewportPointerOwner::ViewportSurface;
    }

    EditorViewportPointerOwner eventOwner = primaryCaptureOwner_ !=
            EditorViewportPointerOwner::None
        ? primaryCaptureOwner_
        : hoverOwner;
    const bool eventOwnerIsViewport =
        eventOwner == EditorViewportPointerOwner::ViewportSurface ||
        eventOwner == EditorViewportPointerOwner::InteractiveTool;
    if (input.primaryPressed && eventOwnerIsViewport &&
        !viewportCameraCaptureActive_) {
        primaryCaptureOwner_ = eventOwner;
        state_.viewportPrimaryPressed = true;
    }
    if (primaryCaptureOwner_ != EditorViewportPointerOwner::None) {
        eventOwner = primaryCaptureOwner_;
        state_.viewportPrimaryDown = input.primaryDown;
        state_.viewportPrimaryReleased = input.primaryReleased;
    }

    if (input.cameraCapturePressed && cameraSurfaceCanOwnPointer &&
        primaryCaptureOwner_ == EditorViewportPointerOwner::None &&
        !viewportCameraCaptureActive_) {
        viewportCameraCaptureActive_ = true;
        state_.viewportCameraCaptureStarted = true;
    }
    if (viewportCameraCaptureActive_) {
        eventOwner = EditorViewportPointerOwner::ViewportCamera;
        state_.viewportCameraCaptureDown = input.cameraCaptureDown;
        state_.viewportCameraCaptureReleased = input.cameraCaptureReleased;
    }
    state_.pointerOwner = eventOwner;
    state_.captureOwner = viewportCameraCaptureActive_
        ? EditorViewportPointerOwner::ViewportCamera
        : primaryCaptureOwner_;
    if (input.primaryReleased && primaryCaptureOwner_ != EditorViewportPointerOwner::None) {
        primaryCaptureOwner_ = EditorViewportPointerOwner::None;
    }
    if (input.cameraCaptureReleased && viewportCameraCaptureActive_) {
        viewportCameraCaptureActive_ = false;
    }
    state_.revision = nextRevision;
}

bool EditorViewportInteractionService::CanMutateAuthoring() const {
    return state_.documentEditable && state_.authoringMutationAllowed;
}

bool EditorViewportInteractionService::CanUseViewportInput() const {
    return CanUseInteractiveToolInput() ||
        CanUseViewportCameraInput() || CanUseSceneInput();
}

bool EditorViewportInteractionService::CanUseInteractiveToolInput() const {
    return state_.pointerOwner == EditorViewportPointerOwner::InteractiveTool;
}

bool EditorViewportInteractionService::CanUseViewportCameraInput() const {
    return state_.pointerOwner == EditorViewportPointerOwner::ViewportCamera;
}

bool EditorViewportInteractionService::CanUseSceneInput() const {
    return state_.pointerOwner == EditorViewportPointerOwner::ViewportSurface;
}

const char* EditorViewportInteractionService::BoundaryLabel() const {
    if (!ViewportAvailable()) {
        return "ViewportUnavailable";
    }
    if (!state_.mouseAvailable) {
        return "MouseUnavailable";
    }
    return state_.mouseInsideViewport ? "InsideViewport" : "OutsideViewport";
}

const char* EditorViewportInteractionService::AuthoringLabel() const {
    return CanMutateAuthoring() ? "AuthoringOpen" : "AuthoringLocked";
}

const char* EditorViewportInteractionService::ViewportInputLabel() const {
    if (CanUseInteractiveToolInput()) return "InteractiveToolInputReady";
    if (CanUseViewportCameraInput()) return "ViewportCameraInputReady";
    if (CanUseSceneInput()) return "ViewportSceneInputReady";
    return "ViewportInputBlocked";
}

const char* EditorViewportInteractionService::DisabledReason() const {
    if (!state_.documentEditable) {
        return "Course document is not editable.";
    }
    if (!state_.authoringMutationAllowed) {
        return state_.playSessionActive
            ? "Authoring is locked during Play/Sim."
            : "Authoring mutation is locked.";
    }
    if (!ViewportAvailable()) {
        return "Viewport rect is unavailable.";
    }
    if (!state_.mouseAvailable) {
        return "Mouse position is unavailable.";
    }
    if (!state_.mouseInsideViewport) {
        return "Mouse is outside the editor viewport.";
    }
    if (state_.popupOrModalActive) {
        return "A popup or modal owns mouse input.";
    }
    if (state_.viewportUiBlocked) {
        return "Viewport overlay UI owns mouse input.";
    }
    if (state_.imguiWantsMouse && !state_.viewportOwnsMouse) {
        return "Editor UI is capturing mouse input.";
    }
    return "";
}

bool EditorViewportInteractionService::Contains(const EditorPanelRect& rect, float x, float y) {
    return rect.Valid() &&
        x >= rect.x &&
        y >= rect.y &&
        x < rect.x + rect.width &&
        y < rect.y + rect.height;
}

} // namespace editor
