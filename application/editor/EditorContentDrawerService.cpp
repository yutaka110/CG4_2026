#include "EditorContentDrawerService.h"

#include <algorithm>
#include <cmath>

namespace editor {
namespace {

constexpr float kDrawerHeightRatio = 0.62f;
constexpr float kDrawerMinimumHeight = 300.0f;
constexpr float kDrawerTransitionSeconds = 0.16f;
constexpr float kVisibleThreshold = 0.001f;

} // namespace

const char* ToString(EditorContentDrawerState state) {
    switch (state) {
    case EditorContentDrawerState::Closed:
        return "Closed";
    case EditorContentDrawerState::Opening:
        return "Opening";
    case EditorContentDrawerState::Open:
        return "Open";
    case EditorContentDrawerState::Pinned:
        return "Pinned";
    case EditorContentDrawerState::Closing:
        return "Closing";
    }
    return "Unknown";
}

bool EditorContentDrawerService::Open() {
    if (state_ == EditorContentDrawerState::Open ||
        state_ == EditorContentDrawerState::Pinned ||
        state_ == EditorContentDrawerState::Opening) {
        return false;
    }
    state_ = EditorContentDrawerState::Opening;
    focusRequested_ = true;
    Touch();
    return true;
}

bool EditorContentDrawerService::Close() {
    if (state_ == EditorContentDrawerState::Closed ||
        state_ == EditorContentDrawerState::Closing) {
        return false;
    }
    state_ = EditorContentDrawerState::Closing;
    focusRequested_ = false;
    Touch();
    return true;
}

bool EditorContentDrawerService::Toggle() {
    return IsVisible() && state_ != EditorContentDrawerState::Closing
        ? Close()
        : Open();
}

bool EditorContentDrawerService::SetPinned(bool pinned) {
    if (!IsVisible() || state_ == EditorContentDrawerState::Closing) {
        return false;
    }
    const EditorContentDrawerState next =
        pinned ? EditorContentDrawerState::Pinned : EditorContentDrawerState::Open;
    if (state_ == next) {
        return false;
    }
    state_ = next;
    openness_ = 1.0f;
    Touch();
    return true;
}

bool EditorContentDrawerService::DockInLayout() {
    if (!IsVisible()) {
        return false;
    }
    state_ = EditorContentDrawerState::Closed;
    openness_ = 0.0f;
    focusRequested_ = false;
    Touch();
    return true;
}

void EditorContentDrawerService::Tick(float deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return;
    }
    const float step = (std::clamp)(
        deltaSeconds / kDrawerTransitionSeconds,
        0.0f,
        1.0f);
    if (state_ == EditorContentDrawerState::Opening) {
        openness_ = (std::min)(1.0f, openness_ + step);
        if (openness_ >= 1.0f) {
            state_ = EditorContentDrawerState::Open;
            Touch();
        }
    } else if (state_ == EditorContentDrawerState::Closing) {
        openness_ = (std::max)(0.0f, openness_ - step);
        if (openness_ <= kVisibleThreshold) {
            openness_ = 0.0f;
            state_ = EditorContentDrawerState::Closed;
            Touch();
        }
    }
}

EditorPanelRect EditorContentDrawerService::ResolvePresentationRect(
    const EditorPanelRect& centerWorkspace) const {
    if (!IsVisible() || openness_ <= kVisibleThreshold ||
        !centerWorkspace.Valid()) {
        return {};
    }

    const float fullHeight = (std::clamp)(
        (std::max)(centerWorkspace.height * kDrawerHeightRatio,
            kDrawerMinimumHeight),
        0.0f,
        centerWorkspace.height);
    const float bottom = centerWorkspace.y + centerWorkspace.height;
    const float slideOffset = fullHeight * (1.0f - openness_);
    return EditorPanelRect{
        centerWorkspace.x,
        bottom - fullHeight + slideOffset,
        centerWorkspace.width,
        fullHeight};
}

bool EditorContentDrawerService::Contains(
    const EditorPanelRect& rect,
    float x,
    float y) const {
    return rect.Valid() &&
        x >= rect.x && x < rect.x + rect.width &&
        y >= rect.y && y < rect.y + rect.height;
}

bool EditorContentDrawerService::HandleOutsidePointerPress(
    const EditorPanelRect& rect,
    float x,
    float y,
    bool primaryPressed,
    bool popupOrModalActive) {
    if (!primaryPressed || popupOrModalActive || IsPinned() ||
        !IsVisible() || state_ == EditorContentDrawerState::Closing ||
        Contains(rect, x, y)) {
        return false;
    }
    return Close();
}

bool EditorContentDrawerService::BlocksViewportPointer(
    const EditorPanelRect& rect,
    float x,
    float y,
    bool primaryPressed) const {
    if (!IsVisible()) {
        return false;
    }
    return Contains(rect, x, y) || (!IsPinned() && primaryPressed);
}

bool EditorContentDrawerService::ConsumeFocusRequest() {
    const bool requested = focusRequested_;
    focusRequested_ = false;
    return requested;
}

void EditorContentDrawerService::Touch() {
    ++revision_;
}

} // namespace editor
