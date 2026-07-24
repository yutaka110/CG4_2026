#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor {

struct EditorSceneRuntimeObjectHandle {
    static constexpr uint32_t kInvalidIndex =
        (std::numeric_limits<uint32_t>::max)();

    uint32_t index = kInvalidIndex;
    uint32_t generation = 0;

    bool Valid() const noexcept {
        return index != kInvalidIndex && generation != 0;
    }
    friend bool operator==(
        const EditorSceneRuntimeObjectHandle&,
        const EditorSceneRuntimeObjectHandle&) = default;
};

struct EditorSceneRuntimeObjectSource {
    std::string stableId;
    std::string entityGuid;
    std::string componentTypeId;
    uint64_t sourceHash = 0;
};

struct EditorSceneRuntimeObjectRecord {
    EditorSceneRuntimeObjectHandle handle{};
    EditorSceneRuntimeObjectSource source{};
};

enum class EditorSceneRuntimeObjectDeltaKind : uint32_t {
    Added = 0,
    Modified,
    Removed,
};

struct EditorSceneRuntimeObjectDelta {
    EditorSceneRuntimeObjectDeltaKind kind =
        EditorSceneRuntimeObjectDeltaKind::Added;
    EditorSceneRuntimeObjectRecord previous{};
    EditorSceneRuntimeObjectSource desired{};

    std::string_view StableId() const noexcept {
        return kind == EditorSceneRuntimeObjectDeltaKind::Removed
            ? std::string_view(previous.source.stableId)
            : std::string_view(desired.stableId);
    }
};

class EditorSceneRuntimeObjectRegistry {
public:
    bool Diff(
        const std::vector<EditorSceneRuntimeObjectSource>& desired,
        std::vector<EditorSceneRuntimeObjectDelta>& outDeltas,
        std::string* errorMessage = nullptr) const;
    bool Synchronize(
        const std::vector<EditorSceneRuntimeObjectSource>& desired,
        std::string* errorMessage = nullptr);
    void Clear() noexcept;

    const EditorSceneRuntimeObjectRecord* Find(
        std::string_view stableId) const;
    const EditorSceneRuntimeObjectRecord* Resolve(
        EditorSceneRuntimeObjectHandle handle) const;
    std::vector<EditorSceneRuntimeObjectRecord> Ordered() const;

    std::size_t Count() const noexcept { return lookup_.size(); }
    uint64_t Revision() const noexcept { return revision_; }

private:
    struct Slot {
        uint32_t generation = 1;
        bool occupied = false;
        EditorSceneRuntimeObjectRecord record{};
    };

    static uint32_t NextGeneration(uint32_t generation) noexcept;
    void Release(uint32_t index) noexcept;
    EditorSceneRuntimeObjectRecord* Insert(
        EditorSceneRuntimeObjectSource source);

    std::vector<Slot> slots_;
    std::vector<uint32_t> freeIndices_;
    std::unordered_map<std::string, uint32_t> lookup_;
    uint64_t revision_ = 0;
};

} // namespace editor
