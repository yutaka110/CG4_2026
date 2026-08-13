#include "CourseOverviewMapRenderer.h"

#include <algorithm>

namespace editor {
namespace {

EditorObjectHandle MakeHandle(
    EditorDomainId domain,
    std::string stableId,
    uint64_t localIndex,
    uint32_t generation,
    std::string displayName) {
    return {domain, std::move(stableId), localIndex, generation, std::move(displayName)};
}

bool IsSelected(const EditorSelection* selection, const EditorObjectHandle& handle) {
    return selection != nullptr && selection->Contains(handle);
}

} // namespace

CourseOverviewMapFrame CourseOverviewMapRenderer::Build(
    const CourseOverviewMapRenderInput& input) const {
    CourseOverviewMapFrame frame = BuildStatic(input);
    if (!frame.valid || input.projection == nullptr) return frame;
    const CourseOverviewMapDynamicPlayheadOverlay overlay =
        BuildPlayheadOverlay(*input.projection, input.playheadDistance);
    if (overlay.visible) {
        frame.markers.push_back({CourseOverviewMapItemKind::Playhead,
            overlay.position, overlay.worldPosition, overlay.color,
            overlay.radius, overlay.railDistance, {}, {}, false, false, true, false});
    }
    return frame;
}

CourseOverviewMapFrame CourseOverviewMapRenderer::BuildStatic(
    const CourseOverviewMapRenderInput& input) const {
    CourseOverviewMapFrame frame{};
    if (input.projection == nullptr || !input.projection->State().valid ||
        input.rail == nullptr || !input.rail->IsValid()) {
        frame.message = "Overview Map requires a valid projection and rail model.";
        return frame;
    }
    frame.valid = true;
    frame.rect = input.projection->State().rect;

    const RailPath& path = input.rail->RuntimePath();
    const auto& segments = input.rail->Segments();
    const uint32_t samples = (std::max)(2u, style_.samplesPerSegment);
    for (uint32_t segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
        const CourseRailSegment& segment = segments[segmentIndex];
        const EditorObjectHandle handle = MakeHandle(
            EditorDomainId::CourseRailSegment,
            "course-rail-segment:" + segment.guid,
            segmentIndex,
            input.railGeneration,
            "Rail Segment");
        const bool selected = IsSelected(input.selection, handle);
        const uint32_t color = selected
            ? style_.selectedColor : style_.railColor;
        for (uint32_t sampleIndex = 1; sampleIndex < samples; ++sampleIndex) {
            const RailPathSample a = path.EvaluateSegmentAt(
                segmentIndex, static_cast<float>(sampleIndex - 1u) / static_cast<float>(samples - 1u));
            const RailPathSample b = path.EvaluateSegmentAt(
                segmentIndex, static_cast<float>(sampleIndex) / static_cast<float>(samples - 1u));
            const auto pa = input.projection->ProjectWorldScreenOnly(a.position);
            const auto pb = input.projection->ProjectWorldScreenOnly(b.position);
            if (!pa.valid || !pb.valid) continue;
            frame.lines.push_back({CourseOverviewMapItemKind::RailSegment,
                pa.mapPosition, pb.mapPosition, a.position, b.position, color,
                style_.railThickness, handle, segment.guid, true, selected});
        }
        ++frame.stats.railSegments;
    }

    const auto& points = path.ControlPoints();
    for (uint32_t index = 0; index < points.size(); ++index) {
        const auto projected = input.projection->ProjectWorldScreenOnly(points[index].position);
        if (!projected.valid) continue;
        const EditorObjectHandle handle = MakeHandle(
            EditorDomainId::CourseRailControlPoint,
            "course-rail-point:" + points[index].editorGuid,
            index,
            input.railGeneration,
            "Rail Control Point");
        const float pointDistance = index < segments.size()
            ? segments[index].startDistance : input.rail->Length();
        frame.markers.push_back({CourseOverviewMapItemKind::RailControlPoint,
            projected.mapPosition, points[index].position,
            IsSelected(input.selection, handle) ? style_.selectedColor : style_.railPointColor,
            style_.pointRadius, pointDistance, handle, points[index].editorGuid,
            true, IsSelected(input.selection, handle), true, false});
        ++frame.stats.railControlPoints;
    }

    if (input.enemies != nullptr && input.enemies->IsValid()) {
        const auto& placements = input.enemies->Placements();
        for (uint32_t index = 0; index < placements.size(); ++index) {
            const CourseEnemyPlacement& placement = placements[index];
            if (!placement.editorVisible || (!style_.showDisabled && !placement.enabled)) continue;
            const auto resolved = input.enemies->Resolve(placement);
            if (!resolved.valid) continue;
            const auto projected = input.projection->ProjectWorldScreenOnly(resolved.worldPosition);
            if (!projected.valid) continue;
            const EditorObjectHandle handle = MakeHandle(
                EditorDomainId::CourseEnemyPlacement,
                "course-enemy-placement:" + placement.editorGuid,
                index,
                input.enemyGeneration,
                placement.actorAssetId.empty() ? "Enemy Placement" : placement.actorAssetId);
            const bool selected = IsSelected(input.selection, handle);
            uint32_t color = placement.enabled ? style_.enemyColor : style_.disabledColor;
            if (placement.editorLocked) color = style_.lockedColor;
            if (selected) color = style_.selectedColor;
            frame.markers.push_back({CourseOverviewMapItemKind::EnemyPlacement,
                projected.mapPosition, resolved.worldPosition, color, style_.enemyRadius,
                resolved.runtimeDistance, handle, placement.editorGuid, true, selected,
                placement.enabled, placement.editorLocked});
            if (style_.showLabels) {
                frame.labels.push_back({{projected.mapPosition.x + 8.0f, projected.mapPosition.y - 8.0f},
                    color, handle.displayName, CourseOverviewMapItemKind::EnemyPlacement,
                    handle, selected});
            }
            ++frame.stats.enemies;
        }
    }

    if (input.waves != nullptr && input.waves->IsValid()) {
        const auto& waves = input.waves->Waves();
        for (uint32_t index = 0; index < waves.size(); ++index) {
            const CourseWaveDefinition& wave = waves[index];
            if (!wave.editorVisible || (!style_.showDisabled && !wave.enabled)) continue;
            const auto projected = input.projection->ProjectRail(wave.triggerRailDistance);
            if (!projected.valid) continue;
            const EditorObjectHandle handle = MakeHandle(
                EditorDomainId::CourseWaveDefinition,
                "course-wave:" + wave.editorGuid,
                index,
                input.waveGeneration,
                wave.displayName.empty() ? "Course Wave" : wave.displayName);
            const bool selected = IsSelected(input.selection, handle);
            uint32_t color = wave.enabled ? style_.waveColor : style_.disabledColor;
            if (wave.editorLocked) color = style_.lockedColor;
            if (selected) color = style_.selectedColor;
            frame.markers.push_back({CourseOverviewMapItemKind::Wave, projected.mapPosition,
                projected.worldPosition, color, style_.waveRadius, projected.railDistance,
                handle, wave.editorGuid, true, selected, wave.enabled, wave.editorLocked});
            if (style_.showLabels) {
                frame.labels.push_back({{projected.mapPosition.x + 9.0f, projected.mapPosition.y + 5.0f},
                    color, handle.displayName, CourseOverviewMapItemKind::Wave,
                    handle, selected});
            }
            if (style_.showPrewarm && wave.prewarmDistance > 0.0f) {
                const auto prewarm = input.projection->ProjectRail(
                    (std::max)(0.0f, wave.triggerRailDistance - wave.prewarmDistance));
                if (prewarm.valid) {
                    frame.lines.push_back({CourseOverviewMapItemKind::WavePrewarm,
                        prewarm.mapPosition, projected.mapPosition, prewarm.worldPosition,
                        projected.worldPosition, style_.prewarmColor, 2.0f, {}, wave.editorGuid, false});
                }
            }
            ++frame.stats.waves;
        }
        if (style_.showTransitions) {
            for (const CourseWaveDefinition& wave : waves) {
                if (wave.nextWaveGuid.empty() || !wave.editorVisible) continue;
                const CourseWaveDefinition* next = input.waves->Find(wave.nextWaveGuid);
                if (next == nullptr || !next->editorVisible) continue;
                const auto a = input.projection->ProjectRail(wave.triggerRailDistance);
                const auto b = input.projection->ProjectRail(next->triggerRailDistance);
                if (!a.valid || !b.valid) continue;
                frame.lines.push_back({CourseOverviewMapItemKind::WaveTransition,
                    a.mapPosition, b.mapPosition, a.worldPosition, b.worldPosition,
                    style_.transitionColor, 1.5f, {}, wave.editorGuid, false});
                ++frame.stats.transitions;
            }
        }
    }

    return frame;
}

CourseOverviewMapDynamicPlayheadOverlay
CourseOverviewMapRenderer::BuildPlayheadOverlay(
    const CourseOverviewMapProjection& projection,
    float playheadDistance) const {
    CourseOverviewMapDynamicPlayheadOverlay overlay{};
    if (!projection.State().valid) return overlay;
    overlay.valid = true;
    overlay.rect = projection.State().rect;
    overlay.color = style_.playheadColor;
    if (playheadDistance < 0.0f) return overlay;
    const CourseOverviewMapProjectedPoint projected =
        projection.ProjectRail(playheadDistance);
    if (!projected.valid) return overlay;
    overlay.visible = true;
    overlay.position = projected.mapPosition;
    overlay.worldPosition = projected.worldPosition;
    overlay.railDistance = projected.railDistance;
    return overlay;
}

const char* ToString(CourseOverviewMapItemKind kind) {
    switch (kind) {
    case CourseOverviewMapItemKind::None: return "None";
    case CourseOverviewMapItemKind::RailSegment: return "Rail Segment";
    case CourseOverviewMapItemKind::RailControlPoint: return "Rail Control Point";
    case CourseOverviewMapItemKind::EnemyPlacement: return "Enemy Placement";
    case CourseOverviewMapItemKind::Wave: return "Wave";
    case CourseOverviewMapItemKind::WavePrewarm: return "Wave Prewarm";
    case CourseOverviewMapItemKind::WaveTransition: return "Wave Transition";
    case CourseOverviewMapItemKind::Playhead: return "Playhead";
    }
    return "Unknown";
}

} // namespace editor
