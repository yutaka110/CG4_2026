#pragma once

#include "EditorRuntimeApplyChange.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class IEditorRuntimeApplyExecutionService : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.runtimeApply.execution";

    std::string_view ServiceId() const noexcept final { return kServiceId; }
    virtual EditorUndoResult ApplyRuntimeChange(
        const EditorRuntimeApplyChange& change,
        EditorTransactionApplyMode mode) = 0;
};

} // namespace editor
