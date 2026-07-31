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
    case EditorWorldMutationKind::SetRuntimeEnabled:
        return EditorWorldObjectCapability::RuntimeActivation;
    case EditorWorldMutationKind::AddComponent:
    case EditorWorldMutationKind::RemoveComponent:
    case EditorWorldMutationKind::SetComponentEnabled:
    case EditorWorldMutationKind::SetComponentAssetReference:
    case EditorWorldMutationKind::SetComponentEntityReference:
    case EditorWorldMutationKind::SetupPatrol:
    case EditorWorldMutationKind::SetGimmickDefinition:
    case EditorWorldMutationKind::SetGimmickParameter:
    case EditorWorldMutationKind::MutateGimmickEventBinding:
    case EditorWorldMutationKind::MutateGimmickEventSequence:
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
    case EditorWorldMutationKind::SetRuntimeEnabled: return "Runtime Enabled";
    case EditorWorldMutationKind::AddComponent: return "Add Component";
    case EditorWorldMutationKind::RemoveComponent: return "Remove Component";
    case EditorWorldMutationKind::SetComponentEnabled: return "Set Component Enabled";
    case EditorWorldMutationKind::SetComponentProperty: return "Set Component Property";
    case EditorWorldMutationKind::SetComponentAssetReference: return "Set Component Asset Reference";
    case EditorWorldMutationKind::SetComponentEntityReference: return "Set Component Entity Reference";
    case EditorWorldMutationKind::SetupPatrol: return "Set Up Patrol";
    case EditorWorldMutationKind::SetGimmickDefinition: return "Set Gimmick Definition";
    case EditorWorldMutationKind::SetGimmickParameter: return "Set Gimmick Parameter";
    case EditorWorldMutationKind::MutateGimmickEventBinding:
        return "Edit Gimmick Event Binding";
    case EditorWorldMutationKind::MutateGimmickEventSequence:
        return "Edit Gimmick Event Sequence";
    }
    return "World Mutation";
}

} // namespace editor
