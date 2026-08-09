#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "CourseOverviewMapPickingService.h"
#include "CourseOverviewMapVisibilityService.h"
#include "CoursePreviewSimulationSystem.h"
#include "CourseRailEditorController.h"
#include "CourseEnemyEditorController.h"
#include "CourseWaveEditorController.h"

namespace editor {

enum class CourseOverviewMapViewId : uint8_t {
    None,
    Top,
    Side,
};

struct CourseOverviewMapMultiViewBinding final {
    const CourseRailEditorController* rail = nullptr;
    const CourseEnemyEditorController* enemies = nullptr;
    const CourseWaveEditorController* waves = nullptr;
    EditorSelection* selection = nullptr;
    const CoursePreviewSimulationSystem* preview = nullptr;
};

struct CourseOverviewMapMultiViewCrosshair final {
    bool valid = false;
    float railDistance = 0.0f;
    Vector3 worldPosition{};
    Vector2 topPosition{};
    Vector2 sidePosition{};
};

struct CourseOverviewMapMultiViewState final {
    bool enabled = false;
    bool bound = false;
    bool valid = false;
    CourseOverviewMapViewId hoveredView = CourseOverviewMapViewId::None;
    CourseOverviewMapPickResult hovered{};
    CourseOverviewMapMultiViewCrosshair crosshair{};
    uint64_t frameRevision = 0;
    uint64_t frameCacheHits = 0;
    uint64_t playheadOverlayRevision = 0;
    uint64_t playheadOverlayCacheHits = 0;
    std::string message;
};

// Owns synchronized Top and Side map projections. Both consume one immutable
// authoring snapshot and one canonical EditorSelection/crosshair rail distance.
class CourseOverviewMapMultiViewCoordinator final {
public:
    bool Bind(CourseOverviewMapMultiViewBinding binding, std::string* errorMessage = nullptr);
    void Unbind();
    void SetEnabled(bool enabled);
    void SetViewport(CourseOverviewMapRect rect);
    void SetPreviewCourse(const CourseAsset* previewCourse);
    void ClearPreviewCourse();
    bool Rebuild(float fallbackPlayheadDistance = -1.0f, std::string* errorMessage = nullptr);

    CourseOverviewMapViewId ViewAt(Vector2 mapPosition) const;
    CourseOverviewMapPickResult HoverAt(Vector2 mapPosition);
    CourseOverviewMapPickResult SelectAt(
        Vector2 mapPosition,
        bool additive = false,
        bool toggle = false,
        bool cycle = false);
    bool UpdateCrosshair(Vector2 mapPosition);
    void SetFocusDistance(float railDistance);
    void Pan(CourseOverviewMapViewId view, Vector2 deltaPixels);
    void ZoomAt(CourseOverviewMapViewId view, Vector2 mapPosition, float factor);
    void FrameAll();

    const CourseOverviewMapMultiViewState& State() const noexcept { return state_; }
    const CourseOverviewMapFrame& TopFrame() const noexcept { return topFrame_; }
    const CourseOverviewMapFrame& SideFrame() const noexcept { return sideFrame_; }
    const CourseOverviewMapVisibleFrame& TopVisibleFrame() const noexcept {
        return topVisibility_.Frame();
    }
    const CourseOverviewMapVisibleFrame& SideVisibleFrame() const noexcept {
        return sideVisibility_.Frame();
    }
    const CourseOverviewMapDynamicPlayheadOverlay& TopPlayheadOverlay() const noexcept {
        return topPlayheadOverlay_;
    }
    const CourseOverviewMapDynamicPlayheadOverlay& SidePlayheadOverlay() const noexcept {
        return sidePlayheadOverlay_;
    }
    const CourseOverviewMapProjection& TopProjection() const noexcept { return topProjection_; }
    const CourseOverviewMapProjection& SideProjection() const noexcept { return sideProjection_; }
    CourseOverviewMapVisibilityService& TopVisibility() noexcept {
        return topVisibility_;
    }
    CourseOverviewMapVisibilityService& SideVisibility() noexcept {
        return sideVisibility_;
    }

private:
    struct RetainedFrameKey final {
        uint64_t railRevision = 0;
        uint64_t enemyRevision = 0;
        uint64_t waveRevision = 0;
        uint64_t previewRevision = 0;
        uint64_t rendererSignature = 0;
        uint64_t viewportRevision = 0;
        uint64_t topVisibilitySettingsRevision = 0;
        uint64_t sideVisibilitySettingsRevision = 0;
        uint32_t railGeneration = 0;
        uint32_t enemyGeneration = 0;
        uint32_t waveGeneration = 0;
        uint32_t selectionRevision = 0;
        CourseOverviewMapRect viewport{};
        CourseOverviewMapProjectionSettings topProjection{};
        CourseOverviewMapProjectionSettings sideProjection{};
        bool hasEnemies = false;
        bool hasWaves = false;
    };

    struct PlayheadOverlayKey final {
        uint64_t staticFrameRevision = 0;
        uint64_t rendererSignature = 0;
        float playheadDistance = -1.0f;
    };

    bool Validate(std::string* errorMessage) const;
    float PlayheadDistance(float fallback) const;
    const CourseRailAuthoringModel* RailModel() const;
    const CourseEnemyAuthoringModel* EnemyModel() const;
    const CourseWaveAuthoringModel* WaveModel() const;
    void RefreshCrosshairPositions();
    RetainedFrameKey BuildFrameKey() const;
    static bool SameFrameKey(
        const RetainedFrameKey& lhs,
        const RetainedFrameKey& rhs) noexcept;
    void InvalidateFrameCache() noexcept;
    void RefreshPlayheadOverlays(float playheadDistance);
    void InvalidatePlayheadOverlays() noexcept;

    CourseOverviewMapMultiViewBinding binding_{};
    CourseOverviewMapMultiViewState state_{};
    CourseOverviewMapRect viewport_{};
    CourseOverviewMapProjectionSettings topSettings_{};
    CourseOverviewMapProjectionSettings sideSettings_{};
    CourseOverviewMapProjection topProjection_{};
    CourseOverviewMapProjection sideProjection_{};
    CourseOverviewMapRenderer renderer_{};
    CourseOverviewMapPickingService picking_{};
    CourseOverviewMapVisibilityService topVisibility_{};
    CourseOverviewMapVisibilityService sideVisibility_{};
    CourseOverviewMapFrame topFrame_{};
    CourseOverviewMapFrame sideFrame_{};
    CourseOverviewMapDynamicPlayheadOverlay topPlayheadOverlay_{};
    CourseOverviewMapDynamicPlayheadOverlay sidePlayheadOverlay_{};
    std::optional<RetainedFrameKey> retainedFrameKey_;
    std::optional<PlayheadOverlayKey> playheadOverlayKey_;
    uint64_t previewRevision_ = 0;
    uint64_t viewportRevision_ = 0;
    CourseAsset previewCourse_{};
    std::optional<CourseRailAuthoringModel> previewRail_;
    std::optional<CourseEnemyAuthoringModel> previewEnemies_;
    std::optional<CourseWaveAuthoringModel> previewWaves_;
    Vector2 lastPickPosition_{};
    std::size_t cycleOffset_ = 0;
};

const char* ToString(CourseOverviewMapViewId view);

} // namespace editor
