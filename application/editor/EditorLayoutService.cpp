#include "EditorLayoutService.h"

namespace editor {

void EditorLayoutService::Configure(const EditorLayoutConfig& config) {
    config_ = config;
    ++revision_;
}

bool EditorLayoutService::ToolbarVisible() const {
    return config_.developerToolsVisible && config_.toolbarVisible;
}

bool EditorLayoutService::DocumentTabsVisible() const {
    return config_.developerToolsVisible && config_.documentTabsVisible;
}

bool EditorLayoutService::StatusBarVisible() const {
    return config_.developerToolsVisible && config_.statusBarVisible;
}

float EditorLayoutService::ToolbarHeight() const {
    return ToolbarVisible() ? config_.toolbarHeight : 0.0f;
}

float EditorLayoutService::DocumentTabsHeight() const {
    return DocumentTabsVisible() ? config_.documentTabsHeight : 0.0f;
}

float EditorLayoutService::StatusBarHeight() const {
    return StatusBarVisible() ? config_.statusBarHeight : 0.0f;
}

float EditorLayoutService::DocumentTabsTopOffset() const {
    return ToolbarHeight();
}

float EditorLayoutService::TopReservedHeight() const {
    return ToolbarHeight() + DocumentTabsHeight();
}

float EditorLayoutService::BottomReservedHeight() const {
    return StatusBarHeight();
}

} // namespace editor
