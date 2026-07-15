#include "EditorPlacementQueryService.h"

#include <algorithm>
#include <cmath>

namespace editor {
namespace {

float SnapValue(float value, float interval) {
    if (!std::isfinite(value) || !std::isfinite(interval) || interval <= 0.0f) return value;
    return std::round(value / interval) * interval;
}

float DistanceSquared(const Vector3& lhs, const Vector3& rhs) {
    const float x = lhs.x - rhs.x;
    const float y = lhs.y - rhs.y;
    const float z = lhs.z - rhs.z;
    return x * x + y * y + z * z;
}

} // namespace

EditorPlacementQueryResult EditorPlacementQueryService::QueryDisplay(
    const EditorViewportCoordinateService& coordinates,
    float displayX,
    float displayY,
    const EditorPlacementQuerySettings& settings) const {
    EditorPlacementQueryResult result{};
    if (!coordinates.DisplayPointInside(displayX, displayY)) return result;
    const EditorViewportWorldRay ray = coordinates.DisplayToWorldRay(displayX, displayY);
    if (!ray.valid) return result;

    float originAxis = 0.0f;
    float directionAxis = 0.0f;
    switch (settings.plane) {
    case EditorPlacementPlane::XZ:
        originAxis = ray.origin.y;
        directionAxis = ray.direction.y;
        result.normal = {0.0f, 1.0f, 0.0f};
        break;
    case EditorPlacementPlane::XY:
        originAxis = ray.origin.z;
        directionAxis = ray.direction.z;
        result.normal = {0.0f, 0.0f, 1.0f};
        break;
    case EditorPlacementPlane::YZ:
        originAxis = ray.origin.x;
        directionAxis = ray.direction.x;
        result.normal = {1.0f, 0.0f, 0.0f};
        break;
    }
    if (std::abs(directionAxis) <= 1.0e-6f) return result;
    const float distance = (settings.planeOffset - originAxis) / directionAxis;
    if (!std::isfinite(distance) || distance < 0.0f) return result;
    result.position = {
        ray.origin.x + ray.direction.x * distance,
        ray.origin.y + ray.direction.y * distance,
        ray.origin.z + ray.direction.z * distance};
    if (settings.gridSnapEnabled) {
        result.position = SnapPosition(
            result.position, settings.plane, settings.planeOffset,
            (std::max)(0.001f, settings.gridSize));
    }
    result.render = coordinates.WorldToRender(result.position);
    result.rayDistance = distance;
    result.usedFallbackPlane = true;
    result.valid = result.render.valid;
    return result;
}

Vector3 EditorPlacementQueryService::SnapPosition(
    Vector3 position,
    EditorPlacementPlane plane,
    float planeOffset,
    float gridSize) {
    position.x = SnapValue(position.x, gridSize);
    position.y = SnapValue(position.y, gridSize);
    position.z = SnapValue(position.z, gridSize);
    switch (plane) {
    case EditorPlacementPlane::XZ: position.y = planeOffset; break;
    case EditorPlacementPlane::XY: position.z = planeOffset; break;
    case EditorPlacementPlane::YZ: position.x = planeOffset; break;
    }
    return position;
}

void EditorBrushStrokeSampler::Begin(
    Vector3 position,
    const EditorBrushStrokeSettings& settings) {
    samples_.clear();
    active_ = true;
    if (settings.maxSamples > 0) samples_.push_back(position);
}

bool EditorBrushStrokeSampler::Append(
    Vector3 position,
    const EditorBrushStrokeSettings& settings) {
    if (!active_ || samples_.size() >= settings.maxSamples) return false;
    if (samples_.empty()) {
        samples_.push_back(position);
        return true;
    }
    const float spacing = (std::max)(0.001f, settings.spacing);
    if (DistanceSquared(samples_.back(), position) < spacing * spacing) return false;
    samples_.push_back(position);
    return true;
}

void EditorBrushStrokeSampler::Cancel() {
    active_ = false;
    samples_.clear();
}

const char* ToString(EditorPlacementPlane plane) {
    switch (plane) {
    case EditorPlacementPlane::XZ: return "XZ";
    case EditorPlacementPlane::XY: return "XY";
    case EditorPlacementPlane::YZ: return "YZ";
    }
    return "Unknown";
}

} // namespace editor
