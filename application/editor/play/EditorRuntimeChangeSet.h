#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorRuntimeChange {
    std::string providerId;
    std::string changeId;
    std::string label;
    uint64_t beforeFingerprint = 0;
    uint64_t afterFingerprint = 0;
    bool selected = true;
};

class EditorRuntimeChangeSet {
public:
    void BeginRefresh();
    void Add(EditorRuntimeChange change);
    void EndRefresh();
    void Clear();

    bool SetSelected(std::string_view providerId, std::string_view changeId, bool selected);
    void SelectAll(bool selected);
    bool HasChanges() const noexcept { return !changes_.empty(); }
    bool HasSelectedChanges() const;
    bool ProviderSelected(std::string_view providerId) const;
    std::size_t Count() const noexcept { return changes_.size(); }
    std::size_t SelectedCount() const;
    uint32_t Revision() const noexcept { return revision_; }
    const std::vector<EditorRuntimeChange>& Changes() const noexcept { return changes_; }

private:
    std::vector<EditorRuntimeChange> changes_;
    std::vector<EditorRuntimeChange> previousChanges_;
    uint32_t revision_ = 0;
};

} // namespace editor
