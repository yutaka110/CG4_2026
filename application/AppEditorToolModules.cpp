#include "AppEditorToolModules.h"

#include "AppImGuiLayer.h"
#include "AppRuntimeState.h"
#include "editor/CourseEditorCommandProvider.h"
#include "editor/EditorAssetCommandProvider.h"
#include "editor/EditorAssetMutationExecutor.h"
#include "editor/EditorBuiltinCommandProvider.h"
#include "editor/EditorBuiltinDetailsSectionProviders.h"
#include "editor/EditorContext.h"
#include "editor/EditorMenuBar.h"
#include "editor/EditorPlaySessionLifecycleService.h"
#include "editor/EditorPlaySessionRuntimeControlService.h"
#include "editor/EditorPropertyRegistry.h"
#include "editor/EditorRuntimeAuthoringApplyService.h"
#include "editor/EditorToolbar.h"

#include <string>
#include <utility>

namespace editor {
namespace {

bool HasCommandPipelineInput(const AppEditorCommandToolModuleInput& input) {
    return input.editorContext != nullptr &&
        input.editorContext->commands != nullptr &&
        input.frameContext != nullptr &&
        input.runtimeState != nullptr;
}

EditorCommandResult UndoEditorTransaction(
    const AppImGuiFrameContext& context,
    AppRuntimeState& runtimeState,
    const EditorRuntimeAuthoringApplyService& runtimeAuthoringApply,
    EditorTransactionStack* transactions,
    EditorAssetRegistry* assets,
    EditorDirtyStateService* dirtyState,
    EditorNotificationCenter* notifications) {
    const EditorTransactionRecord* next =
        transactions != nullptr ? transactions->NextUndoTransaction() : nullptr;
    if (next != nullptr &&
        next->payload.kind == EditorTransactionPayloadKind::AssetMutation) {
        if (assets == nullptr) {
            return EditorCommandResult{false, "Asset registry is unavailable."};
        }
        EditorAssetMutationExecutor executor(*assets);
        EditorAssetMutationResult applyResult{};
        const bool applied =
            transactions->Undo(
                [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                    applyResult = executor.ApplyTransaction(record, mode);
                    return applyResult.succeeded;
                });
        if (notifications != nullptr && !applyResult.message.empty()) {
            notifications->Push(
                applied ? EditorNotificationSeverity::Info : EditorNotificationSeverity::Error,
                "Asset",
                applyResult.message);
        }
        return EditorCommandResult{
            applied,
            applyResult.message.empty()
                ? std::string("Asset undo failed.")
                : applyResult.message};
    }
    if (next != nullptr &&
        next->payload.kind == EditorTransactionPayloadKind::RuntimeAuthoringApply) {
        EditorRuntimeAuthoringApplyResult applyResult{};
        const bool applied =
            transactions->Undo(
                [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                    applyResult =
                        runtimeAuthoringApply.ApplyTransaction(
                            EditorRuntimeAuthoringApplyRequest{
                                nullptr,
                                nullptr,
                                context.course,
                                &runtimeState,
                                transactions,
                                dirtyState,
                                notifications,
                                0,
                                "editor.command.runtimeApplyUndo"},
                            record,
                            mode);
                    return applyResult.succeeded;
                });
        return EditorCommandResult{
            applied,
            applyResult.message.empty()
                ? std::string("Runtime authoring apply undo failed.")
                : applyResult.message};
    }
    if (runtimeState.terrain.courseObjectUndoDepth == 0) {
        return EditorCommandResult{false, "Undo stack is empty."};
    }
    runtimeState.terrain.courseObjectUndoRequested = true;
    return EditorCommandResult{true, "Queued course object undo."};
}

EditorCommandResult RedoEditorTransaction(
    const AppImGuiFrameContext& context,
    AppRuntimeState& runtimeState,
    const EditorRuntimeAuthoringApplyService& runtimeAuthoringApply,
    EditorTransactionStack* transactions,
    EditorAssetRegistry* assets,
    EditorDirtyStateService* dirtyState,
    EditorNotificationCenter* notifications) {
    const EditorTransactionRecord* next =
        transactions != nullptr ? transactions->NextRedoTransaction() : nullptr;
    if (next != nullptr &&
        next->payload.kind == EditorTransactionPayloadKind::AssetMutation) {
        if (assets == nullptr) {
            return EditorCommandResult{false, "Asset registry is unavailable."};
        }
        EditorAssetMutationExecutor executor(*assets);
        EditorAssetMutationResult applyResult{};
        const bool applied =
            transactions->Redo(
                [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                    applyResult = executor.ApplyTransaction(record, mode);
                    return applyResult.succeeded;
                });
        if (notifications != nullptr && !applyResult.message.empty()) {
            notifications->Push(
                applied ? EditorNotificationSeverity::Info : EditorNotificationSeverity::Error,
                "Asset",
                applyResult.message);
        }
        return EditorCommandResult{
            applied,
            applyResult.message.empty()
                ? std::string("Asset redo failed.")
                : applyResult.message};
    }
    if (next != nullptr &&
        next->payload.kind == EditorTransactionPayloadKind::RuntimeAuthoringApply) {
        EditorRuntimeAuthoringApplyResult applyResult{};
        const bool applied =
            transactions->Redo(
                [&](const EditorTransactionRecord& record, EditorTransactionApplyMode mode) {
                    applyResult =
                        runtimeAuthoringApply.ApplyTransaction(
                            EditorRuntimeAuthoringApplyRequest{
                                nullptr,
                                nullptr,
                                context.course,
                                &runtimeState,
                                transactions,
                                dirtyState,
                                notifications,
                                0,
                                "editor.command.runtimeApplyRedo"},
                            record,
                            mode);
                    return applyResult.succeeded;
                });
        return EditorCommandResult{
            applied,
            applyResult.message.empty()
                ? std::string("Runtime authoring apply redo failed.")
                : applyResult.message};
    }
    if (runtimeState.terrain.courseObjectRedoDepth == 0) {
        return EditorCommandResult{false, "Redo stack is empty."};
    }
    runtimeState.terrain.courseObjectRedoRequested = true;
    return EditorCommandResult{true, "Queued course object redo."};
}

} // namespace

void RegisterAppEditorStartupToolModules(
    EditorToolModuleRegistry& modules,
    const AppEditorStartupToolModuleInput& input,
    EditorToolRegistry* diagnostics) {
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.startup.properties",
                "Built-in Property Descriptors",
                0,
                false,
                {}},
            [](EditorToolModuleRegistrationContext& moduleContext) {
                if (moduleContext.propertyRegistry != nullptr &&
                    moduleContext.propertyRegistry->Count() == 0) {
                    RegisterBuiltInEditorProperties(*moduleContext.propertyRegistry);
                }
            }},
        diagnostics);
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.startup.detailsSections",
                "Built-in Details Section Providers",
                10,
                false,
                {}},
            [](EditorToolModuleRegistrationContext& moduleContext) {
                if (moduleContext.detailsSectionProviders != nullptr &&
                    moduleContext.detailsSectionProviders->Count() == 0) {
                    RegisterBuiltInEditorDetailsSectionProviders(*moduleContext.detailsSectionProviders);
                }
            }},
        diagnostics);
    (void)input;
}

