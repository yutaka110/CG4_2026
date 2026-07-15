#pragma once

#include "EditorRuntimeApplyChange.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class EditorRuntimeApplyUndoCommand final : public IEditorUndoCommand {
public:
    explicit EditorRuntimeApplyUndoCommand(EditorRuntimeApplyChange change);

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override { return "runtime-apply"; }
    std::string_view TypeId() const noexcept override { return "runtime-apply.provider-delta"; }

    const EditorRuntimeApplyChange& Change() const noexcept { return change_; }

private:
    EditorRuntimeApplyChange change_;
};

} // namespace editor
