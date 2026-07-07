#pragma once

#include "EditorSelection.h"
#include "EditorValidation.h"

namespace editor {

struct EditorDiagnosticsPanelContext {
    const EditorValidationReport* validationReport = nullptr;
    const EditorSelection* selection = nullptr;
};

void DrawEditorDiagnosticsPanel(const EditorDiagnosticsPanelContext& context);

} // namespace editor
