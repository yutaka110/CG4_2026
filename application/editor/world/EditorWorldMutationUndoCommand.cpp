#include "EditorWorldMutationUndoCommand.h"

#include "IEditorWorldMutationExecutionService.h"
#include "../core/EditorExecutionContext.h"

#include <utility>

namespace editor {

EditorWorldMutationUndoCommand::EditorWorldMutationUndoCommand(
    EditorWorldMutationKind kind,
    EditorWorldMutationState before,
    EditorWorldMutationState after)
    : kind_(kind), before_(std::move(before)), after_(std::move(after)) {}

EditorUndoResult EditorWorldMutationUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    IEditorExecutionService* untyped =
        context.Find(IEditorWorldMutationExecutionService::kServiceId);
    auto* service = dynamic_cast<IEditorWorldMutationExecutionService*>(untyped);
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Editor World mutation execution service is unavailable.");
    }
    return service->ApplyWorldMutationState(
        mode == EditorTransactionApplyMode::Undo ? before_ : after_);
}

std::size_t EditorWorldMutationUndoCommand::EstimatedBytes() const noexcept {
    return sizeof(EditorWorldMutationUndoCommand) +
        before_.EstimatedBytes() + after_.EstimatedBytes();
}

} // namespace editor
