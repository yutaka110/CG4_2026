#pragma once

#include <cstdint>

#include "EditorPanelLayoutService.h"

namespace editor {

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
    bool AuthoringInputLocked() const { return !CanMutateAuthoring(); }

    const char* BoundaryLabel() const;
    const char* AuthoringLabel() const;
    const char* ViewportInputLabel() const;
    const char* DisabledReason() const;

private:
    static bool Contains(const EditorPanelRect& rect, float x, float y);

    EditorViewportInteractionState state_{};
};

} // namespace editor
