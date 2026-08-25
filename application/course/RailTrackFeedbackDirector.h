#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "CourseAsset.h"

struct RailTrackFeedbackDirectorSettings final {
    bool enabled = true;
    float distanceDiscontinuityThreshold = 120.0f;
    float maximumAdditiveBankDegrees = 30.0f;
    float maximumSuspensionOffset = 1.5f;
    size_t maximumCuesPerFrame = 8;
    size_t maximumActiveImpulses = 16;
};

struct RailTrackFeedbackInput final {
    const CourseAsset* course = nullptr;
    float previousDistance = 0.0f;
    float distance = 0.0f;
    float speed = 0.0f;
    float maximumSpeed = 1.0f;
    float deltaTime = 0.0f;
    bool gameplayActive = true;
    RailTrackFeedbackDirectorSettings settings{};
};

struct RailTrackFeedbackCue final {
    uint64_t sequence = 0;
    std::string eventGuid;
    CourseRailRideEventType type = CourseRailRideEventType::RailJoint;
    float railDistance = 0.0f;
    float intensity = 0.0f;
    std::string audioCueId;
    std::string vfxCueId;
};

struct RailTrackFeedbackFrame final {
    static constexpr size_t kMaximumCueCount = 8;

    bool valid = false;
    bool active = false;
    bool historyResetThisFrame = false;
    bool bankOverrideActive = false;
    float bankOverrideDegrees = 0.0f;
    float bankOverrideBlend = 0.0f;
    float additiveBankDegrees = 0.0f;
    float suspensionOffset = 0.0f;
    float cameraShake = 0.0f;
    float cameraFovKick = 0.0f;
    float cameraRollKickRadians = 0.0f;
    float hapticLow = 0.0f;
    float hapticHigh = 0.0f;
    std::array<RailTrackFeedbackCue, kMaximumCueCount> cues{};
    size_t cueCount = 0;
    uint64_t sourceCourseRevision = 0;
    uint64_t revision = 0;
};

// Evaluates continuous local track feel and detects point crossings. It never
// changes rail distance or vehicle state, and emits bounded presentation-only
// outputs suitable for retry-safe audio/VFX/haptic dispatch.
class RailTrackFeedbackDirector final {
public:
    void Reset();
    const RailTrackFeedbackFrame& Update(const RailTrackFeedbackInput& input);
    const RailTrackFeedbackFrame& Frame() const noexcept { return frame_; }

private:
    struct ActiveImpulse final {
        CourseRailRideEventDefinition event{};
        float remainingSeconds = 0.0f;
        float durationSeconds = 0.0f;
        float intensity = 0.0f;
    };

    void PushCue(
        const CourseRailRideEventDefinition& event,
        float intensity,
        size_t maximumCues);

    RailTrackFeedbackFrame frame_{};
    std::vector<ActiveImpulse> activeImpulses_;
    std::unordered_set<std::string> firedOnceGuids_;
    float elapsedSeconds_ = 0.0f;
    uint64_t nextCueSequence_ = 1;
    uint64_t revision_ = 0;
};
