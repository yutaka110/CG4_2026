#pragma once

namespace editor {
class EditorAssetSelection;
class EditorDocumentManager;
class EditorNotificationCenter;
class EditorProductionAiAuthoringPipeline;

struct EditorProductionAiAuthoringPanelContext {
    EditorProductionAiAuthoringPipeline* pipeline = nullptr;
    EditorDocumentManager* documents = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    bool canMutate = false;
};

void DrawEditorProductionAiAuthoringPanel(
    const EditorProductionAiAuthoringPanelContext& context);
} // namespace editor
