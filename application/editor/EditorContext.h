#pragma once

#include <functional>

namespace editor {

class CourseDocumentAdapter;
class EditorAssetRegistry;
class EditorAssetSelection;
class EditorAssetThumbnailService;
class IEditorAssetWorkspaceStatusProvider;
class EditorCommandInputRouter;
class EditorCommandPalette;
class EditorCommandRegistry;
class EditorDirtyStateService;
class EditorDocumentLifecycleService;
class EditorDocumentManager;
class EditorDocumentSaveService;
class EditorLayoutService;
class EditorLayoutPersistenceService;
class EditorModalConfirmService;
class EditorNotificationCenter;
class EditorPanelLayoutService;
class EditorPrefabService;
class EditorMaterialGraphService;
class EditorVfxGraphService;
class EditorAnimationStateMachineService;
class EditorGameplayVisualScriptService;
class EditorPropertyAccessor;
class EditorPropertyEditService;
class EditorPropertyRegistry;
class EditorPlaySessionState;
class EditorRailRuntimePause;
class EditorRuntimeInspector;
class EditorSelection;
class EditorSequencerService;
class EditorToolRegistry;
class EditorTransformGizmoService;
class EditorTransactionStack;
class EditorViewportInteractionService;
class EditorViewportCoordinateService;
class EditorViewportOverlayService;
class EditorViewportSelectionBridge;
class EditorWorldModel;
class EditorWorldMutationService;
class SceneWorldObjectProvider;
class EditorToolManager;
class EditorExecutionContext;
class IEditorWorldMutationExecutionService;
struct EditorWorldMutationResult;
struct EditorCommandContext;
struct EditorSaveApplyPolicyInput;
struct EditorValidationReport;

struct EditorContext {
    EditorSelection* selection = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorAssetRegistry* assets = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorAssetThumbnailService* assetThumbnails = nullptr;
    const CourseDocumentAdapter* courseDocument = nullptr;
    EditorPropertyRegistry* propertyRegistry = nullptr;
    EditorPropertyAccessor* propertyAccessor = nullptr;
    EditorPropertyEditService* propertyEditService = nullptr;
    const EditorValidationReport* validationReport = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorDocumentLifecycleService* documentLifecycle = nullptr;
    EditorDocumentManager* documentManager = nullptr;
    EditorDocumentSaveService* documentSaveService = nullptr;
    EditorWorldModel* worldModel = nullptr;
    IEditorWorldMutationExecutionService* worldMutationExecution = nullptr;
    EditorLayoutService* layout = nullptr;
    EditorPanelLayoutService* panelLayout = nullptr;
    EditorViewportInteractionService* viewportInteraction = nullptr;
    EditorViewportCoordinateService* viewportCoordinates = nullptr;
    EditorViewportSelectionBridge* viewportSelectionBridge = nullptr;
    EditorTransformGizmoService* transformGizmo = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    EditorModalConfirmService* confirmService = nullptr;
    const EditorSaveApplyPolicyInput* saveApplyPolicy = nullptr;
    EditorRuntimeInspector* runtimeInspector = nullptr;
    EditorPlaySessionState* playSession = nullptr;
    EditorRailRuntimePause* railRuntimePause = nullptr;
    EditorToolRegistry* tools = nullptr;

    EditorCommandRegistry* commands = nullptr;
    const EditorCommandContext* commandContext = nullptr;
    EditorCommandInputRouter* commandInputRouter = nullptr;
    EditorCommandPalette* commandPalette = nullptr;

    bool developerToolsVisible = false;
    EditorViewportOverlayService* viewportOverlay = nullptr;
    EditorLayoutPersistenceService* layoutPersistence = nullptr;
    EditorWorldMutationService* worldMutations = nullptr;
    SceneWorldObjectProvider* sceneWorldProvider = nullptr;
    std::function<void(const EditorWorldMutationResult&)> onWorldMutated;
    const IEditorAssetWorkspaceStatusProvider* assetWorkspaceStatus = nullptr;
    EditorSequencerService* sequencer = nullptr;
    EditorPrefabService* prefabs = nullptr;
    EditorMaterialGraphService* materialGraphs = nullptr;
    EditorVfxGraphService* vfxGraphs = nullptr;
    EditorAnimationStateMachineService* animationStateMachines = nullptr;
    EditorGameplayVisualScriptService* gameplayVisualScripts = nullptr;
    EditorToolManager* interactiveTools = nullptr;
    EditorExecutionContext* interactiveExecution = nullptr;

    bool HasCommandServices() const {
        return commands != nullptr && commandContext != nullptr;
    }
};

} // namespace editor
