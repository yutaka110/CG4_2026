#pragma once

#include "EditorPanelLayoutService.h"

#include <cstdint>

namespace editor {

enum class EditorContentDrawerState {
    Closed,
    Opening,
    Open,
    Pinned,
    Closing,
};

const char* ToString(EditorContentDrawerState state);

class EditorContentDrawerService {
public:
    bool Open();
    bool Close();
    bool Toggle();
    bool SetPinned(bool pinned);
    bool DockInLayout();
    void Tick(float deltaSeconds);

    EditorPanelRect ResolvePresentationRect(
        const EditorPanelRect& centerWorkspace) const;
    bool Contains(
        const EditorPanelRect& rect,
        float x,
        float y) const;
    bool HandleOutsidePointerPress(
        const EditorPanelRect& rect,
        float x,
        float y,
        bool primaryPressed,
        bool popupOrModalActive);

    bool IsVisible() const {
        return state_ != EditorContentDrawerState::Closed;
    }
    bool IsPinned() const {
        return state_ == EditorContentDrawerState::Pinned;
    }
    bool IsTransitioning() const {
        return state_ == EditorContentDrawerState::Opening ||
            state_ == EditorContentDrawerState::Closing;
    }
    bool BlocksViewportPointer(
        const EditorPanelRect& rect,
        float x,
        float y,
        bool primaryPressed) const;
    bool ConsumeFocusRequest();

    EditorContentDrawerState State() const { return state_; }
    float Openness() const { return openness_; }
    uint32_t Revision() const { return revision_; }

private:
    void Touch();

    EditorContentDrawerState state_ = EditorContentDrawerState::Closed;
    float openness_ = 0.0f;
    bool focusRequested_ = false;
    uint32_t revision_ = 0;
};

} // namespace editor
