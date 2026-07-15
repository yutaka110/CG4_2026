#pragma once

#include "EditorWorldModel.h"
#include "IEditorWorldMutationExecutionService.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class EditorTransactionStack;

struct EditorPreparedWorldMutation {
    EditorWorldMutationState before;
    EditorWorldMutationState after;
    std::vector<EditorWorldObjectId> resultingSelectionIds;
    EditorDocumentId document;
    EditorObjectHandle transactionTarget;
    std::string label;
    EditorUndoCommandPtr command;
    std::string message;

    bool Valid() const noexcept {
        return before.IsValid() && after.IsValid() && command != nullptr &&
            document.IsValid() && !label.empty();
    }
};

class EditorWorldMutationExecutionService final
    : public IEditorWorldMutationExecutionService {
public:
    EditorWorldMutationExecutionService(
        EditorWorldObjectRegistry& registry,
        EditorWorldModel* model = nullptr)
        : registry_(registry), model_(model) {}

    EditorUndoResult ApplyWorldMutationState(
        const EditorWorldMutationState& state) override;

private:
    EditorWorldObjectRegistry& registry_;
    EditorWorldModel* model_ = nullptr;
};

class EditorWorldMutationService {
public:
    EditorWorldMutationService(
        EditorWorldObjectRegistry& registry,
        EditorWorldModel& model)
        : registry_(registry), model_(model) {}

    EditorWorldMutationResult Execute(
        const EditorWorldMutationRequest& request,
        EditorTransactionStack& transactions,
        bool canMutateAuthoring);
    bool Prepare(
        const EditorWorldMutationRequest& request,
        bool canMutateAuthoring,
        EditorPreparedWorldMutation& outPrepared,
        std::string* errorMessage = nullptr) const;
    EditorWorldMutationResult ResolveCommitted(
        const EditorPreparedWorldMutation& prepared) const;

private:
    EditorWorldObjectRegistry& registry_;
    EditorWorldModel& model_;
};

} // namespace editor
