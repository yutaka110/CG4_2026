#include "EditorCommandContext.h"

#include "EditorAssetSelection.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorPropertyAccessor.h"
#include "EditorPropertyRegistry.h"
#include "EditorTransactionStack.h"

namespace editor {

EditorCommandContext BuildEditorCommandContext(const EditorCommandContextInput& input) {
    EditorCommandContext context;
    context.developerToolsVisible = input.developerToolsVisible;
    const EditorAuthoringMutationGuard mutationGuard =
        MakeEditorAuthoringMutationGuard(input.playSession);
    context.canMutateAuthoring = mutationGuard.CanMutate();
    context.authoringLockedByPlaySession = mutationGuard.LockedByPlaySession();

    const EditorObjectHandle* selectedObject = nullptr;
    if (input.selection != nullptr) {
        selectedObject = input.selection->Primary();
        context.hasSelectedObject = selectedObject != nullptr;
        context.selectedObjectCount = input.selection->Count();
        context.selectionRevision = input.selection->Revision();
        if (selectedObject != nullptr) {
            context.selectedObjectDomain = selectedObject->domain;
        }
    }

    const EditorAssetHandle* selectedAsset = nullptr;
    if (input.assetSelection != nullptr) {
        selectedAsset = input.assetSelection->Primary();
        context.hasSelectedAsset = selectedAsset != nullptr;
        context.assetSelectionRevision = input.assetSelection->Revision();
        if (selectedAsset != nullptr) {
            context.selectedAssetKind = selectedAsset->kind;
            context.selectedAssetReferenceable = selectedAsset->referenceable;
        }
    }

    context.detailsAvailable =
        selectedObject != nullptr &&
        input.propertyRegistry != nullptr &&
        input.propertyAccessor != nullptr;
    if (context.detailsAvailable) {
        const std::vector<const EditorPropertyDescriptor*> properties =
            input.propertyRegistry->FindByDomain(selectedObject->domain);
        context.detailsRegisteredPropertyCount = properties.size();
        context.detailsHasRegisteredProperties = !properties.empty();

        for (const EditorPropertyDescriptor* descriptor : properties) {
            if (descriptor == nullptr) {
                continue;
            }
            const bool canAccess = input.propertyAccessor->CanAccess(*selectedObject, *descriptor);
            if (!canAccess) {
                continue;
            }

            ++context.detailsAccessiblePropertyCount;
            context.detailsCanRead = true;
            if (!descriptor->readOnly) {
                ++context.detailsEditablePropertyCount;
                if (context.canMutateAuthoring) {
                    context.detailsCanEdit = true;
                }
            }

            if (selectedAsset != nullptr &&
                selectedAsset->referenceable &&
                context.canMutateAuthoring &&
                !descriptor->readOnly &&
                descriptor->kind == EditorPropertyKind::AssetRef &&
                descriptor->assetKind == selectedAsset->kind) {
                context.detailsCanUseSelectedAsset = true;
            }
        }
    }

    if (input.transactions != nullptr) {
        const EditorTransactionLegacyMirror& legacyMirror = input.transactions->LegacyMirror();
        context.transactionRevision = input.transactions->Revision();
        if (legacyMirror.active) {
            context.canUndo = legacyMirror.undoDepth > 0;
            context.canRedo = legacyMirror.redoDepth > 0;
            context.transactionRevision = legacyMirror.revision;
        } else {
            context.canUndo = input.transactions->CanUndo();
            context.canRedo = input.transactions->CanRedo();
        }
    }

    return context;
}

} // namespace editor
