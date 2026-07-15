#pragma once

#include "../EditorValidationService.h"

namespace editor {
class EditorAnimationStateMachineDocumentProvider;
class EditorAssetRegistry;
class EditorDocumentManager;

class EditorAnimationStateMachineDiagnosticsAdapter final : public EditorValidationAdapter {
public:
    EditorAnimationStateMachineDiagnosticsAdapter(
        const EditorAnimationStateMachineDocumentProvider* provider,
        const EditorDocumentManager* documents, const EditorAssetRegistry* assets)
        : provider_(provider), documents_(documents), assets_(assets) {}
    void Validate(EditorValidationReport& report) const override;
private:
    const EditorAnimationStateMachineDocumentProvider* provider_ = nullptr;
    const EditorDocumentManager* documents_ = nullptr;
    const EditorAssetRegistry* assets_ = nullptr;
};
} // namespace editor
