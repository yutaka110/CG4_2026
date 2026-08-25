#pragma once

#include <cstdint>
#include <string>

#include "CourseAsset.h"
#include "../terrain/RailPath.h"

struct RailRideDirectorInput final {
    const CourseAsset* course = nullptr;
    const RailPath* railPath = nullptr;
    float distance = 0.0f;
    float baseRequestedSpeed = 0.0f;
};

struct RailRideDirectorFrame final {
    bool active = false;
    bool speedBeatActive = false;
    float distance = 0.0f;
    float profileBlend = 0.0f;
    float speedBeatBlend = 0.0f;
    float baseRequestedSpeed = 0.0f;
    float requestedSpeed = 0.0f;
    float accelerationScale = 1.0f;
    float brakingScale = 1.0f;
    float maximumJerk = 120.0f;
    float cornerEntryLookAheadDistance = 48.0f;
    float cornerSpeedScale = 1.0f;
    float anticipatedSignedCurvature = 0.0f;
    float visualBankScale = 1.0f;
    float maximumVisualBankDegrees = 18.0f;
    float cameraShotWeight = 0.0f;
    CourseRideSpeedMode speedMode = CourseRideSpeedMode::Inherit;
    std::string profileGuid;
    std::string profileName = "-";
    std::string cameraShotId;
    std::string speedBeatGuid;
    std::string speedBeatName = "-";
    RailRideSpeedBeatType speedBeatType = RailRideSpeedBeatType::Approach;
    std::string reason = "no ride profile";
    uint64_t revision = 0;
};

// Converts authored ride intent into policy/presentation outputs. It never
// advances distance or integrates velocity.
class RailRideDirector final {
public:
    void Reset();
    const RailRideDirectorFrame& Evaluate(const RailRideDirectorInput& input);
    const RailRideDirectorFrame& LastFrame() const noexcept { return frame_; }

private:
    RailRideDirectorFrame frame_{};
    uint64_t revision_ = 0;
};
