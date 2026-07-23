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
#include "editor/core/EditorExecutionContext.h"
#include "editor/play/EditorRuntimeApplyExecutionService.h"
#include "editor/documents/EditorDocumentManager.h"
#include "editor/world/IEditorWorldMutationExecutionService.h"
#include "editor/world/EditorWorldMutationUndoCommand.h"
#include "editor/sequencer/EditorSequencer.h"
#include "editor/prefab/EditorPrefabService.h"
#include "editor/material/EditorMaterialGraph.h"
#include "editor/vfx/EditorVfxGraph.h"
#include "editor/animation/EditorAnimationStateMachine.h"
#include "editor/gameplay/EditorGameplayVisualScript.h"
#include "editor/ai/EditorProductionAiAuthoringPipeline.h"
#include "editor/navigation/EditorProductionNavigationAuthoringPipeline.h"
#include "editor/scene/EditorBlenderSceneImportCommandProvider.h"
#include "editor/scene/EditorBlenderSceneImportTransaction.h"

#include <string>
#include <memory>
#include <utility>

namespace editor {
namespace {

bool HasCommandPipelineInput(const AppEditorCommandToolModuleInput& input) {
    return input.editorContext != nullptr &&
        input.editorContext->commands != nullptr &&
        input.frameContext != nullptr &&
        input.runtimeState != nullptr;
}

EditorDocumentId WorldMutationDocument(const EditorTransactionRecord* transaction) {
    if (transaction == nullptr || transaction->command == nullptr) return {};
    const auto* command = dynamic_cast<const EditorWorldMutationUndoCommand*>(
        transaction->command.get());
    return command != nullptr ? command->Document() : EditorDocumentId{};
}

void MarkWorldMutationDirty(
    const EditorDocumentId& document,
    std::string reason,
    EditorDirtyStateService* dirtyState,
    EditorDocumentManager* documentManager) {
    if (!document.IsValid()) return;
    if (dirtyState != nullptr) {
        dirtyState->MarkDirty(
            document.type == EditorDocumentTypes::Course
                ? EditorDirtyDomain::CourseAuthoring
                : EditorDirtyDomain::Unknown,
            "world:" + document.assetGuid,
            document.type + " World",
            reason,
            dirtyState->Revision() + 1);
    }
    if (documentManager != nullptr && documentManager->Find(document) != nullptr) {
        documentManager->MarkDirty(document, std::move(reason));
    }
}

EditorCommandResult UndoEditorTransaction(
    const AppImGuiFrameContext& context,
    AppRuntimeState& runtimeState,
    EditorTransactionStack* transactions,
    EditorAssetRegistry* assets,
    EditorDirtyStateService* dirtyState,
    EditorNotificationCenter* notifications,
    IEditorWorldMutationExecutionService* worldExecution,
    EditorDocumentManager* documentManager,
    EditorSequencerService* sequencer,
    EditorPrefabService* prefabs,
    EditorMaterialGraphService* materialGraphs,
    EditorVfxGraphService* vfxGraphs,
    EditorAnimationStateMachineService* animationStateMachines,
    EditorGameplayVisualScriptService* gameplayVisualScripts,
    EditorProductionAiAuthoringPipeline* aiAuthoring,
    EditorProductionNavigationAuthoringPipeline* navigationAuthoring,
    IEditorBlenderSceneImportExecutionService* blenderSceneImportExecution) {
    const EditorTransactionRecord* next =
        transactions != nullptr ? transactions->NextUndoTransaction() : nullptr;
    if (next != nullptr && next->command != nullptr &&
        (next->command->DomainId() == "asset" || next->command->DomainId() == "runtime-apply" ||
            next->command->DomainId() == "world" || next->command->DomainId() == "sequencer" ||
            next->command->DomainId() == "prefab" || next->command->DomainId() == "material-graph" ||
            next->command->DomainId() == "vfx-graph" ||
            next->command->DomainId() == "animation-state-machine" ||
            next->command->DomainId() == "gameplay-visual-script" ||
            next->command->DomainId() == "ai-authoring" ||
            next->command->DomainId() == "navigation-authoring" ||
            next->command->DomainId() == "blender-scene-import")) {
        const bool assetCommand = next->command->DomainId() == "asset";
        const bool worldCommand = next->command->DomainId() == "world";
        const bool sequencerCommand = next->command->DomainId() == "sequencer";
        const bool prefabCommand = next->command->DomainId() == "prefab";
        const bool materialGraphCommand = next->command->DomainId() == "material-graph";
        const bool vfxGraphCommand = next->command->DomainId() == "vfx-graph";
        const bool animationStateMachineCommand = next->command->DomainId() == "animation-state-machine";
        const bool gameplayVisualScriptCommand = next->command->DomainId() == "gameplay-visual-script";
        const bool aiAuthoringCommand = next->command->DomainId() == "ai-authoring";
        const bool navigationAuthoringCommand = next->command->DomainId() == "navigation-authoring";
        const bool blenderSceneImportCommand =
            next->command->DomainId() == "blender-scene-import";
        const EditorDocumentId worldDocument = worldCommand
            ? WorldMutationDocument(next)
            : EditorDocumentId{};
        if (assets == nullptr) {
            if (assetCommand) return EditorCommandResult{false, "Asset registry is unavailable."};
        }
        EditorExecutionContext execution;
        EditorError registrationError;
        std::unique_ptr<EditorAssetMutationExecutor> assetExecution;
        if (assetCommand) {
            assetExecution = std::make_unique<EditorAssetMutationExecutor>(*assets);
            execution.Register(*assetExecution, &registrationError);
        }
        EditorRuntimeApplyExecutionService runtimeExecution(
            EditorRuntimeApplyExecutionTargets{
                context.course, &runtimeState, context.effectRuntime, context.postProcessStack,
                dirtyState, notifications, "editor.command.runtimeApplyUndo"});
        if (!assetCommand && !worldCommand && !sequencerCommand && !prefabCommand &&
            !materialGraphCommand && !vfxGraphCommand && !animationStateMachineCommand &&
            !gameplayVisualScriptCommand && !aiAuthoringCommand &&
            !navigationAuthoringCommand && !blenderSceneImportCommand) {
            execution.Register(runtimeExecution, &registrationError);
        }
        if (worldCommand && worldExecution != nullptr) {
            execution.Register(*worldExecution, &registrationError);
        }
        if (sequencerCommand && sequencer != nullptr) {
            execution.Register(*sequencer, &registrationError);
        }
        if (prefabCommand && prefabs != nullptr) {
            execution.Register(*prefabs, &registrationError);
        }
        if (materialGraphCommand && materialGraphs != nullptr) {
            execution.Register(*materialGraphs, &registrationError);
        }
        if (vfxGraphCommand && vfxGraphs != nullptr) {
            execution.Register(*vfxGraphs, &registrationError);
        }
        if (animationStateMachineCommand && animationStateMachines != nullptr) {
            execution.Register(*animationStateMachines, &registrationError);
        }
        if (gameplayVisualScriptCommand && gameplayVisualScripts != nullptr) {
            execution.Register(*gameplayVisualScripts, &registrationError);
        }
        if (aiAuthoringCommand && aiAuthoring != nullptr) {
            execution.Register(*aiAuthoring, &registrationError);
        }
        if (navigationAuthoringCommand && navigationAuthoring != nullptr) {
            execution.Register(*navigationAuthoring, &registrationError);
        }
        if (blenderSceneImportCommand && blenderSceneImportExecution != nullptr) {
            execution.Register(*blenderSceneImportExecution, &registrationError);
        }
        EditorError error;
        const bool applied = transactions->Undo(execution, &error);
        if (!applied && notifications != nullptr) {
            notifications->Push(EditorNotificationSeverity::Error, "Transaction", error.message);
        }
        if (applied && worldCommand) {
            MarkWorldMutationDirty(
                worldDocument,
                "World mutation undo applied.",
                dirtyState,
                documentManager);
        }
        return EditorCommandResult{applied, applied ? "Undo completed." : error.message};
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
    EditorTransactionStack* transactions,
    EditorAssetRegistry* assets,
    EditorDirtyStateService* dirtyState,
    EditorNotificationCenter* notifications,
    IEditorWorldMutationExecutionService* worldExecution,
    EditorDocumentManager* documentManager,
    EditorSequencerService* sequencer,
    EditorPrefabService* prefabs,
    EditorMaterialGraphService* materialGraphs,
    EditorVfxGraphService* vfxGraphs,
    EditorAnimationStateMachineService* animationStateMachines,
    EditorGameplayVisualScriptService* gameplayVisualScripts,
    EditorProductionAiAuthoringPipeline* aiAuthoring,
    EditorProductionNavigationAuthoringPipeline* navigationAuthoring,
    IEditorBlenderSceneImportExecutionService* blenderSceneImportExecution) {
    const EditorTransactionRecord* next =
        transactions != nullptr ? transactions->NextRedoTransaction() : nullptr;
    if (next != nullptr && next->command != nullptr &&
        (next->command->DomainId() == "asset" || next->command->DomainId() == "runtime-apply" ||
            next->command->DomainId() == "world" || next->command->DomainId() == "sequencer" ||
            next->command->DomainId() == "prefab" || next->command->DomainId() == "material-graph" ||
            next->command->DomainId() == "vfx-graph" ||
            next->command->DomainId() == "animation-state-machine" ||
            next->command->DomainId() == "gameplay-visual-script" ||
            next->command->DomainId() == "ai-authoring" ||
            next->command->DomainId() == "navigation-authoring" ||
            next->command->DomainId() == "blender-scene-import")) {
        const bool assetCommand = next->command->DomainId() == "asset";
        const bool worldCommand = next->command->DomainId() == "world";
        const bool sequencerCommand = next->command->DomainId() == "sequencer";
        const bool prefabCommand = next->command->DomainId() == "prefab";
        const bool materialGraphCommand = next->command->DomainId() == "material-graph";
        const bool vfxGraphCommand = next->command->DomainId() == "vfx-graph";
        const bool animationStateMachineCommand = next->command->DomainId() == "animation-state-machine";
        const bool gameplayVisualScriptCommand = next->command->DomainId() == "gameplay-visual-script";
        const bool aiAuthoringCommand = next->command->DomainId() == "ai-authoring";
        const bool navigationAuthoringCommand = next->command->DomainId() == "navigation-authoring";
        const bool blenderSceneImportCommand =
            next->command->DomainId() == "blender-scene-import";
        const EditorDocumentId worldDocument = worldCommand
            ? WorldMutationDocument(next)
            : EditorDocumentId{};
        if (assets == nullptr) {
            if (assetCommand) return EditorCommandResult{false, "Asset registry is unavailable."};
        }
        EditorExecutionContext execution;
        EditorError registrationError;
        std::unique_ptr<EditorAssetMutationExecutor> assetExecution;
        if (assetCommand) {
            assetExecution = std::make_unique<EditorAssetMutationExecutor>(*assets);
            execution.Register(*assetExecution, &registrationError);
        }
        EditorRuntimeApplyExecutionService runtimeExecution(
            EditorRuntimeApplyExecutionTargets{
                context.course, &runtimeState, context.effectRuntime, context.postProcessStack,
                dirtyState, notifications, "editor.command.runtimeApplyRedo"});
        if (!assetCommand && !worldCommand && !sequencerCommand && !prefabCommand &&
            !materialGraphCommand && !vfxGraphCommand && !animationStateMachineCommand &&
            !gameplayVisualScriptCommand && !aiAuthoringCommand &&
            !navigationAuthoringCommand && !blenderSceneImportCommand) {
            execution.Register(runtimeExecution, &registrationError);
        }
        if (worldCommand && worldExecution != nullptr) {
            execution.Register(*worldExecution, &registrationError);
        }
        if (sequencerCommand && sequencer != nullptr) {
            execution.Register(*sequencer, &registrationError);
        }
        if (prefabCommand && prefabs != nullptr) {
            execution.Register(*prefabs, &registrationError);
        }
        if (materialGraphCommand && materialGraphs != nullptr) {
            execution.Register(*materialGraphs, &registrationError);
        }
        if (vfxGraphCommand && vfxGraphs != nullptr) {
            execution.Register(*vfxGraphs, &registrationError);
        }
        if (animationStateMachineCommand && animationStateMachines != nullptr) {
            execution.Register(*animationStateMachines, &registrationError);
        }
        if (gameplayVisualScriptCommand && gameplayVisualScripts != nullptr) {
            execution.Register(*gameplayVisualScripts, &registrationError);
        }
        if (aiAuthoringCommand && aiAuthoring != nullptr) {
            execution.Register(*aiAuthoring, &registrationError);
        }
        if (navigationAuthoringCommand && navigationAuthoring != nullptr) {
            execution.Register(*navigationAuthoring, &registrationError);
        }
        if (blenderSceneImportCommand && blenderSceneImportExecution != nullptr) {
            execution.Register(*blenderSceneImportExecution, &registrationError);
        }
        EditorError error;
        const bool applied = transactions->Redo(execution, &error);
        if (!applied && notifications != nullptr) {
            notifications->Push(EditorNotificationSeverity::Error, "Transaction", error.message);
        }
        if (applied && worldCommand) {
            MarkWorldMutationDirty(
                worldDocument,
                "World mutation redo applied.",
                dirtyState,
                documentManager);
        }
        return EditorCommandResult{applied, applied ? "Redo completed." : error.message};
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
                if (input.materialGraphValidation != nullptr) {
                    moduleContext.tools->RegisterValidationAdapter(
                        EditorValidationAdapterRegistrationDescriptor{
                            {},
                            "material.validation.graph",
                            input.materialGraphValidation,
                            40,
                            true,
                            {}},
                        *moduleContext.validation);
                }
                if (input.vfxGraphValidation != nullptr) {
                    moduleContext.tools->RegisterValidationAdapter(
                        EditorValidationAdapterRegistrationDescriptor{
                            {},
                            "vfx.validation.graph",
                            input.vfxGraphValidation,
                            45,
                            true,
                            {}},
                        *moduleContext.validation);
                }
                if (input.animationStateMachineValidation != nullptr) {
                    moduleContext.tools->RegisterValidationAdapter(
                        EditorValidationAdapterRegistrationDescriptor{
                            {}, "animation.validation.stateMachine",
                            input.animationStateMachineValidation, 50, true, {}},
                        *moduleContext.validation);
                }
                if (input.gameplayVisualScriptValidation != nullptr) {
                    moduleContext.tools->RegisterValidationAdapter(
                        EditorValidationAdapterRegistrationDescriptor{
                            {}, "gameplay.validation.visualScript",
                            input.gameplayVisualScriptValidation, 55, true, {}},
                        *moduleContext.validation);
                }
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
                         notifications = editorContext.notifications,
                         worldExecution = editorContext.worldMutationExecution,
                         documentManager = editorContext.documentManager,
                         sequencer = editorContext.sequencer,
                         prefabs = editorContext.prefabs,
                         materialGraphs = editorContext.materialGraphs,
                         vfxGraphs = editorContext.vfxGraphs,
                         animationStateMachines = editorContext.animationStateMachines,
                         gameplayVisualScripts = editorContext.gameplayVisualScripts,
                         aiAuthoring = editorContext.aiAuthoring,
                         navigationAuthoring = editorContext.navigationAuthoring,
                         blenderSceneImportExecution =
                             editorContext.blenderSceneImportExecution]() {
                            return UndoEditorTransaction(
                                context,
                                runtimeState,
                                transactions,
                                assets,
                                dirtyState,
                                notifications,
                                worldExecution,
                                documentManager,
                                sequencer,
                                prefabs,
                                materialGraphs,
                                vfxGraphs,
                                animationStateMachines,
                                gameplayVisualScripts,
                                aiAuthoring,
                                navigationAuthoring,
                                blenderSceneImportExecution);
                        },
                        [&context,
                         &runtimeState,
                         &runtimeAuthoringApply,
                         transactions = editorContext.transactions,
                         assets = editorContext.assets,
                         dirtyState = editorContext.dirtyState,
                         notifications = editorContext.notifications,
                         worldExecution = editorContext.worldMutationExecution,
                         documentManager = editorContext.documentManager,
                         sequencer = editorContext.sequencer,
                         prefabs = editorContext.prefabs,
                         materialGraphs = editorContext.materialGraphs,
                         vfxGraphs = editorContext.vfxGraphs,
                         animationStateMachines = editorContext.animationStateMachines,
                         gameplayVisualScripts = editorContext.gameplayVisualScripts,
                         aiAuthoring = editorContext.aiAuthoring,
                         navigationAuthoring = editorContext.navigationAuthoring,
                         blenderSceneImportExecution =
                             editorContext.blenderSceneImportExecution]() {
                            return RedoEditorTransaction(
                                context,
                                runtimeState,
                                transactions,
                                assets,
                                dirtyState,
                                notifications,
                                worldExecution,
                                documentManager,
                                sequencer,
                                prefabs,
                                materialGraphs,
                                vfxGraphs,
                                animationStateMachines,
                                gameplayVisualScripts,
                                aiAuthoring,
                                navigationAuthoring,
                                blenderSceneImportExecution);
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
                                        "editor.command.playSession",
                                        context.effectRuntime,
                                        context.postProcessStack},
                                    mode);
                            if (result.succeeded && context.onBeginGameplaySpawns) {
                                std::string spawnError;
                                if (!context.onBeginGameplaySpawns(&spawnError)) {
                                    playSessionLifecycle.Stop(
                                        EditorPlaySessionLifecycleRequest{
                                            playSession,
                                            &playSessionSnapshot,
                                            context.course,
                                            &runtimeState,
                                            notifications,
                                            "editor.command.playSession",
                                            context.effectRuntime,
                                            context.postProcessStack});
                                    return EditorCommandResult{
                                        false,
                                        spawnError.empty()
                                            ? std::string("Runtime Spawn initialization failed.")
                                            : std::move(spawnError)};
                                }
                            }
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
                                        "editor.command.playSession",
                                        context.effectRuntime,
                                        context.postProcessStack});
                            if (result.succeeded && context.onStopGameplaySpawns) {
                                context.onStopGameplaySpawns();
                            }
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
                                        "editor.command.runtimeControl",
                                        context.effectRuntime,
                                        context.postProcessStack});
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
                                        "editor.command.runtimeControl",
                                        context.effectRuntime,
                                        context.postProcessStack});
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
                                        "editor.command.runtimeControl",
                                        context.effectRuntime,
                                        context.postProcessStack});
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
                                        "editor.command.runtimeControl",
                                        context.effectRuntime,
                                        context.postProcessStack});
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
                                         effectRuntime = context.effectRuntime,
                                         postProcessStack = context.postProcessStack,
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
                                                    "editor.command.runtimeApply",
                                                    effectRuntime,
                                                    postProcessStack});
                                        },
                                        []() {}});
                            return EditorCommandResult{
                                requested,
                                requested
                                    ? std::string("Runtime apply confirmation requested.")
                                    : std::string("Runtime apply confirmation is already pending.")};
                        },
                        [&runtimeState](EditorTransformGizmoMode mode) {
                            runtimeState.terrain.courseObjectGizmoMode =
                                ToCourseGizmoMode(mode);
                            runtimeState.terrain.courseObjectActiveAxis = -1;
                            return EditorCommandResult{
                                true, std::string("Transform mode: ") + ToString(mode) + "."};
                        },
                        [&runtimeState]() {
                            runtimeState.terrain.courseObjectGizmoSpace =
                                runtimeState.terrain.courseObjectGizmoSpace == 0 ? 1 : 0;
                            return EditorCommandResult{
                                true,
                                std::string("Transform space: ") +
                                    (runtimeState.terrain.courseObjectGizmoSpace == 0
                                         ? "World."
                                         : "Local.")};
                        },
                        [&runtimeState]() {
                            runtimeState.terrain.courseObjectSnapEnabled =
                                !runtimeState.terrain.courseObjectSnapEnabled;
                            return EditorCommandResult{
                                true,
                                runtimeState.terrain.courseObjectSnapEnabled
                                    ? "Transform snap enabled."
                                    : "Transform snap disabled."};
                        }});
                builtinProvider.RegisterCommands(editorContext);
            }},
        diagnostics);
    modules.Register(
        EditorToolModuleRegistration{
            EditorToolModuleDescriptor{
                {},
                "editor.module.commands.blenderSceneImport",
                "Blender Scene Import Commands",
                15,
                false,
                {}},
            [input](EditorToolModuleRegistrationContext&) {
                if (!HasCommandPipelineInput(input)) {
                    return;
                }
                EditorBlenderSceneImportCommandProvider provider;
                provider.RegisterCommands(*input.editorContext);
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
