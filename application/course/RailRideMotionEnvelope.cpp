#include "RailRideMotionEnvelope.h"

#include <algorithm>
#include <cmath>

namespace {

float Dot(Vector3 a, Vector3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Lerp(float a, float b, float t) noexcept {
    return a + (b - a) * (std::clamp)(t, 0.0f, 1.0f);
}

float LookAheadCurvature(
    const RailPath& path,
    float distance,
    float lookAhead) {
    const float span = (std::max)(0.1f, lookAhead);
    const float start = (std::clamp)(distance, 0.0f, path.Length());
    const float end = (std::clamp)(distance + span, 0.0f, path.Length());
    const RailPathSample from = path.Evaluate(start);
    const RailPathSample to = path.Evaluate(end);
    const float traveled = (std::max)(0.1f, end - start);
    return std::acos((std::clamp)(Dot(from.tangent, to.tangent), -1.0f, 1.0f)) /
        traveled;
}

} // namespace

void RailRideMotionEnvelope::Reset() {
    frame_ = {};
    revision_ = 0;
}

const RailRideMotionEnvelopeFrame& RailRideMotionEnvelope::Evaluate(
    const RailRideMotionEnvelopeInput& input) {
    RailRideMotionEnvelopeFrame next{};
    next.revision = ++revision_;
    if (input.vehicleDefinition == nullptr || input.ride == nullptr) {
        frame_ = next;
        return frame_;
    }

    const RailVehicleDefinition& definition = *input.vehicleDefinition;
    const RailRideDirectorFrame& ride = *input.ride;
    next.sourceRideRevision = ride.revision;
    next.sourceRequestedSpeed = (std::clamp)(
        ride.requestedSpeed, 0.0f, definition.maximumSpeed);
    next.requestedSpeed = next.sourceRequestedSpeed;
    next.cornerSafeSpeed = definition.maximumSpeed;
    next.accelerationLimit = definition.acceleration;
    next.brakingLimit = definition.serviceBrakeDeceleration;
    next.jerkLimit = 10000.0f;
    next.active = ride.active;
    if (!ride.active) {
        frame_ = next;
        return frame_;
    }

    const float blend = (std::clamp)(ride.profileBlend, 0.0f, 1.0f);
    next.accelerationLimit = definition.acceleration *
        Lerp(1.0f, ride.accelerationScale, blend);
    next.brakingLimit = definition.serviceBrakeDeceleration *
        Lerp(1.0f, ride.brakingScale, blend);
    next.jerkLimit = blend > 0.0001f
        ? (std::min)(10000.0f, ride.maximumJerk / blend)
        : 10000.0f;
    if (input.railPath != nullptr && input.railPath->Length() > 0.0f) {
        const float distance = input.vehicleState != nullptr
            ? input.vehicleState->distance : ride.distance;
        next.anticipatedCurvature = LookAheadCurvature(
            *input.railPath, distance, ride.cornerEntryLookAheadDistance);
        if (next.anticipatedCurvature > 0.00001f) {
            const float rawSafeSpeed = std::sqrt(
                definition.maximumLateralAcceleration /
                next.anticipatedCurvature) * ride.cornerSpeedScale;
            next.cornerSafeSpeed = Lerp(
                definition.maximumSpeed,
                (std::clamp)(rawSafeSpeed, 0.0f, definition.maximumSpeed),
                blend);
            next.requestedSpeed = (std::min)(
                next.sourceRequestedSpeed, next.cornerSafeSpeed);
            next.cornerLimited =
                next.requestedSpeed + 0.001f < next.sourceRequestedSpeed;
        }
    }
    frame_ = next;
    return frame_;
}
