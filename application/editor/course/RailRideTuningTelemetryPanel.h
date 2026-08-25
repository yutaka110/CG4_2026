#pragma once

#include "../../course/RailRideTuningTelemetry.h"

#include <cstdint>
#include <vector>

namespace editor {

class RailRideTuningTelemetryPanel final {
public:
    void Draw(RailRideTuningTelemetry* telemetry);

private:
    void RebuildPlotCache(const RailRideTuningTelemetry& telemetry);

    uint64_t cachedRevision_ = UINT64_MAX;
    int visibleSampleCount_ = 480;
    std::vector<float> actualSpeed_{};
    std::vector<float> requestedSpeed_{};
    std::vector<float> safeSpeed_{};
    std::vector<float> acceleration_{};
    std::vector<float> jerk_{};
    std::vector<float> targetBank_{};
    std::vector<float> visualBank_{};
    std::vector<float> cameraStability_{};
};

} // namespace editor
