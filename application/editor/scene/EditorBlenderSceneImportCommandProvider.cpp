#include "EditorBlenderSceneImportCommandProvider.h"

#include "EditorBlenderSceneImportTransaction.h"
#include "../EditorCommandContext.h"
#include "../EditorCommandRegistry.h"
#include "../EditorContext.h"
#include "../EditorToolRegistration.h"
#include "../documents/EditorDocumentManager.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace editor {
namespace {

const EditorDocumentRecord* ActiveSceneDocument(
    const EditorDocumentManager* documents) {
    const EditorDocumentRecord* active =
        documents != nullptr ? documents->Active() : nullptr;
    return active != nullptr && active->open &&
            active->id.type == EditorDocumentTypes::Scene
        ? active
        : nullptr;
}

std::string ImportDisabledReason(
    const EditorCommandContext& commandContext,
    const EditorDocumentManager* documents,
    const EditorBlenderSceneImportWorkflow* workflow,
    bool hasFileSelector,
    bool reimport) {
    if (!commandContext.canMutateAuthoring) {
        return "Authoring changes are unavailable during the current editor state.";
    }
    const EditorDocumentRecord* active = ActiveSceneDocument(documents);
    if (active == nullptr) {
        return "Open and activate a Scene document first.";
    }
    if (workflow == nullptr) {
        return "Blender Scene Import service is unavailable.";
    }
    if (!hasFileSelector) {
        return "Blender Level JSON file selection is unavailable.";
    }
    if (reimport && !workflow->HasImportedScene(active->id)) {
        return "The active Scene has no Blender import provenance to reimport.";
    }
    return {};
}

EditorCommandResult ExecuteImport(
    EditorBlenderSceneImportMode mode,
    EditorDocumentManager* documents,
    EditorBlenderSceneImportWorkflow* workflow,
    const std::function<std::optional<std::filesystem::path>()>& selectFile) {
    const EditorDocumentRecord* active = ActiveSceneDocument(documents);
    if (active == nullptr || workflow == nullptr || !selectFile) {
        return EditorCommandResult{
            false,
            "Blender Scene Import command dependencies are unavailable."};
    }
    const EditorDocumentId document = active->id;
    const std::optional<std::filesystem::path> sourcePath = selectFile();
    if (!sourcePath.has_value()) {
        return EditorCommandResult{
            true,
            "Blender Level JSON selection cancelled."};
    }
    const EditorBlenderSceneImportTransactionResult result =
        workflow->Execute(mode, document, *sourcePath);
    return EditorCommandResult{
        result.succeeded,
        result.message,
        result.warning};
}

} // namespace

void EditorBlenderSceneImportCommandProvider::RegisterCommands(
    EditorContext& context) const {
    if (context.commands == nullptr || context.commandContext == nullptr) {
        return;
    }

    const EditorCommandContext& commandContext = *context.commandContext;
    EditorDocumentManager* documents = context.documentManager;
    EditorBlenderSceneImportWorkflow* workflow =
        context.blenderSceneImportWorkflow;
    const auto selectFile = context.selectBlenderLevelJsonFile;
    const bool hasFileSelector = static_cast<bool>(selectFile);

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "scene.blender.import",
            "Import Blender Level JSON...",
            "Scene",
            "",
            [&commandContext, documents, workflow, hasFileSelector]() {
                return ImportDisabledReason(
                    commandContext,
                    documents,
                    workflow,
                    hasFileSelector,
                    false).empty();
            },
            [&commandContext, documents, workflow, hasFileSelector]() {
                return ImportDisabledReason(
                    commandContext,
                    documents,
                    workflow,
                    hasFileSelector,
                    false);
            },
            [documents, workflow, selectFile]() {
                return ExecuteImport(
                    EditorBlenderSceneImportMode::Import,
                    documents,
                    workflow,
                    selectFile);
            }});

    RegisterEditorToolCommand(
        context,
        EditorCommand{
            "scene.blender.reimport",
            "Reimport Blender Level JSON...",
            "Scene",
            "",
            [&commandContext, documents, workflow, hasFileSelector]() {
                return ImportDisabledReason(
                    commandContext,
                    documents,
                    workflow,
                    hasFileSelector,
                    true).empty();
            },
            [&commandContext, documents, workflow, hasFileSelector]() {
                return ImportDisabledReason(
                    commandContext,
                    documents,
                    workflow,
                    hasFileSelector,
                    true);
            },
            [documents, workflow, selectFile]() {
                return ExecuteImport(
                    EditorBlenderSceneImportMode::Reimport,
                    documents,
                    workflow,
                    selectFile);
            }});
}

} // namespace editor
