#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "CourseRailEditorController.h"
#include "../EditorViewportOverlay.h"

namespace ge3::debug { class DebugDrawSystem; }

namespace editor {

struct CourseRailViewportStyle final {
    uint32_t railColor = 0xfff4c64eu;
    uint32_t selectedColor = 0xff52ff8au;
    uint32_t hoveredColor = 0xff55aaffu;
    uint32_t controlPointColor = 0xffffd86au;
    uint32_t corridorColor = 0x8090dfffu;
    uint32_t anchorColor = 0xffe57cffu;
    uint32_t directionColor = 0xff77f0ffu;
    uint32_t tangentLineColor = 0xffb78affu;
    uint32_t incomingTangentColor = 0xffff8ab7u;
    uint32_t outgoingTangentColor = 0xff8affd4u;
    float railThickness = 2.0f;
    float controlPointRadius = 5.0f;
    float selectedPointRadius = 7.0f;
    uint32_t subdivisionsPerSegment = 16;
    bool showControlPointLabels = true;
    bool showCorridor = true;
    bool showDirection = true;
    bool showAnchors = true;
    bool showSelectedTangents = true;
};

struct CourseRailViewportRenderStats final {
    uint32_t segments = 0;
    uint32_t linePrimitives = 0;
    uint32_t visibleControlPoints = 0;
    uint32_t anchors = 0;
    uint32_t tangentHandles = 0;
    uint32_t rejectedBehindCamera = 0;
    bool modelValid = false;
};

// Produces both depth-aware world debug lines and crisp screen-space editor
// handles from the same CourseRailAuthoringModel.
class CourseRailViewportRenderer final : public IEditorViewportOverlayProvider {
public:
    explicit CourseRailViewportRenderer(const CourseRailEditorController* controller = nullptr)
        : controller_(controller) {}

    std::string_view Id() const override { return "editor.course.rail-viewport"; }
    EditorViewportOverlayLayerId Layer() const override {
        return EditorViewportOverlayLayerId::CourseNavigation;
    }
    void Build(
        const EditorViewportOverlayFrameContext& context,
        EditorViewportOverlayCommandSink& sink) const override;

    void AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw) const;
    void SetController(const CourseRailEditorController* controller) { controller_ = controller; }
    void SetPreviewModel(const CourseRailAuthoringModel* model) { previewModel_ = model; }
    void SetStyle(CourseRailViewportStyle style) { style_ = style; }
    const CourseRailViewportStyle& Style() const noexcept { return style_; }
    void SetSelectedPoint(std::string guid) { selectedPointGuid_ = std::move(guid); }
    void SetSelectedSegment(std::string guid) { selectedSegmentGuid_ = std::move(guid); }
    void SetHoveredPoint(std::string guid) { hoveredPointGuid_ = std::move(guid); }
    void SetHoveredSegment(std::string guid) { hoveredSegmentGuid_ = std::move(guid); }
    const CourseRailViewportRenderStats& Stats() const noexcept { return stats_; }

private:
    const CourseRailEditorController* controller_ = nullptr;
    const CourseRailAuthoringModel* previewModel_ = nullptr;
    CourseRailViewportStyle style_{};
    std::string selectedPointGuid_;
    std::string selectedSegmentGuid_;
    std::string hoveredPointGuid_;
    std::string hoveredSegmentGuid_;
    mutable CourseRailViewportRenderStats stats_{};
};

} // namespace editor
