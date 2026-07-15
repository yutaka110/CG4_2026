#pragma once

#include <string>
#include <utility>

namespace editor {

enum class EditorErrorCode {
    None = 0,
    InvalidArgument,
    Busy,
    NotAvailable,
    MissingService,
    MemoryBudgetExceeded,
    ApplyFailed,
};

struct EditorError final {
    EditorErrorCode code = EditorErrorCode::None;
    std::string message;

    bool HasError() const noexcept { return code != EditorErrorCode::None; }

    void Clear() {
        code = EditorErrorCode::None;
        message.clear();
    }

    void Set(EditorErrorCode newCode, std::string newMessage) {
        code = newCode;
        message = std::move(newMessage);
    }
};

inline void SetEditorError(
    EditorError* error,
    EditorErrorCode code,
    std::string message) {
    if (error != nullptr) {
        error->Set(code, std::move(message));
    }
}

inline void ClearEditorError(EditorError* error) {
    if (error != nullptr) {
        error->Clear();
    }
}

} // namespace editor
