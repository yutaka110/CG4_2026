#pragma once

#include "EditorPanelLayoutService.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorPanelRegistry.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor {

struct EditorPanelHostAction {
    std::string label;
    std::string tooltip;
    std::function<void()> execute;
};

class EditorPanelHost {
public:
    void DrawTabs(
        const EditorPanelRegistry& registry,
        EditorPanelHostArea area,
        EditorLayoutPersistenceService* persistence = nullptr);
    void DrawArea(
        const EditorPanelRegistry& registry,
        EditorPanelHostArea area,
        const EditorPanelRect& rect,
        const char* windowId,
        EditorLayoutPersistenceService* persistence = nullptr,
        const std::vector<EditorPanelHostAction>* actions = nullptr);

private:
    void DrawBottomDock(
        const EditorPanelRegistry& registry,
        EditorLayoutPersistenceService& persistence,
        const char* windowId);

    std::unordered_map<EditorPanelHostArea, std::string> appliedActivePanels_;
};

} // namespace editor
