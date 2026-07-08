#include "EditorAssetSelection.h"

#include <utility>

namespace editor {

void EditorAssetSelection::Clear() {
    if (!primary_.Valid()) {
        return;
    }
    primary_ = {};
    Touch();
}

void EditorAssetSelection::SetPrimary(EditorAssetHandle handle) {
    if (primary_.SameAsset(handle) &&
        primary_.registryRevision == handle.registryRevision &&
        primary_.guid == handle.guid &&
        primary_.logicalPath == handle.logicalPath &&
        primary_.displayName == handle.displayName &&
        primary_.sourcePath == handle.sourcePath &&
        primary_.metadataPath == handle.metadataPath &&
        primary_.referenceable == handle.referenceable &&
        primary_.missing == handle.missing &&
        primary_.hasMetadata == handle.hasMetadata &&
        primary_.provisionalGuid == handle.provisionalGuid) {
        return;
    }

    primary_ = std::move(handle);
    Touch();
}

const EditorAssetHandle* EditorAssetSelection::Primary() const {
    return primary_.Valid() ? &primary_ : nullptr;
}

void EditorAssetSelection::Touch() {
    ++revision_;
}

EditorAssetHandle MakeEditorAssetHandle(
    const EditorAssetRecord& record,
    uint32_t registryRevision) {
    EditorAssetHandle handle{};
    handle.kind = record.kind;
    handle.id = record.id;
    handle.guid = record.guid;
    handle.logicalPath = record.logicalPath;
    handle.displayName = record.displayName;
    handle.sourcePath = record.sourcePath;
    handle.metadataPath = record.metadataPath;
    handle.registryRevision = registryRevision;
    handle.referenceable = record.referenceable;
    handle.missing = record.missing;
    handle.hasMetadata = record.hasMetadata;
    handle.provisionalGuid = record.provisionalGuid;
    return handle;
}

} // namespace editor
