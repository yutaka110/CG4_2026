#include "CourseOverviewMapMultiViewCoordinator.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace editor {
namespace {

float SquaredDistance(Vector2 a, Vector2 b) {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return x * x + y * y;
}

RailPathSample EvaluateDistance(const CourseRailAuthoringModel& rail, float distance) {
    if (distance >= rail.Length() && !rail.Segments().empty()) {
        return rail.RuntimePath().EvaluateSegmentAt(rail.Segments().back().pointIndex, 1.0f);
    }
    return rail.RuntimePath().Evaluate((std::max)(0.0f, distance));
}

uint64_t HashValue(uint64_t hash, uint64_t value) {
    hash ^= value;
    return hash * 1099511628211ull;
}

uint64_t HashFloat(uint64_t hash, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return HashValue(hash, bits);
}

uint64_t RendererSignature(const CourseOverviewMapStyle& style) {
    uint64_t hash = 1469598103934665603ull;
    hash = HashValue(hash, style.railColor);
    hash = HashValue(hash, style.railPointColor);
    hash = HashValue(hash, style.enemyColor);
    hash = HashValue(hash, style.waveColor);
    hash = HashValue(hash, style.selectedColor);
    hash = HashValue(hash, style.disabledColor);
    hash = HashValue(hash, style.lockedColor);
    hash = HashValue(hash, style.prewarmColor);
    hash = HashValue(hash, style.transitionColor);
    hash = HashValue(hash, style.playheadColor);
    hash = HashFloat(hash, style.railThickness);
    hash = HashFloat(hash, style.pointRadius);
    hash = HashFloat(hash, style.enemyRadius);
    hash = HashFloat(hash, style.waveRadius);
    hash = HashValue(hash, style.samplesPerSegment);
    hash = HashValue(hash, style.showLabels);
    hash = HashValue(hash, style.showDisabled);
    hash = HashValue(hash, style.showTransitions);
    hash = HashValue(hash, style.showPrewarm);
    return hash;
}

bool SameRect(const CourseOverviewMapRect& lhs, const CourseOverviewMapRect& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width &&
        lhs.height == rhs.height;
}

bool SameProjection(
    const CourseOverviewMapProjectionSettings& lhs,
    const CourseOverviewMapProjectionSettings& rhs) {
    return lhs.mode == rhs.mode && lhs.zoom == rhs.zoom &&
        lhs.panPixels.x == rhs.panPixels.x && lhs.panPixels.y == rhs.panPixels.y &&
        lhs.paddingPixels == rhs.paddingPixels &&
        lhs.freeYawRadians == rhs.freeYawRadians &&
        lhs.freePitchRadians == rhs.freePitchRadians &&
        lhs.fitSamplesPerSegment == rhs.fitSamplesPerSegment;
}

} // namespace

bool CourseOverviewMapMultiViewCoordinator::Bind(
    CourseOverviewMapMultiViewBinding binding,
    std::string* errorMessage) {
    if (state_.bound && binding.rail == binding_.rail &&
        binding.enemies == binding_.enemies && binding.waves == binding_.waves &&
        binding.selection == binding_.selection && binding.preview == binding_.preview) {
        return Validate(errorMessage);
    }
    ClearPreviewCourse();
    topFrame_ = {};
    sideFrame_ = {};
    topVisibility_.Invalidate();
    sideVisibility_.Invalidate();
    topPlayheadOverlay_ = {};
    sidePlayheadOverlay_ = {};
    InvalidateFrameCache();
    InvalidatePlayheadOverlays();
    state_.valid = false;
    state_.hoveredView = CourseOverviewMapViewId::None;
    state_.hovered = {};
    state_.crosshair = {};
    cycleOffset_ = 0;
    binding_ = binding;
    if (!Validate(errorMessage)) {
        binding_ = {};
        state_.bound = false;
        return false;
    }
    state_.bound = true;
    topSettings_.mode = CourseOverviewMapProjectionMode::Top;
    sideSettings_.mode = CourseOverviewMapProjectionMode::Side;
    return true;
}

void CourseOverviewMapMultiViewCoordinator::Unbind() {
    ClearPreviewCourse();
    binding_ = {};
    state_ = {};
    topFrame_ = {};
    sideFrame_ = {};
    topVisibility_.Invalidate();
    sideVisibility_.Invalidate();
    topPlayheadOverlay_ = {};
    sidePlayheadOverlay_ = {};
    InvalidateFrameCache();
    InvalidatePlayheadOverlays();
}

