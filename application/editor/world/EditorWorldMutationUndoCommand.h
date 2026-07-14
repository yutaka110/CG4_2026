#pragma once

#include "EditorWorldMutation.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class EditorWorldMutationUndoCommand final : public IEditorUndoCommand {
public:
    EditorWorldMutationUndoCommand(
        EditorWorldMutationKind kind,
        EditorWorldMutationState before,
        EditorWorldMutationState after);

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override { return "world"; }
    std::string_view TypeId() const noexcept override { return "world.mutation"; }
    const EditorDocumentId& Document() const noexcept { return before_.document; }

private:
    EditorWorldMutationKind kind_ = EditorWorldMutationKind::Rename;
    EditorWorldMutationState before_;
    EditorWorldMutationState after_;
};

} // namespace editor
