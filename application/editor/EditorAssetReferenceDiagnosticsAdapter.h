#pragma once

#include "EditorAssetRegistry.h"
#include "EditorValidationService.h"

namespace editor {

class EditorAssetReferenceDiagnosticsAdapter final : public EditorValidationAdapter {
public:
    explicit EditorAssetReferenceDiagnosticsAdapter(const EditorAssetRegistry* assetRegistry);

    void Validate(EditorValidationReport& report) const override;

private:
    const EditorAssetRegistry* assetRegistry_ = nullptr;
};

std::string BuildEditorAssetDiagnosticStableId(EditorAssetKind kind, std::string_view id);

} // namespace editor
