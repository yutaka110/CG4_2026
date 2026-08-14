#pragma once

#include "CourseMap3DViewportRenderer.h"

#include <cstddef>
#include <vector>

namespace editor {

struct CourseMap3DPickSettings final {
    float minimumLineRadius = 0.25f;
    float lineTolerancePixels = 7.0f;
};

struct CourseMap3DPickResult final {
    bool hit = false;
    CourseOverviewMapItemKind kind = CourseOverviewMapItemKind::None;
    EditorObjectHandle handle{};
    std::string guid;
    CourseMap3DRay ray{};
    Vector3 worldPosition{};
    float railDistance = 0.0f;
    float rayDistance = 0.0f;
    float missDistance = 0.0f;
};

// Perspective world-ray picking over the renderer's retained proxies. Marker
// spheres use screen-stable world radii; rail lines are tested as capsules.
class CourseMap3DPickingService final {
public:
    std::vector<CourseMap3DPickResult> PickAll(
        const CourseMap3DFrame& frame,
        Vector2 screenPosition,
        CourseMap3DPickSettings settings = {}) const;
    CourseMap3DPickResult Pick(
        const CourseMap3DFrame& frame,
        Vector2 screenPosition,
        std::size_t cycleOffset = 0,
        CourseMap3DPickSettings settings = {}) const;
};

} // namespace editor
