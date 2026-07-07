#include "EditorBuiltinCommandProvider.h"

#include "EditorCommandContext.h"
#include "EditorCommandPalette.h"
#include "EditorContext.h"

namespace editor {

EditorBuiltinCommandProvider::EditorBuiltinCommandProvider(EditorBuiltinCommandProviderInput input)
    : input_(std::move(input)) {
}

void EditorBuiltinCommandProvider::RegisterCommands(EditorContext& context) const {
    if (context.commands == nullptr || context.commandContext == nullptr) {
        return;
    }

    EditorCommandRegistry& registry = *context.commands;
    const EditorCommandContext& commandContext = *context.commandContext;
    EditorCommandPalette* commandPalette = context.commandPalette;
    const EditorBuiltinCommandProviderInput input = input_;

    registry.Register(
        EditorCommand{
            "editor.commandPalette",
            "Command Palette",
            "Editor",
            "Ctrl+P",
            [&commandContext]() { return commandContext.developerToolsVisible; },
            [&commandContext]() {
                return commandContext.developerToolsVisible ? std::string() : std::string("Developer tools are hidden.");
            },
            [commandPalette]() {
                if (commandPalette == nullptr) {
                    return EditorCommandResult{false, "Command palette is unavailable."};
                }
                commandPalette->Open();
                return EditorCommandResult{true, "Opened command palette."};
            }});

    registry.Register(
        EditorCommand{
            "editor.undo",
            "Undo",
            "Editor",
            "Ctrl+Z",
            [&commandContext]() { return commandContext.canUndo; },
            [&commandContext]() {
                return commandContext.canUndo ? std::string() : std::string("No editor transaction can be undone.");
            },
            [input]() {
                return input.undo ? input.undo() : EditorCommandResult{false, "Undo callback is unavailable."};
            }});

    registry.Register(
        EditorCommand{
            "editor.redo",
            "Redo",
            "Editor",
            "Ctrl+Y",
            [&commandContext]() { return commandContext.canRedo; },
            [&commandContext]() {
                return commandContext.canRedo ? std::string() : std::string("No editor transaction can be redone.");
            },
            [input]() {
                return input.redo ? input.redo() : EditorCommandResult{false, "Redo callback is unavailable."};
            }});
}

} // namespace editor
