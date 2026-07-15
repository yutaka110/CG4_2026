#pragma once

#include "EditorAssetMutationChange.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class EditorAssetMutationUndoCommand final : public IEditorUndoCommand {
public:
    explicit EditorAssetMutationUndoCommand(EditorAssetMutationChange change);
    ~EditorAssetMutationUndoCommand() override;

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override { return "asset"; }
    std::string_view TypeId() const noexcept override { return "asset.mutation"; }

    const EditorAssetMutationChange& Change() const noexcept { return change_; }
    void PreserveExternalPayload() noexcept { cleanupExternalPayload_ = false; }

private:
    EditorAssetMutationChange change_;
    bool cleanupExternalPayload_ = true;
};

} // namespace editor
