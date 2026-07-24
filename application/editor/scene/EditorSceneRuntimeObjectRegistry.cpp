#include "EditorSceneRuntimeObjectRegistry.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

bool ValidateSources(
    const std::vector<EditorSceneRuntimeObjectSource>& desired,
    std::string* errorMessage) {
    std::unordered_set<std::string> stableIds;
    stableIds.reserve(desired.size());
    for (const EditorSceneRuntimeObjectSource& source : desired) {
        if (source.stableId.empty() || source.entityGuid.empty() ||
            source.componentTypeId.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Runtime Scene Object source requires Stable ID, Entity GUID, "
                    "and Component Type ID.";
            }
            return false;
        }
        if (!stableIds.insert(source.stableId).second) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    "Runtime Scene Object Stable ID is duplicated: " +
                    source.stableId;
            }
            return false;
        }
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool SameSource(
    const EditorSceneRuntimeObjectSource& lhs,
    const EditorSceneRuntimeObjectSource& rhs) noexcept {
    return lhs.stableId == rhs.stableId &&
        lhs.entityGuid == rhs.entityGuid &&
        lhs.componentTypeId == rhs.componentTypeId &&
        lhs.sourceHash == rhs.sourceHash;
}

} // namespace

bool EditorSceneRuntimeObjectRegistry::Diff(
    const std::vector<EditorSceneRuntimeObjectSource>& desired,
    std::vector<EditorSceneRuntimeObjectDelta>& outDeltas,
    std::string* errorMessage) const {
    outDeltas.clear();
    if (!ValidateSources(desired, errorMessage)) return false;

    std::unordered_map<std::string, const EditorSceneRuntimeObjectSource*>
        desiredByStableId;
    desiredByStableId.reserve(desired.size());
    for (const EditorSceneRuntimeObjectSource& source : desired) {
        desiredByStableId.emplace(source.stableId, &source);
    }

    outDeltas.reserve(lookup_.size() + desired.size());
    for (const auto& [stableId, index] : lookup_) {
        const Slot& slot = slots_[index];
        const auto found = desiredByStableId.find(stableId);
        if (found == desiredByStableId.end()) {
            EditorSceneRuntimeObjectDelta delta{};
            delta.kind = EditorSceneRuntimeObjectDeltaKind::Removed;
            delta.previous = slot.record;
            outDeltas.push_back(std::move(delta));
            continue;
        }
        if (!SameSource(slot.record.source, *found->second)) {
            EditorSceneRuntimeObjectDelta delta{};
            delta.kind = EditorSceneRuntimeObjectDeltaKind::Modified;
            delta.previous = slot.record;
            delta.desired = *found->second;
            outDeltas.push_back(std::move(delta));
        }
    }

    for (const EditorSceneRuntimeObjectSource& source : desired) {
        if (lookup_.find(source.stableId) != lookup_.end()) continue;
        EditorSceneRuntimeObjectDelta delta{};
        delta.kind = EditorSceneRuntimeObjectDeltaKind::Added;
        delta.desired = source;
        outDeltas.push_back(std::move(delta));
    }

    std::sort(
        outDeltas.begin(),
        outDeltas.end(),
        [](const auto& lhs, const auto& rhs) {
            if (lhs.StableId() != rhs.StableId()) {
                return lhs.StableId() < rhs.StableId();
            }
            return lhs.kind < rhs.kind;
        });
    return true;
}

