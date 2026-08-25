#pragma once

#include <cstdint>
#include <string>
#include <string_view>

enum class CourseRailRideEventType : uint8_t {
    BankOverride,
    BankImpulse,
    Rumble,
    RailJoint,
    Drop,
    Landing,
};

enum class CourseRailRideBankMode : uint8_t {
    None,
    Additive,
    Override,
};

// Persistent distance-authored source of truth for local track feel. Ride
// Profile still owns continuous curve-derived intent; these events layer
// deliberate local banking, vibration and one-shot feedback over it.
struct CourseRailRideEventDefinition final {
    std::string editorGuid;
    std::string displayName = "Rail Ride Event";
    float startDistance = 0.0f;
    float endDistance = 0.25f;
    CourseRailRideEventType type = CourseRailRideEventType::RailJoint;
    CourseRailRideBankMode bankMode = CourseRailRideBankMode::None;
    float bankDegrees = 0.0f;
    float rumbleAmplitude = 0.35f;
    float rumbleFrequencyHz = 12.0f;
    float suspensionAmplitude = 0.08f;
    float cameraShake = 0.055f;
    float cameraFovKick = 0.0f;
    float cameraRollKickDegrees = 0.0f;
    float hapticLow = 0.18f;
    float hapticHigh = 0.12f;
    float speedInfluence = 0.65f;
    float blendInDistance = 0.0f;
    float blendOutDistance = 0.0f;
    int priority = 0;
    std::string audioCueId = "rail_joint";
    std::string vfxCueId;
    bool triggerOncePerRun = false;
    bool enabled = true;
    bool editorVisible = true;
    bool editorLocked = false;

    bool Validate(std::string* errorMessage = nullptr) const;
    bool IsContinuous() const noexcept;
};

const char* ToCourseRailRideEventTypeString(
    CourseRailRideEventType type) noexcept;
CourseRailRideEventType ParseCourseRailRideEventType(
    std::string_view text) noexcept;
const char* ToCourseRailRideBankModeString(
    CourseRailRideBankMode mode) noexcept;
CourseRailRideBankMode ParseCourseRailRideBankMode(
    std::string_view text) noexcept;
