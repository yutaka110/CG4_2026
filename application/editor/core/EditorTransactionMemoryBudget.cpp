#include "EditorTransactionMemoryBudget.h"

namespace editor {

bool EditorTransactionMemoryBudget::SetLimitBytes(
    std::size_t bytes,
    EditorError* error) {
    ClearEditorError(error);
    if (bytes == 0) {
        SetEditorError(
            error,
            EditorErrorCode::InvalidArgument,
            "Transaction history memory budget must be greater than zero.");
        return false;
    }
    limitBytes_ = bytes;
    return true;
}

bool EditorTransactionMemoryBudget::AcceptsSingleRecord(
    std::size_t bytes) const noexcept {
    return bytes <= limitBytes_;
}

bool EditorTransactionMemoryBudget::ExceededBy(
    std::size_t historyBytes) const noexcept {
    return historyBytes > limitBytes_;
}

} // namespace editor
