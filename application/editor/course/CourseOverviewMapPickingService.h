#pragma once

#include <cstddef>
#include <vector>

#include "CourseOverviewMapRenderer.h"

namespace editor {

struct CourseOverviewMapPickSettings final {
    float markerTolerancePixels = 3.0f;
    float lineTolerancePixels = 5.0f;
};

struct CourseOverviewMapPickResult final {
    bool hit = false;
    CourseOverviewMapItemKind kind = CourseOverviewMapItemKind::None;
    EditorObjectHandle handle{};
    std::string guid;
    Vector2 mapPosition{};
    Vector3 worldPosition{};
    float railDistance = 0.0f;
    float distancePixels = 0.0f;
};

// CPU picking over the renderer's retained command frame. This avoids a
// second, subtly different projection or hit proxy implementation.
class CourseOverviewMapPickingService final {
public:
    std::vector<CourseOverviewMapPickResult> PickAll(
        const CourseOverviewMapFrame& frame,
        Vector2 mapPosition,
        CourseOverviewMapPickSettings settings = {}) const;
    CourseOverviewMapPickResult Pick(
        const CourseOverviewMapFrame& frame,
        Vector2 mapPosition,
        std::size_t cycleOffset = 0,
        CourseOverviewMapPickSettings settings = {}) const;
};

} // namespace editor
