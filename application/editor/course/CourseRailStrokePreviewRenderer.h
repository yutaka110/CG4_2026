#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CourseRailSketchTool.h"

namespace editor {

struct CourseRailStrokePreviewLine final {
    Vector2 start{};
    Vector2 end{};
    uint32_t color = 0xffffffffu;
    float thickness = 1.0f;
};

struct CourseRailStrokePreviewMarker final {
    Vector2 position{};
    uint32_t color = 0xffffffffu;
    float radius = 3.0f;
};

struct CourseRailStrokePreviewStyle final {
    uint32_t rawStrokeColor = 0x80ffffffu;
    uint32_t fittedCurveColor = 0xff52ff8au;
    uint32_t controlPointColor = 0xffffd37au;
    uint32_t invalidColor = 0xff4b4bffu;
    float rawThickness = 1.5f;
    float fittedThickness = 3.0f;
};

struct CourseRailStrokePreviewFrame final {
    bool visible = false;
    bool valid = false;
    std::vector<CourseRailStrokePreviewLine> lines;
    std::vector<CourseRailStrokePreviewMarker> markers;
    uint32_t rawSamples = 0;
    uint32_t fittedControlPoints = 0;
    std::string message;
};

// Retained overlay renderer for raw pointer ink versus the fitted rail curve.
// The distinction makes simplification and validation visible before commit.
class CourseRailStrokePreviewRenderer final {
public:
    CourseRailStrokePreviewFrame Build(
        const CourseRailSketchTool& tool,
        const CourseOverviewMapProjection& projection) const;
    void SetStyle(CourseRailStrokePreviewStyle style) { style_ = style; }
    const CourseRailStrokePreviewStyle& Style() const noexcept { return style_; }

private:
    CourseRailStrokePreviewStyle style_{};
};

} // namespace editor
