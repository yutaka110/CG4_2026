#include "CourseRailPickingService.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace editor {
namespace {

struct ScreenCandidate {
    float squaredDistance = (std::numeric_limits<float>::max)();
    float depth = 1.0f;
    float lineT = 0.0f;
};

ScreenCandidate DistanceToScreenSegment(
    float x,
    float y,
    const EditorViewportProjectedPoint& a,
    const EditorViewportProjectedPoint& b) {
    ScreenCandidate result{};
    const float dx = b.display.x - a.display.x;
    const float dy = b.display.y - a.display.y;
    const float denominator = dx * dx + dy * dy;
    result.lineT = denominator > 0.000001f
        ? (std::clamp)(((x - a.display.x) * dx + (y - a.display.y) * dy) / denominator, 0.0f, 1.0f)
        : 0.0f;
    const float nearestX = a.display.x + dx * result.lineT;
    const float nearestY = a.display.y + dy * result.lineT;
    const float distanceX = x - nearestX;
    const float distanceY = y - nearestY;
    result.squaredDistance = distanceX * distanceX + distanceY * distanceY;
    result.depth = a.depth + (b.depth - a.depth) * result.lineT;
    return result;
}

bool Better(float distanceSquared, float depth, float bestDistanceSquared, float bestDepth) {
    constexpr float kTieEpsilon = 0.01f;
    return distanceSquared + kTieEpsilon < bestDistanceSquared ||
        (std::fabs(distanceSquared - bestDistanceSquared) <= kTieEpsilon && depth < bestDepth);
}

} // namespace

CourseRailPickResult CourseRailPickingService::PickDisplay(
    const CourseRailAuthoringModel& model,
    const EditorViewportCoordinateService& coordinates,
    float displayX,
    float displayY,
    const CourseRailPickingSettings& settings) const {
    CourseRailPickResult pointPick{};
    CourseRailPickResult segmentPick{};
    CourseRailPickResult tangentPick{};
    if (!model.IsValid() || !coordinates.ViewportAvailable() ||
        !coordinates.DisplayPointInside(displayX, displayY)) {
        return {};
    }

    const float pointRadius = (std::max)(settings.controlPointRadiusPixels, 1.0f);
    float bestPointDistance = pointRadius * pointRadius;
    float bestPointDepth = 1.0f;
    const auto& points = model.RuntimePath().ControlPoints();

    if (settings.includeTangentHandles && !settings.tangentPointGuid.empty()) {
        const std::optional<uint32_t> tangentIndex =
            model.FindPointIndex(settings.tangentPointGuid);
        if (tangentIndex.has_value()) {
            const float radius = (std::max)(settings.controlPointRadiusPixels, 1.0f);
            float bestDistance = radius * radius;
            float bestDepth = 1.0f;
            for (const bool incoming : {true, false}) {
                const Vector3 handle = model.RuntimePath().TangentHandlePosition(
                    *tangentIndex, incoming);
                const EditorViewportProjectedPoint projected = coordinates.ProjectWorld(handle);
                if (!projected.valid || !projected.inDepth || !projected.onscreen) continue;
                const float dx = displayX - projected.display.x;
                const float dy = displayY - projected.display.y;
                const float distanceSquared = dx * dx + dy * dy;
                if (distanceSquared > radius * radius ||
                    !Better(distanceSquared, projected.depth, bestDistance, bestDepth)) continue;
                bestDistance = distanceSquared;
                bestDepth = projected.depth;
                tangentPick.hit = true;
                tangentPick.kind = incoming
                    ? CourseRailPickKind::IncomingTangent
                    : CourseRailPickKind::OutgoingTangent;
                tangentPick.guid = points[*tangentIndex].editorGuid;
                tangentPick.pointIndex = *tangentIndex;
                tangentPick.screenDistancePixels = std::sqrt(distanceSquared);
                tangentPick.depth = projected.depth;
                tangentPick.worldPosition = handle;
            }
        }
    }
    for (uint32_t index = 0; index < points.size(); ++index) {
        const EditorViewportProjectedPoint projected = coordinates.ProjectWorld(points[index].position);
        if (!projected.valid || !projected.inDepth || !projected.onscreen) continue;
        const float dx = displayX - projected.display.x;
        const float dy = displayY - projected.display.y;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > pointRadius * pointRadius ||
            !Better(distanceSquared, projected.depth, bestPointDistance, bestPointDepth)) {
            continue;
        }
        bestPointDistance = distanceSquared;
        bestPointDepth = projected.depth;
        pointPick.hit = true;
        pointPick.kind = CourseRailPickKind::ControlPoint;
        pointPick.guid = points[index].editorGuid;
        pointPick.pointIndex = index;
        pointPick.screenDistancePixels = std::sqrt(distanceSquared);
        pointPick.depth = projected.depth;
        pointPick.worldPosition = points[index].position;
    }

    const uint32_t subdivisions = (std::max)(settings.subdivisionsPerSegment, 4u);
    const float segmentRadius = (std::max)(settings.segmentRadiusPixels, 1.0f);
    float bestSegmentDistance = segmentRadius * segmentRadius;
    float bestSegmentDepth = 1.0f;
    for (uint32_t segmentIndex = 0; segmentIndex < model.Segments().size(); ++segmentIndex) {
        const CourseRailSegment& segment = model.Segments()[segmentIndex];
        EditorViewportProjectedPoint previous = coordinates.ProjectWorld(
            model.RuntimePath().EvaluateSegmentAt(segment.pointIndex, 0.0f).position);
        for (uint32_t step = 1; step <= subdivisions; ++step) {
            const float t1 = static_cast<float>(step) / static_cast<float>(subdivisions);
            const RailPathSample currentSample =
                model.RuntimePath().EvaluateSegmentAt(segment.pointIndex, t1);
            const EditorViewportProjectedPoint current = coordinates.ProjectWorld(currentSample.position);
            if (previous.valid && current.valid && previous.inDepth && current.inDepth &&
                (previous.onscreen || current.onscreen)) {
                const ScreenCandidate candidate = DistanceToScreenSegment(
                    displayX, displayY, previous, current);
                if (candidate.squaredDistance <= segmentRadius * segmentRadius &&
                    Better(candidate.squaredDistance, candidate.depth,
                        bestSegmentDistance, bestSegmentDepth)) {
                    const float t0 = static_cast<float>(step - 1) /
                        static_cast<float>(subdivisions);
                    const float railT = t0 + (t1 - t0) * candidate.lineT;
                    const RailPathSample nearest =
                        model.RuntimePath().EvaluateSegmentAt(segment.pointIndex, railT);
                    bestSegmentDistance = candidate.squaredDistance;
                    bestSegmentDepth = candidate.depth;
                    segmentPick.hit = true;
                    segmentPick.kind = CourseRailPickKind::Segment;
                    segmentPick.guid = segment.guid;
                    segmentPick.segmentIndex = segmentIndex;
                    segmentPick.normalizedT = railT;
                    segmentPick.screenDistancePixels = std::sqrt(candidate.squaredDistance);
                    segmentPick.depth = candidate.depth;
                    segmentPick.worldPosition = nearest.position;
                }
            }
            previous = current;
        }
    }

    if (tangentPick.hit) return tangentPick;
    if (settings.preferControlPoints && pointPick.hit) return pointPick;
    if (!pointPick.hit) return segmentPick;
    if (!segmentPick.hit) return pointPick;
    return pointPick.screenDistancePixels <= segmentPick.screenDistancePixels
        ? pointPick : segmentPick;
}

