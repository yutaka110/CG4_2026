#pragma once

#include "EditorCommandProvider.h"
#include "EditorCommandRegistry.h"
#include "EditorPlaySessionState.h"
#include "EditorTransformGizmoService.h"

#include <functional>

namespace editor {

struct EditorBuiltinCommandProviderInput {
    std::function<EditorCommandResult()> undo;
    std::function<EditorCommandResult()> redo;
    std::function<EditorCommandResult(EditorPlaySessionMode)> beginPlaySession;
    std::function<EditorCommandResult()> stopPlaySession;
    std::function<EditorCommandResult()> pauseRuntime;
    std::function<EditorCommandResult()> resumeRuntime;
    std::function<EditorCommandResult()> stepRuntime;
    std::function<EditorCommandResult()> toggleViewportPossession;
    std::function<EditorCommandResult()> resetRuntime;
    std::function<EditorCommandResult()> applyRuntimeChanges;
    std::function<EditorCommandResult(EditorTransformGizmoMode)> setTransformMode;
    std::function<EditorCommandResult()> toggleTransformSpace;
    std::function<EditorCommandResult()> toggleTransformSnap;
};

class EditorBuiltinCommandProvider final : public EditorCommandProvider {
public:
    explicit EditorBuiltinCommandProvider(EditorBuiltinCommandProviderInput input);

    void RegisterCommands(EditorContext& context) const override;

private:
    EditorBuiltinCommandProviderInput input_;
};

} // namespace editor
