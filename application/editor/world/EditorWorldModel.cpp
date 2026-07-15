#include "EditorWorldModel.h"

#include <algorithm>

namespace editor {

EditorWorldModelRefreshResult EditorWorldModel::Refresh() {
    EditorWorldModelRefreshResult result{};
    std::vector<EditorWorldObjectRecord> next;
    std::vector<std::string> diagnostics;
    for (IEditorWorldObjectProvider* provider : registry_.Providers()) {
        if (provider == nullptr) continue;
        EditorWorldProviderEnumeration enumeration{};
        std::string error;
        if (!provider->Enumerate(&enumeration, &error)) {
            result.message = error.empty()
                ? "World provider enumeration failed: " + std::string(provider->ProviderId())
                : error;
            return result;
        }
        diagnostics.insert(
            diagnostics.end(), enumeration.diagnostics.begin(), enumeration.diagnostics.end());
        for (EditorWorldObjectRecord& record : enumeration.objects) {
            if (record.providerId.empty()) record.providerId = std::string(provider->ProviderId());
            next.push_back(std::move(record));
        }
    }

    std::stable_sort(
        next.begin(), next.end(),
        [](const EditorWorldObjectRecord& lhs, const EditorWorldObjectRecord& rhs) {
            if (lhs.providerId != rhs.providerId) return lhs.providerId < rhs.providerId;
            if (lhs.sortKey != rhs.sortKey) return lhs.sortKey < rhs.sortKey;
            return lhs.handle.stableId < rhs.handle.stableId;
        });

    objects_ = std::move(next);
    diagnostics_ = std::move(diagnostics);
    index_.clear();
    childrenIndex_.clear();
    index_.reserve(objects_.size());
    for (std::size_t index = 0; index < objects_.size(); ++index) {
        EditorWorldObjectRecord& record = objects_[index];
        if (record.handle.stableId.empty()) {
            record.missing = true;
            diagnostics_.push_back("World object has an empty stable id: " + record.displayName);
            continue;
        }
        const std::string key = IndexKey(record.handle);
        if (!index_.emplace(key, index).second) {
            record.missing = true;
            diagnostics_.push_back("Duplicate world object handle: " + record.handle.stableId);
        }
        childrenIndex_[record.parent.stableId].push_back(index);
    }
    ValidateHierarchy();
    const uint64_t nextFingerprint = Fingerprint();
    result.changed = nextFingerprint != fingerprint_;
    if (result.changed) {
        fingerprint_ = nextFingerprint;
        ++revision_;
    }
    result.succeeded = true;
    result.objectCount = objects_.size();
    result.missingCount = static_cast<std::size_t>(std::count_if(
        objects_.begin(), objects_.end(),
        [](const EditorWorldObjectRecord& value) { return value.missing; }));
    result.diagnostics = diagnostics_;
    result.message = diagnostics_.empty()
        ? "Editor World Model refreshed."
        : "Editor World Model refreshed with diagnostics.";
    return result;
}

const EditorWorldObjectRecord* EditorWorldModel::Resolve(
    const EditorObjectHandle& handle) const {
    const auto found = index_.find(IndexKey(handle));
    if (found == index_.end() || found->second >= objects_.size()) return nullptr;
    return &objects_[found->second];
}

const EditorWorldObjectRecord* EditorWorldModel::FindByStableId(
    std::string_view stableId) const {
    const auto found = index_.find(std::string(stableId));
    if (found == index_.end() || found->second >= objects_.size()) return nullptr;
    return &objects_[found->second];
}

const EditorWorldObjectRecord* EditorWorldModel::FindByDomainIndex(
    EditorDomainId domain,
    uint64_t localIndex) const {
    for (const EditorWorldObjectRecord& record : objects_) {
        if (record.handle.domain == domain && record.handle.localIndex == localIndex &&
            !record.virtualNode) {
            return &record;
        }
    }
    return nullptr;
}

const EditorWorldObjectRecord* EditorWorldModel::FindByObjectGuid(
    std::string_view providerId,
    std::string_view objectGuid) const {
    for (const EditorWorldObjectRecord& record : objects_) {
        if (record.providerId == providerId && record.objectGuid == objectGuid) return &record;
    }
    return nullptr;
}

std::vector<const EditorWorldObjectRecord*> EditorWorldModel::ChildrenOf(
    const EditorObjectHandle& parent) const {
    std::vector<const EditorWorldObjectRecord*> children;
    const auto found = childrenIndex_.find(parent.stableId);
    if (found == childrenIndex_.end()) return children;
    children.reserve(found->second.size());
    for (const std::size_t index : found->second) {
        if (index < objects_.size()) children.push_back(&objects_[index]);
    }
    return children;
}

std::vector<const EditorWorldObjectRecord*> EditorWorldModel::ForDocument(
    const EditorDocumentId& document) const {
    std::vector<const EditorWorldObjectRecord*> result;
    for (const EditorWorldObjectRecord& record : objects_) {
        if (record.document == document) result.push_back(&record);
    }
    return result;
}

std::string EditorWorldModel::IndexKey(const EditorObjectHandle& handle) {
    if (!handle.stableId.empty()) return handle.stableId;
    return std::to_string(static_cast<uint32_t>(handle.domain)) + "|" +
        std::to_string(handle.localIndex) + "|" + std::to_string(handle.generation);
}

bool EditorWorldModel::ValidateHierarchy() {
    bool valid = true;
    const auto parentOf = [this](const EditorWorldObjectRecord* record) {
        if (record == nullptr || record->parent.stableId.empty()) {
            return static_cast<const EditorWorldObjectRecord*>(nullptr);
        }
        return Resolve(record->parent);
    };
    for (EditorWorldObjectRecord& record : objects_) {
        if (record.parent.stableId.empty()) continue;
        if (parentOf(&record) == nullptr) {
            record.missing = true;
            diagnostics_.push_back("Missing parent for world object: " + record.handle.stableId);
            valid = false;
            continue;
        }

        // Floyd's cycle detection avoids allocating an unordered_set for every
        // object. Large scenes normally have shallow hierarchies, so this keeps
        // validation linear and allocation-free while retaining cycle safety.
        const EditorWorldObjectRecord* slow = &record;
        const EditorWorldObjectRecord* fast = &record;
        while (slow != nullptr && fast != nullptr) {
            slow = parentOf(slow);
            fast = parentOf(parentOf(fast));
            if (slow != nullptr && slow == fast) {
                record.missing = true;
                diagnostics_.push_back("World hierarchy cycle detected: " + record.handle.stableId);
                valid = false;
                break;
            }
        }
    }
    return valid;
}

uint64_t EditorWorldModel::Fingerprint() const {
    uint64_t hash = 1469598103934665603ull;
    auto append = [&](std::string_view value) {
        for (const unsigned char byte : value) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
    };
    for (const EditorWorldObjectRecord& record : objects_) {
        append(record.handle.stableId);
        append(record.parent.stableId);
        append(record.displayName);
        append(record.typeName);
        hash ^= record.capabilities;
        hash *= 1099511628211ull;
        hash ^= static_cast<uint64_t>(record.visible) |
            (static_cast<uint64_t>(record.locked) << 1) |
            (static_cast<uint64_t>(record.runtimeOnly) << 2) |
            (static_cast<uint64_t>(record.missing) << 3);
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace editor
