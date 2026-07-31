#include "EditorProductionMeshEditableSourceMetadata.h"

#include "../EditorAssetRegistry.h"

#include <algorithm>
#include <charconv>
#include <system_error>
#include <utility>
#include <vector>

namespace editor {
namespace {

bool ParseNonZeroHash(std::string_view value, uint64_t& output) noexcept {
    if (value.empty()) return false;
    uint64_t parsed = 0;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr != end || parsed == 0) {
        return false;
    }
    output = parsed;
    return true;
}

std::size_t PropertyCount(
    const EditorSceneComponent& component,
    std::string_view name,
    const EditorSceneProperty** first) {
    std::size_t count = 0;
    for (const EditorSceneProperty& property : component.properties) {
        if (property.name != name) continue;
        if (count == 0 && first != nullptr) *first = &property;
        ++count;
    }
    return count;
}

void RemoveMetadataProperties(
    std::vector<EditorSceneProperty>& properties) {
    std::erase_if(
        properties,
        [](const EditorSceneProperty& property) {
            return property.name ==
                    kEditorEditableSourceAssetGuidProperty ||
                property.name ==
                    kEditorEditableSourceGeometryHashProperty;
        });
}

bool RequireMeshRenderer(
    const EditorSceneComponent& component,
    std::string* errorMessage) {
    if (component.typeId == kEditorMeshRendererComponentType) return true;
    if (errorMessage != nullptr) {
        *errorMessage =
            "Editable source metadata belongs to a Mesh Renderer Component.";
    }
    return false;
}

} // namespace

bool EditorProductionMeshEditableSourceIdentity::Validate(
    std::string* errorMessage) const {
    if (!IsDurableEditorAssetGuid(assetGuid)) {
        if (errorMessage != nullptr) {
            *errorMessage =
                "Editable source Asset GUID must be a durable Asset identity.";
        }
        return false;
    }
    if (sourceGeometryHash == 0) {
        if (errorMessage != nullptr) {
            *errorMessage =
                "Editable source Geometry hash must be non-zero.";
        }
        return false;
    }
    return true;
}

EditorProductionMeshEditableSourceMetadataReadResult
ReadEditorProductionMeshEditableSourceMetadata(
    const EditorSceneComponent& component) {
    EditorProductionMeshEditableSourceMetadataReadResult result{};
    if (component.typeId != kEditorMeshRendererComponentType) {
        result.state =
            EditorProductionMeshEditableSourceMetadataState::Invalid;
        result.message =
            "Editable source metadata belongs to a Mesh Renderer Component.";
        return result;
    }

    const EditorSceneProperty* guidProperty = nullptr;
    const EditorSceneProperty* hashProperty = nullptr;
    const std::size_t guidCount = PropertyCount(
        component,
        kEditorEditableSourceAssetGuidProperty,
        &guidProperty);
    const std::size_t hashCount = PropertyCount(
        component,
        kEditorEditableSourceGeometryHashProperty,
        &hashProperty);
    if (guidCount == 0 && hashCount == 0) {
        return result;
    }
    if (guidCount != 1 || hashCount != 1) {
        result.state =
            EditorProductionMeshEditableSourceMetadataState::Invalid;
        result.message =
            "Editable source GUID and Geometry hash must exist exactly once "
            "as a pair.";
        return result;
    }

    result.identity.assetGuid = guidProperty->value;
    if (!ParseNonZeroHash(
            hashProperty->value,
            result.identity.sourceGeometryHash)) {
        result.state =
            EditorProductionMeshEditableSourceMetadataState::Invalid;
        result.message =
            "Editable source Geometry hash is not a non-zero 64-bit value.";
        return result;
    }
    if (!result.identity.Validate(&result.message)) {
        result.state =
            EditorProductionMeshEditableSourceMetadataState::Invalid;
        return result;
    }
    result.state = EditorProductionMeshEditableSourceMetadataState::Valid;
    return result;
}

bool WriteEditorProductionMeshEditableSourceMetadata(
    EditorSceneComponent& component,
    const EditorProductionMeshEditableSourceIdentity& identity,
    std::string* errorMessage) {
    if (!RequireMeshRenderer(component, errorMessage) ||
        !identity.Validate(errorMessage)) {
        return false;
    }

    // Construct the replacement before swapping so a failed allocation leaves
    // the Component unchanged.
    std::vector<EditorSceneProperty> replacement = component.properties;
    RemoveMetadataProperties(replacement);
    replacement.push_back({
        std::string(kEditorEditableSourceAssetGuidProperty),
        identity.assetGuid});
    replacement.push_back({
        std::string(kEditorEditableSourceGeometryHashProperty),
        std::to_string(identity.sourceGeometryHash)});
    component.properties.swap(replacement);
    return true;
}

bool ClearEditorProductionMeshEditableSourceMetadata(
    EditorSceneComponent& component,
    std::string* errorMessage) {
    if (!RequireMeshRenderer(component, errorMessage)) return false;
    std::vector<EditorSceneProperty> replacement = component.properties;
    RemoveMetadataProperties(replacement);
    component.properties.swap(replacement);
    return true;
}

} // namespace editor
