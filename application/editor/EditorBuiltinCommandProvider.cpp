#include "EditorBuiltinCommandProvider.h"

#include "EditorCommandContext.h"
#include "EditorCommandPalette.h"
#include "EditorContext.h"
#include "EditorPlaySessionState.h"
#include "EditorSaveApplyPolicy.h"

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
    EditorPlaySessionState* playSession = context.playSession;
    const EditorBuiltinCommandProviderInput input = input_;
    const EditorSaveApplyPolicyInput saveApplyPolicyInput{
        commandContext.developerToolsVisible,
        false,
        false,
        false,
        context.dirtyState,
        context.validationReport,
        playSession};

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
            "editor.play",
            "Play",
            "Editor",
            "",
            [saveApplyPolicyInput]() {
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::BeginPlaySession,
                    saveApplyPolicyInput).allowed;
            },
            [saveApplyPolicyInput]() {
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::BeginPlaySession,
                    saveApplyPolicyInput).reason;
            },
            [playSession, input, saveApplyPolicyInput]() {
                if (playSession == nullptr) {
                    return EditorCommandResult{false, "Play session state is unavailable."};
                }
                const EditorSaveApplyDecision decision =
                    EvaluateEditorSaveApplyPolicy(
                        EditorSaveApplyAction::BeginPlaySession,
                        saveApplyPolicyInput);
                if (!decision.allowed) {
                    return EditorCommandResult{false, decision.reason};
                }
                if (input.beginPlaySession) {
                    EditorCommandResult result = input.beginPlaySession(EditorPlaySessionMode::Playing);
                    if (result.succeeded && !decision.warning.empty()) {
                        result.message += " " + decision.warning;
                        result.warning = true;
                    }
                    return result;
                }
                playSession->Play();
                return EditorCommandResult{
                    true,
                    decision.warning.empty()
                        ? std::string("Entered Play mode boundary.")
                        : std::string("Entered Play mode boundary. ") + decision.warning,
                    !decision.warning.empty()};
            }});

    registry.Register(
        EditorCommand{
            "editor.simulate",
            "Simulate",
            "Editor",
            "",
            [saveApplyPolicyInput]() {
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::BeginPlaySession,
                    saveApplyPolicyInput).allowed;
            },
            [saveApplyPolicyInput]() {
                return EvaluateEditorSaveApplyPolicy(
                    EditorSaveApplyAction::BeginPlaySession,
                    saveApplyPolicyInput).reason;
            },
            [playSession, input, saveApplyPolicyInput]() {
                if (playSession == nullptr) {
                    return EditorCommandResult{false, "Play session state is unavailable."};
                }
                const EditorSaveApplyDecision decision =
                    EvaluateEditorSaveApplyPolicy(
                        EditorSaveApplyAction::BeginPlaySession,
                        saveApplyPolicyInput);
                if (!decision.allowed) {
                    return EditorCommandResult{false, decision.reason};
                }
                if (input.beginPlaySession) {
                    EditorCommandResult result = input.beginPlaySession(EditorPlaySessionMode::Simulating);
                    if (result.succeeded && !decision.warning.empty()) {
                        result.message += " " + decision.warning;
                        result.warning = true;
                    }
                    return result;
                }
                playSession->Simulate();
                return EditorCommandResult{
                    true,
                    decision.warning.empty()
                        ? std::string("Entered Simulate mode boundary.")
                        : std::string("Entered Simulate mode boundary. ") + decision.warning,
                    !decision.warning.empty()};
            }});

    registry.Register(
        EditorCommand{
            "editor.stop",
            "Stop",
            "Editor",
            "",
            [playSession, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    playSession != nullptr &&
                    playSession->IsActive();
            },
            [playSession, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (playSession == nullptr) {
                    return std::string("Play session state is unavailable.");
                }
                return playSession->IsActive()
                    ? std::string()
                    : std::string("No Play or Simulate session is active.");
            },
            [playSession, input]() {
                if (playSession == nullptr) {
                    return EditorCommandResult{false, "Play session state is unavailable."};
                }
                if (input.stopPlaySession) {
                    return input.stopPlaySession();
                }
                playSession->Stop();
                return EditorCommandResult{true, "Stopped Play/Simulate boundary."};
            }});

    registry.Register(
        EditorCommand{
            "editor.undo",
            "Undo",
            "Editor",
            "Ctrl+Z",
            [&commandContext]() {
                return commandContext.canMutateAuthoring && commandContext.canUndo;
            },
            [&commandContext]() {
                if (!commandContext.canMutateAuthoring) {
                    return std::string("Authoring is locked during Play/Sim.");
                }
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
            [&commandContext]() {
                return commandContext.canMutateAuthoring && commandContext.canRedo;
            },
            [&commandContext]() {
                if (!commandContext.canMutateAuthoring) {
                    return std::string("Authoring is locked during Play/Sim.");
                }
                return commandContext.canRedo ? std::string() : std::string("No editor transaction can be redone.");
            },
            [input]() {
                return input.redo ? input.redo() : EditorCommandResult{false, "Redo callback is unavailable."};
            }});
}

} // namespace editor
