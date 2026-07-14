#pragma once

#include "EditorAssetRegistry.h"
#include "EditorSelection.h"
#include "EditorValidation.h"

namespace editor {

class EditorAssetSelection;
class EditorWorldModel;

struct EditorDiagnosticsPanelContext {
    const EditorValidationReport* validationReport = nullptr;
    EditorSelection* selection = nullptr;
    const EditorAssetRegistry* assetRegistry = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    const EditorWorldModel* worldModel = nullptr;
};

void DrawEditorDiagnosticsPanel(const EditorDiagnosticsPanelContext& context);

} // namespace editor
