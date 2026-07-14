#include "EditorPlaySnapshot.h"

namespace editor {

bool EditorPlaySnapshot::Contains(std::string_view providerId) const {
    return entries_.contains(std::string(providerId));
}

const EditorPlaySnapshotEntry* EditorPlaySnapshot::Find(std::string_view providerId) const {
    const auto found = entries_.find(std::string(providerId));
    return found == entries_.end() ? nullptr : &found->second;
}

bool EditorPlaySnapshot::ReplaceFrom(
    std::string_view providerId,
    const EditorPlaySnapshot& source,
    EditorError* error) {
    const EditorPlaySnapshotEntry* entry = source.Find(providerId);
    if (entry == nullptr) {
        SetEditorError(
            error,
            EditorErrorCode::NotAvailable,
            "Replacement Play snapshot entry is missing for provider: " + std::string(providerId));
        return false;
    }
    entries_[std::string(providerId)] = *entry;
    ClearEditorError(error);
    return true;
}

void EditorPlaySnapshot::Clear() {
    entries_.clear();
    sessionSerial_ = 0;
}

} // namespace editor
