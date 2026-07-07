#pragma once

#include <string>
#include <vector>

#include "CourseAsset.h"
#include "../terrain/RailPath.h"

enum class RailSpeedZoneMode {
    Cruise,
    Combat,
    HighSpeed,
    Tunnel,
    Boss,
    Setpiece,
    Cinematic,
    Recovery,
};

struct RailSpeedDirectorSettings {
    bool enabled = true;
    float minSpeed = 10.0f;
    float maxSpeed = 64.0f;
    float cruiseMultiplier = 1.0f;
    float combatMultiplier = 0.82f;
    float highSpeedMultiplier = 1.26f;
    float tunnelMultiplier = 0.72f;
    float bossMultiplier = 0.76f;
    float setpieceMultiplier = 0.84f;
    float cinematicMultiplier = 0.72f;
    float recoveryMultiplier = 1.04f;
    float acceleration = 18.0f;
    float deceleration = 30.0f;
    float eventSlowMultiplier = 0.76f;
    float eventBoostMultiplier = 1.12f;
    float eventBlendDuration = 1.10f;
};

struct RailSpeedDirectorFrameInput {
    const CourseAsset* course = nullptr;
    const RailPath* railPath = nullptr;
    const CourseSection* section = nullptr;
    float distance = 0.0f;
    float deltaTime = 0.016f;
};

struct RailSpeedDirectorFrame {
    float distance = 0.0f;
    float baseSpeed = 0.0f;
    float targetSpeed = 0.0f;
    float smoothedSpeed = 0.0f;
    float zoneMultiplier = 1.0f;
    float eventMultiplier = 1.0f;
    RailSpeedZoneMode mode = RailSpeedZoneMode::Cruise;
    std::string modeName = "Cruise";
    std::string sectionName = "-";
    std::string reason = "rail";
    bool enabled = true;
};

class RailSpeedDirector {
public:
    void Reset(float speed = 0.0f);
    void NotifyCourseEvents(const std::vector<CourseEventMarker>& events);
    RailSpeedDirectorFrame Evaluate(const RailSpeedDirectorFrameInput& input);

    const RailSpeedDirectorSettings& Settings() const { return settings_; }
    RailSpeedDirectorSettings& MutableSettings() { return settings_; }
    const RailSpeedDirectorFrame& LastFrame() const { return lastFrame_; }

private:
    RailSpeedZoneMode ResolveMode(const CourseSection* section) const;
    float MultiplierForMode(RailSpeedZoneMode mode) const;

    RailSpeedDirectorSettings settings_{};
    RailSpeedDirectorFrame lastFrame_{};
    float smoothedSpeed_ = 0.0f;
    bool hasSmoothedSpeed_ = false;
    float eventSlowTimer_ = 0.0f;
    float eventBoostTimer_ = 0.0f;
};

const char* ToRailSpeedZoneModeString(RailSpeedZoneMode mode);