EditorViewportPickResult CourseRailPickingService::ToViewportPick(
    const CourseRailPickResult& pick,
    uint32_t generation) const {
    if (!pick.hit) return {};
    EditorViewportPickResult result{};
    result.hit = true;
    result.source = EditorViewportPickSource::CourseViewport;
    const bool pointLike = pick.kind == CourseRailPickKind::ControlPoint || pick.IsTangentHandle();
    result.domain = pointLike
        ? EditorDomainId::CourseRailControlPoint
        : EditorDomainId::CourseRailSegment;
    result.localIndex = pointLike
        ? pick.pointIndex : pick.segmentIndex;
    result.generation = generation;
    result.displayName = pick.kind == CourseRailPickKind::IncomingTangent
        ? "Rail Incoming Tangent"
        : pick.kind == CourseRailPickKind::OutgoingTangent
            ? "Rail Outgoing Tangent"
            : pick.kind == CourseRailPickKind::ControlPoint
                ? "Rail Control Point" : "Rail Segment";
    result.canonicalHandle.domain = result.domain;
    result.canonicalHandle.stableId = pointLike
        ? "course-rail-point:" + pick.guid
        : "course-rail-segment:" + pick.guid;
    result.canonicalHandle.localIndex = result.localIndex;
    result.canonicalHandle.generation = generation;
    result.canonicalHandle.displayName = result.displayName;
    return result;
}

const char* ToString(CourseRailPickKind kind) {
    switch (kind) {
    case CourseRailPickKind::None: return "None";
    case CourseRailPickKind::ControlPoint: return "ControlPoint";
    case CourseRailPickKind::Segment: return "Segment";
    case CourseRailPickKind::IncomingTangent: return "IncomingTangent";
    case CourseRailPickKind::OutgoingTangent: return "OutgoingTangent";
    }
    return "Unknown";
}

} // namespace editor
