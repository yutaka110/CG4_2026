#pragma once

#include <string>
#include <string_view>

enum class CourseRideSpeedMode {
    Inherit,
    Cruise,
    Combat,
    HighSpeed,
    Tunnel,
    Boss,
    Setpiece,
    Cinematic,
    Recovery,
};

// Persistent authoring source of truth for the intended feel of one rail
// interval. Runtime integration remains owned by RailVehicleMovementSystem.
struct CourseRideProfileDefinition final {
    std::string editorGuid;
    std::string displayName = "Ride Profile";
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    CourseRideSpeedMode speedMode = CourseRideSpeedMode::Inherit;
    float speedMultiplier = 1.0f;
    // A negative value means "use the upstream speed request".
    float targetSpeedOverride = -1.0f;
    // Motion-envelope constraints. These shape the requested acceleration
    // without taking speed integration away from RailVehicleMovementSystem.
    float accelerationScale = 1.0f;
    float brakingScale = 1.0f;
    float maximumJerk = 120.0f;
    float cornerEntryLookAheadDistance = 48.0f;
    float cornerSpeedScale = 1.0f;
    float turnAnticipationDistance = 24.0f;
    float visualBankScale = 1.0f;
    float maximumVisualBankDegrees = 18.0f;
    float blendInDistance = 16.0f;
    float blendOutDistance = 16.0f;
    std::string cameraShotId;
    bool enabled = true;
    bool editorVisible = true;
    bool editorLocked = false;

    bool Validate(std::string* errorMessage = nullptr) const;
};

const char* ToCourseRideSpeedModeString(CourseRideSpeedMode mode);
CourseRideSpeedMode ParseCourseRideSpeedMode(std::string_view text);
