#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CourseEnemyAuthoringModel.h"
#include "../EditorViewportCoordinateService.h"
#include "../EditorViewportSelectionBridge.h"

namespace editor {

struct CourseEnemyPickingSettings final {
    float markerRadiusPixels = 14.0f;
    bool includeDisabled = true;
    bool includeHidden = false;
    bool includeLocked = true;
};

struct CourseEnemyPickResult final {
    bool hit = false;
    std::string placementGuid;
    std::string actorAssetId;
    uint32_t placementIndex = 0;
    float screenDistancePixels = 0.0f;
    float depth = 1.0f;
    Vector3 worldPosition{};
    bool enabled = true;
    bool locked = false;
};

// Screen-space picking for the exact marker positions emitted by
// CourseEnemyViewportRenderer, with depth tie-breaking and stable GUID output.
class CourseEnemyPickingService final {
public:
    CourseEnemyPickResult PickDisplay(
        const CourseEnemyAuthoringModel& model,
        const EditorViewportCoordinateService& coordinates,
        float displayX,
        float displayY,
        const CourseEnemyPickingSettings& settings = {}) const;

    // Marquee query over the same projected marker centers used by PickDisplay.
    // Results are deterministic (front-to-back, then persistent placement order).
    std::vector<CourseEnemyPickResult> PickDisplayRect(
        const CourseEnemyAuthoringModel& model,
        const EditorViewportCoordinateService& coordinates,
        float displayX0,
        float displayY0,
        float displayX1,
        float displayY1,
        const CourseEnemyPickingSettings& settings = {}) const;

    EditorViewportPickResult ToViewportPick(
        const CourseEnemyPickResult& pick,
        uint32_t generation) const;
};

} // namespace editor
