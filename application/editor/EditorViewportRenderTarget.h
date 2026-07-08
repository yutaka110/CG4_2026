#pragma once

#include <cstdint>

#include "EditorPanelLayoutService.h"

namespace editor {

struct EditorViewportRenderTargetInput {
    bool enabled = false;
    EditorPanelRect viewportRect{};
    uint32_t fallbackWidth = 0;
    uint32_t fallbackHeight = 0;
    uint32_t minWidth = 64;
    uint32_t minHeight = 64;
    uint32_t maxWidth = 4096;
    uint32_t maxHeight = 4096;
};

struct EditorViewportRenderTargetState {
    bool enabled = false;
    EditorPanelRect displayRect{};
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    float aspectRatio = 16.0f / 9.0f;
    uint32_t revision = 0;

    bool Valid() const {
        return renderWidth > 0 && renderHeight > 0;
    }
};

class EditorViewportRenderTarget {
public:
    void Update(const EditorViewportRenderTargetInput& input);

    const EditorViewportRenderTargetState& State() const { return state_; }
    uint32_t Revision() const { return state_.revision; }

private:
    static uint32_t ClampDimension(uint32_t value, uint32_t minimum, uint32_t maximum);
    static uint32_t RoundDimension(float value);
    void TouchIfChanged(const EditorViewportRenderTargetState& next);

    EditorViewportRenderTargetState state_{};
};

} // namespace editor