void RunAppEditorStartupToolPipeline(const AppEditorStartupToolModuleInput& input) {
    EditorToolModuleRegistry modules;
    RegisterAppEditorStartupToolModules(modules, input, input.tools);
    modules.RunStartupRegistrations(
        EditorToolModuleRegistrationContext{
            input.tools,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            input.propertyRegistry,
            input.detailsSectionProviders});
}

void RegisterAppEditorFrameProviderToolModules(
    EditorToolModuleRegistry& modules,
    const AppEditorFrameProviderToolModuleInput& input,
    EditorToolRegistry* diagnostics) {
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.assets",
                "Editor Asset Providers",
                0,
                false,
                {}},
            [](EditorToolModuleRegistrationContext& moduleContext) {
                if (moduleContext.tools == nullptr) {
                    return;
                }
                moduleContext.tools->RegisterAssetProvider(
                    EditorAssetProviderDescriptor{
                        {},
                        "editor.asset.provider.registry",
                        "Editor Asset Registry",
                        "Asset",
                        true,
                        {}});
            }},
        diagnostics);
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.properties",
                "Editor Property Accessors",
                10,
                false,
                {}},
            [input](EditorToolModuleRegistrationContext& moduleContext) {
                if (moduleContext.tools == nullptr || moduleContext.propertyAccessors == nullptr ||
                    moduleContext.previewPropertyAccessors == nullptr) {
                    return;
                }
                moduleContext.tools->RegisterPropertyAccessor(
                    EditorPropertyAccessorRegistrationDescriptor{
                        {},
                        "course.propertyAccessor.authoring",
                        input.coursePropertyAccessor,
                        0,
                        true,
                        {}},
                    *moduleContext.propertyAccessors);
                moduleContext.tools->RegisterPropertyAccessor(
                    EditorPropertyAccessorRegistrationDescriptor{
                        {},
                        "production.propertyAccessor.authoring",
                        input.productionPropertyAccessor,
                        10,
                        true,
                        {}},
                    *moduleContext.propertyAccessors);
                moduleContext.tools->RegisterPropertyAccessor(
                    EditorPropertyAccessorRegistrationDescriptor{
                        {},
                        "course.propertyAccessor.preview",
                        input.coursePreviewPropertyAccessor,
                        0,
                        true,
                        EditorToolFeatureGate{
                            EditorToolFeatureState::Hidden,
                            "editor.previewPropertyAccessor",
                            true}},
                    *moduleContext.previewPropertyAccessors);
                moduleContext.tools->RegisterPropertyAccessor(
                    EditorPropertyAccessorRegistrationDescriptor{
                        {},
                        "production.propertyAccessor.preview",
                        input.productionPreviewPropertyAccessor,
                        10,
                        true,
                        EditorToolFeatureGate{
                            EditorToolFeatureState::Hidden,
                            "editor.previewPropertyAccessor",
                            true}},
                    *moduleContext.previewPropertyAccessors);
            }},
        diagnostics);
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.validation",
                "Editor Validation Adapters",
                20,
                false,
                {}},
            [input](EditorToolModuleRegistrationContext& moduleContext) {
                if (moduleContext.tools == nullptr || moduleContext.validation == nullptr) {
                    return;
                }
                moduleContext.tools->RegisterValidationAdapter(
                    EditorValidationAdapterRegistrationDescriptor{
                        {},
                        "course.validation.objects",
                        input.courseValidation,
                        0,
                        true,
                        {}},
                    *moduleContext.validation);
                moduleContext.tools->RegisterValidationAdapter(
                    EditorValidationAdapterRegistrationDescriptor{
                        {},
                        "vfx.validation.effects",
                        input.vfxValidation,
                        10,
                        true,
                        {}},
                    *moduleContext.validation);
                moduleContext.tools->RegisterValidationAdapter(
                    EditorValidationAdapterRegistrationDescriptor{
                        {},
                        "asset.validation.references",
                        input.assetReferenceValidation,
                        20,
                        true,
                        {}},
                    *moduleContext.validation);
                moduleContext.tools->RegisterValidationAdapter(
                    EditorValidationAdapterRegistrationDescriptor{
                        {},
                        "asset.validation.thumbnails",
                        input.assetThumbnailValidation,
                        30,
                        true,
                        {}},
                    *moduleContext.validation);
            }},
        diagnostics);
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.runtimeWatch",
                "Editor Runtime Watch Providers",
                30,
                false,
                {}},
            [](EditorToolModuleRegistrationContext& moduleContext) {
                if (moduleContext.tools != nullptr) {
                    RegisterDefaultEditorRuntimeWatchProvider(*moduleContext.tools);
                }
            }},
        diagnostics);
}

