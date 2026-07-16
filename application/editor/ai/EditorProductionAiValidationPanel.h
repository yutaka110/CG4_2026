#pragma once

namespace editor {
class EditorNotificationCenter;
class EditorProductionAiAuthoringPipeline;
class EditorProductionAiValidationPipeline;

struct EditorProductionAiValidationPanelContext {
    EditorProductionAiValidationPipeline* validation = nullptr;
    EditorProductionAiAuthoringPipeline* authoring = nullptr;
    EditorNotificationCenter* notifications = nullptr;
};

void DrawEditorProductionAiValidationPanel(
    const EditorProductionAiValidationPanelContext& context);
} // namespace editor
