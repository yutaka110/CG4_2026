#include "RailRideTuningTelemetry.h"

#include <algorithm>
#include <utility>

RailRideTuningTelemetry::RailRideTuningTelemetry(std::size_t capacity)
    : capacity_((std::max)(capacity, std::size_t{64})) {}

void RailRideTuningTelemetry::Record(
    const RailRideDirectorFrame& ride,
    const RailRideMotionEnvelopeFrame& envelope,
    const RailVehicleRuntimeState& vehicle,
    const RailVehicleRideDynamicsFrame& dynamics,
    const RailCameraDirectorFrame& camera) {
    if (paused_) return;
    RailRideTuningTelemetrySample sample{};
    sample.frameIndex = vehicle.frameIndex;
    sample.distance = vehicle.distance;
    sample.baseRequestedSpeed = ride.baseRequestedSpeed;
    sample.rideRequestedSpeed = ride.requestedSpeed;
    sample.envelopeRequestedSpeed = envelope.requestedSpeed;
    sample.cornerSafeSpeed = envelope.cornerSafeSpeed;
    sample.actualSpeed = vehicle.speed;
    sample.acceleration = vehicle.acceleration;
    sample.accelerationJerk = dynamics.accelerationJerk;
    sample.curvature = vehicle.curvature;
    sample.anticipatedCurvature = envelope.anticipatedCurvature;
    sample.targetBankDegrees = dynamics.targetBankDegrees;
    sample.visualBankDegrees = dynamics.visualBankDegrees;
    sample.visualPitchDegrees = dynamics.visualPitchDegrees;
    sample.visualYawDegrees = dynamics.visualYawDegrees;
    sample.cameraShotWeight = camera.cinematicShotWeight;
    sample.cameraAngularVelocityDegrees = camera.angularVelocityDeg;
    sample.cameraStabilityScore = camera.stabilityScore;
    sample.cornerLimited = envelope.cornerLimited;
    sample.profileName = ride.profileName;
    sample.speedBeatActive = ride.speedBeatActive;
    sample.speedBeatBlend = ride.speedBeatBlend;
    sample.speedBeatName = ride.speedBeatName;
    sample.speedBeatType = ride.speedBeatType;
    sample.cameraShotId = ride.cameraShotId;
    samples_.push_back(std::move(sample));
    while (samples_.size() > capacity_) samples_.pop_front();
    ++revision_;
}

void RailRideTuningTelemetry::Clear() {
    samples_.clear();
    ++revision_;
}
