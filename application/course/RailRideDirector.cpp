#include "RailRideDirector.h"

#include <algorithm>
#include <cmath>

namespace {

float Dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(Vector3 a, Vector3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

float SmoothStep(float value) {
    const float t = (std::clamp)(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float ProfileWeight(const CourseRideProfileDefinition& profile, float distance) {
    if (!profile.enabled || distance < profile.startDistance || distance > profile.endDistance) {
        return 0.0f;
    }
    float weight = 1.0f;
    if (profile.blendInDistance > 0.001f) {
        weight = (std::min)(weight,
            SmoothStep((distance - profile.startDistance) / profile.blendInDistance));
    }
    if (profile.blendOutDistance > 0.001f) {
        weight = (std::min)(weight,
            SmoothStep((profile.endDistance - distance) / profile.blendOutDistance));
    }
    return weight;
}

float SpeedBeatWeight(const RailRideSpeedBeatDefinition& beat, float distance) {
    if (!beat.enabled || distance < beat.startDistance || distance > beat.endDistance) return 0.0f;
    float weight=1.0f;
    if (beat.blendInDistance>0.001f)
        weight=(std::min)(weight,SmoothStep((distance-beat.startDistance)/beat.blendInDistance));
    if (beat.blendOutDistance>0.001f)
        weight=(std::min)(weight,SmoothStep((beat.endDistance-distance)/beat.blendOutDistance));
    return weight;
}

float AnticipatedCurvature(
    const RailPath& railPath,
    float distance,
    float anticipationDistance) {
    anticipationDistance = (std::max)(0.10f, anticipationDistance);
    const RailPathSample current = railPath.Evaluate(distance);
    const RailPathSample ahead = railPath.Evaluate(distance + anticipationDistance);
    const float cosine = (std::clamp)(Dot(current.tangent, ahead.tangent), -1.0f, 1.0f);
    const float angle = std::acos(cosine);
    const float sign = Dot(Cross(current.tangent, ahead.tangent), current.up) < 0.0f
        ? -1.0f : 1.0f;
    return sign * angle / (std::max)(0.001f, anticipationDistance);
}

} // namespace

void RailRideDirector::Reset() {
    frame_ = {};
    revision_ = 0;
}

const RailRideDirectorFrame& RailRideDirector::Evaluate(
    const RailRideDirectorInput& input) {
    RailRideDirectorFrame next{};
    next.distance = input.distance;
    next.baseRequestedSpeed = (std::max)(0.0f, input.baseRequestedSpeed);
    next.requestedSpeed = next.baseRequestedSpeed;
    next.revision = ++revision_;

    if (input.course == nullptr || input.railPath == nullptr ||
        input.railPath->Length() <= 0.0f) {
        next.reason = "ride source unavailable";
        frame_ = std::move(next);
        return frame_;
    }
    const CourseRideProfileDefinition* profile =
        input.course->FindRideProfile(input.distance);
    if (profile != nullptr && profile->enabled) {
        next.active = true;
        next.profileGuid = profile->editorGuid;
        next.profileName = profile->displayName.empty() ? "Ride Profile" : profile->displayName;
        next.speedMode = profile->speedMode;
        next.profileBlend = ProfileWeight(*profile, input.distance);
        const float desiredSpeed = profile->targetSpeedOverride >= 0.0f
            ? profile->targetSpeedOverride
            : next.baseRequestedSpeed * profile->speedMultiplier;
        next.requestedSpeed = (std::max)(0.0f,
            next.baseRequestedSpeed + (desiredSpeed - next.baseRequestedSpeed) * next.profileBlend);
        next.accelerationScale = profile->accelerationScale;
        next.brakingScale = profile->brakingScale;
        next.maximumJerk = profile->maximumJerk;
        next.cornerEntryLookAheadDistance = profile->cornerEntryLookAheadDistance;
        next.cornerSpeedScale = profile->cornerSpeedScale;
        next.anticipatedSignedCurvature = AnticipatedCurvature(
            *input.railPath, input.distance, profile->turnAnticipationDistance);
        next.visualBankScale = profile->visualBankScale;
        next.maximumVisualBankDegrees = profile->maximumVisualBankDegrees;
        next.cameraShotId = profile->cameraShotId;
        next.cameraShotWeight = profile->cameraShotId.empty() ? 0.0f : next.profileBlend;
        next.reason = profile->targetSpeedOverride >= 0.0f
            ? "authored target speed" : "authored speed multiplier";
    }

    const RailRideSpeedBeatDefinition* beat=input.course->FindRideSpeedBeat(input.distance);
    if (beat != nullptr && beat->enabled) {
        next.active=true;
        next.speedBeatActive=true;
        next.speedBeatGuid=beat->editorGuid;
        next.speedBeatName=beat->displayName.empty()?"Speed Beat":beat->displayName;
        next.speedBeatType=beat->type;
        next.speedBeatBlend=SpeedBeatWeight(*beat,input.distance);
        const float beforeBeat=next.requestedSpeed;
        const float desired=beat->targetSpeedOverride>=0.0f
            ? beat->targetSpeedOverride : beforeBeat*beat->speedMultiplier;
        next.requestedSpeed=(std::max)(0.0f,
            beforeBeat+(desired-beforeBeat)*next.speedBeatBlend);
        next.accelerationScale*=1.0f+(beat->accelerationScale-1.0f)*next.speedBeatBlend;
        next.brakingScale*=1.0f+(beat->brakingScale-1.0f)*next.speedBeatBlend;
        next.maximumJerk=(std::min)(next.maximumJerk,
            next.maximumJerk+(beat->maximumJerk-next.maximumJerk)*next.speedBeatBlend);
        next.reason += next.reason.empty()?"speed beat":" + speed beat";
    }
    frame_ = std::move(next);
    return frame_;
}
