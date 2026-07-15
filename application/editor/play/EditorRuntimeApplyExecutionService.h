#pragma once

#include "IEditorRuntimeApplyExecutionService.h"

class EffectRuntime;
class PostProcessStack;
struct AppRuntimeState;
struct CourseAsset;

namespace editor {

class EditorDirtyStateService;
class EditorNotificationCenter;

struct EditorRuntimeApplyExecutionTargets {
    CourseAsset* course = nullptr;
    AppRuntimeState* runtimeState = nullptr;
    EffectRuntime* effectRuntime = nullptr;
    PostProcessStack* postProcessStack = nullptr;
    EditorDirtyStateService* dirtyState = nullptr;
    EditorNotificationCenter* notifications = nullptr;
    const char* source = "editor.runtimeApply.command";
};

class EditorRuntimeApplyExecutionService final : public IEditorRuntimeApplyExecutionService {
public:
    explicit EditorRuntimeApplyExecutionService(EditorRuntimeApplyExecutionTargets targets)
        : targets_(targets) {}

    EditorUndoResult ApplyRuntimeChange(
        const EditorRuntimeApplyChange& change,
        EditorTransactionApplyMode mode) override;

private:
    EditorRuntimeApplyExecutionTargets targets_{};
};

} // namespace editor
