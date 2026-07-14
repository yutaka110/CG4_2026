#pragma once

namespace editor {

class EditorAssetSelection;
class EditorDocumentManager;
class EditorMaterialGraphService;
class EditorNotificationCenter;

struct EditorMaterialGraphPanelContext {
    EditorMaterialGraphService* service = nullptr;
    EditorDocumentManager* documents = nullptr;
    const EditorAssetSelection* assetSelection = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutate = true;
};

void DrawEditorMaterialGraphPanel(const EditorMaterialGraphPanelContext& context);

} // namespace editor
