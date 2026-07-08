#pragma once

#include <cstdint>

namespace editor {

struct EditorLayoutConfig {
    bool developerToolsVisible = false;
    bool toolbarVisible = true;
    bool documentTabsVisible = true;
    bool statusBarVisible = true;
    float toolbarHeight = 36.0f;
    float documentTabsHeight = 30.0f;
    float statusBarHeight = 26.0f;
};

class EditorLayoutService {
public:
    void Configure(const EditorLayoutConfig& config);

    bool DeveloperToolsVisible() const { return config_.developerToolsVisible; }
    bool ToolbarVisible() const;
    bool DocumentTabsVisible() const;
    bool StatusBarVisible() const;

    float ToolbarHeight() const;
    float DocumentTabsHeight() const;
    float StatusBarHeight() const;
    float ToolbarTopOffset() const { return 0.0f; }
    float DocumentTabsTopOffset() const;
    float TopReservedHeight() const;
    float BottomReservedHeight() const;

    uint32_t Revision() const { return revision_; }

private:
    EditorLayoutConfig config_{};
    uint32_t revision_ = 0;
};

} // namespace editor
