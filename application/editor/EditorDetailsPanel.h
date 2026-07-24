#pragma once

#include "EditorDetailsEditController.h"
#include "EditorDetailsSectionProvider.h"
#include "EditorDetailsViewState.h"
#include "EditorPropertyClipboardService.h"
#include "EditorPropertyRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorSelection.h"
#include "EditorPropertyAccessor.h"
#include "EditorTransactionStack.h"
#include "EditorValidation.h"
#include "world/EditorWorldModel.h"

namespace editor {

class EditorPrefabService;
class EditorSceneComponentRegistry;
class EditorGimmickDefinitionRegistry;
class EditorGimmickRuntimeWorld;
class EditorGimmickPresentationPhysicsAdapter;
class EditorGimmickRuntimeEventRouter;
class EditorGimmickRuntimeInteractionSystem;
class EditorGimmickRuntimeTriggerSystem;

struct EditorDetailsPanelContext {
    const EditorSelection* selection = nullptr;
    const EditorSceneComponentRegistry* sceneComponentRegistry = nullptr;
    const EditorGimmickDefinitionRegistry* gimmickDefinitionRegistry = nullptr;
    const EditorGimmickRuntimeWorld* gimmickRuntimeWorld = nullptr;
    const EditorGimmickPresentationPhysicsAdapter*
        gimmickRuntimeAdapter = nullptr;
    const EditorGimmickRuntimeEventRouter*
        gimmickRuntimeEventRouter = nullptr;
    const EditorGimmickRuntimeInteractionSystem*
        gimmickRuntimeInteraction = nullptr;
    const EditorGimmickRuntimeTriggerSystem*
        gimmickRuntimeTriggers = nullptr;
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
    EditorDetailsViewState* viewState = nullptr;
    IEditorPrefabOverrideProvider* prefabOverrides = nullptr;
    EditorPrefabService* prefabService = nullptr;
    const EditorWorldModel* worldModel = nullptr;
    bool canMutateAuthoring = true;
    EditorWorldMutationService* worldMutations = nullptr;
    SceneWorldObjectProvider* sceneWorldProvider = nullptr;
    std::function<void(const EditorWorldMutationResult&)> onWorldMutated;
};

void DrawEditorDetailsPanel(const EditorDetailsPanelContext& context);

} // namespace editor
