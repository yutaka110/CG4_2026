#include "EditorBlenderSceneImportTransaction.h"

#include "../EditorAssetRegistry.h"
#include "../EditorDirtyStateService.h"
#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"
#include "../documents/EditorDocumentManager.h"
#include "../documents/EditorSceneDocumentProvider.h"
#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace editor {
namespace {

std::size_t SceneBytes(const EditorScene& scene) noexcept {
    std::size_t bytes =
        sizeof(EditorScene) +
        scene.entities.capacity() * sizeof(EditorSceneEntity) +
        scene.prefabInstances.capacity() * sizeof(EditorScenePrefabInstance);
    for (const EditorSceneEntity& entity : scene.entities) {
        bytes += entity.guid.capacity() + entity.parentGuid.capacity() +
            entity.name.capacity() + 3;
        bytes += entity.components.capacity() * sizeof(EditorSceneComponent);
        for (const EditorSceneComponent& component : entity.components) {
            bytes += component.typeId.capacity() + 1;
            bytes +=
                component.properties.capacity() * sizeof(EditorSceneProperty) +
                component.references.capacity() * sizeof(EditorSceneObjectReference);
            for (const EditorSceneProperty& property : component.properties) {
                bytes += property.name.capacity() + property.value.capacity() + 2;
            }
            for (const EditorSceneObjectReference& reference : component.references) {
                bytes += reference.property.capacity() +
                    reference.entityGuid.capacity() +
                    reference.assetGuid.capacity() + 3;
            }
        }
    }
    for (const EditorScenePrefabInstance& instance : scene.prefabInstances) {
        bytes += instance.instanceGuid.capacity() +
            instance.prefabAssetGuid.capacity() +
            instance.rootEntityGuid.capacity() + 3;
        bytes +=
            instance.bindings.capacity() * sizeof(EditorScenePrefabEntityBinding) +
            instance.overrides.capacity() * sizeof(EditorScenePrefabOverride);
        for (const EditorScenePrefabEntityBinding& binding : instance.bindings) {
            bytes += binding.sourceEntityGuid.capacity() +
                binding.instanceEntityGuid.capacity() + 2;
        }
        for (const EditorScenePrefabOverride& value : instance.overrides) {
            bytes += value.id.capacity() + value.sourceEntityGuid.capacity() +
                value.instanceEntityGuid.capacity() +
                value.componentTypeId.capacity() +
                value.propertyName.capacity() +
                value.inheritedValue.capacity() +
                value.instanceValue.capacity() + 7;
        }
    }
    return bytes;
}

void MarkSceneDirty(
    const EditorDocumentId& document,
    std::string_view reason,
    EditorDocumentManager* documents,
    EditorDirtyStateService* dirtyState) {
    if (documents != nullptr) {
        documents->MarkDirty(document, reason);
    }
    if (dirtyState != nullptr) {
        dirtyState->MarkDirty(
            EditorDirtyDomain::Unknown,
            "scene.blender-import:" + document.assetGuid,
            "Blender Scene Import",
            std::string(reason),
            dirtyState->Revision() + 1);
    }
}

std::string ImportSummary(const EditorBlenderSceneImportResult& result) {
    std::string message = result.message;
    message += " Created " + std::to_string(result.createdObjectCount);
    message += ", updated " + std::to_string(result.updatedObjectCount);
    message += ", removed " + std::to_string(result.removedObjectCount) + ".";
    if (result.unresolvedMeshCount > 0) {
        message += " Unresolved meshes: " +
            std::to_string(result.unresolvedMeshCount) + ".";
    }
    if (!result.warnings.empty()) {
        message += " Warnings: " + std::to_string(result.warnings.size()) + ".";
    }
    return message;
}

} // namespace

EditorBlenderSceneImportUndoCommand::EditorBlenderSceneImportUndoCommand(
    EditorDocumentId document,
    EditorScene before,
    EditorScene after)
    : document_(std::move(document)),
      before_(std::move(before)),
      after_(std::move(after)) {}

EditorUndoResult EditorBlenderSceneImportUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    IEditorExecutionService* untyped =
        context.Find(kEditorBlenderSceneImportExecutionServiceId);
    auto* service =
        dynamic_cast<IEditorBlenderSceneImportExecutionService*>(untyped);
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Blender Scene Import execution service is unavailable.");
    }
    return service->ApplyBlenderSceneSnapshot(
        document_,
        mode == EditorTransactionApplyMode::Undo ? before_ : after_,
        mode == EditorTransactionApplyMode::Undo
            ? "Blender Scene Import undo applied."
            : "Blender Scene Import redo applied.");
}

std::size_t
EditorBlenderSceneImportUndoCommand::EstimatedBytes() const noexcept {
    return sizeof(*this) +
        document_.assetGuid.capacity() +
        document_.type.capacity() + 2 +
        SceneBytes(before_) +
        SceneBytes(after_);
}

EditorBlenderSceneImportExecutionService::
EditorBlenderSceneImportExecutionService(
    EditorSceneDocumentProvider& scenes,
    EditorDocumentManager* documents,
    EditorDirtyStateService* dirtyState,
    SceneChangedCallback sceneChanged)
    : scenes_(scenes),
      documents_(documents),
      dirtyState_(dirtyState),
      sceneChanged_(std::move(sceneChanged)) {}

