#pragma once

namespace editor {

class CourseDocumentAdapter;
class EditorAssetRegistry;
class EditorAssetSelection;
class EditorCommandInputRouter;
class EditorCommandPalette;
class EditorCommandRegistry;
class EditorDirtyStateService;
class EditorDocumentLifecycleService;
class EditorLayoutService;
class EditorModalConfirmService;
class EditorNotificationCenter;
class EditorPanelLayoutService;
class EditorPropertyAccessor;
class EditorPropertyRegistry;
class EditorPlaySessionState;
class EditorRailRuntimePause;
class EditorRuntimeInspector;
class EditorSelection;
class EditorTransformGizmoService;
class EditorTransactionStack;
class EditorViewportInteractionService;
class EditorViewportSelectionBridge;
struct EditorCommandContext;
struct EditorSaveApplyPolicyInput;
struct EditorValidationReport;

struct EditorContext {
    EditorSelection* selection = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorAssetRegistry* assets = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    const CourseDocumentAdapter* courseDocument = nullptr;
    EditorPropertyRegistry* propertyRegistry = nullptr;
    EditorPropertyAccessor* propertyAccessor = nullptr;
    const EditorValidationReport* validationReport = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorDocumentLifecycleService* documentLifecycle = nullptr;
    EditorLayoutService* layout = nullptr;
    EditorPanelLayoutService* panelLayout = nullptr;
    EditorViewportInteractionService* viewportInteraction = nullptr;
    EditorViewportSelectionBridge* viewportSelectionBridge = nullptr;
    EditorTransformGizmoService* transformGizmo = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    EditorModalConfirmService* confirmService = nullptr;
    const EditorSaveApplyPolicyInput* saveApplyPolicy = nullptr;
    EditorRuntimeInspector* runtimeInspector = nullptr;
    EditorPlaySessionState* playSession = nullptr;
    EditorRailRuntimePause* railRuntimePause = nullptr;

    EditorCommandRegistry* commands = nullptr;
    const EditorCommandContext* commandContext = nullptr;
    EditorCommandInputRouter* commandInputRouter = nullptr;
    EditorCommandPalette* commandPalette = nullptr;

    bool developerToolsVisible = false;

    bool HasCommandServices() const {
        return commands != nullptr && commandContext != nullptr;
    }
};

} // namespace editor
