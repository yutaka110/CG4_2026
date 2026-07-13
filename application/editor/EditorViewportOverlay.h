#pragma once

#include "EditorViewportRenderTarget.h"

#include "../../externals/imgui/imgui.h"

#include <cstdint>

namespace editor {

class EditorViewportOverlayScope {
public:
    EditorViewportOverlayScope(
        const EditorViewportRenderTargetState& viewport,
        uint32_t fallbackWidth,
        uint32_t fallbackHeight,
        ImDrawList* drawList = nullptr);
    ~EditorViewportOverlayScope();

    EditorViewportOverlayScope(const EditorViewportOverlayScope&) = delete;
    EditorViewportOverlayScope& operator=(const EditorViewportOverlayScope&) = delete;

    bool Active() const { return drawList_ != nullptr && renderWidth_ > 0.0f && renderHeight_ > 0.0f; }
    ImDrawList* DrawList() const { return drawList_; }

    float RenderWidth() const { return renderWidth_; }
    float RenderHeight() const { return renderHeight_; }
    ImVec2 DisplayMin() const { return displayMin_; }
    ImVec2 DisplayMax() const { return displayMax_; }
    ImVec2 DisplayCenter() const;

    ImVec2 ToDisplay(float renderX, float renderY) const;
    ImVec2 ToDisplay(const ImVec2& renderPoint) const;
    float ScaleX(float value) const;
    float ScaleY(float value) const;
    float ScaleRadius(float value) const;
    bool RenderPointVisible(float renderX, float renderY, float margin = 0.0f) const;

private:
    ImDrawList* drawList_ = nullptr;
    ImVec2 displayMin_{};
    ImVec2 displayMax_{};
    float renderWidth_ = 0.0f;
    float renderHeight_ = 0.0f;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    bool clipPushed_ = false;
};

} // namespace editor
