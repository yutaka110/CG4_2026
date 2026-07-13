#pragma once

#include "EditorDetailsEditController.h"
#include "EditorDetailsSectionProvider.h"
#include "EditorPropertyClipboardService.h"
#include "EditorPropertyRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorSelection.h"
#include "EditorPropertyAccessor.h"
#include "EditorTransactionStack.h"
#include "EditorValidation.h"

namespace editor {

struct EditorDetailsPanelContext {
    const EditorSelection* selection = nullptr;
    const EditorPropertyRegistry* propertyRegistry = nullptr;
    EditorPropertyAccessor* propertyAccessor = nullptr;
    EditorPropertyAccessor* previewPropertyAccessor = nullptr;
    EditorPropertyEditSession* propertyEditSession = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    EditorPropertyClipboardService* propertyClipboard = nullptr;
    const EditorAssetRegistry* assetRegistry = nullptr;
    const EditorAssetSelection* assetSelection = nullptr;
    const EditorValidationReport* validationReport = nullptr;
    const EditorDetailsSectionProviderRegistry* sectionProviders = nullptr;
    bool canMutateAuthoring = true;
};

void DrawEditorDetailsPanel(const EditorDetailsPanelContext& context);

} // namespace editor
