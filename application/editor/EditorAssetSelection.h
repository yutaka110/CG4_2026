#pragma once

#include <cstdint>
#include <array>
#include <string>

#include "EditorAssetRegistry.h"

namespace editor {

inline constexpr const char* kEditorAssetDragDropPayloadId = "EDITOR_ASSET";

struct EditorAssetDragDropPayload {
    EditorAssetKind kind = EditorAssetKind::Unknown;
    std::array<char, 65> guid{};
    std::array<char, 256> id{};
    std::array<char, 128> displayName{};
};

struct EditorAssetHandle {
    EditorAssetKind kind = EditorAssetKind::Unknown;
    std::string id;
    std::string guid;
    std::string logicalPath;
    std::string displayName;
    std::string sourcePath;
    std::string metadataPath;
    std::string thumbnailKey;
    uint64_t sourceTimestamp = 0;
    uint32_t registryRevision = 0;
    bool referenceable = false;
    bool missing = false;
    bool hasMetadata = false;
    bool provisionalGuid = false;

    bool Valid() const { return kind != EditorAssetKind::Unknown && !id.empty(); }
    bool SameAsset(const EditorAssetHandle& other) const {
        return kind == other.kind && id == other.id;
    }
};

struct EditorAssetHandleResolveResult {
    const EditorAssetRecord* record = nullptr;
    bool found = false;
    bool revisionCurrent = false;
    bool identityCurrent = false;

    bool Current() const { return found && revisionCurrent && identityCurrent; }
};

class EditorAssetSelection {
public:
    void Clear();
    void SetPrimary(EditorAssetHandle handle);

    const EditorAssetHandle* Primary() const;
    bool HasPrimary() const { return primary_.Valid(); }
    uint32_t Revision() const { return revision_; }

private:
    void Touch();

    EditorAssetHandle primary_{};
    uint32_t revision_ = 0;
};

EditorAssetHandle MakeEditorAssetHandle(
    const EditorAssetRecord& record,
    uint32_t registryRevision);
EditorAssetHandleResolveResult ResolveEditorAssetHandle(
    const EditorAssetRegistry& registry,
    const EditorAssetHandle& handle);
bool IsEditorAssetHandleCurrent(
    const EditorAssetRegistry& registry,
    const EditorAssetHandle& handle);
EditorAssetHandle RefreshEditorAssetHandle(
    const EditorAssetRegistry& registry,
    const EditorAssetHandle& handle);

} // namespace editor
