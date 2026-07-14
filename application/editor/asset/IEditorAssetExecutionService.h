#pragma once

#include "EditorAssetMutationChange.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class IEditorAssetExecutionService : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.asset.execution";

    std::string_view ServiceId() const noexcept final { return kServiceId; }
    virtual EditorUndoResult ApplyAssetMutation(
        const EditorAssetMutationChange& change,
        EditorTransactionApplyMode mode) = 0;
};

} // namespace editor
