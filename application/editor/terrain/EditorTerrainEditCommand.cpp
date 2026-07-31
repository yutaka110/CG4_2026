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

EditorUndoResult EditorTerrainEditExecutionService::ApplyTerrainSnapshot(
    std::string_view documentKey,
    const TerrainEditLayer& snapshot,
    const TerrainEditDirtyRegion& dirty,
    EditorTransactionApplyMode mode) {
    if (layer_ == nullptr || documentKey.empty() || documentKey != documentKey_) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Active Course Terrain Edit Layer does not match the command document.");
    }
    std::string error;
    if (!layer_->ReplaceFromSnapshot(snapshot, &error)) {
        return EditorUndoResult::Failure(
            EditorErrorCode::ApplyFailed,
            error.empty() ? "Terrain snapshot could not be restored." : error);
    }
    if (onChanged_) onChanged_(dirty);
    return EditorUndoResult::Success(
        mode == EditorTransactionApplyMode::Redo
            ? "Terrain stroke snapshot applied."
            : "Terrain stroke snapshot restored.");
}

EditorTerrainEditUndoCommand::EditorTerrainEditUndoCommand(
    std::string documentKey,
    TerrainEditLayer beforeSnapshot,
    TerrainEditLayer afterSnapshot,
    TerrainEditDirtyRegion dirty,
    std::vector<TerrainBrushStamp> stamps)
    : documentKey_(std::move(documentKey)),
      beforeSnapshot_(std::move(beforeSnapshot)),
      afterSnapshot_(std::move(afterSnapshot)),
      dirty_(dirty),
      stamps_(std::move(stamps)) {}

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
    return service->ApplyTerrainSnapshot(
        documentKey_,
        mode == EditorTransactionApplyMode::Redo
            ? afterSnapshot_
            : beforeSnapshot_,
        dirty_,
        mode);
}

std::size_t EditorTerrainEditUndoCommand::EstimatedBytes() const noexcept {
    std::size_t bytes = sizeof(EditorTerrainEditUndoCommand) + documentKey_.capacity();
    const auto addStamps = [&bytes](const std::vector<TerrainBrushStamp>& values) {
        bytes += values.capacity() * sizeof(TerrainBrushStamp);
        for (const TerrainBrushStamp& stamp : values) {
            bytes += stamp.strokeGuid.capacity() + stamp.stampGuid.capacity();
        }
    };
    addStamps(beforeSnapshot_.Stamps());
    addStamps(afterSnapshot_.Stamps());
    addStamps(stamps_);
    return bytes;
}

} // namespace editor

