#include "EditorWorldMutation.h"

namespace editor {

std::size_t EditorWorldMutationState::EstimatedBytes() const noexcept {
    return sizeof(EditorWorldMutationState) + providerId.capacity() + 1 +
        document.assetGuid.capacity() + document.type.capacity() + 2 +
        (payload != nullptr ? payload->EstimatedBytes() : 0);
}

EditorWorldObjectCapability CapabilityForEditorWorldMutation(
    EditorWorldMutationKind kind) noexcept {
    switch (kind) {
    case EditorWorldMutationKind::Create: return EditorWorldObjectCapability::Create;
    case EditorWorldMutationKind::Rename: return EditorWorldObjectCapability::Rename;
    case EditorWorldMutationKind::Reparent: return EditorWorldObjectCapability::Reparent;
    case EditorWorldMutationKind::Duplicate: return EditorWorldObjectCapability::Duplicate;
    case EditorWorldMutationKind::Delete: return EditorWorldObjectCapability::Delete;
    case EditorWorldMutationKind::SetVisibility: return EditorWorldObjectCapability::Visibility;
    case EditorWorldMutationKind::SetLocked: return EditorWorldObjectCapability::Lock;
    case EditorWorldMutationKind::AddComponent:
    case EditorWorldMutationKind::RemoveComponent:
        return EditorWorldObjectCapability::Components;
    case EditorWorldMutationKind::SetComponentProperty:
        return EditorWorldObjectCapability::Transform;
    }
    return EditorWorldObjectCapability::None;
}

const char* ToString(EditorWorldMutationKind kind) noexcept {
    switch (kind) {
    case EditorWorldMutationKind::Create: return "Create";
    case EditorWorldMutationKind::Rename: return "Rename";
    case EditorWorldMutationKind::Reparent: return "Reparent";
    case EditorWorldMutationKind::Duplicate: return "Duplicate";
    case EditorWorldMutationKind::Delete: return "Delete";
    case EditorWorldMutationKind::SetVisibility: return "Visibility";
    case EditorWorldMutationKind::SetLocked: return "Lock";
    case EditorWorldMutationKind::AddComponent: return "Add Component";
    case EditorWorldMutationKind::RemoveComponent: return "Remove Component";
    case EditorWorldMutationKind::SetComponentProperty: return "Set Component Property";
    }
    return "World Mutation";
}

} // namespace editor
