#include "CourseEnemyViewportRenderer.h"

#include <algorithm>

#include "../../diagnostics/DebugDrawSystem.h"

namespace editor {
namespace {

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vector4 ToVectorColor(uint32_t rgba) {
    return {
        static_cast<float>(rgba & 0xffu) / 255.0f,
        static_cast<float>((rgba >> 8) & 0xffu) / 255.0f,
        static_cast<float>((rgba >> 16) & 0xffu) / 255.0f,
        static_cast<float>((rgba >> 24) & 0xffu) / 255.0f};
}

bool SubmitWorldLine(
    const EditorViewportCoordinateService& coordinates,
    EditorViewportOverlayCommandSink& sink,
    const Vector3& a,
    const Vector3& b,
    uint32_t color,
    float thickness,
    CourseEnemyViewportRenderStats& stats,
    bool selected) {
    const EditorViewportProjectedPoint pa = coordinates.ProjectWorld(a);
    const EditorViewportProjectedPoint pb = coordinates.ProjectWorld(b);
    if (!pa.valid || !pb.valid || !pa.inDepth || !pb.inDepth) {
        ++stats.rejectedBehindCamera;
        return false;
    }
    if (!pa.onscreen && !pb.onscreen) return false;
    EditorViewportOverlayItemOptions options{};
    options.selected = selected;
    if (!sink.Line(
            pa.render.x, pa.render.y, pb.render.x, pb.render.y,
            color, thickness, options)) {
        return false;
    }
    ++stats.linePrimitives;
    return true;
}

uint32_t MarkerColor(
    const CourseEnemyViewportStyle& style,
    const CourseEnemyPlacement& placement,
    bool selected,
    bool hovered) {
    if (selected) return style.selectedColor;
    if (hovered) return style.hoveredColor;
    if (placement.editorLocked) return style.lockedColor;
    if (!placement.enabled) return style.disabledColor;
    return style.enemyColor;
}

} // namespace

void CourseEnemyViewportRenderer::SetSelectedPlacements(
    const std::vector<std::string>& guids) {
    selectedPlacementGuids_.clear();
    selectedPlacementGuids_.insert(guids.begin(), guids.end());
}

void CourseEnemyViewportRenderer::Build(
    const EditorViewportOverlayFrameContext& context,
    EditorViewportOverlayCommandSink& sink) const {
    stats_ = {};
    if (controller_ == nullptr || context.coordinates == nullptr) return;
    const CourseEnemyAuthoringModel* model = previewModel_ != nullptr
        ? previewModel_ : controller_->Model();
    if (model == nullptr || !model->IsValid()) return;
    stats_.modelValid = true;
    stats_.authoredPlacements = static_cast<uint32_t>(model->Placements().size());

    for (const CourseEnemyPlacement& placement : model->Placements()) {
        if (!placement.editorVisible || (!placement.enabled && !style_.showDisabled)) continue;
        const CourseEnemyPlacementResolution resolved = model->Resolve(placement);
        if (!resolved.valid) {
            ++stats_.invalidPlacements;
            continue;
        }
        const bool selected = selectedPlacementGuids_.find(placement.editorGuid) !=
            selectedPlacementGuids_.end();
        const bool hovered = placement.editorGuid == hoveredPlacementGuid_;
        const uint32_t color = MarkerColor(style_, placement, selected, hovered);

        if (style_.showAnchorLinks) {
            SubmitWorldLine(
                *context.coordinates,
                sink,
                resolved.railSample.position,
                resolved.worldPosition,
                style_.anchorLineColor,
                style_.lineThickness,
                stats_,
                selected);
        }
        if (style_.showFacing) {
            SubmitWorldLine(
                *context.coordinates,
                sink,
                resolved.worldPosition,
                Add(resolved.worldPosition,
                    Scale(resolved.railSample.tangent, style_.facingLength)),
                style_.facingColor,
                selected ? 2.0f : style_.lineThickness,
                stats_,
                selected);
        }

        const EditorViewportProjectedPoint projected =
            context.coordinates->ProjectWorld(resolved.worldPosition);
        if (!projected.valid || !projected.inDepth || !projected.onscreen) {
            ++stats_.rejectedBehindCamera;
            continue;
        }
        const float radius = selected
            ? style_.selectedMarkerRadius : style_.markerRadius;
        EditorViewportOverlayItemOptions options{};
        options.selected = selected;
        options.background = style_.showLabels;
        options.priority = selected ? 180 : hovered ? 160 : 100;
        sink.CircleFilled(projected.render.x, projected.render.y, radius, color, options);
        sink.Circle(
            projected.render.x, projected.render.y, radius + 2.0f,
            color, selected ? 2.5f : 1.25f, options);
        sink.Line(
            projected.render.x - radius - 3.0f, projected.render.y,
            projected.render.x + radius + 3.0f, projected.render.y,
            color, 1.0f, options);
        sink.Line(
            projected.render.x, projected.render.y - radius - 3.0f,
            projected.render.x, projected.render.y + radius + 3.0f,
            color, 1.0f, options);
        if (style_.showLabels) {
            std::string label = placement.actorAssetId;
            if (!placement.waveGroupGuid.empty()) {
                label += " [" + placement.waveGroupGuid + "]";
            }
            if (!placement.enabled) label += " (Disabled)";
            if (placement.editorLocked) label += " (Locked)";
            sink.Label(
                projected.render.x + radius + 7.0f,
                projected.render.y - radius - 3.0f,
                std::move(label),
                color,
                options);
        }
        ++stats_.visiblePlacements;
        if (!placement.enabled) ++stats_.disabledPlacements;
        if (selected) ++stats_.selectedPlacements;
    }
}

void CourseEnemyViewportRenderer::AppendDebugDraw(
    ge3::debug::DebugDrawSystem& debugDraw) const {
    if (controller_ == nullptr) return;
    const CourseEnemyAuthoringModel* model = previewModel_ != nullptr
        ? previewModel_ : controller_->Model();
    if (model == nullptr || !model->IsValid()) return;
    for (const CourseEnemyPlacement& placement : model->Placements()) {
        if (!placement.editorVisible || (!placement.enabled && !style_.showDisabled)) continue;
        const CourseEnemyPlacementResolution resolved = model->Resolve(placement);
        if (!resolved.valid) continue;
        const bool selected = selectedPlacementGuids_.find(placement.editorGuid) !=
            selectedPlacementGuids_.end();
        const bool hovered = placement.editorGuid == hoveredPlacementGuid_;
        const uint32_t color = MarkerColor(style_, placement, selected, hovered);
        if (style_.showAnchorLinks) {
            debugDraw.AddLine(
                resolved.railSample.position,
                resolved.worldPosition,
                ToVectorColor(style_.anchorLineColor));
        }
        if (style_.showFacing) {
            debugDraw.AddLine(
                resolved.worldPosition,
                Add(resolved.worldPosition,
                    Scale(resolved.railSample.tangent, style_.facingLength)),
                ToVectorColor(style_.facingColor));
        }
        debugDraw.AddPoint(
            resolved.worldPosition,
            selected ? 1.2f : 0.8f,
            ToVectorColor(color));
    }
}

} // namespace editor
