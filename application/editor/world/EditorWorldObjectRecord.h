#pragma once

#include "../EditorSelection.h"
#include "../documents/EditorDocumentId.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace editor {

enum class EditorWorldObjectCapability : uint32_t {
    None = 0,
    Rename = 1u << 0,
    Reparent = 1u << 1,
    Duplicate = 1u << 2,
    Delete = 1u << 3,
    Visibility = 1u << 4,
    Lock = 1u << 5,
    Transform = 1u << 6,
    Create = 1u << 7,
    Components = 1u << 8,
};

using EditorWorldObjectCapabilities = uint32_t;

constexpr EditorWorldObjectCapabilities operator|(
    EditorWorldObjectCapability lhs,
    EditorWorldObjectCapability rhs) noexcept {
    return static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs);
}

constexpr EditorWorldObjectCapabilities operator|(
    EditorWorldObjectCapabilities lhs,
    EditorWorldObjectCapability rhs) noexcept {
    return lhs | static_cast<uint32_t>(rhs);
}

constexpr bool HasEditorWorldCapability(
    EditorWorldObjectCapabilities value,
    EditorWorldObjectCapability capability) noexcept {
    return (value & static_cast<uint32_t>(capability)) != 0;
}

struct EditorWorldObjectId {
    EditorDocumentId document;
    std::string providerId;
    std::string objectGuid;

    bool IsValid() const noexcept {
        return document.IsValid() && !providerId.empty() && !objectGuid.empty();
    }
    std::string StableId() const;
};

struct EditorWorldObjectRecord {
    EditorObjectHandle handle;
    EditorObjectHandle parent;
    EditorDocumentId document;
    std::string providerId;
    std::string objectGuid;
    std::string displayName;
    std::string typeName;
    std::string sortKey;
    EditorWorldObjectCapabilities capabilities = 0;
    bool visible = true;
    bool locked = false;
    bool runtimeOnly = false;
    bool missing = false;
    bool virtualNode = false;
};

std::string BuildEditorWorldStableId(
    const EditorDocumentId& document,
    std::string_view providerId,
    std::string_view objectGuid);
std::string MakeDeterministicEditorWorldGuid(
    std::string_view nameSpace,
    std::string_view kind,
    std::string_view legacyKey,
    uint64_t ordinal);
std::string GenerateEditorWorldGuid();

} // namespace editor