void CourseOverviewMapMultiViewCoordinator::SetEnabled(bool enabled) {
    state_.enabled = enabled;
}

void CourseOverviewMapMultiViewCoordinator::SetViewport(CourseOverviewMapRect rect) {
    if (SameRect(viewport_, rect)) return;
    viewport_ = rect;
    ++viewportRevision_;
    InvalidateFrameCache();
}

void CourseOverviewMapMultiViewCoordinator::SetPreviewCourse(
    const CourseAsset* previewCourse) {
    if (previewCourse == nullptr) {
        ClearPreviewCourse();
        return;
    }
    previewCourse_ = *previewCourse;
    previewRail_.emplace(previewCourse_);
    previewEnemies_.emplace(previewCourse_);
    previewWaves_.emplace(previewCourse_);
    ++previewRevision_;
    InvalidateFrameCache();
}

void CourseOverviewMapMultiViewCoordinator::ClearPreviewCourse() {
    const bool hadPreview = previewRail_.has_value() || previewEnemies_.has_value() ||
        previewWaves_.has_value();
    previewWaves_.reset();
    previewEnemies_.reset();
    previewRail_.reset();
    previewCourse_ = {};
    if (hadPreview) {
        ++previewRevision_;
        InvalidateFrameCache();
    }
}

bool CourseOverviewMapMultiViewCoordinator::Rebuild(
    float fallbackPlayheadDistance,
    std::string* errorMessage) {
    if (!state_.enabled) return false;
    if (!Validate(errorMessage) || !viewport_.Valid()) {
        state_.valid = false;
        state_.message = errorMessage != nullptr ? *errorMessage : "Multi View is unavailable.";
        InvalidateFrameCache();
        InvalidatePlayheadOverlays();
        return false;
    }
    const float playheadDistance = PlayheadDistance(fallbackPlayheadDistance);
    const RetainedFrameKey key = BuildFrameKey();
    if (retainedFrameKey_.has_value() && state_.valid && topFrame_.valid &&
        sideFrame_.valid && SameFrameKey(*retainedFrameKey_, key)) {
        ++state_.frameCacheHits;
        RefreshCrosshairPositions();
        RefreshPlayheadOverlays(playheadDistance);
        return true;
    }
    const float gap = 4.0f;
    const float half = (viewport_.width - gap) * 0.5f;
    const CourseOverviewMapRect topRect{viewport_.x, viewport_.y, half, viewport_.height};
    const CourseOverviewMapRect sideRect{viewport_.x + half + gap, viewport_.y, half, viewport_.height};
    const CourseRailAuthoringModel* rail = RailModel();
    const CourseRailAuthoringModel* boundsRail = binding_.rail->Model();
    if (!topProjection_.Configure(rail, topRect, topSettings_, errorMessage, boundsRail) ||
        !sideProjection_.Configure(rail, sideRect, sideSettings_, errorMessage, boundsRail)) {
        state_.valid = false;
        InvalidateFrameCache();
        InvalidatePlayheadOverlays();
        return false;
    }
    CourseOverviewMapRenderInput input{};
    input.rail = rail;
    input.enemies = EnemyModel();
    input.waves = WaveModel();
    input.selection = binding_.selection;
    input.playheadDistance = -1.0f;
    input.railGeneration = binding_.rail->State().bindingGeneration;
    if (binding_.enemies != nullptr) input.enemyGeneration = binding_.enemies->State().bindingGeneration;
    if (binding_.waves != nullptr) input.waveGeneration = binding_.waves->State().bindingGeneration;
    input.projection = &topProjection_;
    topFrame_ = renderer_.BuildStatic(input);
    input.projection = &sideProjection_;
    sideFrame_ = renderer_.BuildStatic(input);
    state_.valid = topFrame_.valid && sideFrame_.valid;
    state_.message = state_.valid ? "Top and Side views synchronized." : "Multi View frame build failed.";
    if (state_.valid) retainedFrameKey_ = key;
    else InvalidateFrameCache();
    RefreshCrosshairPositions();
    ++state_.frameRevision;
    topVisibility_.Build(topFrame_, state_.frameRevision);
    sideVisibility_.Build(sideFrame_, state_.frameRevision);
    RefreshPlayheadOverlays(playheadDistance);
    return state_.valid;
}

