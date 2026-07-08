#pragma once

#include "EditorSelection.h"
#include "EditorValidation.h"

namespace editor {

class EditorAssetSelection;

struct EditorDiagnosticsPanelContext {
    const EditorValidationReport* validationReport = nullptr;
    const EditorSelection* selection = nullptr;
    const EditorAssetSelection* assetSelection = nullptr;
};

void DrawEditorDiagnosticsPanel(const EditorDiagnosticsPanelContext& context);

} // namespace editor
