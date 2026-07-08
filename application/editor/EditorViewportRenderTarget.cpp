#include "EditorViewportRenderTarget.h"

#include <cmath>

namespace editor {

void EditorViewportRenderTarget::Update(const EditorViewportRenderTargetInput& input) {
    EditorViewportRenderTargetState next = state_;
    const uint32_t fallbackWidth =
        ClampDimension(input.fallbackWidth, input.minWidth, input.maxWidth);
    const uint32_t fallbackHeight =
        ClampDimension(input.fallbackHeight, input.minHeight, input.maxHeight);

    if (!input.enabled || !input.viewportRect.Valid()) {
        next.enabled = false;
        next.displayRect = EditorPanelRect{};
        next.renderWidth = fallbackWidth;
        next.renderHeight = fallbackHeight;
    } else {
        const uint32_t requestedWidth = RoundDimension(input.viewportRect.width);
        const uint32_t requestedHeight = RoundDimension(input.viewportRect.height);
        next.enabled = true;
        next.displayRect = input.viewportRect;
        next.renderWidth = ClampDimension(requestedWidth, input.minWidth, input.maxWidth);
        next.renderHeight = ClampDimension(requestedHeight, input.minHeight, input.maxHeight);
    }

    next.aspectRatio = next.renderHeight > 0
        ? static_cast<float>(next.renderWidth) / static_cast<float>(next.renderHeight)
        : 16.0f / 9.0f;
    TouchIfChanged(next);
}

uint32_t EditorViewportRenderTarget::ClampDimension(
    uint32_t value,
    uint32_t minimum,
    uint32_t maximum) {
    if (maximum < minimum) {
        maximum = minimum;
    }
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

uint32_t EditorViewportRenderTarget::RoundDimension(float value) {
    if (value <= 0.0f) {
        return 0;
    }
    return static_cast<uint32_t>(std::lround(value));
}

void EditorViewportRenderTarget::TouchIfChanged(
    const EditorViewportRenderTargetState& next) {
    const bool changed =
        state_.enabled != next.enabled ||
        state_.displayRect.x != next.displayRect.x ||
        state_.displayRect.y != next.displayRect.y ||
        state_.displayRect.width != next.displayRect.width ||
        state_.displayRect.height != next.displayRect.height ||
        state_.renderWidth != next.renderWidth ||
        state_.renderHeight != next.renderHeight;
    const uint32_t revision = changed ? state_.revision + 1 : state_.revision;
    state_ = next;
    state_.revision = revision;
}

} // namespace editor
