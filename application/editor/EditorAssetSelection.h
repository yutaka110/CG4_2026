#pragma once

#include <cstdint>
#include <string>

#include "EditorAssetRegistry.h"

namespace editor {

struct EditorAssetHandle {
    EditorAssetKind kind = EditorAssetKind::Unknown;
    std::string id;
    std::string displayName;
    std::string sourcePath;
    uint32_t registryRevision = 0;
    bool referenceable = false;

    bool Valid() const { return kind != EditorAssetKind::Unknown && !id.empty(); }
    bool SameAsset(const EditorAssetHandle& other) const {
        return kind == other.kind && id == other.id;
    }
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

} // namespace editor
