#pragma once

#include "editor/EditorToolRegistration.h"

#include <functional>

struct AppImGuiFrameContext;
struct AppRuntimeState;

namespace editor {

class EditorPlaySessionIsolationSnapshot;
class EditorPlaySessionLifecycleService;
class EditorPlaySessionRuntimeControlService;
class EditorRuntimeAuthoringApplyService;
class EditorValidationAdapter;

struct AppEditorStartupToolModuleInput {
    EditorToolRegistry* tools = nullptr;
    EditorPropertyRegistry* propertyRegistry = nullptr;
    EditorDetailsSectionProviderRegistry* detailsSectionProviders = nullptr;
};

struct AppEditorFrameProviderToolModuleInput {
    EditorToolRegistry* tools = nullptr;
    EditorCompositePropertyAccessor* propertyAccessors = nullptr;
    EditorCompositePropertyAccessor* previewPropertyAccessors = nullptr;
    EditorValidationService* validation = nullptr;
    EditorPropertyAccessor* coursePropertyAccessor = nullptr;
    EditorPropertyAccessor* productionPropertyAccessor = nullptr;
    EditorPropertyAccessor* coursePreviewPropertyAccessor = nullptr;
    EditorPropertyAccessor* productionPreviewPropertyAccessor = nullptr;
    const EditorValidationAdapter* courseValidation = nullptr;
    const EditorValidationAdapter* vfxValidation = nullptr;
    const EditorValidationAdapter* assetReferenceValidation = nullptr;
    const EditorValidationAdapter* assetThumbnailValidation = nullptr;
    const EditorValidationAdapter* materialGraphValidation = nullptr;
    const EditorValidationAdapter* vfxGraphValidation = nullptr;
    const EditorValidationAdapter* animationStateMachineValidation = nullptr;
    const EditorValidationAdapter* gameplayVisualScriptValidation = nullptr;
};

struct AppEditorCommandToolModuleInput {
    EditorContext* editorContext = nullptr;
    const AppImGuiFrameContext* frameContext = nullptr;
    AppRuntimeState* runtimeState = nullptr;
    const EditorPlaySessionLifecycleService* playSessionLifecycle = nullptr;
    const EditorPlaySessionRuntimeControlService* playSessionRuntimeControl = nullptr;
    const EditorRuntimeAuthoringApplyService* runtimeAuthoringApply = nullptr;
    EditorPlaySessionIsolationSnapshot* playSessionSnapshot = nullptr;
    std::function<void()> closeCourseDocument;
    std::function<void()> reopenCourseDocument;
};

void RegisterAppEditorStartupToolModules(
    EditorToolModuleRegistry& modules,
    const AppEditorStartupToolModuleInput& input,
    EditorToolRegistry* diagnostics);
void RunAppEditorStartupToolPipeline(const AppEditorStartupToolModuleInput& input);

void RegisterAppEditorFrameProviderToolModules(
    EditorToolModuleRegistry& modules,
    const AppEditorFrameProviderToolModuleInput& input,
    EditorToolRegistry* diagnostics);
void RunAppEditorFrameProviderToolPipeline(const AppEditorFrameProviderToolModuleInput& input);

void RegisterAppEditorCommandToolModules(
    EditorToolModuleRegistry& modules,
    const AppEditorCommandToolModuleInput& input,
    EditorToolRegistry* diagnostics);
void RunAppEditorCommandToolPipeline(const AppEditorCommandToolModuleInput& input);

} // namespace editor
