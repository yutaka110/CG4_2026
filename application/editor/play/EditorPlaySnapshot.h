#pragma once

#include <any>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../core/EditorError.h"

namespace editor {

struct EditorPlaySnapshotEntry {
    std::any payload;
    uint64_t authoringFingerprint = 0;
};

class EditorPlaySnapshot {
public:
    template <typename T>
    bool Store(
        std::string providerId,
        const T& value,
        uint64_t authoringFingerprint,
        EditorError* error = nullptr) {
        if (providerId.empty()) {
            SetEditorError(error, EditorErrorCode::InvalidArgument, "Play snapshot provider id is empty.");
            return false;
        }
        entries_[std::move(providerId)] = EditorPlaySnapshotEntry{value, authoringFingerprint};
        ClearEditorError(error);
        return true;
    }

    template <typename T>
    const T* Read(std::string_view providerId, EditorError* error = nullptr) const {
        const auto found = entries_.find(std::string(providerId));
        if (found == entries_.end()) {
            SetEditorError(
                error,
                EditorErrorCode::NotAvailable,
                "Play snapshot entry is missing for provider: " + std::string(providerId));
            return nullptr;
        }
        const T* value = std::any_cast<T>(&found->second.payload);
        if (value == nullptr) {
            SetEditorError(
                error,
                EditorErrorCode::ApplyFailed,
                "Play snapshot payload type mismatch for provider: " + std::string(providerId));
            return nullptr;
        }
        ClearEditorError(error);
        return value;
    }

    bool Contains(std::string_view providerId) const;
    const EditorPlaySnapshotEntry* Find(std::string_view providerId) const;
    bool ReplaceFrom(
        std::string_view providerId,
        const EditorPlaySnapshot& source,
        EditorError* error = nullptr);
    void Clear();

    bool Empty() const noexcept { return entries_.empty(); }
    std::size_t Count() const noexcept { return entries_.size(); }
    uint64_t SessionSerial() const noexcept { return sessionSerial_; }
    void BindSession(uint64_t serial) noexcept { sessionSerial_ = serial; }

private:
    std::unordered_map<std::string, EditorPlaySnapshotEntry> entries_;
    uint64_t sessionSerial_ = 0;
};

} // namespace editor
