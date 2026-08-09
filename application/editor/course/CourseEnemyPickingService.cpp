#include "CourseEnemyPickingService.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace editor {

CourseEnemyPickResult CourseEnemyPickingService::PickDisplay(
    const CourseEnemyAuthoringModel& model,
    const EditorViewportCoordinateService& coordinates,
    float displayX,
    float displayY,
    const CourseEnemyPickingSettings& settings) const {
    if (!model.IsValid() || !coordinates.ViewportAvailable() ||
        !coordinates.DisplayPointInside(displayX, displayY)) {
        return {};
    }

    CourseEnemyPickResult best{};
    const float radius = (std::max)(settings.markerRadiusPixels, 1.0f);
    float bestDistanceSquared = radius * radius;
    float bestDepth = (std::numeric_limits<float>::max)();
    constexpr float kTieEpsilon = 0.01f;
    const auto& placements = model.Placements();
    for (std::size_t index = 0; index < placements.size(); ++index) {
        const CourseEnemyPlacement& placement = placements[index];
        if ((!settings.includeHidden && !placement.editorVisible) ||
            (!settings.includeDisabled && !placement.enabled) ||
            (!settings.includeLocked && placement.editorLocked)) {
            continue;
        }
        const CourseEnemyPlacementResolution resolved = model.Resolve(placement);
        if (!resolved.valid) continue;
        const EditorViewportProjectedPoint projected =
            coordinates.ProjectWorld(resolved.worldPosition);
        if (!projected.valid || !projected.inDepth || !projected.onscreen) continue;
        const float dx = displayX - projected.display.x;
        const float dy = displayY - projected.display.y;
        const float distanceSquared = dx * dx + dy * dy;
        const bool better = distanceSquared + kTieEpsilon < bestDistanceSquared ||
            (std::fabs(distanceSquared - bestDistanceSquared) <= kTieEpsilon &&
                projected.depth < bestDepth);
        if (distanceSquared > radius * radius || !better) continue;
        bestDistanceSquared = distanceSquared;
        bestDepth = projected.depth;
        best.hit = true;
        best.placementGuid = placement.editorGuid;
        best.actorAssetId = placement.actorAssetId;
        best.placementIndex = static_cast<uint32_t>(index);
        best.screenDistancePixels = std::sqrt(distanceSquared);
        best.depth = projected.depth;
        best.worldPosition = resolved.worldPosition;
        best.enabled = placement.enabled;
        best.locked = placement.editorLocked;
    }
    return best;
}

std::vector<CourseEnemyPickResult> CourseEnemyPickingService::PickDisplayRect(
    const CourseEnemyAuthoringModel& model,
    const EditorViewportCoordinateService& coordinates,
    float displayX0,
    float displayY0,
    float displayX1,
    float displayY1,
    const CourseEnemyPickingSettings& settings) const {
    std::vector<CourseEnemyPickResult> result;
    if (!model.IsValid() || !coordinates.ViewportAvailable()) return result;

    const float left = (std::min)(displayX0, displayX1);
    const float right = (std::max)(displayX0, displayX1);
    const float top = (std::min)(displayY0, displayY1);
    const float bottom = (std::max)(displayY0, displayY1);
    const auto& placements = model.Placements();
    result.reserve(placements.size());
    for (std::size_t index = 0; index < placements.size(); ++index) {
        const CourseEnemyPlacement& placement = placements[index];
        if ((!settings.includeHidden && !placement.editorVisible) ||
            (!settings.includeDisabled && !placement.enabled) ||
            (!settings.includeLocked && placement.editorLocked)) {
            continue;
        }
        const CourseEnemyPlacementResolution resolved = model.Resolve(placement);
        if (!resolved.valid) continue;
        const EditorViewportProjectedPoint projected =
            coordinates.ProjectWorld(resolved.worldPosition);
        if (!projected.valid || !projected.inDepth || !projected.onscreen ||
            projected.display.x < left || projected.display.x > right ||
            projected.display.y < top || projected.display.y > bottom) {
            continue;
        }
        CourseEnemyPickResult pick{};
        pick.hit = true;
        pick.placementGuid = placement.editorGuid;
        pick.actorAssetId = placement.actorAssetId;
        pick.placementIndex = static_cast<uint32_t>(index);
        pick.depth = projected.depth;
        pick.worldPosition = resolved.worldPosition;
        pick.enabled = placement.enabled;
        pick.locked = placement.editorLocked;
        result.push_back(std::move(pick));
    }
    std::stable_sort(result.begin(), result.end(),
        [](const CourseEnemyPickResult& leftPick,
           const CourseEnemyPickResult& rightPick) {
            if (leftPick.depth != rightPick.depth) return leftPick.depth < rightPick.depth;
            return leftPick.placementIndex < rightPick.placementIndex;
        });
    return result;
}

EditorViewportPickResult CourseEnemyPickingService::ToViewportPick(
    const CourseEnemyPickResult& pick,
    uint32_t generation) const {
    if (!pick.hit) return {};
    EditorViewportPickResult result{};
    result.hit = true;
    result.source = EditorViewportPickSource::CourseViewport;
    result.domain = EditorDomainId::CourseEnemyPlacement;
    result.localIndex = pick.placementIndex;
    result.generation = generation;
    result.displayName = pick.actorAssetId.empty()
        ? "Enemy Placement" : pick.actorAssetId;
    result.canonicalHandle.domain = result.domain;
    result.canonicalHandle.stableId =
        "course-enemy-placement:" + pick.placementGuid;
    result.canonicalHandle.localIndex = result.localIndex;
    result.canonicalHandle.generation = generation;
    result.canonicalHandle.displayName = result.displayName;
    return result;
}

} // namespace editor
