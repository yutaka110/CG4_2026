#pragma once

#include "../EditorViewportCoordinateService.h"

#include <cstddef>
#include <vector>

namespace editor {

enum class EditorPlacementPlane {
    XZ,
    XY,
    YZ,
};

struct EditorPlacementQuerySettings {
    EditorPlacementPlane plane = EditorPlacementPlane::XZ;
    float planeOffset = 0.0f;
    bool gridSnapEnabled = true;
    float gridSize = 1.0f;
};

struct EditorPlacementQueryResult {
    Vector3 position{};
    Vector3 normal{0.0f, 1.0f, 0.0f};
    EditorViewportCoordinatePoint render{};
    float rayDistance = 0.0f;
    bool usedFallbackPlane = false;
    bool valid = false;
};

class EditorPlacementQueryService {
public:
    EditorPlacementQueryResult QueryDisplay(
        const EditorViewportCoordinateService& coordinates,
        float displayX,
        float displayY,
        const EditorPlacementQuerySettings& settings) const;

    static Vector3 SnapPosition(
        Vector3 position,
        EditorPlacementPlane plane,
        float planeOffset,
        float gridSize);
};

struct EditorBrushStrokeSettings {
    float spacing = 2.0f;
    std::size_t maxSamples = 256;
};

class EditorBrushStrokeSampler {
public:
    void Begin(Vector3 position, const EditorBrushStrokeSettings& settings);
    bool Append(Vector3 position, const EditorBrushStrokeSettings& settings);
    void End() { active_ = false; }
    void Cancel();

    bool Active() const { return active_; }
    bool Empty() const { return samples_.empty(); }
    const std::vector<Vector3>& Samples() const { return samples_; }

private:
    bool active_ = false;
    std::vector<Vector3> samples_;
};

const char* ToString(EditorPlacementPlane plane);

} // namespace editor
