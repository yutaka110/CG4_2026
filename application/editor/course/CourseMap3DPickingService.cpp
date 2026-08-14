#include "CourseMap3DPickingService.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace editor {
namespace {

Vector3 Add(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 Sub(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 Scale(Vector3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float Dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

bool RaySphere(const CourseMap3DRay& ray, Vector3 center, float radius,
    float& distance) {
    const Vector3 toCenter = Sub(center, ray.origin);
    const float projected = Dot(toCenter, ray.direction);
    const float perpendicularSquared = Dot(toCenter, toCenter) - projected * projected;
    const float radiusSquared = radius * radius;
    if (perpendicularSquared > radiusSquared) return false;
    const float halfChord = std::sqrt((std::max)(0.0f,
        radiusSquared - perpendicularSquared));
    distance = projected - halfChord;
    if (distance < 0.0f) distance = projected + halfChord;
    return distance >= 0.0f;
}

// Closest points between a ray and a finite segment.
float RaySegmentDistance(const CourseMap3DRay& ray, Vector3 a, Vector3 b,
    float& rayDistance, float& segmentT) {
    const Vector3 v = Sub(b, a);
    const Vector3 w = Sub(ray.origin, a);
    const float aa = Dot(ray.direction, ray.direction);
    const float bb = Dot(ray.direction, v);
    const float cc = Dot(v, v);
    const float dd = Dot(ray.direction, w);
    const float ee = Dot(v, w);
    const float denominator = aa * cc - bb * bb;
    float s = 0.0f;
    float t = cc > 1.0e-8f ? (ee / cc) : 0.0f;
    if (denominator > 1.0e-8f) {
        s = (bb * ee - cc * dd) / denominator;
        t = (aa * ee - bb * dd) / denominator;
    }
    if (s < 0.0f) {
        s = 0.0f;
        t = cc > 1.0e-8f ? (ee / cc) : 0.0f;
    }
    t = (std::clamp)(t, 0.0f, 1.0f);
    s = (std::max)(0.0f, (bb * t - dd) / aa);
    const Vector3 onRay = Add(ray.origin, Scale(ray.direction, s));
    const Vector3 onSegment = Add(a, Scale(v, t));
    const Vector3 delta = Sub(onRay, onSegment);
    rayDistance = s;
    segmentT = t;
    return std::sqrt(Dot(delta, delta));
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

float PixelWorldRadius(float pixels, float depth, const CourseMap3DFrame& frame) {
    return pixels * 2.0f * depth *
        std::tan(frame.camera.verticalFovRadians * 0.5f) /
        (std::max)(1.0f, frame.viewport.height);
}

} // namespace

std::vector<CourseMap3DPickResult> CourseMap3DPickingService::PickAll(
    const CourseMap3DFrame& frame, Vector2 screenPosition,
    CourseMap3DPickSettings settings) const {
    std::vector<CourseMap3DPickResult> candidates;
    if (!frame.valid || !frame.viewport.Contains(screenPosition)) return candidates;
    const CourseMap3DRay ray = BuildCourseMap3DScreenRay(
        screenPosition, frame.camera, frame.viewport);
    if (!ray.valid) return candidates;

    for (const CourseMap3DMarker& marker : frame.markers) {
        if (!marker.selectable || marker.handle.stableId.empty()) continue;
        float distance = 0.0f;
        if (!RaySphere(ray, marker.world, marker.worldRadius, distance)) continue;
        candidates.push_back({true, marker.kind, marker.handle, marker.guid, ray,
            Add(ray.origin, Scale(ray.direction, distance)), marker.railDistance,
            distance, 0.0f});
    }
    for (const CourseMap3DLine& line : frame.lines) {
        if (!line.selectable || line.handle.stableId.empty()) continue;
        float distance = 0.0f;
        float segmentT = 0.0f;
        const float miss = RaySegmentDistance(ray, line.worldStart, line.worldEnd,
            distance, segmentT);
        const float depth = line.startDepth +
            (line.endDepth - line.startDepth) * segmentT;
        const float radius = (std::max)(settings.minimumLineRadius,
            PixelWorldRadius(settings.lineTolerancePixels, depth, frame));
        if (miss > radius) continue;
        candidates.push_back({true, line.kind, line.handle, line.guid, ray,
            Add(line.worldStart, Scale(Sub(line.worldEnd, line.worldStart), segmentT)),
            0.0f, distance, miss});
    }

    std::unordered_map<std::string, std::size_t> uniqueById;
    std::vector<CourseMap3DPickResult> unique;
    for (CourseMap3DPickResult& candidate : candidates) {
        const auto found = uniqueById.find(candidate.handle.stableId);
        if (found == uniqueById.end()) {
            uniqueById.emplace(candidate.handle.stableId, unique.size());
            unique.push_back(std::move(candidate));
        } else if (candidate.rayDistance < unique[found->second].rayDistance) {
            unique[found->second] = std::move(candidate);
        }
    }
    std::stable_sort(unique.begin(), unique.end(),
        [](const CourseMap3DPickResult& a, const CourseMap3DPickResult& b) {
            const int ap = Priority(a.kind);
            const int bp = Priority(b.kind);
            if (ap != bp) return ap < bp;
            if (a.rayDistance != b.rayDistance) return a.rayDistance < b.rayDistance;
            return a.handle.stableId < b.handle.stableId;
        });
    return unique;
}

CourseMap3DPickResult CourseMap3DPickingService::Pick(
    const CourseMap3DFrame& frame, Vector2 screenPosition,
    std::size_t cycleOffset, CourseMap3DPickSettings settings) const {
    const auto candidates = PickAll(frame, screenPosition, settings);
    return candidates.empty() ? CourseMap3DPickResult{}
                              : candidates[cycleOffset % candidates.size()];
}

} // namespace editor
