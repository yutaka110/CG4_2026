#include "CourseRideProfileDefinition.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace {

std::string Lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool FiniteInRange(float value, float minimum, float maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

} // namespace

bool CourseRideProfileDefinition::Validate(std::string* errorMessage) const {
    const auto reject = [errorMessage](const char* message) {
        if (errorMessage != nullptr) *errorMessage = message;
        return false;
    };
    if (editorGuid.empty()) return reject("Ride profile editor GUID is empty.");
    if (!std::isfinite(startDistance) || !std::isfinite(endDistance) ||
        endDistance <= startDistance) {
        return reject("Ride profile distance interval is invalid.");
    }
    if (!FiniteInRange(speedMultiplier, 0.0f, 4.0f) ||
        !std::isfinite(targetSpeedOverride) || targetSpeedOverride < -1.0f ||
        targetSpeedOverride > 1000.0f) {
        return reject("Ride profile speed policy is invalid.");
    }
    if (!FiniteInRange(accelerationScale, 0.05f, 4.0f) ||
        !FiniteInRange(brakingScale, 0.05f, 4.0f) ||
        !FiniteInRange(maximumJerk, 0.1f, 2000.0f) ||
        !FiniteInRange(cornerEntryLookAheadDistance, 0.1f, 1000.0f) ||
        !FiniteInRange(cornerSpeedScale, 0.1f, 2.0f)) {
        return reject("Ride profile motion envelope is invalid.");
    }
    if (!FiniteInRange(turnAnticipationDistance, 0.0f, 500.0f) ||
        !FiniteInRange(visualBankScale, 0.0f, 4.0f) ||
        !FiniteInRange(maximumVisualBankDegrees, 0.0f, 60.0f)) {
        return reject("Ride profile turn presentation is invalid.");
    }
    const float length = endDistance - startDistance;
    if (!FiniteInRange(blendInDistance, 0.0f, length) ||
        !FiniteInRange(blendOutDistance, 0.0f, length)) {
        return reject("Ride profile blend distance is invalid.");
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const char* ToCourseRideSpeedModeString(CourseRideSpeedMode mode) {
    switch (mode) {
    case CourseRideSpeedMode::Inherit: return "Inherit";
    case CourseRideSpeedMode::Cruise: return "Cruise";
    case CourseRideSpeedMode::Combat: return "Combat";
    case CourseRideSpeedMode::HighSpeed: return "HighSpeed";
    case CourseRideSpeedMode::Tunnel: return "Tunnel";
    case CourseRideSpeedMode::Boss: return "Boss";
    case CourseRideSpeedMode::Setpiece: return "Setpiece";
    case CourseRideSpeedMode::Cinematic: return "Cinematic";
    case CourseRideSpeedMode::Recovery: return "Recovery";
    }
    return "Inherit";
}

CourseRideSpeedMode ParseCourseRideSpeedMode(std::string_view text) {
    const std::string value = Lower(text);
    if (value == "cruise") return CourseRideSpeedMode::Cruise;
    if (value == "combat") return CourseRideSpeedMode::Combat;
    if (value == "highspeed" || value == "high_speed" || value == "high speed") {
        return CourseRideSpeedMode::HighSpeed;
    }
    if (value == "tunnel") return CourseRideSpeedMode::Tunnel;
    if (value == "boss") return CourseRideSpeedMode::Boss;
    if (value == "setpiece") return CourseRideSpeedMode::Setpiece;
    if (value == "cinematic") return CourseRideSpeedMode::Cinematic;
    if (value == "recovery") return CourseRideSpeedMode::Recovery;
    return CourseRideSpeedMode::Inherit;
}
