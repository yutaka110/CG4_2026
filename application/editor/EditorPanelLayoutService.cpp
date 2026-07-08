#include "EditorPanelLayoutService.h"

namespace editor {

void EditorPanelLayoutService::Configure(const EditorPanelLayoutConfig& config) {
    config_ = config;

    const float reservedTop =
        Clamp(config.topReservedHeight, 0.0f, config.workHeight * 0.25f);
    const float reservedBottom =
        Clamp(config.bottomReservedHeight, 0.0f, config.workHeight * 0.25f);
    const float contentHeight =
        Clamp(config.workHeight - reservedTop - reservedBottom, 0.0f, config.workHeight);

    contentRect_ = EditorPanelRect{
        config.workX,
        config.workY + reservedTop,
        config.workWidth,
        contentHeight};

    if (!config.developerToolsVisible || !contentRect_.Valid()) {
        inspectorRect_ = EditorPanelRect{};
        diagnosticsRect_ = EditorPanelRect{};
        viewportRect_ = contentRect_;
        ++revision_;
        return;
    }

    float inspectorWidth =
        Clamp(
            contentRect_.width * config.inspectorWidthRatio,
            config.inspectorMinWidth,
            config.inspectorMaxWidth);
    inspectorWidth =
        Clamp(
            inspectorWidth,
            280.0f,
            contentRect_.width * config.inspectorMaxContentRatio);

    float diagnosticsHeight =
        Clamp(
            contentRect_.height * config.diagnosticsHeightRatio,
            config.diagnosticsMinHeight,
            config.diagnosticsMaxHeight);
    diagnosticsHeight =
        Clamp(
            diagnosticsHeight,
            160.0f,
            contentRect_.height * config.diagnosticsMaxContentRatio);

    inspectorRect_ = EditorPanelRect{
        contentRect_.x + contentRect_.width - inspectorWidth,
        contentRect_.y,
        inspectorWidth,
        contentRect_.height};
    diagnosticsRect_ = EditorPanelRect{
        contentRect_.x,
        contentRect_.y + contentRect_.height - diagnosticsHeight,
        contentRect_.width - inspectorWidth,
        diagnosticsHeight};
    viewportRect_ = EditorPanelRect{
        contentRect_.x,
        contentRect_.y,
        contentRect_.width - inspectorWidth,
        contentRect_.height - diagnosticsHeight};

    ++revision_;
}

float EditorPanelLayoutService::Clamp(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

} // namespace editor
