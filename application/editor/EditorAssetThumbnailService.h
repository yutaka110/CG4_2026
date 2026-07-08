#pragma once

#include "EditorAssetRegistry.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorAssetThumbnailStatus {
    Missing,
    Unsupported,
    Pending,
    Ready,
    Failed,
};

struct EditorAssetThumbnailEntry {
    std::string key;
    EditorAssetKind kind = EditorAssetKind::Unknown;
    std::string assetId;
    std::string label;
    std::string detail;
    uint64_t sourceTimestamp = 0;
    uint32_t generation = 0;
    EditorAssetThumbnailStatus status = EditorAssetThumbnailStatus::Pending;
    bool fallbackIcon = false;
};

class EditorAssetThumbnailService {
public:
    void Clear();
    void Sync(const EditorAssetRegistry& registry);

    EditorAssetThumbnailEntry Resolve(const EditorAssetRecord& record) const;
    const EditorAssetThumbnailEntry* Find(std::string_view key) const;

    uint32_t Revision() const { return revision_; }
    uint32_t RegistryRevision() const { return registryRevision_; }
    std::size_t Count() const { return entries_.size(); }
    std::size_t Count(EditorAssetThumbnailStatus status) const;
    const std::vector<EditorAssetThumbnailEntry>& Entries() const { return entries_; }

private:
    void Touch();

    std::vector<EditorAssetThumbnailEntry> entries_;
    uint32_t revision_ = 0;
    uint32_t registryRevision_ = 0;
};

const char* ToString(EditorAssetThumbnailStatus status);
std::string BuildEditorAssetThumbnailKey(const EditorAssetRecord& record);
bool EditorAssetKindSupportsThumbnailPreview(EditorAssetKind kind);

} // namespace editor
