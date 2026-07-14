#include "EditorViewportInteractionService.h"

#include "EditorViewportCoordinateService.h"

namespace editor {

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
    state_.revision = nextRevision;
}

bool EditorViewportInteractionService::CanMutateAuthoring() const {
    return state_.documentEditable && state_.authoringMutationAllowed;
}

bool EditorViewportInteractionService::CanUseViewportInput() const {
    return CanMutateAuthoring() &&
        ViewportAvailable() &&
        state_.mouseAvailable &&
        state_.mouseInsideViewport &&
        !state_.imguiWantsMouse;
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
    return CanUseViewportInput() ? "ViewportInputReady" : "ViewportInputBlocked";
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
    if (state_.imguiWantsMouse) {
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