void RunAppEditorFrameProviderToolPipeline(const AppEditorFrameProviderToolModuleInput& input) {
    EditorToolModuleRegistry modules;
    RegisterAppEditorFrameProviderToolModules(modules, input, input.tools);
    modules.RunFrameRegistrations(
        EditorToolModuleRegistrationContext{
            input.tools,
            nullptr,
            nullptr,
            input.propertyAccessors,
            input.previewPropertyAccessors,
            input.validation});
}

void RegisterAppEditorCommandToolModules(
    EditorToolModuleRegistry& modules,
    const AppEditorCommandToolModuleInput& input,
    EditorToolRegistry* diagnostics) {
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.commands.course",
                "Course Editor Commands",
                0,
                false,
                {}},
            [input](EditorToolModuleRegistrationContext&) {
                if (!HasCommandPipelineInput(input)) {
                    return;
                }
                AppRuntimeState& runtimeState = *input.runtimeState;
                const AppImGuiFrameContext& context = *input.frameContext;
                CourseEditorCommandProvider courseProvider(
                    CourseEditorCommandProviderInput{
                        context.onSaveCourse,
                        context.onApplyCourse,
                        context.onReloadCourse,
                        input.closeCourseDocument,
                        input.reopenCourseDocument,
                        context.onTeleportCourseToDistance,
                        [&runtimeState]() {
                            return runtimeState.terrain.freezeCourseRuntime;
                        },
                        [&runtimeState](bool frozen) {
                            runtimeState.terrain.freezeCourseRuntime = frozen;
                        },
                        context.courseDistance});
                courseProvider.RegisterCommands(*input.editorContext);
            }},
        diagnostics);
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.commands.builtin",
                "Built-in Editor Commands",
                10,
                false,
                {}},
            [input](EditorToolModuleRegistrationContext&) {
                if (!HasCommandPipelineInput(input) ||
                    input.playSessionLifecycle == nullptr ||
                    input.playSessionRuntimeControl == nullptr ||
                    input.runtimeAuthoringApply == nullptr ||
                    input.playSessionSnapshot == nullptr) {
                    return;
                }
                EditorContext& editorContext = *input.editorContext;
                const AppImGuiFrameContext& context = *input.frameContext;
                AppRuntimeState& runtimeState = *input.runtimeState;
                const EditorPlaySessionLifecycleService& playSessionLifecycle = *input.playSessionLifecycle;
                const EditorPlaySessionRuntimeControlService& playSessionRuntimeControl =
                    *input.playSessionRuntimeControl;
                const EditorRuntimeAuthoringApplyService& runtimeAuthoringApply = *input.runtimeAuthoringApply;
                EditorPlaySessionIsolationSnapshot& playSessionSnapshot = *input.playSessionSnapshot;
                EditorBuiltinCommandProvider builtinProvider(
                    EditorBuiltinCommandProviderInput{
                        [&context,
                         &runtimeState,
                         &runtimeAuthoringApply,
                         transactions = editorContext.transactions,
                         assets = editorContext.assets,
                         dirtyState = editorContext.dirtyState,
                         notifications = editorContext.notifications]() {
                            return UndoEditorTransaction(
                                context,
                                runtimeState,
                                runtimeAuthoringApply,
                                transactions,
                                assets,
                                dirtyState,
                                notifications);
                        },
                        [&context,
                         &runtimeState,
                         &runtimeAuthoringApply,
                         transactions = editorContext.transactions,
                         assets = editorContext.assets,
                         dirtyState = editorContext.dirtyState,
                         notifications = editorContext.notifications]() {
                            return RedoEditorTransaction(
                                context,
                                runtimeState,
                                runtimeAuthoringApply,
                                transactions,
                                assets,
                                dirtyState,
                                notifications);
                        },
                        [&context,
                         &runtimeState,
                         &playSessionLifecycle,
                         &playSessionSnapshot,
                         playSession = editorContext.playSession,
                         notifications = editorContext.notifications](EditorPlaySessionMode mode) {
                            const EditorPlaySessionLifecycleResult result =
                                playSessionLifecycle.Begin(
                                    EditorPlaySessionLifecycleRequest{
                                        playSession,
                                        &playSessionSnapshot,
                                        context.course,
                                        &runtimeState,
                                        notifications,
                                        "editor.command.playSession"},
                                    mode);
                            return EditorCommandResult{result.succeeded, result.message};
                        },
                        [&context,
                         &runtimeState,
                         &playSessionLifecycle,
                         &playSessionSnapshot,
                         playSession = editorContext.playSession,
                         notifications = editorContext.notifications]() {
                            const EditorPlaySessionLifecycleResult result =
                                playSessionLifecycle.Stop(
                                    EditorPlaySessionLifecycleRequest{
                                        playSession,
                                        &playSessionSnapshot,
                                        context.course,
                                        &runtimeState,
                                        notifications,
                                        "editor.command.playSession"});
                            return EditorCommandResult{result.succeeded, result.message};
                        },
                        [&context,
                         &runtimeState,
                         &playSessionRuntimeControl,
                         &playSessionSnapshot,
                         playSession = editorContext.playSession,
                         notifications = editorContext.notifications]() {
                            const EditorPlaySessionRuntimeControlResult result =
                                playSessionRuntimeControl.Pause(
                                    EditorPlaySessionRuntimeControlRequest{
                                        playSession,
                                        &playSessionSnapshot,
                                        context.course,
                                        &runtimeState,
                                        notifications,
                                        "editor.command.runtimeControl"});
                            return EditorCommandResult{result.succeeded, result.message};
                        },
                        [&context,
                         &runtimeState,
                         &playSessionRuntimeControl,
                         &playSessionSnapshot,
                         playSession = editorContext.playSession,
                         notifications = editorContext.notifications]() {
                            const EditorPlaySessionRuntimeControlResult result =
                                playSessionRuntimeControl.Resume(
                                    EditorPlaySessionRuntimeControlRequest{
                                        playSession,
                                        &playSessionSnapshot,
                                        context.course,
                                        &runtimeState,
                                        notifications,
                                        "editor.command.runtimeControl"});
                            return EditorCommandResult{result.succeeded, result.message};
                        },
                        [&context,
                         &runtimeState,
                         &playSessionRuntimeControl,
                         &playSessionSnapshot,
                         playSession = editorContext.playSession,
                         notifications = editorContext.notifications]() {
                            const EditorPlaySessionRuntimeControlResult result =
                                playSessionRuntimeControl.Step(
                                    EditorPlaySessionRuntimeControlRequest{
                                        playSession,
                                        &playSessionSnapshot,
                                        context.course,
                                        &runtimeState,
                                        notifications,
                                        "editor.command.runtimeControl"});
                            return EditorCommandResult{result.succeeded, result.message};
                        },
                        [&context,
                         &runtimeState,
                         &playSessionRuntimeControl,
                         &playSessionSnapshot,
                         playSession = editorContext.playSession,
                         notifications = editorContext.notifications]() {
                            const EditorPlaySessionRuntimeControlResult result =
                                playSessionRuntimeControl.ResetRuntime(
                                    EditorPlaySessionRuntimeControlRequest{
                                        playSession,
                                        &playSessionSnapshot,
                                        context.course,
                                        &runtimeState,
                                        notifications,
                                        "editor.command.runtimeControl"});
                            if (result.succeeded && context.onApplyCourse) {
                                context.onApplyCourse();
                            }
                            return EditorCommandResult{result.succeeded, result.message};
                        },
                        [&context,
                         &runtimeState,
                         &runtimeAuthoringApply,
                         &playSessionSnapshot,
                         playSession = editorContext.playSession,
                         transactions = editorContext.transactions,
                         dirtyState = editorContext.dirtyState,
                         notifications = editorContext.notifications,
                         confirmService = editorContext.confirmService,
                         validationErrorCount = editorContext.validationReport != nullptr
                             ? editorContext.validationReport->errorCount
                             : 0u]() {
                            if (confirmService == nullptr) {
                                return EditorCommandResult{false, "Confirmation service is unavailable."};
                            }
                            if (validationErrorCount > 0) {
                                return EditorCommandResult{false, "Runtime apply blocked: validation errors."};
                            }
                            const bool requested =
                                confirmService->Request(
                                    EditorModalConfirmRequest{
                                        0,
                                        EditorModalConfirmSeverity::Warning,
                                        "Runtime Apply",
                                        "Apply Runtime Changes",
                                        "Apply the current Play/Sim runtime authoring state to the editable authoring snapshot. Stop will keep the applied runtime changes; later runtime changes remain isolated unless applied again.",
                                        "Apply Runtime",
                                        "Cancel",
                                        [&runtimeAuthoringApply,
                                         playSession,
                                         &playSessionSnapshot,
                                         course = context.course,
                                         runtimeStatePtr = &runtimeState,
                                         transactions,
                                         dirtyState,
                                         notifications,
                                         validationErrorCount]() {
                                            runtimeAuthoringApply.Apply(
                                                EditorRuntimeAuthoringApplyRequest{
                                                    playSession,
                                                    &playSessionSnapshot,
                                                    course,
                                                    runtimeStatePtr,
                                                    transactions,
                                                    dirtyState,
                                                    notifications,
                                                    validationErrorCount,
                                                    "editor.command.runtimeApply"});
                                        },
                                        []() {}});
                            return EditorCommandResult{
                                requested,
                                requested
                                    ? std::string("Runtime apply confirmation requested.")
                                    : std::string("Runtime apply confirmation is already pending.")};
                        }});
                builtinProvider.RegisterCommands(editorContext);
            }},
        diagnostics);
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.commands.assets",
                "Asset Editor Commands",
                20,
                false,
                {}},
            [input](EditorToolModuleRegistrationContext&) {
                if (!HasCommandPipelineInput(input)) {
                    return;
                }
                EditorAssetCommandProvider assetProvider;
                assetProvider.RegisterCommands(*input.editorContext);
            }},
        diagnostics);
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.commands.presentation",
                "Editor Command Presentation",
                30,
                false,
                {}},
            [input](EditorToolModuleRegistrationContext&) {
                if (!HasCommandPipelineInput(input) || input.editorContext->tools == nullptr) {
                    return;
                }
                EditorCommandRegistry& registry = *input.editorContext->commands;
                RegisterDefaultEditorMenu(*input.editorContext->tools, registry);
                RegisterDefaultEditorToolbar(*input.editorContext->tools);
                input.editorContext->tools->ValidateMenuCommands(registry);
                input.editorContext->tools->ValidateToolbarCommands(registry);
            }},
        diagnostics);
}

void RunAppEditorCommandToolPipeline(const AppEditorCommandToolModuleInput& input) {
    if (!HasCommandPipelineInput(input)) {
        return;
    }
    input.editorContext->commands->Clear();
    EditorToolModuleRegistry modules;
    RegisterAppEditorCommandToolModules(
        modules,
        input,
        input.editorContext->tools);
    modules.RunFrameRegistrations(
        EditorToolModuleRegistrationContext{
            input.editorContext->tools,
            input.editorContext->commands});
}

} // namespace editor
