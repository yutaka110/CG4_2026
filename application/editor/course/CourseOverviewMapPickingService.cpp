#include "CourseOverviewMapPickingService.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace editor {
namespace {

float SquaredDistance(Vector2 a, Vector2 b) {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return x * x + y * y;
}

float PointSegmentDistance(Vector2 point, Vector2 a, Vector2 b) {
    const float x = b.x - a.x;
    const float y = b.y - a.y;
    const float lengthSquared = x * x + y * y;
    if (lengthSquared <= 0.000001f) return std::sqrt(SquaredDistance(point, a));
    const float t = (std::clamp)(
        ((point.x - a.x) * x + (point.y - a.y) * y) / lengthSquared, 0.0f, 1.0f);
    return std::sqrt(SquaredDistance(point, {a.x + x * t, a.y + y * t}));
}

int Priority(CourseOverviewMapItemKind kind) {
    switch (kind) {
    case CourseOverviewMapItemKind::EnemyPlacement: return 0;
    case CourseOverviewMapItemKind::Wave: return 1;
    case CourseOverviewMapItemKind::RailControlPoint: return 2;
    case CourseOverviewMapItemKind::RailSegment: return 3;
    default: return 100;
    }
}

} // namespace

std::vector<CourseOverviewMapPickResult> CourseOverviewMapPickingService::PickAll(
    const CourseOverviewMapFrame& frame,
    Vector2 mapPosition,
    CourseOverviewMapPickSettings settings) const {
    std::vector<CourseOverviewMapPickResult> candidates;
    if (!frame.valid || !frame.rect.Contains(mapPosition)) return candidates;

    for (const CourseOverviewMapMarker& marker : frame.markers) {
        if (!marker.selectable || marker.handle.stableId.empty()) continue;
        const float distance = std::sqrt(SquaredDistance(mapPosition, marker.position));
        if (distance > marker.radius + settings.markerTolerancePixels) continue;
        candidates.push_back({true, marker.kind, marker.handle, marker.guid,
            marker.position, marker.worldPosition, marker.railDistance, distance});
    }
    for (const CourseOverviewMapLine& line : frame.lines) {
        if (!line.selectable || line.handle.stableId.empty()) continue;
        const float distance = PointSegmentDistance(mapPosition, line.start, line.end);
        if (distance > settings.lineTolerancePixels + line.thickness * 0.5f) continue;
        candidates.push_back({true, line.kind, line.handle, line.guid,
            mapPosition, {(line.worldStart.x + line.worldEnd.x) * 0.5f,
                (line.worldStart.y + line.worldEnd.y) * 0.5f,
                (line.worldStart.z + line.worldEnd.z) * 0.5f}, 0.0f, distance});
    }

    // Sampled rail lines share one stable handle. Keep their closest sample.
    std::unordered_map<std::string, std::size_t> byStableId;
    std::vector<CourseOverviewMapPickResult> unique;
    for (CourseOverviewMapPickResult& candidate : candidates) {
        const auto found = byStableId.find(candidate.handle.stableId);
        if (found == byStableId.end()) {
            byStableId.emplace(candidate.handle.stableId, unique.size());
            unique.push_back(std::move(candidate));
        } else if (candidate.distancePixels < unique[found->second].distancePixels) {
            unique[found->second] = std::move(candidate);
        }
    }
    std::stable_sort(unique.begin(), unique.end(),
        [](const CourseOverviewMapPickResult& a, const CourseOverviewMapPickResult& b) {
            const int ap = Priority(a.kind);
            const int bp = Priority(b.kind);
            if (ap != bp) return ap < bp;
            if (a.distancePixels != b.distancePixels) return a.distancePixels < b.distancePixels;
            return a.handle.stableId < b.handle.stableId;
        });
    return unique;
}

CourseOverviewMapPickResult CourseOverviewMapPickingService::Pick(
    const CourseOverviewMapFrame& frame,
    Vector2 mapPosition,
    std::size_t cycleOffset,
    CourseOverviewMapPickSettings settings) const {
    const auto candidates = PickAll(frame, mapPosition, settings);
    if (candidates.empty()) return {};
    return candidates[cycleOffset % candidates.size()];
}

} // namespace editor
