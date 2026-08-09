#include "CourseRailViewportRenderer.h"

#include <algorithm>
#include <cmath>
#include <optional>

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
    CourseRailViewportRenderStats& stats) {
    const EditorViewportProjectedPoint pa = coordinates.ProjectWorld(a);
    const EditorViewportProjectedPoint pb = coordinates.ProjectWorld(b);
    if (!pa.valid || !pb.valid || !pa.inDepth || !pb.inDepth) {
        ++stats.rejectedBehindCamera;
        return false;
    }
    if (!pa.onscreen && !pb.onscreen) return false;
    if (!sink.Line(pa.render.x, pa.render.y, pb.render.x, pb.render.y, color, thickness)) {
        return false;
    }
    ++stats.linePrimitives;
    return true;
}

uint32_t SegmentColor(
    const CourseRailViewportStyle& style,
    std::string_view guid,
    std::string_view selected,
    std::string_view hovered) {
    if (guid == selected) return style.selectedColor;
    if (guid == hovered) return style.hoveredColor;
    return style.railColor;
}

} // namespace

void CourseRailViewportRenderer::Build(
    const EditorViewportOverlayFrameContext& context,
    EditorViewportOverlayCommandSink& sink) const {
    stats_ = {};
    if (controller_ == nullptr || context.coordinates == nullptr) return;
    const CourseRailAuthoringModel* model = previewModel_ != nullptr
        ? previewModel_ : controller_->Model();
    if (model == nullptr) return;
    stats_.modelValid = true;
    stats_.segments = static_cast<uint32_t>(model->Segments().size());
    const uint32_t subdivisions = (std::max)(style_.subdivisionsPerSegment, 4u);

    for (const CourseRailSegment& segment : model->Segments()) {
        const uint32_t color = SegmentColor(
            style_, segment.guid, selectedSegmentGuid_, hoveredSegmentGuid_);
        RailPathSample previous = model->RuntimePath().EvaluateSegmentAt(segment.pointIndex, 0.0f);
        for (uint32_t step = 1; step <= subdivisions; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(subdivisions);
            const RailPathSample current = model->RuntimePath().EvaluateSegmentAt(segment.pointIndex, t);
            SubmitWorldLine(
                *context.coordinates, sink, previous.position, current.position,
                color, style_.railThickness, stats_);
            if (style_.showCorridor) {
                const Vector3 previousLeft = Add(previous.position, Scale(previous.right, -previous.corridorRadius));
                const Vector3 currentLeft = Add(current.position, Scale(current.right, -current.corridorRadius));
                const Vector3 previousRight = Add(previous.position, Scale(previous.right, previous.corridorRadius));
                const Vector3 currentRight = Add(current.position, Scale(current.right, current.corridorRadius));
                SubmitWorldLine(*context.coordinates, sink, previousLeft, currentLeft,
                    style_.corridorColor, 1.0f, stats_);
                SubmitWorldLine(*context.coordinates, sink, previousRight, currentRight,
                    style_.corridorColor, 1.0f, stats_);
            }
            previous = current;
        }

        if (style_.showDirection) {
            const RailPathSample midpoint = model->RuntimePath().EvaluateSegmentAt(segment.pointIndex, 0.5f);
            const float arrowLength = (std::clamp)(segment.length * 0.08f, 3.0f, 12.0f);
            const Vector3 tip = Add(midpoint.position, Scale(midpoint.tangent, arrowLength));
            const Vector3 wingCenter = Add(tip, Scale(midpoint.tangent, -arrowLength * 0.35f));
            SubmitWorldLine(*context.coordinates, sink, midpoint.position, tip,
                style_.directionColor, 2.0f, stats_);
            SubmitWorldLine(*context.coordinates, sink, tip,
                Add(wingCenter, Scale(midpoint.right, arrowLength * 0.25f)),
                style_.directionColor, 2.0f, stats_);
            SubmitWorldLine(*context.coordinates, sink, tip,
                Add(wingCenter, Scale(midpoint.right, -arrowLength * 0.25f)),
                style_.directionColor, 2.0f, stats_);
        }
    }

    const CourseAsset* course = controller_->Course();
    if (course == nullptr) return;
    const auto& displayPoints = model->RuntimePath().ControlPoints();
    for (std::size_t index = 0; index < displayPoints.size(); ++index) {
        const RailPathControlPoint& point = displayPoints[index];
        const EditorViewportProjectedPoint projected = context.coordinates->ProjectWorld(point.position);
        if (!projected.valid || !projected.inDepth || !projected.onscreen) continue;
        const bool selected = point.editorGuid == selectedPointGuid_;
        const bool hovered = point.editorGuid == hoveredPointGuid_;
        const uint32_t color = selected
            ? style_.selectedColor
            : hovered ? style_.hoveredColor : style_.controlPointColor;
        sink.CircleFilled(
            projected.render.x,
            projected.render.y,
            selected ? style_.selectedPointRadius : style_.controlPointRadius,
            color,
            EditorViewportOverlayItemOptions{selected});
        if (style_.showControlPointLabels) {
            sink.Label(
                projected.render.x + 8.0f,
                projected.render.y - 8.0f,
                "Rail P" + std::to_string(index),
                color,
                EditorViewportOverlayItemOptions{selected});
        }
        ++stats_.visibleControlPoints;
    }

    if (style_.showSelectedTangents && !selectedPointGuid_.empty()) {
        const std::optional<uint32_t> index = model->FindPointIndex(selectedPointGuid_);
        if (index.has_value()) {
            const Vector3 point = displayPoints[*index].position;
            for (const bool incoming : {true, false}) {
                const Vector3 handle = model->RuntimePath().TangentHandlePosition(*index, incoming);
                SubmitWorldLine(*context.coordinates, sink, point, handle,
                    style_.tangentLineColor, 1.5f, stats_);
                const EditorViewportProjectedPoint projected =
                    context.coordinates->ProjectWorld(handle);
                if (projected.valid && projected.inDepth && projected.onscreen) {
                    sink.RectFilled(
                        projected.render.x - 4.0f, projected.render.y - 4.0f,
                        projected.render.x + 4.0f, projected.render.y + 4.0f,
                        incoming ? style_.incomingTangentColor : style_.outgoingTangentColor,
                        EditorViewportOverlayItemOptions{true});
                    ++stats_.tangentHandles;
                }
            }
        }
    }

    if (style_.showAnchors) {
        for (const CourseRailAnchorBinding& binding : course->railAnchors) {
            const RailAnchorResolution resolution = model->Resolve(binding.anchor);
            if (!resolution.valid) continue;
            SubmitWorldLine(
                *context.coordinates, sink,
                resolution.railSample.position,
                resolution.worldPosition,
                style_.anchorColor,
                1.5f,
                stats_);
            const EditorViewportProjectedPoint projected =
                context.coordinates->ProjectWorld(resolution.worldPosition);
            if (projected.valid && projected.inDepth && projected.onscreen) {
                sink.Circle(projected.render.x, projected.render.y, 4.0f, style_.anchorColor, 1.5f);
                ++stats_.anchors;
            }
        }
    }
}

