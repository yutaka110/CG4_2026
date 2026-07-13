#include "EditorCommandContext.h"

#include "EditorAssetSelection.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorPropertyAccessor.h"
#include "EditorPropertyRegistry.h"
#include "EditorTransactionStack.h"

#include <algorithm>
#include <vector>

namespace editor {
namespace {

bool IsSingleDomainSelection(const std::vector<EditorObjectHandle>& selectedObjects) {
    if (selectedObjects.empty()) {
        return true;
    }
    const EditorDomainId domain = selectedObjects.front().domain;
    return std::all_of(
        selectedObjects.begin(),
        selectedObjects.end(),
        [domain](const EditorObjectHandle& selected) {
            return selected.domain == domain;
        });
}

bool CanAccessEverySelectedObject(
    const EditorPropertyAccessor& accessor,
    const std::vector<EditorObjectHandle>& selectedObjects,
    const EditorPropertyDescriptor& descriptor) {
    return std::all_of(
        selectedObjects.begin(),
        selectedObjects.end(),
        [&](const EditorObjectHandle& selected) {
            return accessor.CanAccess(selected, descriptor);
        });
}

} // namespace

EditorCommandContext BuildEditorCommandContext(const EditorCommandContextInput& input) {
    EditorCommandContext context;
    context.developerToolsVisible = input.developerToolsVisible;
    const EditorAuthoringMutationGuard mutationGuard =
        MakeEditorAuthoringMutationGuard(input.playSession);
    context.canMutateAuthoring = mutationGuard.CanMutate();
    context.authoringLockedByPlaySession = mutationGuard.LockedByPlaySession();

    const EditorObjectHandle* selectedObject = nullptr;
    std::vector<EditorObjectHandle> selectedObjects;
    if (input.selection != nullptr) {
        selectedObject = input.selection->Primary();
        selectedObjects = input.selection->Handles();
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
        std::vector<const EditorPropertyDescriptor*> properties;
        if (IsSingleDomainSelection(selectedObjects)) {
            properties = input.propertyRegistry->FindByDomain(selectedObject->domain);
            if (selectedObjects.size() > 1) {
                properties.erase(
                    std::remove_if(
                        properties.begin(),
                        properties.end(),
                        [](const EditorPropertyDescriptor* descriptor) {
                            return descriptor == nullptr || !descriptor->supportsMultiEdit;
                        }),
                    properties.end());
            }
        }
        context.detailsRegisteredPropertyCount = properties.size();
        context.detailsHasRegisteredProperties = !properties.empty();

        for (const EditorPropertyDescriptor* descriptor : properties) {
            if (descriptor == nullptr) {
                continue;
            }
            const bool canAccess =
                CanAccessEverySelectedObject(*input.propertyAccessor, selectedObjects, *descriptor);
            if (!canAccess) {
                continue;
            }

            ++context.detailsAccessiblePropertyCount;
            context.detailsCanRead = true;
            const bool supportsSelectionEdit =
                selectedObjects.size() <= 1 || descriptor->supportsMultiEdit;
            if (!descriptor->readOnly && supportsSelectionEdit) {
                ++context.detailsEditablePropertyCount;
                if (context.canMutateAuthoring) {
                    context.detailsCanEdit = true;
                }
            }

            if (selectedAsset != nullptr &&
                selectedAsset->referenceable &&
                context.canMutateAuthoring &&
                !descriptor->readOnly &&
                supportsSelectionEdit &&
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
            context.canUndo = legacyMirror.undoDepth > 0 || input.transactions->CanUndo();
            context.canRedo = legacyMirror.redoDepth > 0 || input.transactions->CanRedo();
            context.transactionRevision = legacyMirror.revision;
        } else {
            context.canUndo = input.transactions->CanUndo();
            context.canRedo = input.transactions->CanRedo();
        }
    }

    return context;
}

} // namespace editor
