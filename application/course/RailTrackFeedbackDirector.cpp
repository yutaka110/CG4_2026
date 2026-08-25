#include "RailTrackFeedbackDirector.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;

float SmoothStep(float value) {
    const float t = (std::clamp)(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float EventWeight(
    const CourseRailRideEventDefinition& event,
    float distance) {
    if (!event.enabled || distance < event.startDistance ||
        distance > event.endDistance) return 0.0f;
    float weight = 1.0f;
    if (event.blendInDistance > 0.001f) {
        weight = (std::min)(weight, SmoothStep(
            (distance - event.startDistance) / event.blendInDistance));
    }
    if (event.blendOutDistance > 0.001f) {
        weight = (std::min)(weight, SmoothStep(
            (event.endDistance - distance) / event.blendOutDistance));
    }
    return weight;
}

float SpeedIntensity(
    const CourseRailRideEventDefinition& event,
    float speedNormalized) {
    return (std::clamp)(
        1.0f + ((std::clamp)(speedNormalized, 0.0f, 1.0f) - 0.5f) *
            event.speedInfluence,
        0.15f,
        2.0f);
}
} // namespace

void RailTrackFeedbackDirector::Reset() {
    frame_ = {};
    activeImpulses_.clear();
    firedOnceGuids_.clear();
    elapsedSeconds_ = 0.0f;
    nextCueSequence_ = 1;
    revision_ = 0;
}

const RailTrackFeedbackFrame& RailTrackFeedbackDirector::Update(
    const RailTrackFeedbackInput& input) {
    frame_.cueCount = 0;
    frame_.cues = {};
    RailTrackFeedbackFrame next{};
    next.revision = ++revision_;
    if (!input.settings.enabled || input.course == nullptr ||
        !std::isfinite(input.previousDistance) ||
        !std::isfinite(input.distance)) {
        activeImpulses_.clear();
        frame_ = std::move(next);
        return frame_;
    }

    next.valid = true;
    next.sourceCourseRevision = input.course->railRideEvents.size();
    if (!input.gameplayActive) {
        // Pause mutes outputs without consuming point crossings or destroying
        // active envelopes; the same authored event resumes deterministically.
        frame_ = std::move(next);
        return frame_;
    }
    const float dt = std::isfinite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    elapsedSeconds_ += dt;
    const float traversal = input.distance - input.previousDistance;
    const bool discontinuity = std::abs(traversal) >
        (std::max)(0.1f, input.settings.distanceDiscontinuityThreshold);
    if (discontinuity) {
        activeImpulses_.clear();
        next.historyResetThisFrame = true;
    }
    const float speedNormalized = (std::clamp)(
        input.speed / (std::max)(1.0f, input.maximumSpeed), 0.0f, 1.0f);
    int overridePriority = -1001;

    for (const CourseRailRideEventDefinition& event :
         input.course->railRideEvents) {
        if (!event.enabled) continue;
        const float weight = EventWeight(event, input.distance);
        const float intensity = SpeedIntensity(event, speedNormalized);
        if (weight > 0.0f && event.IsContinuous()) {
            next.active = true;
            if (event.bankMode == CourseRailRideBankMode::Override &&
                event.priority >= overridePriority) {
                overridePriority = event.priority;
                next.bankOverrideActive = true;
                next.bankOverrideDegrees = event.bankDegrees;
                next.bankOverrideBlend = weight;
            } else if (event.bankMode == CourseRailRideBankMode::Additive) {
                next.additiveBankDegrees += event.bankDegrees * weight;
            }
            if (event.type == CourseRailRideEventType::Rumble) {
                const float phase = elapsedSeconds_ * event.rumbleFrequencyHz *
                    2.0f * kPi;
                next.suspensionOffset += std::sin(phase) *
                    event.suspensionAmplitude * event.rumbleAmplitude *
                    weight * intensity;
                next.cameraShake = (std::max)(
                    next.cameraShake,
                    event.cameraShake * event.rumbleAmplitude * weight * intensity);
                next.hapticLow = (std::max)(
                    next.hapticLow, event.hapticLow * weight * intensity);
                next.hapticHigh = (std::max)(
                    next.hapticHigh, event.hapticHigh * weight * intensity);
            }
        }

        const bool crossed = !discontinuity && traversal > 0.0f &&
            input.previousDistance <= event.startDistance &&
            input.distance >= event.startDistance;
        const bool alreadyFired = event.triggerOncePerRun &&
            firedOnceGuids_.contains(event.editorGuid);
        if (!crossed || alreadyFired) continue;
        if (event.triggerOncePerRun) firedOnceGuids_.insert(event.editorGuid);
        PushCue(event, intensity, input.settings.maximumCuesPerFrame);
        if (!event.IsContinuous() &&
            activeImpulses_.size() < input.settings.maximumActiveImpulses) {
            const float duration = (std::clamp)(
                (event.endDistance - event.startDistance) /
                    (std::max)(1.0f, input.speed),
                0.05f,
                1.0f);
            activeImpulses_.push_back({event, duration, duration, intensity});
        }
    }

    for (ActiveImpulse& impulse : activeImpulses_) {
        const float envelope = impulse.durationSeconds > 0.0f
            ? (std::clamp)(impulse.remainingSeconds / impulse.durationSeconds,
                           0.0f, 1.0f)
            : 0.0f;
        const CourseRailRideEventDefinition& event = impulse.event;
        next.active = true;
        if (event.bankMode == CourseRailRideBankMode::Override &&
            event.priority >= overridePriority) {
            overridePriority = event.priority;
            next.bankOverrideActive = true;
            next.bankOverrideDegrees = event.bankDegrees;
            next.bankOverrideBlend = envelope;
        } else if (event.bankMode == CourseRailRideBankMode::Additive) {
            next.additiveBankDegrees += event.bankDegrees * envelope;
        }
        float suspensionSign = 1.0f;
        if (event.type == CourseRailRideEventType::Drop) suspensionSign = -1.0f;
        next.suspensionOffset += event.suspensionAmplitude *
            suspensionSign * envelope * impulse.intensity;
        next.cameraShake = (std::max)(
            next.cameraShake,
            event.cameraShake * envelope * impulse.intensity);
        next.cameraFovKick += event.cameraFovKick * envelope;
        next.cameraRollKickRadians +=
            event.cameraRollKickDegrees * kPi / 180.0f * envelope;
        next.hapticLow = (std::max)(
            next.hapticLow, event.hapticLow * envelope * impulse.intensity);
        next.hapticHigh = (std::max)(
            next.hapticHigh, event.hapticHigh * envelope * impulse.intensity);
        impulse.remainingSeconds = (std::max)(0.0f,
            impulse.remainingSeconds - dt);
    }
    std::erase_if(activeImpulses_, [](const ActiveImpulse& impulse) {
        return impulse.remainingSeconds <= 0.0f;
    });

    next.additiveBankDegrees = (std::clamp)(
        next.additiveBankDegrees,
        -input.settings.maximumAdditiveBankDegrees,
        input.settings.maximumAdditiveBankDegrees);
    next.suspensionOffset = (std::clamp)(
        next.suspensionOffset,
        -input.settings.maximumSuspensionOffset,
        input.settings.maximumSuspensionOffset);
    next.hapticLow = (std::clamp)(next.hapticLow, 0.0f, 1.0f);
    next.hapticHigh = (std::clamp)(next.hapticHigh, 0.0f, 1.0f);
    next.cueCount = frame_.cueCount;
    next.cues = std::move(frame_.cues);
    // PushCue writes into frame_ so preserve the newly generated bounded cues
    // while replacing every other field atomically.
    frame_ = std::move(next);
    return frame_;
}

void RailTrackFeedbackDirector::PushCue(
    const CourseRailRideEventDefinition& event,
    float intensity,
    size_t maximumCues) {
    const size_t budget = (std::min)(
        maximumCues,
        RailTrackFeedbackFrame::kMaximumCueCount);
    if (frame_.cueCount >= budget) return;
    RailTrackFeedbackCue& cue = frame_.cues[frame_.cueCount++];
    cue.sequence = nextCueSequence_++;
    cue.eventGuid = event.editorGuid;
    cue.type = event.type;
    cue.railDistance = event.startDistance;
    cue.intensity = intensity;
    cue.audioCueId = event.audioCueId;
    cue.vfxCueId = event.vfxCueId;
}
