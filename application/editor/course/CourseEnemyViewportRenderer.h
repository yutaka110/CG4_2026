#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "CourseEnemyEditorController.h"
#include "../EditorViewportOverlay.h"

namespace ge3::debug { class DebugDrawSystem; }

namespace editor {

struct CourseEnemyViewportStyle final {
    uint32_t enemyColor = 0xff4f5fffu;
    uint32_t selectedColor = 0xff52ff8au;
    uint32_t hoveredColor = 0xff55dfffu;
    uint32_t disabledColor = 0xff777777u;
    uint32_t lockedColor = 0xffb36cffu;
    uint32_t anchorLineColor = 0x90ff9a5au;
    uint32_t facingColor = 0xff78e8ffu;
    float markerRadius = 6.0f;
    float selectedMarkerRadius = 9.0f;
    float lineThickness = 1.5f;
    float facingLength = 5.0f;
    bool showLabels = true;
    bool showAnchorLinks = true;
    bool showFacing = true;
    bool showDisabled = true;
};

struct CourseEnemyViewportRenderStats final {
    uint32_t authoredPlacements = 0;
    uint32_t visiblePlacements = 0;
    uint32_t disabledPlacements = 0;
    uint32_t selectedPlacements = 0;
    uint32_t linePrimitives = 0;
    uint32_t rejectedBehindCamera = 0;
    uint32_t invalidPlacements = 0;
    bool modelValid = false;
};

// Draws persistent enemy instances as editor-only rail-anchored markers. It
// intentionally renders authoring records, not transient CourseSpawnRuntime actors.
class CourseEnemyViewportRenderer final : public IEditorViewportOverlayProvider {
public:
    explicit CourseEnemyViewportRenderer(
        const CourseEnemyEditorController* controller = nullptr)
        : controller_(controller) {}

    std::string_view Id() const override {
        return "editor.course.enemy-viewport";
    }
    EditorViewportOverlayLayerId Layer() const override {
        return EditorViewportOverlayLayerId::CourseNavigation;
    }
    void Build(
        const EditorViewportOverlayFrameContext& context,
        EditorViewportOverlayCommandSink& sink) const override;

    void AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw) const;
    void SetController(const CourseEnemyEditorController* controller) {
        controller_ = controller;
    }
    void SetPreviewModel(const CourseEnemyAuthoringModel* model) {
        previewModel_ = model;
    }
    void SetStyle(CourseEnemyViewportStyle style) { style_ = style; }
    const CourseEnemyViewportStyle& Style() const noexcept { return style_; }
    void SetSelectedPlacements(const std::vector<std::string>& guids);
    void SetHoveredPlacement(std::string guid) {
        hoveredPlacementGuid_ = std::move(guid);
    }
    const CourseEnemyViewportRenderStats& Stats() const noexcept { return stats_; }

private:
    const CourseEnemyEditorController* controller_ = nullptr;
    const CourseEnemyAuthoringModel* previewModel_ = nullptr;
    CourseEnemyViewportStyle style_{};
    std::unordered_set<std::string> selectedPlacementGuids_;
    std::string hoveredPlacementGuid_;
    mutable CourseEnemyViewportRenderStats stats_{};
};

} // namespace editor
