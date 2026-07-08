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
        primary_.displayName == handle.displayName &&
        primary_.sourcePath == handle.sourcePath &&
        primary_.referenceable == handle.referenceable) {
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
    handle.displayName = record.displayName;
    handle.sourcePath = record.sourcePath;
    handle.registryRevision = registryRevision;
    handle.referenceable = record.referenceable;
    return handle;
}

} // namespace editor
