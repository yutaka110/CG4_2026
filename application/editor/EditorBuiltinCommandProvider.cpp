#include "EditorBuiltinCommandProvider.h"

#include "EditorCommandContext.h"
#include "EditorCommandPalette.h"
#include "EditorContext.h"
#include "EditorLayoutPersistenceService.h"
#include "EditorPlaySessionState.h"
#include "EditorSaveApplyPolicy.h"
#include "EditorToolRegistration.h"
#include "documents/EditorDocumentManager.h"
#include "documents/EditorDocumentSaveService.h"

namespace editor {

EditorBuiltinCommandProvider::EditorBuiltinCommandProvider(EditorBuiltinCommandProviderInput input)
    : input_(std::move(input)) {
}

void EditorBuiltinCommandProvider::RegisterCommands(EditorContext& context) const {
    if (context.commands == nullptr || context.commandContext == nullptr) {
        return;
    }

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

    RegisterEditorToolCommand(
        context,
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

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.saveAll",
            "Save All",
            "File",
            "Ctrl+Shift+S",
            [manager = context.documentManager,
             saveService = context.documentSaveService,
             &commandContext]() {
                return commandContext.canMutateAuthoring && manager != nullptr &&
                    saveService != nullptr && manager->DirtyCount() > 0;
            },
            [manager = context.documentManager,
             saveService = context.documentSaveService,
             &commandContext]() {
                if (!commandContext.canMutateAuthoring) {
                    return std::string("Authoring is locked during Play/Sim.");
                }
                if (manager == nullptr || saveService == nullptr) {
                    return std::string("Document Save All service is unavailable.");
                }
                return manager->DirtyCount() > 0
                    ? std::string()
                    : std::string("No dirty documents require saving.");
            },
            [saveService = context.documentSaveService]() {
                if (saveService == nullptr) {
                    return EditorCommandResult{false, "Document Save All service is unavailable."};
                }
                const EditorDocumentSaveResult result = saveService->SaveAll();
                return EditorCommandResult{result.succeeded, result.message};
            }});

