#include "EditorViewportOverlay.h"

#include <algorithm>

namespace editor {

EditorViewportOverlayScope::EditorViewportOverlayScope(
    const EditorViewportRenderTargetState& viewport,
    uint32_t fallbackWidth,
    uint32_t fallbackHeight,
    ImDrawList* drawList) {
    drawList_ = drawList != nullptr ? drawList : ImGui::GetForegroundDrawList();
    if (drawList_ == nullptr) {
        return;
    }

    if (viewport.enabled && viewport.Valid() && viewport.displayRect.Valid()) {
        displayMin_ = ImVec2(viewport.displayRect.x, viewport.displayRect.y);
        displayMax_ = ImVec2(
            viewport.displayRect.x + viewport.displayRect.width,
            viewport.displayRect.y + viewport.displayRect.height);
        renderWidth_ = static_cast<float>((std::max)(1u, viewport.renderWidth));
        renderHeight_ = static_cast<float>((std::max)(1u, viewport.renderHeight));
    } else {
        displayMin_ = ImVec2(0.0f, 0.0f);
        displayMax_ = ImVec2(
            static_cast<float>((std::max)(1u, fallbackWidth)),
            static_cast<float>((std::max)(1u, fallbackHeight)));
        renderWidth_ = displayMax_.x;
        renderHeight_ = displayMax_.y;
    }

    const float displayWidth = (std::max)(1.0f, displayMax_.x - displayMin_.x);
    const float displayHeight = (std::max)(1.0f, displayMax_.y - displayMin_.y);
    scaleX_ = displayWidth / (std::max)(1.0f, renderWidth_);
    scaleY_ = displayHeight / (std::max)(1.0f, renderHeight_);

    drawList_->PushClipRect(displayMin_, displayMax_, true);
    clipPushed_ = true;
}

EditorViewportOverlayScope::~EditorViewportOverlayScope() {
    if (drawList_ != nullptr && clipPushed_) {
        drawList_->PopClipRect();
    }
}

ImVec2 EditorViewportOverlayScope::DisplayCenter() const {
    return ImVec2(
        (displayMin_.x + displayMax_.x) * 0.5f,
        (displayMin_.y + displayMax_.y) * 0.5f);
}

ImVec2 EditorViewportOverlayScope::ToDisplay(float renderX, float renderY) const {
    return ImVec2(
        displayMin_.x + renderX * scaleX_,
        displayMin_.y + renderY * scaleY_);
}

ImVec2 EditorViewportOverlayScope::ToDisplay(const ImVec2& renderPoint) const {
    return ToDisplay(renderPoint.x, renderPoint.y);
}

float EditorViewportOverlayScope::ScaleX(float value) const {
    return value * scaleX_;
}

float EditorViewportOverlayScope::ScaleY(float value) const {
    return value * scaleY_;
}

float EditorViewportOverlayScope::ScaleRadius(float value) const {
    return value * (scaleX_ + scaleY_) * 0.5f;
}

bool EditorViewportOverlayScope::RenderPointVisible(float renderX, float renderY, float margin) const {
    return renderX >= -margin &&
        renderY >= -margin &&
        renderX <= renderWidth_ + margin &&
        renderY <= renderHeight_ + margin;
}

} // namespace editor
