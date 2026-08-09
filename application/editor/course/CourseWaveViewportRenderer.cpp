#include "CourseWaveViewportRenderer.h"

#include "CourseEnemyAuthoringModel.h"
#include "CourseRailAuthoringModel.h"
#include "../../diagnostics/DebugDrawSystem.h"

#include <algorithm>

namespace editor {
namespace {

Vector4 ToVectorColor(uint32_t rgba) {
    return {
        static_cast<float>(rgba & 0xffu) / 255.0f,
        static_cast<float>((rgba >> 8) & 0xffu) / 255.0f,
        static_cast<float>((rgba >> 16) & 0xffu) / 255.0f,
        static_cast<float>((rgba >> 24) & 0xffu) / 255.0f};
}

uint32_t MarkerColor(
    const CourseWaveViewportStyle& style,
    const CourseWaveDefinition& wave,
    bool selected,
    bool hovered) {
    if (selected) return style.selectedColor;
    if (hovered) return style.hoveredColor;
    if (wave.editorLocked) return style.lockedColor;
    if (!wave.enabled) return style.disabledColor;
    return style.waveColor;
}

bool SubmitWorldLine(
    const EditorViewportCoordinateService& coordinates,
    EditorViewportOverlayCommandSink& sink,
    const Vector3& a,
    const Vector3& b,
    uint32_t color,
    float thickness,
    bool selected,
    uint32_t& counter,
    uint32_t& rejected) {
    const EditorViewportProjectedPoint pa = coordinates.ProjectWorld(a);
    const EditorViewportProjectedPoint pb = coordinates.ProjectWorld(b);
    if (!pa.valid || !pb.valid || !pa.inDepth || !pb.inDepth) {
        ++rejected;
        return false;
    }
    if (!pa.onscreen && !pb.onscreen) return false;
    EditorViewportOverlayItemOptions options{};
    options.selected = selected;
    if (!sink.Line(pa.render.x, pa.render.y, pb.render.x, pb.render.y,
            color, thickness, options)) {
        return false;
    }
    ++counter;
    return true;
}

} // namespace

void CourseWaveViewportRenderer::SetSelectedWaves(
    const std::vector<std::string>& guids) {
    selectedWaveGuids_.clear();
    selectedWaveGuids_.insert(guids.begin(), guids.end());
}

void CourseWaveViewportRenderer::Build(
    const EditorViewportOverlayFrameContext& context,
    EditorViewportOverlayCommandSink& sink) const {
    stats_ = {};
    if (controller_ == nullptr || context.coordinates == nullptr ||
        controller_->Course() == nullptr) {
        return;
    }
    const CourseWaveAuthoringModel* model = previewModel_ != nullptr
        ? previewModel_ : controller_->Model();
    if (model == nullptr || !model->IsValid()) return;
    const CourseRailAuthoringModel rail(*controller_->Course());
    const CourseEnemyAuthoringModel enemies(*controller_->Course());
    if (!rail.IsValid()) return;
    stats_.modelValid = true;
    stats_.authoredWaves = static_cast<uint32_t>(model->Waves().size());

    if (style_.showTransitions) {
        for (const CourseWaveDefinition& wave : model->Waves()) {
            if (!wave.editorVisible || wave.nextWaveGuid.empty()) continue;
            const CourseWaveDefinition* next = model->Find(wave.nextWaveGuid);
            if (next == nullptr || !next->editorVisible) continue;
            const Vector3 a = rail.RuntimePath().Evaluate(wave.triggerRailDistance).position;
            const Vector3 b = rail.RuntimePath().Evaluate(next->triggerRailDistance).position;
            const bool selected = selectedWaveGuids_.contains(wave.editorGuid) ||
                selectedWaveGuids_.contains(next->editorGuid);
            SubmitWorldLine(*context.coordinates, sink, a, b,
                style_.transitionColor, selected ? 3.0f : style_.lineThickness,
                selected, stats_.transitionLines, stats_.rejectedBehindCamera);
        }
    }

    for (const CourseWaveDefinition& wave : model->Waves()) {
        if (!wave.editorVisible || (!wave.enabled && !style_.showDisabled)) continue;
        const bool selected = selectedWaveGuids_.contains(wave.editorGuid);
        const bool hovered = wave.editorGuid == hoveredWaveGuid_;
        const uint32_t color = MarkerColor(style_, wave, selected, hovered);
        const RailPathSample trigger = rail.RuntimePath().Evaluate(wave.triggerRailDistance);

        if (style_.showPrewarmRanges && wave.prewarmDistance > 0.0f) {
            const RailPathSample prewarm = rail.RuntimePath().Evaluate(
                (std::max)(0.0f, wave.triggerRailDistance - wave.prewarmDistance));
            uint32_t ignoredCounter = 0;
            SubmitWorldLine(*context.coordinates, sink, prewarm.position, trigger.position,
                style_.prewarmColor, selected ? 3.0f : style_.lineThickness,
                selected, ignoredCounter, stats_.rejectedBehindCamera);
        }
        if (style_.showMemberLinks && enemies.IsValid()) {
            for (const CourseEnemyPlacement* member : model->Members(wave.editorGuid)) {
                if (member == nullptr || !member->editorVisible) continue;
                const CourseEnemyPlacementResolution resolved = enemies.Resolve(*member);
                if (!resolved.valid) continue;
                SubmitWorldLine(*context.coordinates, sink,
                    trigger.position, resolved.worldPosition,
                    style_.memberLinkColor, 1.0f, selected,
                    stats_.memberLinks, stats_.rejectedBehindCamera);
            }
        }

        const EditorViewportProjectedPoint projected =
            context.coordinates->ProjectWorld(trigger.position);
        if (!projected.valid || !projected.inDepth || !projected.onscreen) {
            ++stats_.rejectedBehindCamera;
            continue;
        }
        const float radius = selected ? style_.selectedMarkerRadius : style_.markerRadius;
        EditorViewportOverlayItemOptions options{};
        options.selected = selected;
        options.background = style_.showLabels;
        options.priority = selected ? 190 : hovered ? 170 : 110;
        sink.RectFilled(projected.render.x - radius, projected.render.y - radius,
            projected.render.x + radius, projected.render.y + radius, color, options);
        sink.Rect(projected.render.x - radius - 2.0f, projected.render.y - radius - 2.0f,
            projected.render.x + radius + 2.0f, projected.render.y + radius + 2.0f,
            color, selected ? 2.5f : 1.25f, options);
        if (style_.showLabels) {
            std::string label = wave.displayName + " (" +
                std::to_string(model->Members(wave.editorGuid).size()) + " enemies)";
            if (!wave.enabled) label += " [Disabled]";
            if (wave.editorLocked) label += " [Locked]";
            sink.Label(projected.render.x + radius + 7.0f,
                projected.render.y - radius - 3.0f, std::move(label), color, options);
        }
        ++stats_.visibleWaves;
        if (selected) ++stats_.selectedWaves;
    }
}

void CourseWaveViewportRenderer::AppendDebugDraw(
    ge3::debug::DebugDrawSystem& debugDraw) const {
    if (controller_ == nullptr || controller_->Course() == nullptr) return;
    const CourseWaveAuthoringModel* model = previewModel_ != nullptr
        ? previewModel_ : controller_->Model();
    if (model == nullptr || !model->IsValid()) return;
    const CourseRailAuthoringModel rail(*controller_->Course());
    if (!rail.IsValid()) return;
    for (const CourseWaveDefinition& wave : model->Waves()) {
        if (!wave.editorVisible || (!wave.enabled && !style_.showDisabled)) continue;
        const bool selected = selectedWaveGuids_.contains(wave.editorGuid);
        const bool hovered = wave.editorGuid == hoveredWaveGuid_;
        const uint32_t color = MarkerColor(style_, wave, selected, hovered);
        const Vector3 trigger = rail.RuntimePath().Evaluate(wave.triggerRailDistance).position;
        debugDraw.AddPoint(trigger, selected ? 1.4f : 0.9f, ToVectorColor(color));
        if (style_.showPrewarmRanges && wave.prewarmDistance > 0.0f) {
            const Vector3 prewarm = rail.RuntimePath().Evaluate(
                (std::max)(0.0f, wave.triggerRailDistance - wave.prewarmDistance)).position;
            debugDraw.AddLine(prewarm, trigger, ToVectorColor(style_.prewarmColor));
        }
    }
}

} // namespace editor
