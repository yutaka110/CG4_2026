#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

#include "RailCameraDirector.h"
#include "RailRideDirector.h"
#include "RailRideMotionEnvelope.h"
#include "RailVehicleRideDynamicsSystem.h"

struct RailRideTuningTelemetrySample final {
    uint64_t frameIndex = 0;
    float distance = 0.0f;
    float baseRequestedSpeed = 0.0f;
    float rideRequestedSpeed = 0.0f;
    float envelopeRequestedSpeed = 0.0f;
    float cornerSafeSpeed = 0.0f;
    float actualSpeed = 0.0f;
    float acceleration = 0.0f;
    float accelerationJerk = 0.0f;
    float curvature = 0.0f;
    float anticipatedCurvature = 0.0f;
    float targetBankDegrees = 0.0f;
    float visualBankDegrees = 0.0f;
    float visualPitchDegrees = 0.0f;
    float visualYawDegrees = 0.0f;
    float cameraShotWeight = 0.0f;
    float cameraAngularVelocityDegrees = 0.0f;
    float cameraStabilityScore = 1.0f;
    bool cornerLimited = false;
    std::string profileName;
    bool speedBeatActive = false;
    float speedBeatBlend = 0.0f;
    std::string speedBeatName;
    RailRideSpeedBeatType speedBeatType = RailRideSpeedBeatType::Approach;
    std::string cameraShotId;
};

class RailRideTuningTelemetry final {
public:
    explicit RailRideTuningTelemetry(std::size_t capacity = 2048);

    void Record(
        const RailRideDirectorFrame& ride,
        const RailRideMotionEnvelopeFrame& envelope,
        const RailVehicleRuntimeState& vehicle,
        const RailVehicleRideDynamicsFrame& dynamics,
        const RailCameraDirectorFrame& camera);
    void Clear();
    void SetPaused(bool paused) noexcept { paused_ = paused; }
    bool Paused() const noexcept { return paused_; }
    std::size_t Capacity() const noexcept { return capacity_; }
    uint64_t Revision() const noexcept { return revision_; }
    const std::deque<RailRideTuningTelemetrySample>& Samples() const noexcept {
        return samples_;
    }

private:
    std::size_t capacity_ = 2048;
    bool paused_ = false;
    uint64_t revision_ = 0;
    std::deque<RailRideTuningTelemetrySample> samples_{};
};
