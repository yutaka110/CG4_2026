#pragma once

namespace editor {
class EditorAssetSelection;
class EditorDocumentManager;
class EditorGameplayVisualScriptService;
class EditorNotificationCenter;

struct EditorGameplayVisualScriptPanelContext {
    EditorGameplayVisualScriptService* service = nullptr;
    EditorDocumentManager* documents = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutate = false;
};

void DrawEditorGameplayVisualScriptPanel(
    const EditorGameplayVisualScriptPanelContext& context);
} // namespace editor
