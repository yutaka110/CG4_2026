#pragma once

#include "EditorCommandProvider.h"
#include "EditorCommandRegistry.h"

#include <functional>

namespace editor {

struct EditorBuiltinCommandProviderInput {
    std::function<EditorCommandResult()> undo;
    std::function<EditorCommandResult()> redo;
};

class EditorBuiltinCommandProvider final : public EditorCommandProvider {
public:
    explicit EditorBuiltinCommandProvider(EditorBuiltinCommandProviderInput input);

    void RegisterCommands(EditorContext& context) const override;

private:
    EditorBuiltinCommandProviderInput input_;
};

} // namespace editor
