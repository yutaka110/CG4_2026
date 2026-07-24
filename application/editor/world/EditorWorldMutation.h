#pragma once

#include "EditorWorldObjectRecord.h"
#include "../scene/EditorGimmickEventBindingMutation.h"
#include "../scene/EditorGimmickEventSequenceMutation.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace editor {

enum class EditorWorldMutationKind {
    Create,
    Rename,
    Reparent,
    Duplicate,
    Delete,
    SetVisibility,
    SetLocked,
    SetRuntimeEnabled,
    AddComponent,
    RemoveComponent,
    SetComponentEnabled,
    SetComponentProperty,
    SetComponentAssetReference,
    SetComponentEntityReference,
    SetGimmickDefinition,
    SetGimmickParameter,
    MutateGimmickEventBinding,
    MutateGimmickEventSequence,
};

class IEditorWorldMutationPayload {
public:
    virtual ~IEditorWorldMutationPayload() = default;
    virtual std::size_t EstimatedBytes() const noexcept = 0;
};

struct EditorWorldMutationState {
    std::string providerId;
    EditorDocumentId document;
    std::shared_ptr<const IEditorWorldMutationPayload> payload;

    bool IsValid() const noexcept {
        return !providerId.empty() && document.IsValid() && payload != nullptr;
    }
    std::size_t EstimatedBytes() const noexcept;
};

struct EditorWorldMutationRequest {
    struct InitialProperty {
        std::string componentType;
        std::string property;
        std::string value;
    };

    struct Placement {
        std::string stableGuid;
        std::string name;
        std::vector<InitialProperty> initialProperties;
    };

    EditorWorldMutationKind kind = EditorWorldMutationKind::Rename;
    std::vector<EditorObjectHandle> targets;
    EditorObjectHandle newParent;
    std::string name;
    std::string assetGuid;
    std::string entityGuid;
    std::string assetType;
    std::string componentType;
    std::string property;
    std::string propertyValue;
    std::vector<Placement> placements;
    EditorGimmickEventBindingMutation eventBindingMutation;
    EditorGimmickEventSequenceMutation eventSequenceMutation;
    bool value = false;
};

struct EditorWorldProviderMutationRequest {
    EditorWorldMutationKind kind = EditorWorldMutationKind::Rename;
    std::vector<EditorWorldObjectId> targets;
    EditorWorldObjectId newParent;
    std::string name;
    std::string assetGuid;
    std::string entityGuid;
    std::string assetType;
    std::string componentType;
    std::string property;
    std::string propertyValue;
    std::vector<EditorWorldMutationRequest::Placement> placements;
    EditorGimmickEventBindingMutation eventBindingMutation;
    EditorGimmickEventSequenceMutation eventSequenceMutation;
    bool value = false;
};

struct EditorWorldMutationPlan {
    EditorWorldMutationState before;
    EditorWorldMutationState after;
    std::vector<EditorWorldObjectId> resultingSelection;
    std::string label;
};

struct EditorWorldMutationResult {
    bool succeeded = false;
    bool changed = false;
    EditorDocumentId document;
    std::vector<EditorObjectHandle> resultingSelection;
    std::string message;
};

EditorWorldObjectCapability CapabilityForEditorWorldMutation(
    EditorWorldMutationKind kind) noexcept;
const char* ToString(EditorWorldMutationKind kind) noexcept;

} // namespace editor
