#pragma once

#include "../EditorValidationService.h"

namespace editor {

class EditorAssetRegistry;
class EditorDocumentManager;
class EditorVfxGraphDocumentProvider;

class EditorVfxGraphDiagnosticsAdapter final : public EditorValidationAdapter {
public:
    EditorVfxGraphDiagnosticsAdapter(const EditorVfxGraphDocumentProvider* provider,
        const EditorDocumentManager* documents, const EditorAssetRegistry* assets)
        : provider_(provider), documents_(documents), assets_(assets) {}

    void Validate(EditorValidationReport& report) const override;

private:
    const EditorVfxGraphDocumentProvider* provider_ = nullptr;
    const EditorDocumentManager* documents_ = nullptr;
    const EditorAssetRegistry* assets_ = nullptr;
};

} // namespace editor
