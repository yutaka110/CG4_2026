#pragma once

#include <cstdint>
#include <string>

#include "CourseRailAuthoringModel.h"
#include "../EditorViewportCoordinateService.h"
#include "../EditorViewportSelectionBridge.h"

namespace editor {

enum class CourseRailPickKind {
    None,
    ControlPoint,
    Segment,
    IncomingTangent,
    OutgoingTangent,
};

struct CourseRailPickingSettings final {
    float controlPointRadiusPixels = 14.0f;
    float segmentRadiusPixels = 9.0f;
    uint32_t subdivisionsPerSegment = 32;
    bool preferControlPoints = true;
    bool includeTangentHandles = true;
    std::string tangentPointGuid;
};

struct CourseRailPickResult final {
    bool hit = false;
    CourseRailPickKind kind = CourseRailPickKind::None;
    std::string guid;
    uint32_t pointIndex = 0;
    uint32_t segmentIndex = 0;
    float normalizedT = 0.0f;
    float screenDistancePixels = 0.0f;
    float depth = 1.0f;
    Vector3 worldPosition{};

    bool IsTangentHandle() const noexcept {
        return kind == CourseRailPickKind::IncomingTangent ||
            kind == CourseRailPickKind::OutgoingTangent;
    }
};

// Screen-space rail picking with deterministic priority and stable GUID output.
// It deliberately uses the same projected curve samples as the viewport
// renderer, avoiding disagreement between the visible line and selectable line.
class CourseRailPickingService final {
public:
    CourseRailPickResult PickDisplay(
        const CourseRailAuthoringModel& model,
        const EditorViewportCoordinateService& coordinates,
        float displayX,
        float displayY,
        const CourseRailPickingSettings& settings = {}) const;

    EditorViewportPickResult ToViewportPick(
        const CourseRailPickResult& pick,
        uint32_t generation) const;
};

const char* ToString(CourseRailPickKind kind);

} // namespace editor