void CourseRailViewportRenderer::AppendDebugDraw(
    ge3::debug::DebugDrawSystem& debugDraw) const {
    if (controller_ == nullptr) return;
    const CourseRailAuthoringModel* model = previewModel_ != nullptr
        ? previewModel_ : controller_->Model();
    if (model == nullptr) return;
    const uint32_t subdivisions = (std::max)(style_.subdivisionsPerSegment, 4u);
    const Vector4 corridorColor = ToVectorColor(style_.corridorColor);
    for (const CourseRailSegment& segment : model->Segments()) {
        const Vector4 railColor = ToVectorColor(SegmentColor(
            style_, segment.guid, selectedSegmentGuid_, hoveredSegmentGuid_));
        RailPathSample previous = model->RuntimePath().EvaluateSegmentAt(segment.pointIndex, 0.0f);
        for (uint32_t step = 1; step <= subdivisions; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(subdivisions);
            const RailPathSample current = model->RuntimePath().EvaluateSegmentAt(segment.pointIndex, t);
            debugDraw.AddLine(previous.position, current.position, railColor);
            if (style_.showCorridor) {
                debugDraw.AddLine(
                    Add(previous.position, Scale(previous.right, -previous.corridorRadius)),
                    Add(current.position, Scale(current.right, -current.corridorRadius)),
                    corridorColor);
                debugDraw.AddLine(
                    Add(previous.position, Scale(previous.right, previous.corridorRadius)),
                    Add(current.position, Scale(current.right, current.corridorRadius)),
                    corridorColor);
            }
            previous = current;
        }
    }
    const CourseAsset* course = controller_->Course();
    if (course == nullptr) return;
    const auto& displayPoints = model->RuntimePath().ControlPoints();
    for (const RailPathControlPoint& point : displayPoints) {
        const bool selected = point.editorGuid == selectedPointGuid_;
        const bool hovered = point.editorGuid == hoveredPointGuid_;
        debugDraw.AddPoint(
            point.position,
            selected ? 1.2f : 0.8f,
            ToVectorColor(selected ? style_.selectedColor
                : hovered ? style_.hoveredColor : style_.controlPointColor));
    }
    if (style_.showSelectedTangents && !selectedPointGuid_.empty()) {
        const std::optional<uint32_t> index = model->FindPointIndex(selectedPointGuid_);
        if (index.has_value()) {
            const Vector3 point = displayPoints[*index].position;
            for (const bool incoming : {true, false}) {
                const Vector3 handle = model->RuntimePath().TangentHandlePosition(*index, incoming);
                debugDraw.AddLine(point, handle, ToVectorColor(style_.tangentLineColor));
                debugDraw.AddPoint(handle, 0.65f, ToVectorColor(
                    incoming ? style_.incomingTangentColor : style_.outgoingTangentColor));
            }
        }
    }
    if (style_.showAnchors) {
        for (const CourseRailAnchorBinding& binding : course->railAnchors) {
            const RailAnchorResolution resolution = model->Resolve(binding.anchor);
            if (!resolution.valid) continue;
            debugDraw.AddLine(
                resolution.railSample.position,
                resolution.worldPosition,
                ToVectorColor(style_.anchorColor));
            debugDraw.AddPoint(resolution.worldPosition, 0.55f, ToVectorColor(style_.anchorColor));
        }
    }
}

} // namespace editor
