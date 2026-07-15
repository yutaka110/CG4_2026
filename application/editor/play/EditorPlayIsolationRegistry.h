#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "IEditorPlayIsolationProvider.h"

namespace editor {

class EditorPlaySnapshot;
class EditorRuntimeChangeSet;

class EditorPlayIsolationRegistry {
public:
    bool Register(IEditorPlayIsolationProvider* provider, EditorError* error = nullptr);
    void Clear();

    bool CaptureAll(EditorPlaySnapshot& snapshot, EditorError* error = nullptr) const;
    bool RestoreAll(const EditorPlaySnapshot& snapshot, EditorError* error = nullptr) const;
    bool BuildRuntimeChangeSet(
        const EditorPlaySnapshot& snapshot,
        EditorRuntimeChangeSet& changes,
        EditorError* error = nullptr) const;
    bool AdoptSelected(
        EditorPlaySnapshot& snapshot,
        const EditorRuntimeChangeSet& changes,
        EditorError* error = nullptr) const;
    bool FingerprintsMatch(const EditorPlaySnapshot& snapshot, EditorError* error = nullptr) const;

    const IEditorPlayIsolationProvider* Find(std::string_view providerId) const;
    std::size_t Count() const noexcept { return providers_.size(); }
    const std::vector<IEditorPlayIsolationProvider*>& Providers() const noexcept { return providers_; }

private:
    bool ValidateSnapshotCoverage(const EditorPlaySnapshot& snapshot, EditorError* error) const;

    std::vector<IEditorPlayIsolationProvider*> providers_;
};

} // namespace editor
