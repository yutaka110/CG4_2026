#pragma once

#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"

namespace editor {

struct EditorAssetBrowserPanelContext {
    const EditorAssetRegistry* registry = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
};

void DrawEditorAssetBrowserPanel(const EditorAssetBrowserPanelContext& context);

} // namespace editor
