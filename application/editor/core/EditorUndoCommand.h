#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "EditorError.h"

namespace editor {

class EditorExecutionContext;

enum class EditorTransactionApplyMode {
    Undo,
    Redo,
};

struct EditorUndoResult final {
    bool succeeded = false;
    EditorErrorCode code = EditorErrorCode::ApplyFailed;
    std::string message;

    static EditorUndoResult Success(std::string message = {}) {
        return EditorUndoResult{true, EditorErrorCode::None, std::move(message)};
    }

    static EditorUndoResult Failure(
        EditorErrorCode code,
        std::string message) {
        return EditorUndoResult{false, code, std::move(message)};
    }
};

class IEditorUndoCommand {
public:
    virtual ~IEditorUndoCommand() = default;

    virtual EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const = 0;
    virtual std::size_t EstimatedBytes() const noexcept = 0;
    virtual std::string_view DomainId() const noexcept = 0;
    virtual std::string_view TypeId() const noexcept = 0;
};

using EditorUndoCommandPtr = std::shared_ptr<const IEditorUndoCommand>;

} // namespace editor
