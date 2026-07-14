#pragma once

#include "EditorPanelLayoutService.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorPanelRegistry.h"

#include <string>
#include <unordered_map>

namespace editor {

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
        EditorLayoutPersistenceService* persistence = nullptr);

private:
    void DrawBottomDock(
        const EditorPanelRegistry& registry,
        EditorLayoutPersistenceService& persistence,
        const char* windowId);

    std::unordered_map<EditorPanelHostArea, std::string> appliedActivePanels_;
};

} // namespace editor
