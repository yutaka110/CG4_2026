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
        leftSidebarRect_ = EditorPanelRect{};
        inspectorRect_ = EditorPanelRect{};
        bottomDockRect_ = EditorPanelRect{};
        contentBrowserRect_ = EditorPanelRect{};
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

    float leftSidebarWidth =
        Clamp(
            contentRect_.width * config.leftSidebarWidthRatio,
            config.leftSidebarMinWidth,
            config.leftSidebarMaxWidth);
    leftSidebarWidth =
        Clamp(
            leftSidebarWidth,
            0.0f,
            contentRect_.width * config.leftSidebarMaxContentRatio);

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

    const float centerWidth =
        Clamp(contentRect_.width - inspectorWidth - leftSidebarWidth, 0.0f, contentRect_.width);
    float contentBrowserWidth =
        Clamp(
            centerWidth * config.contentBrowserWidthRatio,
            config.contentBrowserMinWidth,
            config.contentBrowserMaxWidth);
    contentBrowserWidth = Clamp(contentBrowserWidth, 0.0f, centerWidth * 0.5f);

    leftSidebarRect_ = EditorPanelRect{
        contentRect_.x,
        contentRect_.y,
        leftSidebarWidth,
        contentRect_.height};
    inspectorRect_ = EditorPanelRect{
        contentRect_.x + contentRect_.width - inspectorWidth,
        contentRect_.y,
        inspectorWidth,
        contentRect_.height};
    bottomDockRect_ = EditorPanelRect{
        contentRect_.x + leftSidebarWidth,
        contentRect_.y + contentRect_.height - diagnosticsHeight,
        centerWidth,
        diagnosticsHeight};
    contentBrowserRect_ = EditorPanelRect{
        bottomDockRect_.x,
        bottomDockRect_.y,
        contentBrowserWidth,
        bottomDockRect_.height};
    diagnosticsRect_ = EditorPanelRect{
        bottomDockRect_.x + contentBrowserWidth,
        bottomDockRect_.y,
        bottomDockRect_.width - contentBrowserWidth,
        bottomDockRect_.height};
    viewportRect_ = EditorPanelRect{
        contentRect_.x + leftSidebarWidth,
        contentRect_.y,
        centerWidth,
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
