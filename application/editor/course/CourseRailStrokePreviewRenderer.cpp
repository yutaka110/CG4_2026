#include "CourseRailStrokePreviewRenderer.h"

namespace editor {

CourseRailStrokePreviewFrame CourseRailStrokePreviewRenderer::Build(
    const CourseRailSketchTool& tool,
    const CourseOverviewMapProjection& projection) const {
    CourseRailStrokePreviewFrame frame{};
    const auto& raw = tool.RawMapSamples();
    const auto& fitted = tool.FittedControlPoints();
    frame.visible = tool.State().active && (tool.State().drawing || !raw.empty());
    frame.valid = tool.State().previewValid;
    frame.rawSamples = static_cast<uint32_t>(raw.size());
    frame.fittedControlPoints = static_cast<uint32_t>(fitted.size());
    frame.message = tool.State().message;
    if (!frame.visible) return frame;
    const uint32_t rawColor = frame.valid ? style_.rawStrokeColor : style_.invalidColor;
    for (std::size_t index = 1; index < raw.size(); ++index) {
        frame.lines.push_back({raw[index - 1], raw[index], rawColor, style_.rawThickness});
    }
    std::vector<Vector2> fittedMap;
    fittedMap.reserve(fitted.size());
    for (const RailPathControlPoint& point : fitted) {
        const auto projected = projection.ProjectWorld(point.position);
        if (!projected.valid) continue;
        fittedMap.push_back(projected.mapPosition);
        frame.markers.push_back({projected.mapPosition,
            frame.valid ? style_.controlPointColor : style_.invalidColor, 3.5f});
    }
    for (std::size_t index = 1; index < fittedMap.size(); ++index) {
        frame.lines.push_back({fittedMap[index - 1], fittedMap[index],
            frame.valid ? style_.fittedCurveColor : style_.invalidColor,
            style_.fittedThickness});
    }
    return frame;
}

} // namespace editor