bool EditorSceneRuntimeObjectRegistry::Synchronize(
    const std::vector<EditorSceneRuntimeObjectSource>& desired,
    std::string* errorMessage) {
    std::vector<EditorSceneRuntimeObjectDelta> deltas;
    if (!Diff(desired, deltas, errorMessage)) return false;
    if (deltas.empty()) return true;

    for (const EditorSceneRuntimeObjectDelta& delta : deltas) {
        if (delta.kind != EditorSceneRuntimeObjectDeltaKind::Removed) continue;
        const auto found = lookup_.find(delta.previous.source.stableId);
        if (found != lookup_.end()) Release(found->second);
    }
    for (const EditorSceneRuntimeObjectDelta& delta : deltas) {
        if (delta.kind != EditorSceneRuntimeObjectDeltaKind::Modified) continue;
        const auto found = lookup_.find(delta.previous.source.stableId);
        if (found == lookup_.end()) continue;
        Slot& slot = slots_[found->second];
        slot.generation = NextGeneration(slot.generation);
        slot.record.handle.generation = slot.generation;
        slot.record.source = delta.desired;
    }
    for (const EditorSceneRuntimeObjectDelta& delta : deltas) {
        if (delta.kind == EditorSceneRuntimeObjectDeltaKind::Added) {
            Insert(delta.desired);
        }
    }
    ++revision_;
    return true;
}

void EditorSceneRuntimeObjectRegistry::Clear() noexcept {
    if (lookup_.empty()) return;
    lookup_.clear();
    freeIndices_.clear();
    freeIndices_.reserve(slots_.size());
    for (uint32_t index = 0; index < slots_.size(); ++index) {
        Slot& slot = slots_[index];
        slot.generation = NextGeneration(slot.generation);
        slot.occupied = false;
        slot.record = {};
    }
    for (std::size_t index = slots_.size(); index > 0; --index) {
        freeIndices_.push_back(static_cast<uint32_t>(index - 1));
    }
    ++revision_;
}

const EditorSceneRuntimeObjectRecord*
EditorSceneRuntimeObjectRegistry::Find(std::string_view stableId) const {
    const auto found = lookup_.find(std::string(stableId));
    if (found == lookup_.end()) return nullptr;
    return &slots_[found->second].record;
}

const EditorSceneRuntimeObjectRecord*
EditorSceneRuntimeObjectRegistry::Resolve(
    EditorSceneRuntimeObjectHandle handle) const {
    if (!handle.Valid() || handle.index >= slots_.size()) return nullptr;
    const Slot& slot = slots_[handle.index];
    if (!slot.occupied || slot.generation != handle.generation) return nullptr;
    return &slot.record;
}

std::vector<EditorSceneRuntimeObjectRecord>
EditorSceneRuntimeObjectRegistry::Ordered() const {
    std::vector<EditorSceneRuntimeObjectRecord> records;
    records.reserve(lookup_.size());
    for (const Slot& slot : slots_) {
        if (slot.occupied) records.push_back(slot.record);
    }
    std::sort(
        records.begin(),
        records.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.source.stableId < rhs.source.stableId;
        });
    return records;
}

uint32_t EditorSceneRuntimeObjectRegistry::NextGeneration(
    uint32_t generation) noexcept {
    ++generation;
    return generation == 0 ? 1 : generation;
}

void EditorSceneRuntimeObjectRegistry::Release(uint32_t index) noexcept {
    if (index >= slots_.size()) return;
    Slot& slot = slots_[index];
    if (!slot.occupied) return;
    lookup_.erase(slot.record.source.stableId);
    slot.generation = NextGeneration(slot.generation);
    slot.occupied = false;
    slot.record = {};
    freeIndices_.push_back(index);
}

EditorSceneRuntimeObjectRecord* EditorSceneRuntimeObjectRegistry::Insert(
    EditorSceneRuntimeObjectSource source) {
    uint32_t index = 0;
    if (freeIndices_.empty()) {
        index = static_cast<uint32_t>(slots_.size());
        slots_.push_back({});
    } else {
        index = freeIndices_.back();
        freeIndices_.pop_back();
    }
    Slot& slot = slots_[index];
    slot.occupied = true;
    slot.record.handle = {index, slot.generation};
    slot.record.source = std::move(source);
    lookup_.emplace(slot.record.source.stableId, index);
    return &slot.record;
}

} // namespace editor
