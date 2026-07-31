#pragma once

#include "../scene/EditorScene.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace editor {

// Provenance retained by an editable Scene copy. This identifies the immutable
// Production Mesh source snapshot from which the editable Geometry was cloned;
// it is intentionally independent from the hash of Geometry after authoring.
inline constexpr std::string_view kEditorEditableSourceAssetGuidProperty =
    "editableSourceAssetGuid";
inline constexpr std::string_view kEditorEditableSourceGeometryHashProperty =
    "editableSourceGeometryHash";

struct EditorProductionMeshEditableSourceIdentity {
    std::string assetGuid;
    uint64_t sourceGeometryHash = 0;

    bool Validate(std::string* errorMessage = nullptr) const;
};

enum class EditorProductionMeshEditableSourceMetadataState : uint32_t {
    Absent = 0,
    Valid,
    Invalid,
};

struct EditorProductionMeshEditableSourceMetadataReadResult {
    EditorProductionMeshEditableSourceMetadataState state =
        EditorProductionMeshEditableSourceMetadataState::Absent;
    EditorProductionMeshEditableSourceIdentity identity{};
    std::string message;

    bool Succeeded() const noexcept {
        return state !=
            EditorProductionMeshEditableSourceMetadataState::Invalid;
    }
    bool HasIdentity() const noexcept {
        return state ==
            EditorProductionMeshEditableSourceMetadataState::Valid;
    }
};

EditorProductionMeshEditableSourceMetadataReadResult
ReadEditorProductionMeshEditableSourceMetadata(
    const EditorSceneComponent& component);

// Replaces the GUID/hash pair as one logical mutation. The caller owns Scene
// Touch and transaction boundaries.
bool WriteEditorProductionMeshEditableSourceMetadata(
    EditorSceneComponent& component,
    const EditorProductionMeshEditableSourceIdentity& identity,
    std::string* errorMessage = nullptr);

bool ClearEditorProductionMeshEditableSourceMetadata(
    EditorSceneComponent& component,
    std::string* errorMessage = nullptr);

} // namespace editor