CourseOverviewMapViewId CourseOverviewMapMultiViewCoordinator::ViewAt(
    Vector2 mapPosition) const {
    if (topFrame_.rect.Contains(mapPosition)) return CourseOverviewMapViewId::Top;
    if (sideFrame_.rect.Contains(mapPosition)) return CourseOverviewMapViewId::Side;
    return CourseOverviewMapViewId::None;
}

CourseOverviewMapPickResult CourseOverviewMapMultiViewCoordinator::HoverAt(
    Vector2 mapPosition) {
    state_.hoveredView = ViewAt(mapPosition);
    state_.hovered = state_.hoveredView == CourseOverviewMapViewId::Top
        ? picking_.Pick(topFrame_, mapPosition)
        : state_.hoveredView == CourseOverviewMapViewId::Side
            ? picking_.Pick(sideFrame_, mapPosition) : CourseOverviewMapPickResult{};
    if (state_.hovered.hit) SetFocusDistance(state_.hovered.railDistance);
    else UpdateCrosshair(mapPosition);
    return state_.hovered;
}

CourseOverviewMapPickResult CourseOverviewMapMultiViewCoordinator::SelectAt(
    Vector2 mapPosition,
    bool additive,
    bool toggle,
    bool cycle) {
    if (binding_.selection == nullptr) return {};
    const CourseOverviewMapViewId view = ViewAt(mapPosition);
    const CourseOverviewMapFrame* frame = view == CourseOverviewMapViewId::Top
        ? &topFrame_ : view == CourseOverviewMapViewId::Side ? &sideFrame_ : nullptr;
    if (frame == nullptr) return {};
    const auto candidates = picking_.PickAll(*frame, mapPosition);
    if (candidates.empty()) {
        if (!additive && !toggle) binding_.selection->Clear();
        cycleOffset_ = 0;
        return {};
    }
    if (cycle && SquaredDistance(lastPickPosition_, mapPosition) <= 16.0f) ++cycleOffset_;
    else cycleOffset_ = 0;
    const CourseOverviewMapPickResult pick = candidates[cycleOffset_ % candidates.size()];
    if (toggle) binding_.selection->Toggle(pick.handle);
    else if (additive) binding_.selection->Add(pick.handle);
    else binding_.selection->SetPrimary(pick.handle);
    lastPickPosition_ = mapPosition;
    return pick;
}

bool CourseOverviewMapMultiViewCoordinator::UpdateCrosshair(Vector2 mapPosition) {
    const CourseOverviewMapViewId view = ViewAt(mapPosition);
    const CourseOverviewMapProjection* projection = view == CourseOverviewMapViewId::Top
        ? &topProjection_ : view == CourseOverviewMapViewId::Side ? &sideProjection_ : nullptr;
    const CourseRailAuthoringModel* rail = RailModel();
    if (projection == nullptr || rail == nullptr) return false;
    const Vector3 world = projection->Unproject(mapPosition, 0.0f);
    const RailAnchorProjection projected = rail->Project(world, 64);
    if (!projected.valid) return false;
    SetFocusDistance(projected.resolution.railSample.distance);
    state_.hoveredView = view;
    return true;
}

void CourseOverviewMapMultiViewCoordinator::SetFocusDistance(float railDistance) {
    const CourseRailAuthoringModel* rail = RailModel();
    if (rail == nullptr) return;
    state_.crosshair.valid = true;
    state_.crosshair.railDistance = (std::clamp)(railDistance, 0.0f, rail->Length());
    state_.crosshair.worldPosition = EvaluateDistance(*rail, state_.crosshair.railDistance).position;
    RefreshCrosshairPositions();
}

void CourseOverviewMapMultiViewCoordinator::Pan(
    CourseOverviewMapViewId view,
    Vector2 deltaPixels) {
    CourseOverviewMapProjectionSettings* settings = view == CourseOverviewMapViewId::Top
        ? &topSettings_ : view == CourseOverviewMapViewId::Side ? &sideSettings_ : nullptr;
    if (settings == nullptr) return;
    if (deltaPixels.x == 0.0f && deltaPixels.y == 0.0f) return;
    settings->panPixels.x += deltaPixels.x;
    settings->panPixels.y += deltaPixels.y;
    ++viewportRevision_;
    InvalidateFrameCache();
}

