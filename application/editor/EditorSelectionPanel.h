#pragma once

#include "EditorSelection.h"

#include <vector>

namespace editor {

struct EditorSelectionPanelTarget {
    EditorObjectHandle handle;
};

void DrawEditorSelectionPanel(
    EditorSelection& selection,
    const std::vector<EditorSelectionPanelTarget>& targets = {});

} // namespace editor
