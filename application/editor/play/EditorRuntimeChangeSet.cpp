#include "EditorRuntimeChangeSet.h"

#include <algorithm>

namespace editor {

void EditorRuntimeChangeSet::BeginRefresh() {
    previousChanges_ = changes_;
    changes_.clear();
}

void EditorRuntimeChangeSet::Add(EditorRuntimeChange change) {
    if (change.providerId.empty() || change.changeId.empty() ||
        change.beforeFingerprint == change.afterFingerprint) {
        return;
    }
    const auto previous = std::find_if(
        previousChanges_.begin(),
        previousChanges_.end(),
        [&](const EditorRuntimeChange& candidate) {
            return candidate.providerId == change.providerId &&
                candidate.changeId == change.changeId;
        });
    if (previous != previousChanges_.end()) {
        change.selected = previous->selected;
    }
    changes_.push_back(std::move(change));
}

void EditorRuntimeChangeSet::EndRefresh() {
    std::stable_sort(
        changes_.begin(),
        changes_.end(),
        [](const EditorRuntimeChange& lhs, const EditorRuntimeChange& rhs) {
            if (lhs.providerId != rhs.providerId) {
                return lhs.providerId < rhs.providerId;
            }
            return lhs.changeId < rhs.changeId;
        });
    previousChanges_.clear();
    ++revision_;
}

void EditorRuntimeChangeSet::Clear() {
    changes_.clear();
    previousChanges_.clear();
    ++revision_;
}

bool EditorRuntimeChangeSet::SetSelected(
    std::string_view providerId,
    std::string_view changeId,
    bool selected) {
    const auto found = std::find_if(
        changes_.begin(),
        changes_.end(),
        [&](const EditorRuntimeChange& change) {
            return change.providerId == providerId && change.changeId == changeId;
        });
    if (found == changes_.end()) {
        return false;
    }
    if (found->selected != selected) {
        found->selected = selected;
        ++revision_;
    }
    return true;
}

void EditorRuntimeChangeSet::SelectAll(bool selected) {
    bool changed = false;
    for (EditorRuntimeChange& change : changes_) {
        changed = changed || change.selected != selected;
        change.selected = selected;
    }
    if (changed) {
        ++revision_;
    }
}

bool EditorRuntimeChangeSet::HasSelectedChanges() const {
    return std::any_of(changes_.begin(), changes_.end(), [](const EditorRuntimeChange& change) {
        return change.selected;
    });
}

bool EditorRuntimeChangeSet::ProviderSelected(std::string_view providerId) const {
    return std::any_of(changes_.begin(), changes_.end(), [&](const EditorRuntimeChange& change) {
        return change.providerId == providerId && change.selected;
    });
}

std::size_t EditorRuntimeChangeSet::SelectedCount() const {
    return static_cast<std::size_t>(std::count_if(
        changes_.begin(),
        changes_.end(),
        [](const EditorRuntimeChange& change) { return change.selected; }));
}

} // namespace editor
