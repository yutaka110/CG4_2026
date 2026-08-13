#pragma once

#include <cstdint>

namespace editor {

enum class EditorWorldRefreshReason : uint32_t {
    None = 0,
    Initial = 1u << 0u,
    ProviderRegistry = 1u << 1u,
    ProviderSource = 1u << 2u,
    Scene = 1u << 3u,
    Course = 1u << 4u,
    Forced = 1u << 5u,
};

constexpr EditorWorldRefreshReason operator|(
    EditorWorldRefreshReason lhs,
    EditorWorldRefreshReason rhs) noexcept {
    return static_cast<EditorWorldRefreshReason>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr EditorWorldRefreshReason& operator|=(
    EditorWorldRefreshReason& lhs,
    EditorWorldRefreshReason rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool HasEditorWorldRefreshReason(
    EditorWorldRefreshReason value,
    EditorWorldRefreshReason reason) noexcept {
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(reason)) != 0u;
}

// Immutable sources that determine the contents of EditorWorldModel. Frame
// number is deliberately absent: an unchanged world must remain a cache hit.
struct EditorWorldRefreshRevisionKey final {
    uint64_t providerRegistryRevision = 0;
    uint64_t providerSourceRevision = 0;
    uint64_t sceneRevision = 0;
    uint64_t courseRevision = 0;
};

struct EditorWorldRefreshDecision final {
    bool shouldRefresh = false;
    EditorWorldRefreshReason reasons = EditorWorldRefreshReason::None;
    uint64_t requestRevision = 0;
};

struct EditorWorldRefreshRevisionGateState final {
    bool initialized = false;
    bool invalidated = false;
    uint64_t revision = 0;
    uint64_t evaluations = 0;
    uint64_t refreshRequests = 0;
    uint64_t refreshes = 0;
    uint64_t cacheHits = 0;
    uint64_t failures = 0;
    uint64_t invalidations = 0;
    EditorWorldRefreshReason lastReasons = EditorWorldRefreshReason::None;
    EditorWorldRefreshRevisionKey committedKey{};
};

// Revision gate for the expensive provider enumeration/sort/index rebuild.
// Evaluate is side-effect free with respect to the committed key. Call Commit
// only after EditorWorldModel::Refresh succeeds so failures retry next frame.
class EditorWorldRefreshRevisionGate final {
public:
    EditorWorldRefreshDecision Evaluate(
        const EditorWorldRefreshRevisionKey& key) noexcept;
    void Commit(
        const EditorWorldRefreshRevisionKey& key,
        const EditorWorldRefreshDecision& decision) noexcept;
    void RecordFailure() noexcept;
    void Invalidate() noexcept;
    void Reset() noexcept;

    const EditorWorldRefreshRevisionGateState& State() const noexcept {
        return state_;
    }

private:
    EditorWorldRefreshRevisionGateState state_{};
};

} // namespace editor
