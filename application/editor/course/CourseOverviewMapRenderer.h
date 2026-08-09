#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CourseEnemyAuthoringModel.h"
#include "CourseOverviewMapProjection.h"
#include "CourseWaveAuthoringModel.h"
#include "../EditorSelection.h"

namespace editor {

enum class CourseOverviewMapItemKind : uint8_t {
    None,
    RailSegment,
    RailControlPoint,
    EnemyPlacement,
    Wave,
    WavePrewarm,
    WaveTransition,
    Playhead,
};

struct CourseOverviewMapLine final {
    CourseOverviewMapItemKind kind = CourseOverviewMapItemKind::None;
    Vector2 start{};
    Vector2 end{};
    Vector3 worldStart{};
    Vector3 worldEnd{};
    uint32_t color = 0xffffffffu;
    float thickness = 1.0f;
    EditorObjectHandle handle{};
    std::string guid;
    bool selectable = false;
    bool selected = false;
};

struct CourseOverviewMapMarker final {
    CourseOverviewMapItemKind kind = CourseOverviewMapItemKind::None;
    Vector2 position{};
    Vector3 worldPosition{};
    uint32_t color = 0xffffffffu;
    float radius = 4.0f;
    float railDistance = 0.0f;
    EditorObjectHandle handle{};
    std::string guid;
    bool selectable = false;
    bool selected = false;
    bool enabled = true;
    bool locked = false;
};

struct CourseOverviewMapLabel final {
    Vector2 position{};
    uint32_t color = 0xffffffffu;
    std::string text;
    CourseOverviewMapItemKind kind = CourseOverviewMapItemKind::None;
    EditorObjectHandle handle{};
    bool selected = false;
};

struct CourseOverviewMapRenderStats final {
    uint32_t railSegments = 0;
    uint32_t railControlPoints = 0;
    uint32_t enemies = 0;
    uint32_t waves = 0;
    uint32_t transitions = 0;
};

struct CourseOverviewMapFrame final {
    bool valid = false;
    CourseOverviewMapRect rect{};
    std::vector<CourseOverviewMapLine> lines;
    std::vector<CourseOverviewMapMarker> markers;
    std::vector<CourseOverviewMapLabel> labels;
    CourseOverviewMapRenderStats stats{};
    std::string message;
};

// Small per-frame overlay kept separate from the retained course geometry.
// Advancing preview playback updates only this projection result.
struct CourseOverviewMapDynamicPlayheadOverlay final {
    bool valid = false;
    bool visible = false;
    CourseOverviewMapRect rect{};
    Vector2 position{};
    Vector3 worldPosition{};
    uint32_t color = 0xffffffffu;
    float radius = 6.0f;
    float railDistance = 0.0f;
    uint64_t revision = 0;
};

struct CourseOverviewMapStyle final {
    uint32_t railColor = 0xffd89b44u;
    uint32_t railPointColor = 0xffffd37au;
    uint32_t enemyColor = 0xff55dfffu;
    uint32_t waveColor = 0xffffb84fu;
    uint32_t selectedColor = 0xff52ff8au;
    uint32_t disabledColor = 0xff6a6a6au;
    uint32_t lockedColor = 0xffb36cffu;
    uint32_t prewarmColor = 0x8094d8ffu;
    uint32_t transitionColor = 0xb0ffcf69u;
    uint32_t playheadColor = 0xffffffffu;
    float railThickness = 2.5f;
    float pointRadius = 4.5f;
    float enemyRadius = 5.5f;
    float waveRadius = 7.0f;
    uint32_t samplesPerSegment = 20;
    bool showLabels = true;
    bool showDisabled = true;
    bool showTransitions = true;
    bool showPrewarm = true;
};

struct CourseOverviewMapRenderInput final {
    const CourseOverviewMapProjection* projection = nullptr;
    const CourseRailAuthoringModel* rail = nullptr;
    const CourseEnemyAuthoringModel* enemies = nullptr;
    const CourseWaveAuthoringModel* waves = nullptr;
    const EditorSelection* selection = nullptr;
    float playheadDistance = -1.0f;
    uint32_t railGeneration = 0;
    uint32_t enemyGeneration = 0;
    uint32_t waveGeneration = 0;
};

// Builds a retained 2D command frame. Picking consumes this exact frame, so
// visual geometry and interaction can never drift apart.
class CourseOverviewMapRenderer final {
public:
    // Compatibility path that composes the static frame and playhead marker.
    CourseOverviewMapFrame Build(const CourseOverviewMapRenderInput& input) const;
    CourseOverviewMapFrame BuildStatic(const CourseOverviewMapRenderInput& input) const;
    CourseOverviewMapDynamicPlayheadOverlay BuildPlayheadOverlay(
        const CourseOverviewMapProjection& projection,
        float playheadDistance) const;
    void SetStyle(CourseOverviewMapStyle style) { style_ = style; }
    const CourseOverviewMapStyle& Style() const noexcept { return style_; }

private:
    CourseOverviewMapStyle style_{};
};

const char* ToString(CourseOverviewMapItemKind kind);

} // namespace editor
