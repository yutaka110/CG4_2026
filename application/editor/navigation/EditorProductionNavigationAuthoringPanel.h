#pragma once

namespace editor {
class EditorAssetSelection;
class EditorDocumentManager;
class EditorNotificationCenter;
class EditorProductionNavigationAuthoringPipeline;

struct EditorProductionNavigationAuthoringPanelContext {
    EditorProductionNavigationAuthoringPipeline* pipeline = nullptr;
    EditorDocumentManager* documents = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutate = false;
};

void DrawEditorProductionNavigationAuthoringPanel(
    const EditorProductionNavigationAuthoringPanelContext& context);
} // namespace editor
