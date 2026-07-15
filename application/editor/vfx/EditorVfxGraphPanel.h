#pragma once

namespace editor {

class EditorAssetSelection;
class EditorDocumentManager;
class EditorNotificationCenter;
class EditorVfxGraphService;

struct EditorVfxGraphPanelContext {
    EditorVfxGraphService* service = nullptr;
    EditorDocumentManager* documents = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutate = false;
};

void DrawEditorVfxGraphPanel(const EditorVfxGraphPanelContext& context);

} // namespace editor
