#pragma once

namespace editor {
class EditorAnimationStateMachineService;
class EditorAssetSelection;
class EditorDocumentManager;
class EditorNotificationCenter;

struct EditorAnimationStateMachinePanelContext {
    EditorAnimationStateMachineService* service = nullptr;
    EditorDocumentManager* documents = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutate = false;
};

void DrawEditorAnimationStateMachinePanel(
    const EditorAnimationStateMachinePanelContext& context);
} // namespace editor
