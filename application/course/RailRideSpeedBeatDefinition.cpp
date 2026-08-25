#include "RailRideSpeedBeatDefinition.h"

#include <cmath>

bool RailRideSpeedBeatDefinition::Validate(std::string* errorMessage) const {
    const auto fail=[errorMessage](const char* message) {
        if (errorMessage) *errorMessage=message;
        return false;
    };
    if (editorGuid.empty()) return fail("Speed Beat requires a persistent editor GUID.");
    if (!std::isfinite(startDistance) || !std::isfinite(endDistance) ||
        startDistance<0.0f || endDistance<=startDistance)
        return fail("Speed Beat range is invalid.");
    if (!std::isfinite(speedMultiplier) || speedMultiplier<=0.0f)
        return fail("Speed Beat multiplier must be positive.");
    if (!std::isfinite(targetSpeedOverride) || targetSpeedOverride < -1.0f)
        return fail("Speed Beat target override must be -1 or non-negative.");
    if (!std::isfinite(accelerationScale) || accelerationScale<=0.0f ||
        !std::isfinite(brakingScale) || brakingScale<=0.0f)
        return fail("Speed Beat acceleration/braking scales must be positive.");
    if (!std::isfinite(maximumJerk) || maximumJerk<=0.0f)
        return fail("Speed Beat maximum jerk must be positive.");
    if (!std::isfinite(blendInDistance) || blendInDistance<0.0f ||
        !std::isfinite(blendOutDistance) || blendOutDistance<0.0f)
        return fail("Speed Beat blend distances cannot be negative.");
    return true;
}

const char* ToRailRideSpeedBeatTypeString(RailRideSpeedBeatType type) noexcept {
    switch(type) {
    case RailRideSpeedBeatType::Approach: return "approach";
    case RailRideSpeedBeatType::CombatHold: return "combat_hold";
    case RailRideSpeedBeatType::CornerBrake: return "corner_brake";
    case RailRideSpeedBeatType::ReleaseBoost: return "release_boost";
    case RailRideSpeedBeatType::SetpieceHold: return "setpiece_hold";
    case RailRideSpeedBeatType::ExitBoost: return "exit_boost";
    }
    return "approach";
}

RailRideSpeedBeatType ParseRailRideSpeedBeatType(const std::string& text) noexcept {
    if (text=="combat_hold") return RailRideSpeedBeatType::CombatHold;
    if (text=="corner_brake") return RailRideSpeedBeatType::CornerBrake;
    if (text=="release_boost") return RailRideSpeedBeatType::ReleaseBoost;
    if (text=="setpiece_hold") return RailRideSpeedBeatType::SetpieceHold;
    if (text=="exit_boost") return RailRideSpeedBeatType::ExitBoost;
    return RailRideSpeedBeatType::Approach;
}
