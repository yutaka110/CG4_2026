#pragma once
#include "../EditorValidationService.h"
namespace editor {
class EditorDocumentManager;
class EditorGameplayVisualScriptDocumentProvider;
class EditorGameplayVisualScriptDiagnosticsAdapter final : public EditorValidationAdapter {
public:
    EditorGameplayVisualScriptDiagnosticsAdapter(
        const EditorGameplayVisualScriptDocumentProvider* provider,
        const EditorDocumentManager* documents) : provider_(provider), documents_(documents) {}
    void Validate(EditorValidationReport& report) const override;
private:
    const EditorGameplayVisualScriptDocumentProvider* provider_ = nullptr;
    const EditorDocumentManager* documents_ = nullptr;
};
} // namespace editor