    RegisterEditorToolCommand(
        context,
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
                return EditorCommandResult{false, "Play session lifecycle service is unavailable."};
            }});

    RegisterEditorToolCommand(
        context,
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
                return EditorCommandResult{false, "Play session lifecycle service is unavailable."};
            }});

    RegisterEditorToolCommand(
        context,
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
                return EditorCommandResult{false, "Play session lifecycle service is unavailable."};
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.applyRuntimeChanges",
            "Apply Runtime Changes",
            "Editor",
            "",
            [playSession, input, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    playSession != nullptr &&
                    playSession->IsActive() &&
                    static_cast<bool>(input.applyRuntimeChanges);
            },
            [playSession, input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (playSession == nullptr) {
                    return std::string("Play session state is unavailable.");
                }
                if (!playSession->IsActive()) {
                    return std::string("Runtime changes can only be applied during Play/Sim.");
                }
                return input.applyRuntimeChanges
                    ? std::string()
                    : std::string("Runtime authoring apply service is unavailable.");
            },
            [input]() {
                return input.applyRuntimeChanges
                    ? input.applyRuntimeChanges()
                    : EditorCommandResult{false, "Runtime authoring apply service is unavailable."};
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.pauseRuntime",
            "Pause Runtime",
            "Editor",
            "",
            [playSession, input, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    playSession != nullptr &&
                    playSession->IsActive() &&
                    !playSession->RuntimePaused() &&
                    static_cast<bool>(input.pauseRuntime);
            },
            [playSession, input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (playSession == nullptr) {
                    return std::string("Play session state is unavailable.");
                }
                if (!playSession->IsActive()) {
                    return std::string("Runtime can only be paused during Play/Sim.");
                }
                if (playSession->RuntimePaused()) {
                    return std::string("Runtime is already paused.");
                }
                return input.pauseRuntime
                    ? std::string()
                    : std::string("Runtime control service is unavailable.");
            },
            [input]() {
                return input.pauseRuntime
                    ? input.pauseRuntime()
                    : EditorCommandResult{false, "Runtime control service is unavailable."};
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.resumeRuntime",
            "Resume Runtime",
            "Editor",
            "",
            [playSession, input, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    playSession != nullptr &&
                    playSession->IsActive() &&
                    playSession->RuntimePaused() &&
                    static_cast<bool>(input.resumeRuntime);
            },
            [playSession, input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (playSession == nullptr) {
                    return std::string("Play session state is unavailable.");
                }
                if (!playSession->IsActive()) {
                    return std::string("Runtime can only be resumed during Play/Sim.");
                }
                if (!playSession->RuntimePaused()) {
                    return std::string("Runtime is already live.");
                }
                return input.resumeRuntime
                    ? std::string()
                    : std::string("Runtime control service is unavailable.");
            },
            [input]() {
                return input.resumeRuntime
                    ? input.resumeRuntime()
                    : EditorCommandResult{false, "Runtime control service is unavailable."};
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.stepRuntime",
            "Step Runtime",
            "Editor",
            "",
            [playSession, input, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    playSession != nullptr &&
                    playSession->IsActive() &&
                    static_cast<bool>(input.stepRuntime);
            },
            [playSession, input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (playSession == nullptr) {
                    return std::string("Play session state is unavailable.");
                }
                if (!playSession->IsActive()) {
                    return std::string("Runtime can only be stepped during Play/Sim.");
                }
                return input.stepRuntime
                    ? std::string()
                    : std::string("Runtime control service is unavailable.");
            },
            [input]() {
                return input.stepRuntime
                    ? input.stepRuntime()
                    : EditorCommandResult{false, "Runtime control service is unavailable."};
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.resetRuntime",
            "Reset Runtime",
            "Editor",
            "",
            [playSession, input, &commandContext]() {
                return commandContext.developerToolsVisible &&
                    playSession != nullptr &&
                    playSession->IsActive() &&
                    static_cast<bool>(input.resetRuntime);
            },
            [playSession, input, &commandContext]() {
                if (!commandContext.developerToolsVisible) {
                    return std::string("Developer tools are hidden.");
                }
                if (playSession == nullptr) {
                    return std::string("Play session state is unavailable.");
                }
                if (!playSession->IsActive()) {
                    return std::string("Runtime can only be reset during Play/Sim.");
                }
                return input.resetRuntime
                    ? std::string()
                    : std::string("Runtime control service is unavailable.");
            },
            [input]() {
                return input.resetRuntime
                    ? input.resetRuntime()
                    : EditorCommandResult{false, "Runtime control service is unavailable."};
            }});

    RegisterEditorToolCommand(
        context,
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

    RegisterEditorToolCommand(
        context,
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

    const auto transformEnabled = [&commandContext]() {
        return commandContext.developerToolsVisible && commandContext.canMutateAuthoring;
    };
    const auto transformDisabledReason = [&commandContext]() {
        if (!commandContext.developerToolsVisible) {
            return std::string("Developer tools are hidden.");
        }
        return commandContext.canMutateAuthoring
            ? std::string()
            : std::string("Authoring is locked during Play/Sim.");
    };
    const auto registerTransformMode =
        [&](const char* id,
            const char* label,
            const char* shortcut,
            EditorTransformGizmoMode mode) {
            RegisterEditorToolCommand(
                context,
                EditorCommand{
                    id,
                    label,
                    "Edit",
                    shortcut,
                    transformEnabled,
                    transformDisabledReason,
                    [input, mode]() {
                        return input.setTransformMode
                            ? input.setTransformMode(mode)
                            : EditorCommandResult{false, "Transform mode service is unavailable."};
                    }});
        };
    registerTransformMode(
        "editor.transform.translate", "Translate", "W", EditorTransformGizmoMode::Translate);
    registerTransformMode(
        "editor.transform.rotate", "Rotate", "E", EditorTransformGizmoMode::Rotate);
    registerTransformMode(
        "editor.transform.scale", "Scale", "R", EditorTransformGizmoMode::Scale);

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.transform.toggleSpace",
            "Toggle World/Local",
            "Edit",
            "",
            transformEnabled,
            transformDisabledReason,
            [input]() {
                return input.toggleTransformSpace
                    ? input.toggleTransformSpace()
                    : EditorCommandResult{false, "Transform space service is unavailable."};
            }});
    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.transform.toggleSnap",
            "Toggle Transform Snap",
            "Edit",
            "",
            transformEnabled,
            transformDisabledReason,
            [input]() {
                return input.toggleTransformSnap
                    ? input.toggleTransformSnap()
                    : EditorCommandResult{false, "Transform snap service is unavailable."};
            }});

    EditorLayoutPersistenceService* layoutPersistence = context.layoutPersistence;
    const auto registerWindowArea =
        [&](const char* id,
            const char* label,
            EditorBottomDockGroup group,
            bool developer) {
            RegisterEditorToolCommand(
                context,
                EditorCommand{
                    id,
                    label,
                    "Window",
                    "",
                    [layoutPersistence, &commandContext]() {
                        return commandContext.developerToolsVisible && layoutPersistence != nullptr;
                    },
                    [layoutPersistence, &commandContext]() {
                        if (!commandContext.developerToolsVisible) {
                            return std::string("Developer tools are hidden.");
                        }
                        return layoutPersistence != nullptr
                            ? std::string()
                            : std::string("Editor layout persistence is unavailable.");
                    },
                    [layoutPersistence, group, developer]() {
                        if (layoutPersistence == nullptr) {
                            return EditorCommandResult{false, "Editor layout persistence is unavailable."};
                        }
                        if (developer) {
                            layoutPersistence->SetBottomDockDeveloperPanelsVisible(true);
                        }
                        layoutPersistence->SetActiveBottomDockGroup(group);
                        return EditorCommandResult{
                            true, std::string("Opened Bottom Dock ") + ToString(group) + "."};
                    }});
        };
    registerWindowArea(
        "window.bottomDock.output", "Bottom Dock: Output", EditorBottomDockGroup::Output, false);
    registerWindowArea(
        "window.bottomDock.profiling", "Bottom Dock: Profiling", EditorBottomDockGroup::Profiling, false);
    registerWindowArea(
        "window.bottomDock.authoring", "Bottom Dock: Authoring", EditorBottomDockGroup::Authoring, false);
    registerWindowArea(
        "window.bottomDock.developer", "Bottom Dock: Developer", EditorBottomDockGroup::Developer, true);
    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "window.details",
            "Focus Details",
            "Window",
            "",
            [layoutPersistence, &commandContext]() {
                return commandContext.developerToolsVisible && layoutPersistence != nullptr;
            },
            [layoutPersistence]() {
                return layoutPersistence != nullptr
                    ? std::string()
                    : std::string("Editor layout persistence is unavailable.");
            },
            [layoutPersistence]() {
                if (layoutPersistence == nullptr) {
                    return EditorCommandResult{false, "Editor layout persistence is unavailable."};
                }
                layoutPersistence->SetPanelVisible("editor.details", true);
                layoutPersistence->SetActivePanelFromUser(
                    EditorPanelHostArea::RightInspector, "editor.details");
                return EditorCommandResult{true, "Focused Details."};
            }});
    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "editor.help.evolutionDesign",
            "Editor Evolution Design",
            "Help",
            "",
            []() { return true; },
            []() { return std::string(); },
            []() {
                return EditorCommandResult{
                    true, "Editor design: docs/EditorEvolutionDesign.md"};
            }});
}

} // namespace editor
