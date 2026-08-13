#include "EditorWorldRefreshRevisionGate.h"

namespace editor {

EditorWorldRefreshDecision EditorWorldRefreshRevisionGate::Evaluate(
    const EditorWorldRefreshRevisionKey& key) noexcept {
    ++state_.evaluations;
    EditorWorldRefreshDecision decision{};
    if (!state_.initialized) {
        decision.reasons |= EditorWorldRefreshReason::Initial;
    } else {
        if (key.providerRegistryRevision !=
            state_.committedKey.providerRegistryRevision) {
            decision.reasons |= EditorWorldRefreshReason::ProviderRegistry;
        }
        if (key.providerSourceRevision !=
            state_.committedKey.providerSourceRevision) {
            decision.reasons |= EditorWorldRefreshReason::ProviderSource;
        }
        if (key.sceneRevision != state_.committedKey.sceneRevision) {
            decision.reasons |= EditorWorldRefreshReason::Scene;
        }
        if (key.courseRevision != state_.committedKey.courseRevision) {
            decision.reasons |= EditorWorldRefreshReason::Course;
        }
    }
    if (state_.invalidated) {
        decision.reasons |= EditorWorldRefreshReason::Forced;
    }
    decision.shouldRefresh =
        decision.reasons != EditorWorldRefreshReason::None;
    if (decision.shouldRefresh) {
        decision.requestRevision = state_.revision + 1u;
        ++state_.refreshRequests;
    } else {
        ++state_.cacheHits;
    }
    return decision;
}

void EditorWorldRefreshRevisionGate::Commit(
    const EditorWorldRefreshRevisionKey& key,
    const EditorWorldRefreshDecision& decision) noexcept {
    if (!decision.shouldRefresh) return;
    state_.initialized = true;
    state_.invalidated = false;
    state_.committedKey = key;
    state_.lastReasons = decision.reasons;
    state_.revision = decision.requestRevision;
    ++state_.refreshes;
}

void EditorWorldRefreshRevisionGate::RecordFailure() noexcept {
    ++state_.failures;
}

void EditorWorldRefreshRevisionGate::Invalidate() noexcept {
    if (!state_.invalidated) ++state_.invalidations;
    state_.invalidated = true;
}

void EditorWorldRefreshRevisionGate::Reset() noexcept {
    state_ = {};
}

} // namespace editor
