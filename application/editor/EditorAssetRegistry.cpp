#include "EditorAssetRegistry.h"

#include <algorithm>
#include <utility>

namespace editor {

void EditorAssetRegistry::Clear() {
    if (records_.empty()) {
        return;
    }
    records_.clear();
    Touch();
}

bool EditorAssetRegistry::Register(EditorAssetRecord record) {
    if (record.id.empty() || record.kind == EditorAssetKind::Unknown) {
        return false;
    }

    auto it = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& existing) {
            return existing.kind == record.kind && existing.id == record.id;
        });
    if (it != records_.end()) {
        *it = std::move(record);
        Touch();
        return true;
    }

    records_.push_back(std::move(record));
    Touch();
    return true;
}

const EditorAssetRecord* EditorAssetRegistry::Find(EditorAssetKind kind, std::string_view id) const {
    const auto it = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& record) {
            return record.kind == kind && record.id == id;
        });
    return it != records_.end() ? &*it : nullptr;
}

std::vector<const EditorAssetRecord*> EditorAssetRegistry::List(EditorAssetKind kind) const {
    std::vector<const EditorAssetRecord*> results;
    for (const EditorAssetRecord& record : records_) {
        if (record.kind == kind) {
            results.push_back(&record);
        }
    }
    return results;
}

std::size_t EditorAssetRegistry::Count(EditorAssetKind kind) const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& record) {
            return record.kind == kind;
        }));
}

void EditorAssetRegistry::Touch() {
    ++revision_;
}

const char* ToString(EditorAssetKind kind) {
    switch (kind) {
    case EditorAssetKind::Unknown:
        return "Unknown";
    case EditorAssetKind::Mesh:
        return "Mesh";
    case EditorAssetKind::Effect:
        return "Effect";
    case EditorAssetKind::Course:
        return "Course";
    case EditorAssetKind::Texture:
        return "Texture";
    case EditorAssetKind::Audio:
        return "Audio";
    }
    return "Unknown";
}

} // namespace editor