void CourseOverviewMapMultiViewCoordinator::ZoomAt(
    CourseOverviewMapViewId view,
    Vector2 mapPosition,
    float factor) {
    CourseOverviewMapProjectionSettings* settings = view == CourseOverviewMapViewId::Top
        ? &topSettings_ : view == CourseOverviewMapViewId::Side ? &sideSettings_ : nullptr;
    CourseOverviewMapProjection* projection = view == CourseOverviewMapViewId::Top
        ? &topProjection_ : view == CourseOverviewMapViewId::Side ? &sideProjection_ : nullptr;
    if (settings == nullptr || projection == nullptr || !projection->State().valid || factor <= 0.0f) return;
    const Vector2 raw = projection->MapToRaw(mapPosition);
    settings->zoom = (std::clamp)(settings->zoom * factor, 0.05f, 64.0f);
    const CourseRailAuthoringModel* rail = RailModel();
    const CourseRailAuthoringModel* bounds = binding_.rail->Model();
    projection->Configure(rail, projection->State().rect, *settings, nullptr, bounds);
    const Vector2 after = projection->RawToMap(raw);
    settings->panPixels.x += mapPosition.x - after.x;
    settings->panPixels.y += mapPosition.y - after.y;
    ++viewportRevision_;
    InvalidateFrameCache();
}

void CourseOverviewMapMultiViewCoordinator::FrameAll() {
    if (topSettings_.zoom == 1.0f && sideSettings_.zoom == 1.0f &&
        topSettings_.panPixels.x == 0.0f && topSettings_.panPixels.y == 0.0f &&
        sideSettings_.panPixels.x == 0.0f && sideSettings_.panPixels.y == 0.0f) return;
    topSettings_.zoom = sideSettings_.zoom = 1.0f;
    topSettings_.panPixels = sideSettings_.panPixels = {};
    ++viewportRevision_;
    InvalidateFrameCache();
}

bool CourseOverviewMapMultiViewCoordinator::Validate(std::string* errorMessage) const {
    const auto fail = [errorMessage](const char* message) {
        if (errorMessage != nullptr) *errorMessage = message;
        return false;
    };
    if (binding_.rail == nullptr || binding_.rail->Model() == nullptr ||
        binding_.rail->Course() == nullptr || binding_.selection == nullptr) {
        return fail("Multi View requires a valid Rail controller and EditorSelection.");
    }
    const CourseAsset* course = binding_.rail->Course();
    if (binding_.enemies != nullptr && binding_.enemies->Course() != course) {
        return fail("Multi View Enemy controller references another CourseAsset.");
    }
    if (binding_.waves != nullptr && binding_.waves->Course() != course) {
        return fail("Multi View Wave controller references another CourseAsset.");
    }
    return true;
}

float CourseOverviewMapMultiViewCoordinator::PlayheadDistance(float fallback) const {
    return binding_.preview != nullptr && binding_.preview->HasSnapshot()
        ? binding_.preview->Frame().distance : fallback;
}

const CourseRailAuthoringModel* CourseOverviewMapMultiViewCoordinator::RailModel() const {
    return previewRail_.has_value() ? &*previewRail_ : binding_.rail->Model();
}

const CourseEnemyAuthoringModel* CourseOverviewMapMultiViewCoordinator::EnemyModel() const {
    return previewEnemies_.has_value() ? &*previewEnemies_
        : binding_.enemies != nullptr ? binding_.enemies->Model() : nullptr;
}

const CourseWaveAuthoringModel* CourseOverviewMapMultiViewCoordinator::WaveModel() const {
    return previewWaves_.has_value() ? &*previewWaves_
        : binding_.waves != nullptr ? binding_.waves->Model() : nullptr;
}

void CourseOverviewMapMultiViewCoordinator::RefreshCrosshairPositions() {
    if (!state_.crosshair.valid || !topProjection_.State().valid || !sideProjection_.State().valid) return;
    state_.crosshair.topPosition =
        topProjection_.ProjectWorld(state_.crosshair.worldPosition).mapPosition;
    state_.crosshair.sidePosition =
        sideProjection_.ProjectWorld(state_.crosshair.worldPosition).mapPosition;
}

