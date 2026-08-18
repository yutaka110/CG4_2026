#include "RailVehiclePresentationBridge.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

Vector3 Add(Vector3 a, Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Scale(Vector3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

} // namespace

void RailVehiclePresentationBridge::Reset() {
    frame_ = {};
    smoothedBankDegrees_ = 0.0f;
    smoothedPitchDegrees_ = 0.0f;
    previousJointIndex_ = 0;
    jointInitialized_ = false;
    revision_ = 0;
}

void RailVehiclePresentationBridge::Update(
    const RailVehiclePresentationInput& input) {
    frame_ = {};
    if (!input.settings.enabled || input.definition == nullptr ||
        input.state == nullptr || !input.state->initialized) {
        return;
    }
    const RailVehicleDefinition& definition = *input.definition;
    const RailVehicleRuntimeState& state = *input.state;
    const RailVehiclePresentationSettings& settings = input.settings;
    const float deltaTime = std::isfinite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    const float maximumSpeed = (std::max)(1.0f, definition.maximumSpeed);
    const float speedNormalized = (std::clamp)(state.speed / maximumSpeed, 0.0f, 1.0f);
    const float lateralAcceleration =
        state.signedCurvature * state.speed * state.speed;
    const float bankTarget = (std::clamp)(
        -std::atan2(lateralAcceleration, 9.81f) * 180.0f / kPi,
        -settings.maximumVisualBankDegrees,
        settings.maximumVisualBankDegrees);
    const float gradePitch = std::asin((std::clamp)(state.grade, -1.0f, 1.0f)) *
        180.0f / kPi;
    const float accelerationPitch =
        -(std::clamp)(state.acceleration / definition.emergencyBrakeDeceleration,
            -1.0f, 1.0f) * settings.maximumVisualPitchDegrees * 0.45f;
    const float pitchTarget = (std::clamp)(
        gradePitch + accelerationPitch,
        -settings.maximumVisualPitchDegrees,
        settings.maximumVisualPitchDegrees);
    const float response = 1.0f - std::exp(
        -(std::max)(0.01f, settings.smoothingResponse) * deltaTime);
    smoothedBankDegrees_ += (bankTarget - smoothedBankDegrees_) * response;
    smoothedPitchDegrees_ += (pitchTarget - smoothedPitchDegrees_) * response;

    const float wheelRadius = (std::max)(0.01f, settings.wheelRadius);
    const float jointSpacing = (std::max)(0.05f, settings.railJointSpacing);
    const uint64_t jointIndex = static_cast<uint64_t>(
        (std::max)(0.0f, std::floor(state.distance / jointSpacing)));
    const bool crossedJoint = jointInitialized_ && jointIndex != previousJointIndex_;
    previousJointIndex_ = jointIndex;
    jointInitialized_ = true;
    const float jointPhase = std::fmod(state.distance, jointSpacing) / jointSpacing;
    const float jointPulse = std::exp(-jointPhase * 24.0f);
    const float suspension =
        settings.suspensionAmplitude * speedNormalized * jointPulse;

    frame_.visible = true;
    frame_.forward = state.forward;
    frame_.up = state.up;
    frame_.right = state.right;
    frame_.visualPosition = Add(state.position, Scale(state.up, suspension));
    frame_.wheelRotationRadians = std::fmod(state.distance / wheelRadius, 2.0f * kPi);
    frame_.visualBankDegrees = smoothedBankDegrees_;
    frame_.visualPitchDegrees = smoothedPitchDegrees_;
    frame_.suspensionOffset = suspension;
    frame_.speedNormalized = speedNormalized;
    frame_.rollingAudioVolume = speedNormalized;
    frame_.rollingAudioPitch = 0.82f + speedNormalized * 0.48f;
    frame_.brakeAudioVolume = (std::clamp)(
        -state.acceleration / definition.emergencyBrakeDeceleration,
        0.0f,
        1.0f);
    frame_.jointImpactThisFrame =
        crossedJoint && state.speed >= settings.jointImpactSpeedThreshold;
    frame_.brakeSparksActive =
        state.acceleration <= -settings.brakeSparkDecelerationThreshold &&
        state.speed > 4.0f;
    frame_.crossedJointIndex = jointIndex;
    frame_.sourceVehicleRevision = state.revision;
    frame_.revision = ++revision_;
}

