#pragma once

#include "../EditorValidationService.h"

namespace editor {

class EditorAssetRegistry;
class EditorDocumentManager;
class EditorMaterialGraphDocumentProvider;

class EditorMaterialGraphDiagnosticsAdapter final : public EditorValidationAdapter {
public:
    EditorMaterialGraphDiagnosticsAdapter(
        const EditorMaterialGraphDocumentProvider* provider,
        const EditorDocumentManager* documents,
        const EditorAssetRegistry* assets)
        : provider_(provider), documents_(documents), assets_(assets) {}

    void Validate(EditorValidationReport& report) const override;

private:
    const EditorMaterialGraphDocumentProvider* provider_ = nullptr;
    const EditorDocumentManager* documents_ = nullptr;
    const EditorAssetRegistry* assets_ = nullptr;
};

} // namespace editor
