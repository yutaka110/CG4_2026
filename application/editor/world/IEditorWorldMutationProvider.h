#pragma once

#include "EditorWorldMutation.h"

namespace editor {

class IEditorWorldMutationProvider {
public:
    virtual ~IEditorWorldMutationProvider() = default;

    virtual bool BuildMutation(
        const EditorWorldProviderMutationRequest& request,
        EditorWorldMutationPlan* plan,
        std::string* errorMessage) const = 0;
    virtual bool ApplyMutationState(
        const EditorWorldMutationState& state,
        std::string* errorMessage) = 0;
};

} // namespace editor
