#pragma once

#include "EditorCommandProvider.h"
#include "EditorCommandRegistry.h"
#include "EditorPlaySessionState.h"

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
    std::function<EditorCommandResult()> resetRuntime;
    std::function<EditorCommandResult()> applyRuntimeChanges;
};

class EditorBuiltinCommandProvider final : public EditorCommandProvider {
public:
    explicit EditorBuiltinCommandProvider(EditorBuiltinCommandProviderInput input);

    void RegisterCommands(EditorContext& context) const override;

private:
    EditorBuiltinCommandProviderInput input_;
};

} // namespace editor
