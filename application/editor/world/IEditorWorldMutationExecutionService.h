#pragma once

#include "EditorWorldMutation.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class IEditorWorldMutationExecutionService : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.world.mutation";

    std::string_view ServiceId() const noexcept final { return kServiceId; }
    virtual EditorUndoResult ApplyWorldMutationState(
        const EditorWorldMutationState& state) = 0;
};

} // namespace editor
