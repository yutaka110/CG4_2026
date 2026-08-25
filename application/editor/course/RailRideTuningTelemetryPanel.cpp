#include "RailRideTuningTelemetryPanel.h"

#include "../../externals/imgui/imgui.h"
#include "../../course/RailWorldScale.h"

#include <algorithm>
#include <cfloat>

namespace editor {

void RailRideTuningTelemetryPanel::Draw(RailRideTuningTelemetry* telemetry) {
    if (telemetry == nullptr) {
        ImGui::TextDisabled("Ride telemetry runtime is unavailable.");
        return;
    }
    bool paused = telemetry->Paused();
    if (ImGui::Checkbox("Pause Capture", &paused)) telemetry->SetPaused(paused);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) telemetry->Clear();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu samples",
        telemetry->Samples().size(), telemetry->Capacity());
    if (ImGui::SliderInt("Visible Samples", &visibleSampleCount_, 60, 2048)) {
        cachedRevision_ = UINT64_MAX;
    }

    if (telemetry->Samples().empty()) {
        ImGui::TextDisabled("Run or simulate the course to capture ride data.");
        return;
    }
    RebuildPlotCache(*telemetry);
    const RailRideTuningTelemetrySample& current = telemetry->Samples().back();
    ImGui::SeparatorText("Current");
    ImGui::Text("Distance %.2f m (%.2f wu) | Profile %s",
        RailWorldScale::ToMeters(current.distance), current.distance,
        current.profileName.empty() ? "-" : current.profileName.c_str());
    ImGui::Text("Speed Beat %s (%s, blend %.2f)",
        current.speedBeatActive && !current.speedBeatName.empty()
            ? current.speedBeatName.c_str() : "-",
        current.speedBeatActive
            ? ToRailRideSpeedBeatTypeString(current.speedBeatType) : "inactive",
        current.speedBeatBlend);
    ImGui::Text("Speed %.2f m/s (%.1f km/h) | Request %.2f | Corner %.2f",
        RailWorldScale::ToMetersPerSecond(current.actualSpeed),
        RailWorldScale::ToKilometersPerHour(current.actualSpeed),
        RailWorldScale::ToMetersPerSecond(current.envelopeRequestedSpeed),
        RailWorldScale::ToMetersPerSecond(current.cornerSafeSpeed));
    ImGui::Text("Accel %.2f m/s2 | Jerk %.2f m/s3 | Curvature %.5f -> %.5f 1/m",
        RailWorldScale::ToMeters(current.acceleration),
        RailWorldScale::ToMeters(current.accelerationJerk),
        current.curvature * RailWorldScale::kWorldUnitsPerMeter,
        current.anticipatedCurvature * RailWorldScale::kWorldUnitsPerMeter);
    ImGui::Text("Bank %.2f / %.2f deg | Pitch %.2f | Yaw %.2f",
        current.visualBankDegrees, current.targetBankDegrees,
        current.visualPitchDegrees, current.visualYawDegrees);
    ImGui::Text("Shot %s (%.2f) | Camera stability %.2f",
        current.cameraShotId.empty() ? "-" : current.cameraShotId.c_str(),
        current.cameraShotWeight, current.cameraStabilityScore);
    if (current.cornerLimited) {
        ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.22f, 1.0f),
            "CORNER ENVELOPE LIMITING SPEED");
    }

    const ImVec2 plotSize{-1.0f, 86.0f};
    ImGui::SeparatorText("Speed Envelope");
    ImGui::PlotLines("Actual (wu/s)", actualSpeed_.data(),
        static_cast<int>(actualSpeed_.size()), 0, nullptr, 0.0f, FLT_MAX, plotSize);
    ImGui::PlotLines("Requested (wu/s)", requestedSpeed_.data(),
        static_cast<int>(requestedSpeed_.size()), 0, nullptr, 0.0f, FLT_MAX, plotSize);
    ImGui::PlotLines("Corner Safe (wu/s)", safeSpeed_.data(),
        static_cast<int>(safeSpeed_.size()), 0, nullptr, 0.0f, FLT_MAX, plotSize);
    ImGui::SeparatorText("Longitudinal Feel");
    ImGui::PlotLines("Acceleration", acceleration_.data(),
        static_cast<int>(acceleration_.size()), 0, nullptr, -FLT_MAX, FLT_MAX, plotSize);
    ImGui::PlotLines("Jerk", jerk_.data(),
        static_cast<int>(jerk_.size()), 0, nullptr, -FLT_MAX, FLT_MAX, plotSize);
    ImGui::SeparatorText("Ride / Camera Response");
    ImGui::PlotLines("Target Bank", targetBank_.data(),
        static_cast<int>(targetBank_.size()), 0, nullptr, -60.0f, 60.0f, plotSize);
    ImGui::PlotLines("Visual Bank", visualBank_.data(),
        static_cast<int>(visualBank_.size()), 0, nullptr, -60.0f, 60.0f, plotSize);
    ImGui::PlotLines("Camera Stability", cameraStability_.data(),
        static_cast<int>(cameraStability_.size()), 0, nullptr, 0.0f, 1.0f, plotSize);
}

void RailRideTuningTelemetryPanel::RebuildPlotCache(
    const RailRideTuningTelemetry& telemetry) {
    if (cachedRevision_ == telemetry.Revision()) return;
    cachedRevision_ = telemetry.Revision();
    const auto& samples = telemetry.Samples();
    const std::size_t count = (std::min)(
        samples.size(), static_cast<std::size_t>(visibleSampleCount_));
    const std::size_t first = samples.size() - count;
    actualSpeed_.clear(); requestedSpeed_.clear(); safeSpeed_.clear();
    acceleration_.clear(); jerk_.clear(); targetBank_.clear();
    visualBank_.clear(); cameraStability_.clear();
    for (std::size_t index = first; index < samples.size(); ++index) {
        const auto& sample = samples[index];
        actualSpeed_.push_back(sample.actualSpeed);
        requestedSpeed_.push_back(sample.envelopeRequestedSpeed);
        safeSpeed_.push_back(sample.cornerSafeSpeed);
        acceleration_.push_back(sample.acceleration);
        jerk_.push_back(sample.accelerationJerk);
        targetBank_.push_back(sample.targetBankDegrees);
        visualBank_.push_back(sample.visualBankDegrees);
        cameraStability_.push_back(sample.cameraStabilityScore);
    }
}

} // namespace editor
