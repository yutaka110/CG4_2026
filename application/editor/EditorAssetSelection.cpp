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
        primary_.thumbnailKey == handle.thumbnailKey &&
        primary_.sourceTimestamp == handle.sourceTimestamp &&
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
    handle.thumbnailKey = record.thumbnailKey;
    handle.sourceTimestamp = record.sourceTimestamp;
    handle.registryRevision = registryRevision;
    handle.referenceable = record.referenceable;
    handle.missing = record.missing;
    handle.hasMetadata = record.hasMetadata;
    handle.provisionalGuid = record.provisionalGuid;
    return handle;
}

EditorAssetHandleResolveResult ResolveEditorAssetHandle(
    const EditorAssetRegistry& registry,
    const EditorAssetHandle& handle) {
    EditorAssetHandleResolveResult result{};
    if (!handle.Valid()) {
        return result;
    }

    result.record = registry.Find(handle.kind, handle.id);
    result.found = result.record != nullptr;
    result.revisionCurrent = handle.registryRevision == registry.Revision();
    result.identityCurrent =
        result.record != nullptr &&
        (handle.guid.empty() || result.record->guid == handle.guid) &&
        result.record->logicalPath == handle.logicalPath &&
        result.record->sourcePath == handle.sourcePath &&
        result.record->metadataPath == handle.metadataPath &&
        result.record->thumbnailKey == handle.thumbnailKey &&
        result.record->sourceTimestamp == handle.sourceTimestamp &&
        result.record->referenceable == handle.referenceable &&
        result.record->missing == handle.missing &&
        result.record->hasMetadata == handle.hasMetadata &&
        result.record->provisionalGuid == handle.provisionalGuid;
    return result;
}

bool IsEditorAssetHandleCurrent(
    const EditorAssetRegistry& registry,
    const EditorAssetHandle& handle) {
    return ResolveEditorAssetHandle(registry, handle).Current();
}

EditorAssetHandle RefreshEditorAssetHandle(
    const EditorAssetRegistry& registry,
    const EditorAssetHandle& handle) {
    const EditorAssetHandleResolveResult resolved =
        ResolveEditorAssetHandle(registry, handle);
    return resolved.record != nullptr
        ? MakeEditorAssetHandle(*resolved.record, registry.Revision())
        : EditorAssetHandle{};
}

} // namespace editor
