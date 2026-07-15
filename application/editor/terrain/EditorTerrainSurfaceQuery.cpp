#include "EditorTerrainSurfaceQuery.h"

#include "../../terrain/TerrainVolumeField.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace editor {
namespace {
constexpr float kPi = 3.14159265358979323846f;

Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const Vector3& lhs, const Vector3& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float LengthSquared(const Vector3& value) {
    return Dot(value, value);
}

struct LocalPoint {
    float distance = 0.0f;
    float lateral = 0.0f;
    float vertical = 0.0f;
    float angle = 0.0f;
    float surfaceRadius = 1.0f;
};

LocalPoint FindNearestRailPoint(
    const RailPath& railPath,
    const TerrainGenerationSettings& settings,
    const Vector3& world) {
    const float length = railPath.Length();
    constexpr uint32_t kCoarseSamples = 48;
    float bestDistance = 0.0f;
    float bestDistanceSquared = std::numeric_limits<float>::max();
    for (uint32_t index = 0; index <= kCoarseSamples; ++index) {
        const float distance = length * static_cast<float>(index) /
            static_cast<float>(kCoarseSamples);
        const float candidate = LengthSquared(
            Subtract(world, railPath.Evaluate(distance).position));
        if (candidate < bestDistanceSquared) {
            bestDistanceSquared = candidate;
            bestDistance = distance;
        }
    }
    float window = length / static_cast<float>(kCoarseSamples);
    for (uint32_t iteration = 0; iteration < 6; ++iteration) {
        const float left = (std::clamp)(bestDistance - window, 0.0f, length);
        const float right = (std::clamp)(bestDistance + window, 0.0f, length);
        const float candidates[] = {left, (left + bestDistance) * 0.5f,
            bestDistance, (bestDistance + right) * 0.5f, right};
        for (float distance : candidates) {
            const float candidate = LengthSquared(
                Subtract(world, railPath.Evaluate(distance).position));
            if (candidate < bestDistanceSquared) {
                bestDistanceSquared = candidate;
                bestDistance = distance;
            }
        }
        window *= 0.5f;
    }

    const RailPathSample sample = railPath.Evaluate(bestDistance);
    const Vector3 delta = Subtract(world, sample.position);
    const float lateral = Dot(delta, sample.right);
    const float vertical = Dot(delta, sample.up);
    const float lateralRadius = (std::max)(settings.canyonHalfWidth, settings.corridorRadius + 4.0f);
    const float verticalRadius = vertical >= 0.0f
        ? (std::max)(settings.wallHeight, settings.corridorRadius + 4.0f)
        : (std::max)(settings.corridorRadius * 0.92f, 4.0f);
    LocalPoint result{};
    result.distance = bestDistance;
    result.lateral = lateral;
    result.vertical = vertical;
    result.angle = std::atan2(vertical / verticalRadius, lateral / lateralRadius);
    if (result.angle < 0.0f) result.angle += 2.0f * kPi;
    result.surfaceRadius =
        std::sqrt(lateralRadius * lateralRadius + verticalRadius * verticalRadius) * 0.70710678f;
    return result;
}
} // namespace

EditorTerrainSurfaceHit EditorTerrainSurfaceQueryService::Query(
    const EditorViewportCoordinateService& coordinates,
    float displayX,
    float displayY,
    const RailPath& railPath,
    const TerrainGenerationSettings& settings,
    const TerrainEditLayer* edits,
    const TerrainEditLayer* preview) const {
    EditorTerrainSurfaceHit result{};
    if (railPath.Length() <= 0.0f || !coordinates.DisplayPointInside(displayX, displayY)) {
        return result;
    }
    const EditorViewportWorldRay ray = coordinates.DisplayToWorldRay(displayX, displayY);
    if (!ray.valid) return result;

    TerrainVolumeField field(railPath, settings, edits, preview);
    constexpr uint32_t kSteps = 160;
    constexpr float kMaxRayDistance = 1600.0f;
    float previousSdf = 0.0f;
    float previousT = 0.0f;
    bool havePrevious = false;
    float bestAbsSdf = std::numeric_limits<float>::max();
    float bestT = 0.0f;
    LocalPoint bestLocal{};
    for (uint32_t index = 0; index <= kSteps; ++index) {
        const float t = kMaxRayDistance * static_cast<float>(index) /
            static_cast<float>(kSteps);
        const Vector3 point = Add(ray.origin, Scale(ray.direction, t));
        const LocalPoint local = FindNearestRailPoint(railPath, settings, point);
        const float sdf = field.SampleLocal(
            local.distance, local.lateral, local.vertical).sdf;
        const float absolute = std::abs(sdf);
        if (absolute < bestAbsSdf) {
            bestAbsSdf = absolute;
            bestT = t;
            bestLocal = local;
        }
        if (havePrevious && ((previousSdf <= 0.0f && sdf >= 0.0f) ||
            (previousSdf >= 0.0f && sdf <= 0.0f))) {
            float low = previousT;
            float high = t;
            LocalPoint hitLocal = local;
            for (uint32_t refinement = 0; refinement < 8; ++refinement) {
                const float middle = (low + high) * 0.5f;
                const Vector3 middlePoint = Add(ray.origin, Scale(ray.direction, middle));
                hitLocal = FindNearestRailPoint(railPath, settings, middlePoint);
                const float middleSdf = field.SampleLocal(
                    hitLocal.distance, hitLocal.lateral, hitLocal.vertical).sdf;
                if ((previousSdf <= 0.0f && middleSdf <= 0.0f) ||
                    (previousSdf >= 0.0f && middleSdf >= 0.0f)) low = middle;
                else high = middle;
            }
            bestT = (low + high) * 0.5f;
            bestLocal = hitLocal;
            bestAbsSdf = 0.0f;
            break;
        }
        previousSdf = sdf;
        previousT = t;
        havePrevious = true;
    }
    if (bestAbsSdf > 0.08f) return result;

    result.railDistance = bestLocal.distance;
    result.angle = bestLocal.angle;
    result.surfaceRadius = bestLocal.surfaceRadius;
    result.rayDistance = bestT;
    result.position = field.SurfacePoint(
        result.railDistance, result.angle, &result.normal);
    result.valid = true;
    return result;
}

} // namespace editor