CourseOverviewMapMultiViewCoordinator::RetainedFrameKey
CourseOverviewMapMultiViewCoordinator::BuildFrameKey() const {
    RetainedFrameKey key{};
    key.railRevision = binding_.rail->State().mutationRevision;
    key.railGeneration = binding_.rail->State().bindingGeneration;
    key.hasEnemies = binding_.enemies != nullptr;
    key.hasWaves = binding_.waves != nullptr;
    if (binding_.enemies != nullptr) {
        key.enemyRevision = binding_.enemies->State().mutationRevision;
        key.enemyGeneration = binding_.enemies->State().bindingGeneration;
    }
    if (binding_.waves != nullptr) {
        key.waveRevision = binding_.waves->State().mutationRevision;
        key.waveGeneration = binding_.waves->State().bindingGeneration;
    }
    key.selectionRevision = binding_.selection->Revision();
    key.previewRevision = previewRevision_;
    key.rendererSignature = RendererSignature(renderer_.Style());
    key.viewportRevision = viewportRevision_;
    key.topVisibilitySettingsRevision = topVisibility_.SettingsRevision();
    key.sideVisibilitySettingsRevision = sideVisibility_.SettingsRevision();
    key.viewport = viewport_;
    key.topProjection = topSettings_;
    key.sideProjection = sideSettings_;
    return key;
}

bool CourseOverviewMapMultiViewCoordinator::SameFrameKey(
    const RetainedFrameKey& lhs,
    const RetainedFrameKey& rhs) noexcept {
    return lhs.railRevision == rhs.railRevision &&
        lhs.enemyRevision == rhs.enemyRevision && lhs.waveRevision == rhs.waveRevision &&
        lhs.previewRevision == rhs.previewRevision &&
        lhs.rendererSignature == rhs.rendererSignature &&
        lhs.viewportRevision == rhs.viewportRevision &&
        lhs.topVisibilitySettingsRevision == rhs.topVisibilitySettingsRevision &&
        lhs.sideVisibilitySettingsRevision == rhs.sideVisibilitySettingsRevision &&
        lhs.railGeneration == rhs.railGeneration &&
        lhs.enemyGeneration == rhs.enemyGeneration &&
        lhs.waveGeneration == rhs.waveGeneration &&
        lhs.selectionRevision == rhs.selectionRevision &&
        SameRect(lhs.viewport, rhs.viewport) &&
        SameProjection(lhs.topProjection, rhs.topProjection) &&
        SameProjection(lhs.sideProjection, rhs.sideProjection) &&
        lhs.hasEnemies == rhs.hasEnemies && lhs.hasWaves == rhs.hasWaves;
}

void CourseOverviewMapMultiViewCoordinator::InvalidateFrameCache() noexcept {
    retainedFrameKey_.reset();
}

void CourseOverviewMapMultiViewCoordinator::RefreshPlayheadOverlays(
    float playheadDistance) {
    if (!state_.valid || !topProjection_.State().valid ||
        !sideProjection_.State().valid) {
        topPlayheadOverlay_ = {};
        sidePlayheadOverlay_ = {};
        InvalidatePlayheadOverlays();
        return;
    }
    const PlayheadOverlayKey key{
        state_.frameRevision,
        RendererSignature(renderer_.Style()),
        playheadDistance};
    if (playheadOverlayKey_.has_value() && topPlayheadOverlay_.valid &&
        sidePlayheadOverlay_.valid &&
        playheadOverlayKey_->staticFrameRevision == key.staticFrameRevision &&
        playheadOverlayKey_->rendererSignature == key.rendererSignature &&
        playheadOverlayKey_->playheadDistance == key.playheadDistance) {
        ++state_.playheadOverlayCacheHits;
        return;
    }
    topPlayheadOverlay_ = renderer_.BuildPlayheadOverlay(
        topProjection_, playheadDistance);
    sidePlayheadOverlay_ = renderer_.BuildPlayheadOverlay(
        sideProjection_, playheadDistance);
    const uint64_t revision = ++state_.playheadOverlayRevision;
    topPlayheadOverlay_.revision = revision;
    sidePlayheadOverlay_.revision = revision;
    playheadOverlayKey_ = key;
}

void CourseOverviewMapMultiViewCoordinator::InvalidatePlayheadOverlays() noexcept {
    playheadOverlayKey_.reset();
}

const char* ToString(CourseOverviewMapViewId view) {
    switch (view) {
    case CourseOverviewMapViewId::None: return "None";
    case CourseOverviewMapViewId::Top: return "Top";
    case CourseOverviewMapViewId::Side: return "Side";
    }
    return "Unknown";
}

} // namespace editor
