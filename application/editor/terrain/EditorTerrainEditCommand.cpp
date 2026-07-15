#include "EditorTerrainEditCommand.h"

#include "../core/EditorExecutionContext.h"

#include <utility>

namespace editor {

void EditorTerrainEditExecutionService::Bind(
    std::string documentKey,
    TerrainEditLayer* layer,
    ChangedCallback onChanged) {
    documentKey_ = std::move(documentKey);
    layer_ = layer;
    onChanged_ = std::move(onChanged);
}

void EditorTerrainEditExecutionService::Clear() {
    documentKey_.clear();
    layer_ = nullptr;
    onChanged_ = {};
}

EditorUndoResult EditorTerrainEditExecutionService::ApplyTerrainStroke(
    std::string_view documentKey,
    const std::vector<TerrainBrushStamp>& stamps,
    EditorTransactionApplyMode mode) {
    if (layer_ == nullptr || documentKey.empty() || documentKey != documentKey_) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Active Course Terrain Edit Layer does not match the command document.");
    }
    if (stamps.empty()) {
        return EditorUndoResult::Failure(
            EditorErrorCode::InvalidArgument,
            "Terrain edit command contains no brush stamps.");
    }
    std::string error;
    const TerrainEditDirtyRegion dirty = layer_->DirtyRegionFor(stamps);
    const bool succeeded = mode == EditorTransactionApplyMode::Redo
        ? layer_->ApplyStroke(stamps, &error)
        : layer_->RemoveStroke(stamps.front().strokeGuid, &error);
    if (!succeeded) {
        return EditorUndoResult::Failure(
            EditorErrorCode::ApplyFailed,
            error.empty() ? "Terrain stroke could not be applied." : error);
    }
    if (onChanged_) onChanged_(dirty);
    return EditorUndoResult::Success(
        mode == EditorTransactionApplyMode::Redo
            ? "Terrain stroke applied." : "Terrain stroke removed.");
}

EditorTerrainEditUndoCommand::EditorTerrainEditUndoCommand(
    std::string documentKey,
    std::vector<TerrainBrushStamp> stamps)
    : documentKey_(std::move(documentKey)), stamps_(std::move(stamps)) {}

EditorUndoResult EditorTerrainEditUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    IEditorExecutionService* untyped =
        context.Find(IEditorTerrainEditExecutionService::kServiceId);
    auto* service = dynamic_cast<IEditorTerrainEditExecutionService*>(untyped);
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Terrain edit execution service is unavailable.");
    }
    return service->ApplyTerrainStroke(documentKey_, stamps_, mode);
}

std::size_t EditorTerrainEditUndoCommand::EstimatedBytes() const noexcept {
    std::size_t bytes = sizeof(EditorTerrainEditUndoCommand) + documentKey_.capacity();
    bytes += stamps_.capacity() * sizeof(TerrainBrushStamp);
    for (const TerrainBrushStamp& stamp : stamps_) {
        bytes += stamp.strokeGuid.capacity() + stamp.stampGuid.capacity();
    }
    return bytes;
}

} // namespace editor

