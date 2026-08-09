#include "CourseWavePickingService.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace editor {

CourseWavePickResult CourseWavePickingService::PickDisplay(
    const CourseWaveAuthoringModel& model,
    const CourseRailAuthoringModel& rail,
    const EditorViewportCoordinateService& coordinates,
    float displayX,
    float displayY,
    const CourseWavePickingSettings& settings) const {
    if (!model.IsValid() || !rail.IsValid() || !coordinates.ViewportAvailable() ||
        !coordinates.DisplayPointInside(displayX, displayY)) return {};
    CourseWavePickResult best{};
    const float radius = (std::max)(settings.markerRadiusPixels, 1.0f);
    float bestDistanceSquared = radius * radius;
    float bestDepth = (std::numeric_limits<float>::max)();
    constexpr float kTieEpsilon = 0.01f;
    for (std::size_t index = 0; index < model.Waves().size(); ++index) {
        const CourseWaveDefinition& wave = model.Waves()[index];
        if ((!settings.includeHidden && !wave.editorVisible) ||
            (!settings.includeDisabled && !wave.enabled) ||
            (!settings.includeLocked && wave.editorLocked)) continue;
        const Vector3 world = rail.RuntimePath().Evaluate(wave.triggerRailDistance).position;
        const EditorViewportProjectedPoint projected = coordinates.ProjectWorld(world);
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
        best = {true, wave.editorGuid, wave.displayName,
            static_cast<uint32_t>(index), std::sqrt(distanceSquared), projected.depth,
            world, wave.enabled, wave.editorLocked};
    }
    return best;
}

std::vector<CourseWavePickResult> CourseWavePickingService::PickDisplayRect(
    const CourseWaveAuthoringModel& model,
    const CourseRailAuthoringModel& rail,
    const EditorViewportCoordinateService& coordinates,
    float displayX0,
    float displayY0,
    float displayX1,
    float displayY1,
    const CourseWavePickingSettings& settings) const {
    std::vector<CourseWavePickResult> result;
    if (!model.IsValid() || !rail.IsValid() || !coordinates.ViewportAvailable()) return result;
    const float left = (std::min)(displayX0, displayX1);
    const float right = (std::max)(displayX0, displayX1);
    const float top = (std::min)(displayY0, displayY1);
    const float bottom = (std::max)(displayY0, displayY1);
    for (std::size_t index = 0; index < model.Waves().size(); ++index) {
        const CourseWaveDefinition& wave = model.Waves()[index];
        if ((!settings.includeHidden && !wave.editorVisible) ||
            (!settings.includeDisabled && !wave.enabled) ||
            (!settings.includeLocked && wave.editorLocked)) continue;
        const Vector3 world = rail.RuntimePath().Evaluate(wave.triggerRailDistance).position;
        const EditorViewportProjectedPoint projected = coordinates.ProjectWorld(world);
        if (!projected.valid || !projected.inDepth || !projected.onscreen ||
            projected.display.x < left || projected.display.x > right ||
            projected.display.y < top || projected.display.y > bottom) continue;
        result.push_back({true, wave.editorGuid, wave.displayName,
            static_cast<uint32_t>(index), 0.0f, projected.depth, world,
            wave.enabled, wave.editorLocked});
    }
    std::stable_sort(result.begin(), result.end(),
        [](const CourseWavePickResult& a, const CourseWavePickResult& b) {
            if (a.depth != b.depth) return a.depth < b.depth;
            return a.waveIndex < b.waveIndex;
        });
    return result;
}

EditorViewportPickResult CourseWavePickingService::ToViewportPick(
    const CourseWavePickResult& pick,
    uint32_t generation) const {
    if (!pick.hit) return {};
    EditorViewportPickResult result{};
    result.hit = true;
    result.source = EditorViewportPickSource::CourseViewport;
    result.domain = EditorDomainId::CourseWaveDefinition;
    result.localIndex = pick.waveIndex;
    result.generation = generation;
    result.displayName = pick.displayName.empty() ? "Course Wave" : pick.displayName;
    result.canonicalHandle = {
        result.domain,
        "course-wave:" + pick.waveGuid,
        result.localIndex,
        generation,
        result.displayName};
    return result;
}

} // namespace editor
