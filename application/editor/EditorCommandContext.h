#pragma once

#include "EditorAssetRegistry.h"
#include "EditorSelection.h"

#include <cstddef>
#include <cstdint>

namespace editor {

class EditorAssetSelection;
class EditorPlaySessionState;
class EditorPropertyAccessor;
class EditorPropertyRegistry;
class EditorTransactionStack;

struct EditorCommandContextInput {
    const EditorSelection* selection = nullptr;
    const EditorAssetSelection* assetSelection = nullptr;
    const EditorPropertyRegistry* propertyRegistry = nullptr;
    const EditorPropertyAccessor* propertyAccessor = nullptr;
    const EditorTransactionStack* transactions = nullptr;
    const EditorPlaySessionState* playSession = nullptr;
    bool developerToolsVisible = false;
};

struct EditorCommandContext {
    bool developerToolsVisible = false;
    bool canMutateAuthoring = true;
    bool authoringLockedByPlaySession = false;

    bool hasSelectedObject = false;
    EditorDomainId selectedObjectDomain = EditorDomainId::Unknown;
    std::size_t selectedObjectCount = 0;
    uint32_t selectionRevision = 0;

    bool hasSelectedAsset = false;
    EditorAssetKind selectedAssetKind = EditorAssetKind::Unknown;
    bool selectedAssetReferenceable = false;
    uint32_t assetSelectionRevision = 0;

    bool detailsAvailable = false;
    bool detailsHasRegisteredProperties = false;
    bool detailsCanRead = false;
    bool detailsCanEdit = false;
    bool detailsCanUseSelectedAsset = false;
    std::size_t detailsRegisteredPropertyCount = 0;
    std::size_t detailsAccessiblePropertyCount = 0;
    std::size_t detailsEditablePropertyCount = 0;

    bool canUndo = false;
    bool canRedo = false;
    uint32_t transactionRevision = 0;
};

EditorCommandContext BuildEditorCommandContext(const EditorCommandContextInput& input);

} // namespace editor
