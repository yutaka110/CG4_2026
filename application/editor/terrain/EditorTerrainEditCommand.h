#pragma once

#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"
#include "../../terrain/TerrainEditLayer.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class IEditorTerrainEditExecutionService : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.terrain.edit";

    std::string_view ServiceId() const noexcept final { return kServiceId; }
    virtual EditorUndoResult ApplyTerrainStroke(
        std::string_view documentKey,
        const std::vector<TerrainBrushStamp>& stamps,
        EditorTransactionApplyMode mode) = 0;
    virtual EditorUndoResult ApplyTerrainSnapshot(
        std::string_view documentKey,
        const TerrainEditLayer& snapshot,
        const TerrainEditDirtyRegion& dirty,
        EditorTransactionApplyMode mode) = 0;
};

class EditorTerrainEditExecutionService final
    : public IEditorTerrainEditExecutionService {
public:
    using ChangedCallback = std::function<void(const TerrainEditDirtyRegion&)>;

    void Bind(
        std::string documentKey,
        TerrainEditLayer* layer,
        ChangedCallback onChanged = {});
    void Clear();

    EditorUndoResult ApplyTerrainStroke(
        std::string_view documentKey,
        const std::vector<TerrainBrushStamp>& stamps,
        EditorTransactionApplyMode mode) override;
    EditorUndoResult ApplyTerrainSnapshot(
        std::string_view documentKey,
        const TerrainEditLayer& snapshot,
        const TerrainEditDirtyRegion& dirty,
        EditorTransactionApplyMode mode) override;

private:
    std::string documentKey_;
    TerrainEditLayer* layer_ = nullptr;
    ChangedCallback onChanged_;
};

class EditorTerrainEditUndoCommand final : public IEditorUndoCommand {
public:
    EditorTerrainEditUndoCommand(
        std::string documentKey,
        TerrainEditLayer beforeSnapshot,
        TerrainEditLayer afterSnapshot,
        TerrainEditDirtyRegion dirty,
        std::vector<TerrainBrushStamp> stamps);

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override;
    std::size_t EstimatedBytes() const noexcept override;
    std::string_view DomainId() const noexcept override { return "terrain.edit"; }
    std::string_view TypeId() const noexcept override { return "terrain.stroke"; }

    const std::vector<TerrainBrushStamp>& Stamps() const noexcept { return stamps_; }

private:
    std::string documentKey_;
    TerrainEditLayer beforeSnapshot_;
    TerrainEditLayer afterSnapshot_;
    TerrainEditDirtyRegion dirty_{};
    std::vector<TerrainBrushStamp> stamps_;
};

} // namespace editor

