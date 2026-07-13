#pragma once

#include <cstdint>

namespace editor {

struct EditorPanelRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool Valid() const { return width > 0.0f && height > 0.0f; }
};

struct EditorPanelLayoutConfig {
    bool developerToolsVisible = false;
    float workX = 0.0f;
    float workY = 0.0f;
    float workWidth = 0.0f;
    float workHeight = 0.0f;
    float topReservedHeight = 0.0f;
    float bottomReservedHeight = 0.0f;
    float inspectorWidthRatio = 0.28f;
    float inspectorMinWidth = 340.0f;
    float inspectorMaxWidth = 460.0f;
    float inspectorMaxContentRatio = 0.42f;
    float leftSidebarWidthRatio = 0.16f;
    float leftSidebarMinWidth = 220.0f;
    float leftSidebarMaxWidth = 320.0f;
    float leftSidebarMaxContentRatio = 0.24f;
    float diagnosticsHeightRatio = 0.28f;
    float diagnosticsMinHeight = 220.0f;
    float diagnosticsMaxHeight = 360.0f;
    float diagnosticsMaxContentRatio = 0.42f;
    float contentBrowserWidthRatio = 0.32f;
    float contentBrowserMinWidth = 300.0f;
    float contentBrowserMaxWidth = 520.0f;
};

class EditorPanelLayoutService {
public:
    void Configure(const EditorPanelLayoutConfig& config);

    const EditorPanelRect& ContentRect() const { return contentRect_; }
    const EditorPanelRect& LeftSidebarRect() const { return leftSidebarRect_; }
    const EditorPanelRect& InspectorRect() const { return inspectorRect_; }
    const EditorPanelRect& BottomDockRect() const { return bottomDockRect_; }
    const EditorPanelRect& ContentBrowserRect() const { return contentBrowserRect_; }
    const EditorPanelRect& DiagnosticsRect() const { return diagnosticsRect_; }
    const EditorPanelRect& ViewportRect() const { return viewportRect_; }
    uint32_t Revision() const { return revision_; }

private:
    static float Clamp(float value, float minimum, float maximum);

    EditorPanelLayoutConfig config_{};
    EditorPanelRect contentRect_{};
    EditorPanelRect leftSidebarRect_{};
    EditorPanelRect inspectorRect_{};
    EditorPanelRect bottomDockRect_{};
    EditorPanelRect contentBrowserRect_{};
    EditorPanelRect diagnosticsRect_{};
    EditorPanelRect viewportRect_{};
    uint32_t revision_ = 0;
};

} // namespace editor
