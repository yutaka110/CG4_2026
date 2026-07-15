#include "EditorGeometryEditCommand.h"

#include "../core/EditorExecutionContext.h"
#include "../scene/EditorScene.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

std::optional<std::string> PropertyValue(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(
        component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component.properties.end()
        ? std::optional<std::string>{}
        : std::optional<std::string>{found->value};
}

void ApplyProperty(
    EditorSceneComponent& component,
    std::string_view name,
    const std::optional<std::string>& value) {
    const auto found = std::find_if(
        component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& property) { return property.name == name; });
    if (!value.has_value()) {
        if (found != component.properties.end()) component.properties.erase(found);
        return;
    }
    if (found == component.properties.end()) {
        component.properties.push_back({std::string(name), *value});
    } else {
        found->value = *value;
    }
}

std::size_t StateBytes(const EditorGeometryPropertyState& state) {
    return sizeof(state) +
        (state.geometry.has_value() ? state.geometry->capacity() : 0) +
        (state.collision.has_value() ? state.collision->capacity() : 0);
}

} // namespace

void EditorGeometryExecutionService::Bind(
    EditorDocumentId document,
    EditorScene* scene,
    ChangedCallback onChanged) {
    document_ = std::move(document);
    scene_ = scene;
    onChanged_ = std::move(onChanged);
}

void EditorGeometryExecutionService::Clear() {
    document_ = {};
    scene_ = nullptr;
    onChanged_ = {};
}

EditorUndoResult EditorGeometryExecutionService::ApplyGeometryState(
    std::string_view documentKey,
    std::string_view entityGuid,
    const EditorGeometryPropertyState& state) {
    if (scene_ == nullptr || !document_.IsValid() || document_.Key() != documentKey) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Geometry command targets an unavailable Scene document.");
    }
    EditorSceneEntity* entity = scene_->FindEntity(entityGuid);
    EditorSceneComponent* component = entity != nullptr
        ? scene_->FindComponent(*entity, kEditorMeshRendererComponentType)
        : nullptr;
    if (component == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::InvalidArgument,
            "Geometry command target has no Mesh Renderer component.");
    }
    if (state.geometry.has_value()) {
        EditorGeometryMesh decoded;
        std::string error;
        if (!EditorGeometryMesh::Deserialize(*state.geometry, decoded, &error)) {
            return EditorUndoResult::Failure(
                EditorErrorCode::InvalidArgument,
                error.empty() ? "Geometry command contains invalid mesh data." : error);
        }
    }
    if (state.collision.has_value()) {
        EditorGeneratedCollision decoded{};
        if (!DeserializeEditorGeneratedCollision(*state.collision, decoded)) {
            return EditorUndoResult::Failure(
                EditorErrorCode::InvalidArgument,
                "Geometry command contains invalid generated collision data.");
        }
    }
    const bool changed =
        PropertyValue(*component, kEditorEditableGeometryProperty) != state.geometry ||
        PropertyValue(*component, kEditorGeneratedCollisionProperty) != state.collision;
    if (!changed) return EditorUndoResult::Success("Geometry state is already current.");
    ApplyProperty(*component, kEditorEditableGeometryProperty, state.geometry);
    ApplyProperty(*component, kEditorGeneratedCollisionProperty, state.collision);
    scene_->Touch();
    if (onChanged_) onChanged_(entityGuid);
    return EditorUndoResult::Success("Editable Geometry state applied.");
}

EditorGeometryEditUndoCommand::EditorGeometryEditUndoCommand(
    std::string documentKey,
    std::string entityGuid,
    EditorGeometryPropertyState before,
    EditorGeometryPropertyState after)
    : documentKey_(std::move(documentKey)),
      entityGuid_(std::move(entityGuid)),
      before_(std::move(before)),
      after_(std::move(after)) {
}

EditorUndoResult EditorGeometryEditUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    IEditorExecutionService* untyped =
        context.Find(IEditorGeometryExecutionService::kServiceId);
    auto* service = dynamic_cast<IEditorGeometryExecutionService*>(untyped);
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Geometry execution service is unavailable.");
    }
    return service->ApplyGeometryState(
        documentKey_, entityGuid_,
        mode == EditorTransactionApplyMode::Redo ? after_ : before_);
}

std::size_t EditorGeometryEditUndoCommand::EstimatedBytes() const noexcept {
    return sizeof(*this) + documentKey_.capacity() + entityGuid_.capacity() +
        StateBytes(before_) + StateBytes(after_);
}

} // namespace editor
