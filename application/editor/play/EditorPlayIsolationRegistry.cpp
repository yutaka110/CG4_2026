#include "EditorPlayIsolationRegistry.h"

#include "EditorPlaySnapshot.h"
#include "EditorRuntimeChangeSet.h"

#include <algorithm>
#include <string>
#include <utility>

namespace editor {

bool EditorPlayIsolationRegistry::Register(
    IEditorPlayIsolationProvider* provider,
    EditorError* error) {
    if (provider == nullptr || provider->Id().empty()) {
        SetEditorError(error, EditorErrorCode::InvalidArgument, "Play isolation provider is invalid.");
        return false;
    }
    if (Find(provider->Id()) != nullptr) {
        SetEditorError(
            error,
            EditorErrorCode::InvalidArgument,
            "Duplicate Play isolation provider id: " + std::string(provider->Id()));
        return false;
    }
    providers_.push_back(provider);
    std::stable_sort(
        providers_.begin(),
        providers_.end(),
        [](const IEditorPlayIsolationProvider* lhs, const IEditorPlayIsolationProvider* rhs) {
            if (lhs->Order() != rhs->Order()) {
                return lhs->Order() < rhs->Order();
            }
            return lhs->Id() < rhs->Id();
        });
    ClearEditorError(error);
    return true;
}

void EditorPlayIsolationRegistry::Clear() {
    providers_.clear();
}

bool EditorPlayIsolationRegistry::CaptureAll(
    EditorPlaySnapshot& snapshot,
    EditorError* error) const {
    EditorPlaySnapshot working;
    working.BindSession(snapshot.SessionSerial());
    for (const IEditorPlayIsolationProvider* provider : providers_) {
        if (!provider->Available()) {
            SetEditorError(
                error,
                EditorErrorCode::NotAvailable,
                "Required Play isolation provider target is unavailable: " + std::string(provider->Id()));
            return false;
        }
        EditorError providerError;
        if (!provider->Capture(working, &providerError) || !working.Contains(provider->Id())) {
            SetEditorError(
                error,
                providerError.HasError() ? providerError.code : EditorErrorCode::ApplyFailed,
                providerError.message.empty()
                    ? "Play isolation provider did not capture a snapshot: " + std::string(provider->Id())
                    : providerError.message);
            return false;
        }
    }
    snapshot = std::move(working);
    ClearEditorError(error);
    return true;
}

bool EditorPlayIsolationRegistry::RestoreAll(
    const EditorPlaySnapshot& snapshot,
    EditorError* error) const {
    if (!ValidateSnapshotCoverage(snapshot, error)) {
        return false;
    }

    EditorPlaySnapshot rollback;
    if (!CaptureAll(rollback, error)) {
        return false;
    }

    for (auto provider = providers_.rbegin(); provider != providers_.rend(); ++provider) {
        EditorError providerError;
        if (!(*provider)->Restore(snapshot, &providerError)) {
            EditorError rollbackError;
            std::string rollbackFailure;
            for (auto rollbackProvider = providers_.rbegin();
                 rollbackProvider != providers_.rend();
                 ++rollbackProvider) {
                rollbackError = {};
                if (!(*rollbackProvider)->Restore(rollback, &rollbackError) && rollbackFailure.empty()) {
                    rollbackFailure = rollbackError.message.empty()
                        ? "unknown rollback failure"
                        : rollbackError.message;
                }
            }
            std::string failure = providerError.message.empty()
                ? "Play isolation restore failed for provider: " + std::string((*provider)->Id())
                : providerError.message;
            if (!rollbackFailure.empty()) {
                failure += "; rollback could not complete: " + rollbackFailure;
            }
            SetEditorError(
                error,
                providerError.HasError() ? providerError.code : EditorErrorCode::ApplyFailed,
                std::move(failure));
            return false;
        }
    }
    ClearEditorError(error);
    return true;
}

bool EditorPlayIsolationRegistry::BuildRuntimeChangeSet(
    const EditorPlaySnapshot& snapshot,
    EditorRuntimeChangeSet& changes,
    EditorError* error) const {
    if (!ValidateSnapshotCoverage(snapshot, error)) {
        return false;
    }
    EditorRuntimeChangeSet working = changes;
    working.BeginRefresh();
    for (const IEditorPlayIsolationProvider* provider : providers_) {
        EditorError providerError;
        if (!provider->BuildRuntimeChangeSet(snapshot, working, &providerError)) {
            SetEditorError(
                error,
                providerError.HasError() ? providerError.code : EditorErrorCode::ApplyFailed,
                providerError.message.empty()
                    ? "Failed to build Runtime ChangeSet for provider: " + std::string(provider->Id())
                    : providerError.message);
            return false;
        }
    }
    working.EndRefresh();
    changes = std::move(working);
    ClearEditorError(error);
    return true;
}

bool EditorPlayIsolationRegistry::AdoptSelected(
    EditorPlaySnapshot& snapshot,
    const EditorRuntimeChangeSet& changes,
    EditorError* error) const {
    if (!ValidateSnapshotCoverage(snapshot, error)) {
        return false;
    }
    EditorPlaySnapshot replacement = snapshot;
    for (const IEditorPlayIsolationProvider* provider : providers_) {
        if (!changes.ProviderSelected(provider->Id())) {
            continue;
        }
        EditorPlaySnapshot captured;
        EditorError providerError;
        if (!provider->Capture(captured, &providerError) ||
            !replacement.ReplaceFrom(provider->Id(), captured, &providerError)) {
            SetEditorError(
                error,
                providerError.HasError() ? providerError.code : EditorErrorCode::ApplyFailed,
                providerError.message.empty()
                    ? "Failed to adopt Runtime ChangeSet for provider: " + std::string(provider->Id())
                    : providerError.message);
            return false;
        }
    }
    replacement.BindSession(snapshot.SessionSerial());
    snapshot = std::move(replacement);
    ClearEditorError(error);
    return true;
}

bool EditorPlayIsolationRegistry::FingerprintsMatch(
    const EditorPlaySnapshot& snapshot,
    EditorError* error) const {
    if (!ValidateSnapshotCoverage(snapshot, error)) {
        return false;
    }
    for (const IEditorPlayIsolationProvider* provider : providers_) {
        const EditorPlaySnapshotEntry* entry = snapshot.Find(provider->Id());
        if (entry == nullptr || entry->authoringFingerprint != provider->AuthoringFingerprint()) {
            SetEditorError(
                error,
                EditorErrorCode::ApplyFailed,
                "Authoring fingerprint mismatch for Play isolation provider: " +
                    std::string(provider->Id()));
            return false;
        }
    }
    ClearEditorError(error);
    return true;
}

const IEditorPlayIsolationProvider* EditorPlayIsolationRegistry::Find(
    std::string_view providerId) const {
    const auto found = std::find_if(
        providers_.begin(),
        providers_.end(),
        [&](const IEditorPlayIsolationProvider* provider) { return provider->Id() == providerId; });
    return found == providers_.end() ? nullptr : *found;
}

bool EditorPlayIsolationRegistry::ValidateSnapshotCoverage(
    const EditorPlaySnapshot& snapshot,
    EditorError* error) const {
    if (snapshot.Empty()) {
        SetEditorError(error, EditorErrorCode::NotAvailable, "Play isolation snapshot is empty.");
        return false;
    }
    for (const IEditorPlayIsolationProvider* provider : providers_) {
        if (!snapshot.Contains(provider->Id())) {
            SetEditorError(
                error,
                EditorErrorCode::NotAvailable,
                "Play isolation snapshot does not cover provider: " + std::string(provider->Id()));
            return false;
        }
    }
    return true;
}

} // namespace editor
