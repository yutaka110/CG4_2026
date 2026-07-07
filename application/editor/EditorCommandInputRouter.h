#pragma once

#include "EditorCommandRegistry.h"

#include <string>

namespace editor {

struct EditorContext;

struct EditorCommandInputRouterOptions {
    bool enabled = true;
    bool ignoreWhenTextInputActive = true;
};

struct EditorCommandInputDispatch {
    bool handled = false;
    std::string commandId;
    EditorCommandResult result;
};

class EditorCommandInputRouter {
public:
    EditorCommandInputDispatch Dispatch(
        EditorContext& context,
        const EditorCommandInputRouterOptions& options = {});
    EditorCommandInputDispatch Dispatch(
        EditorCommandRegistry& registry,
        const EditorCommandInputRouterOptions& options = {});

    const EditorCommandInputDispatch& LastDispatch() const { return lastDispatch_; }

private:
    EditorCommandInputDispatch lastDispatch_;
};

} // namespace editor
