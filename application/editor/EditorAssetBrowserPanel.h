#pragma once

#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"

namespace editor {

class EditorNotificationCenter;

struct EditorAssetBrowserPanelContext {
    EditorAssetRegistry* registry = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorNotificationCenter* notifications = nullptr;
};

void DrawEditorAssetBrowserPanel(const EditorAssetBrowserPanelContext& context);

} // namespace editor
