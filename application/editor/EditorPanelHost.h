#pragma once

#include "EditorPanelRegistry.h"

namespace editor {

class EditorPanelHost {
public:
    void DrawTabs(const EditorPanelRegistry& registry, EditorPanelHostArea area);
};

} // namespace editor
