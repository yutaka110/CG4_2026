#pragma once

#include "EditorWorldModel.h"
#include "IEditorWorldMutationExecutionService.h"

namespace editor {

class EditorTransactionStack;

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

private:
    EditorWorldObjectRegistry& registry_;
    EditorWorldModel& model_;
};

} // namespace editor
