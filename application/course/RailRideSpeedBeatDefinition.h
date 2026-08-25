#pragma once

#include <string>

enum class RailRideSpeedBeatType {
    Approach,
    CombatHold,
    CornerBrake,
    ReleaseBoost,
    SetpieceHold,
    ExitBoost,
};

struct RailRideSpeedBeatDefinition final {
    std::string editorGuid;
    std::string displayName = "Speed Beat";
    float startDistance = 0.0f;
    float endDistance = 1.0f;
    RailRideSpeedBeatType type = RailRideSpeedBeatType::Approach;
    float speedMultiplier = 1.0f;
    float targetSpeedOverride = -1.0f;
    float accelerationScale = 1.0f;
    float brakingScale = 1.0f;
    float maximumJerk = 120.0f;
    float blendInDistance = 8.0f;
    float blendOutDistance = 8.0f;
    int priority = 0;
    bool enabled = true;
    bool editorVisible = true;
    bool editorLocked = false;

    bool Validate(std::string* errorMessage = nullptr) const;
};

const char* ToRailRideSpeedBeatTypeString(RailRideSpeedBeatType type) noexcept;
RailRideSpeedBeatType ParseRailRideSpeedBeatType(const std::string& text) noexcept;
