#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CourseRailAuthoringModel.h"
#include "CourseWaveAuthoringModel.h"
#include "../EditorViewportCoordinateService.h"
#include "../EditorViewportSelectionBridge.h"

namespace editor {

struct CourseWavePickingSettings final {
    float markerRadiusPixels = 16.0f;
    bool includeDisabled = true;
    bool includeHidden = false;
    bool includeLocked = true;
};

struct CourseWavePickResult final {
    bool hit = false;
    std::string waveGuid;
    std::string displayName;
    uint32_t waveIndex = 0;
    float screenDistancePixels = 0.0f;
    float depth = 1.0f;
    Vector3 worldPosition{};
    bool enabled = true;
    bool locked = false;
};

class CourseWavePickingService final {
public:
    CourseWavePickResult PickDisplay(
        const CourseWaveAuthoringModel& model,
        const CourseRailAuthoringModel& rail,
        const EditorViewportCoordinateService& coordinates,
        float displayX,
        float displayY,
        const CourseWavePickingSettings& settings = {}) const;

    std::vector<CourseWavePickResult> PickDisplayRect(
        const CourseWaveAuthoringModel& model,
        const CourseRailAuthoringModel& rail,
        const EditorViewportCoordinateService& coordinates,
        float displayX0,
        float displayY0,
        float displayX1,
        float displayY1,
        const CourseWavePickingSettings& settings = {}) const;

    EditorViewportPickResult ToViewportPick(
        const CourseWavePickResult& pick,
        uint32_t generation) const;
};

} // namespace editor
