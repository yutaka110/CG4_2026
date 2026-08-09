#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "CourseWaveEditorController.h"
#include "../EditorViewportOverlay.h"

namespace ge3::debug { class DebugDrawSystem; }

namespace editor {

struct CourseWaveViewportStyle final {
    uint32_t waveColor = 0xffffb84fu;
    uint32_t selectedColor = 0xff52ff8au;
    uint32_t hoveredColor = 0xff55dfffu;
    uint32_t disabledColor = 0xff777777u;
    uint32_t lockedColor = 0xffb36cffu;
    uint32_t prewarmColor = 0x9094d8ffu;
    uint32_t transitionColor = 0xb0ffcf69u;
    uint32_t memberLinkColor = 0x70ff9a5au;
    float markerRadius = 7.0f;
    float selectedMarkerRadius = 10.0f;
    float lineThickness = 2.0f;
    bool showLabels = true;
    bool showPrewarmRanges = true;
    bool showTransitions = true;
    bool showMemberLinks = true;
    bool showDisabled = true;
};

struct CourseWaveViewportRenderStats final {
    uint32_t authoredWaves = 0;
    uint32_t visibleWaves = 0;
    uint32_t selectedWaves = 0;
    uint32_t transitionLines = 0;
    uint32_t memberLinks = 0;
    uint32_t rejectedBehindCamera = 0;
    bool modelValid = false;
};

// Renders trigger markers, prewarm ranges, transition edges and membership
// links from the immutable schema-v7 authoring model.
class CourseWaveViewportRenderer final : public IEditorViewportOverlayProvider {
public:
    explicit CourseWaveViewportRenderer(
        const CourseWaveEditorController* controller = nullptr)
        : controller_(controller) {}

    std::string_view Id() const override { return "editor.course.wave-viewport"; }
    EditorViewportOverlayLayerId Layer() const override {
        return EditorViewportOverlayLayerId::CourseNavigation;
    }
    void Build(
        const EditorViewportOverlayFrameContext& context,
        EditorViewportOverlayCommandSink& sink) const override;

    void AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw) const;
    void SetController(const CourseWaveEditorController* controller) {
        controller_ = controller;
    }
    void SetPreviewModel(const CourseWaveAuthoringModel* model) {
        previewModel_ = model;
    }
    void SetStyle(CourseWaveViewportStyle style) { style_ = style; }
    const CourseWaveViewportStyle& Style() const noexcept { return style_; }
    void SetSelectedWaves(const std::vector<std::string>& guids);
    void SetHoveredWave(std::string guid) { hoveredWaveGuid_ = std::move(guid); }
    const CourseWaveViewportRenderStats& Stats() const noexcept { return stats_; }

private:
    const CourseWaveEditorController* controller_ = nullptr;
    const CourseWaveAuthoringModel* previewModel_ = nullptr;
    CourseWaveViewportStyle style_{};
    std::unordered_set<std::string> selectedWaveGuids_;
    std::string hoveredWaveGuid_;
    mutable CourseWaveViewportRenderStats stats_{};
};

} // namespace editor