EditorUndoResult
EditorBlenderSceneImportExecutionService::ApplyBlenderSceneSnapshot(
    const EditorDocumentId& document,
    const EditorScene& snapshot,
    std::string_view reason) {
    EditorScene* live = scenes_.Scene(document);
    if (live == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::NotAvailable,
            "Blender Scene Import target Scene document is unavailable.");
    }
    EditorScene replacement = snapshot;
    const EditorSceneValidationReport validation = replacement.Validate();
    if (!validation.Succeeded()) {
        return EditorUndoResult::Failure(
            EditorErrorCode::ApplyFailed,
            "Blender Scene Import transaction snapshot is invalid: " +
                validation.errors.front());
    }
    replacement.revision = live->revision + 1;
    *live = std::move(replacement);
    MarkSceneDirty(document, reason, documents_, dirtyState_);
    if (sceneChanged_) sceneChanged_(document, reason);
    return EditorUndoResult::Success(std::string(reason));
}

EditorBlenderSceneImportWorkflow::EditorBlenderSceneImportWorkflow(
    EditorSceneDocumentProvider& scenes,
    EditorTransactionStack& transactions,
    const EditorAssetRegistry* assets,
    EditorDocumentManager* documents,
    EditorDirtyStateService* dirtyState,
    SceneChangedCallback sceneChanged)
    : scenes_(scenes),
      transactions_(transactions),
      assets_(assets),
      documents_(documents),
      dirtyState_(dirtyState),
      sceneChanged_(std::move(sceneChanged)) {}

EditorBlenderSceneImportTransactionResult
EditorBlenderSceneImportWorkflow::Execute(
    EditorBlenderSceneImportMode mode,
    const EditorDocumentId& document,
    const std::filesystem::path& sourcePath,
    const EditorBlenderSceneImportOptions& options) {
    EditorBlenderSceneImportTransactionResult result{};
    if (!document.IsValid() || document.type != EditorDocumentTypes::Scene) {
        result.message = "An active Scene document is required.";
        return result;
    }
    if (documents_ != nullptr) {
        const EditorDocumentRecord* record = documents_->Find(document);
        if (record == nullptr || !record->open) {
            result.message = "The target Scene document is not open.";
            return result;
        }
    }
    EditorScene* live = scenes_.Scene(document);
    if (live == nullptr) {
        result.message = "The active Scene live model is unavailable.";
        return result;
    }

    EditorScene before = *live;
    EditorScene after = before;
    EditorBlenderSceneImportService importer(assets_);
    switch (mode) {
    case EditorBlenderSceneImportMode::Import:
        result.importResult =
            importer.ImportFile(sourcePath, after, options);
        break;
    case EditorBlenderSceneImportMode::Reimport:
        result.importResult =
            importer.ReimportFile(sourcePath, after, options);
        break;
    case EditorBlenderSceneImportMode::ImportOrReimport:
        result.importResult =
            importer.ImportOrReimportFile(sourcePath, after, options);
        break;
    }
    if (!result.importResult.succeeded) {
        result.message = result.importResult.message;
        return result;
    }

    const std::string label = result.importResult.reimported
        ? "Reimport Blender Scene"
        : "Import Blender Scene";
    auto command = std::make_shared<EditorBlenderSceneImportUndoCommand>(
        document, before, after);
    EditorObjectHandle target{};
    target.domain = EditorDomainId::SceneEntity;
    target.stableId = BuildEditorWorldStableId(
        document, "scene", result.importResult.rootEntityGuid);
    target.displayName = result.importResult.reimported
        ? "Blender Scene Reimport"
        : "Blender Scene Import";

    EditorError transactionError;
    if (!transactions_.CanPushCommand(
            label, target, command, &transactionError)) {
        result.message = transactionError.message.empty()
            ? "Blender Scene Import transaction cannot be recorded."
            : transactionError.message;
        return result;
    }

    *live = after;
    if (!transactions_.PushCommand(
            label, std::move(target), command, &transactionError)) {
        *live = std::move(before);
        result.message = transactionError.message.empty()
            ? "Blender Scene Import transaction registration failed."
            : transactionError.message;
        return result;
    }

    const std::string reason = result.importResult.reimported
        ? "Blender Scene was reimported."
        : "Blender Scene was imported.";
    MarkSceneDirty(document, reason, documents_, dirtyState_);
    if (sceneChanged_) sceneChanged_(document, reason);
    result.succeeded = true;
    result.warning = !result.importResult.warnings.empty();
    result.message = ImportSummary(result.importResult);
    return result;
}

bool EditorBlenderSceneImportWorkflow::HasImportedScene(
    const EditorDocumentId& document) const {
    const EditorScene* scene = scenes_.Scene(document);
    if (scene == nullptr) return false;
    return std::any_of(
        scene->entities.begin(),
        scene->entities.end(),
        [&](const EditorSceneEntity& entity) {
            return scene->FindComponent(
                       entity,
                       kEditorBlenderSceneSourceComponentType) != nullptr;
        });
}

} // namespace editor
