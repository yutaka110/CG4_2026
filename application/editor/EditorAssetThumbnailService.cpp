#include "EditorAssetThumbnailService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <utility>

namespace editor {
namespace {

std::string ToLower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

std::string ExtensionOf(const std::string& path) {
    return ToLower(std::filesystem::path(path).extension().string());
}

const char* KindFallbackLabel(EditorAssetKind kind) {
    switch (kind) {
    case EditorAssetKind::Mesh:
        return "MESH";
    case EditorAssetKind::Effect:
        return "FX";
    case EditorAssetKind::Course:
        return "CRS";
    case EditorAssetKind::Texture:
        return "TEX";
    case EditorAssetKind::Audio:
        return "AUD";
    case EditorAssetKind::Unknown:
        break;
    }
    return "ASSET";
}

bool IsSupportedTextureExtension(const std::string& extension) {
    return extension == ".png" ||
        extension == ".bmp" ||
        extension == ".dds" ||
        extension == ".jpg" ||
        extension == ".jpeg" ||
        extension == ".tga";
}

EditorAssetThumbnailEntry BuildEntry(
    const EditorAssetRecord& record,
    const EditorAssetThumbnailEntry* previous) {
    EditorAssetThumbnailEntry entry{};
    entry.key = BuildEditorAssetThumbnailKey(record);
    entry.kind = record.kind;
    entry.assetId = record.id;
    entry.label = KindFallbackLabel(record.kind);
    entry.sourceTimestamp = record.sourceTimestamp;
    entry.generation = previous != nullptr ? previous->generation : 1;

    if (record.missing) {
        entry.status = EditorAssetThumbnailStatus::Missing;
        entry.fallbackIcon = true;
        entry.detail = "Source file is missing; using missing-asset fallback.";
    } else if (record.kind == EditorAssetKind::Texture) {
        const std::string extension = ExtensionOf(record.sourcePath);
        if (IsSupportedTextureExtension(extension)) {
            entry.status = EditorAssetThumbnailStatus::Ready;
            entry.fallbackIcon = false;
            entry.detail = "Texture preview cache is ready.";
        } else {
            entry.status = EditorAssetThumbnailStatus::Failed;
            entry.fallbackIcon = true;
            entry.detail = extension.empty()
                ? "Texture preview cannot be generated without a source extension."
                : "Texture preview does not support source extension " + extension + ".";
        }
    } else if (EditorAssetKindSupportsThumbnailPreview(record.kind)) {
        entry.status = EditorAssetThumbnailStatus::Pending;
        entry.fallbackIcon = true;
        entry.detail = "Preview provider is registered but has not produced a thumbnail yet.";
    } else {
        entry.status = EditorAssetThumbnailStatus::Unsupported;
        entry.fallbackIcon = true;
        entry.detail = std::string(ToString(record.kind)) + " uses the kind icon fallback.";
    }

    if (previous != nullptr &&
        (previous->sourceTimestamp != entry.sourceTimestamp ||
            previous->status != entry.status ||
            previous->detail != entry.detail)) {
        entry.generation = previous->generation + 1;
    }
    return entry;
}

} // namespace

void EditorAssetThumbnailService::Clear() {
    if (entries_.empty() && registryRevision_ == 0) {
        return;
    }
    entries_.clear();
    registryRevision_ = 0;
    Touch();
}

void EditorAssetThumbnailService::Sync(const EditorAssetRegistry& registry) {
    std::vector<EditorAssetThumbnailEntry> updated;
    updated.reserve(registry.Records().size());

    bool changed = registryRevision_ != registry.Revision() || entries_.size() != registry.Records().size();
    for (const EditorAssetRecord& record : registry.Records()) {
        const std::string key = BuildEditorAssetThumbnailKey(record);
        const EditorAssetThumbnailEntry* previous = Find(key);
        EditorAssetThumbnailEntry entry = BuildEntry(record, previous);
        if (previous == nullptr ||
            previous->kind != entry.kind ||
            previous->assetId != entry.assetId ||
            previous->label != entry.label ||
            previous->detail != entry.detail ||
            previous->sourceTimestamp != entry.sourceTimestamp ||
            previous->generation != entry.generation ||
            previous->status != entry.status ||
            previous->fallbackIcon != entry.fallbackIcon) {
            changed = true;
        }
        updated.push_back(std::move(entry));
    }

    entries_ = std::move(updated);
    registryRevision_ = registry.Revision();
    if (changed) {
        Touch();
    }
}

EditorAssetThumbnailEntry EditorAssetThumbnailService::Resolve(
    const EditorAssetRecord& record) const {
    const std::string key = BuildEditorAssetThumbnailKey(record);
    if (const EditorAssetThumbnailEntry* entry = Find(key)) {
        return *entry;
    }
    return BuildEntry(record, nullptr);
}

const EditorAssetThumbnailEntry* EditorAssetThumbnailService::Find(std::string_view key) const {
    const auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const EditorAssetThumbnailEntry& entry) {
            return entry.key == key;
        });
    return it != entries_.end() ? &*it : nullptr;
}

std::size_t EditorAssetThumbnailService::Count(EditorAssetThumbnailStatus status) const {
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(),
        entries_.end(),
        [&](const EditorAssetThumbnailEntry& entry) {
            return entry.status == status;
        }));
}

void EditorAssetThumbnailService::Touch() {
    ++revision_;
}

const char* ToString(EditorAssetThumbnailStatus status) {
    switch (status) {
    case EditorAssetThumbnailStatus::Missing:
        return "Missing";
    case EditorAssetThumbnailStatus::Unsupported:
        return "Unsupported";
    case EditorAssetThumbnailStatus::Pending:
        return "Pending";
    case EditorAssetThumbnailStatus::Ready:
        return "Ready";
    case EditorAssetThumbnailStatus::Failed:
        return "Failed";
    }
    return "Unknown";
}

std::string BuildEditorAssetThumbnailKey(const EditorAssetRecord& record) {
    if (!record.thumbnailKey.empty()) {
        return record.thumbnailKey;
    }
    std::string key = "thumb:";
    key += ToString(record.kind);
    key += ':';
    if (!record.guid.empty()) {
        key += record.guid;
    } else if (!record.logicalPath.empty()) {
        key += record.logicalPath;
    } else if (!record.sourcePath.empty()) {
        key += record.sourcePath;
    } else {
        key += record.id;
    }
    return key;
}

bool EditorAssetKindSupportsThumbnailPreview(EditorAssetKind kind) {
    return kind == EditorAssetKind::Texture;
}

} // namespace editor
