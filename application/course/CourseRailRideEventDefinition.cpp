#include "CourseRailRideEventDefinition.h"

#include <cmath>

namespace {
bool FiniteRange(float value, float minimum, float maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
bool SafeToken(const std::string& value) {
    if (value.size() > 128) return false;
    for (unsigned char character : value) {
        if (character == '|' || character == '\n' || character == '\r') return false;
    }
    return true;
}
} // namespace

bool CourseRailRideEventDefinition::Validate(
    std::string* errorMessage) const {
    const auto reject = [errorMessage](const char* message) {
        if (errorMessage != nullptr) *errorMessage = message;
        return false;
    };
    if (editorGuid.empty() || !SafeToken(editorGuid))
        return reject("Rail Ride Event requires a safe persistent editor GUID.");
    if (!SafeToken(displayName) || !SafeToken(audioCueId) || !SafeToken(vfxCueId))
        return reject("Rail Ride Event text fields contain unsupported characters.");
    if (!std::isfinite(startDistance) || !std::isfinite(endDistance) ||
        startDistance < 0.0f || endDistance <= startDistance)
        return reject("Rail Ride Event range is invalid.");
    if (!FiniteRange(bankDegrees, -45.0f, 45.0f) ||
        !FiniteRange(rumbleAmplitude, 0.0f, 2.0f) ||
        !FiniteRange(rumbleFrequencyHz, 0.1f, 80.0f) ||
        !FiniteRange(suspensionAmplitude, -2.0f, 2.0f))
        return reject("Rail Ride Event bank or vibration tuning is outside commercial limits.");
    if (!FiniteRange(cameraShake, 0.0f, 5.0f) ||
        !FiniteRange(cameraFovKick, -0.10f, 0.10f) ||
        !FiniteRange(cameraRollKickDegrees, -20.0f, 20.0f) ||
        !FiniteRange(hapticLow, 0.0f, 1.0f) ||
        !FiniteRange(hapticHigh, 0.0f, 1.0f))
        return reject("Rail Ride Event camera or haptic tuning is invalid.");
    if (!FiniteRange(speedInfluence, 0.0f, 2.0f) ||
        !FiniteRange(blendInDistance, 0.0f, 1000.0f) ||
        !FiniteRange(blendOutDistance, 0.0f, 1000.0f) ||
        priority < -1000 || priority > 1000)
        return reject("Rail Ride Event blending, speed influence or priority is invalid.");
    if (static_cast<int>(type) <
            static_cast<int>(CourseRailRideEventType::BankOverride) ||
        static_cast<int>(type) >
            static_cast<int>(CourseRailRideEventType::Landing) ||
        static_cast<int>(bankMode) <
            static_cast<int>(CourseRailRideBankMode::None) ||
        static_cast<int>(bankMode) >
            static_cast<int>(CourseRailRideBankMode::Override))
        return reject("Rail Ride Event contains an invalid enum value.");
    if (type == CourseRailRideEventType::BankOverride &&
        bankMode == CourseRailRideBankMode::None)
        return reject("Bank Override event requires Additive or Override bank mode.");
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool CourseRailRideEventDefinition::IsContinuous() const noexcept {
    return type == CourseRailRideEventType::BankOverride ||
        type == CourseRailRideEventType::Rumble;
}

const char* ToCourseRailRideEventTypeString(
    CourseRailRideEventType type) noexcept {
    switch (type) {
    case CourseRailRideEventType::BankOverride: return "bank_override";
    case CourseRailRideEventType::BankImpulse: return "bank_impulse";
    case CourseRailRideEventType::Rumble: return "rumble";
    case CourseRailRideEventType::RailJoint: return "rail_joint";
    case CourseRailRideEventType::Drop: return "drop";
    case CourseRailRideEventType::Landing: return "landing";
    }
    return "rail_joint";
}

CourseRailRideEventType ParseCourseRailRideEventType(
    std::string_view text) noexcept {
    if (text == "bank_override") return CourseRailRideEventType::BankOverride;
    if (text == "bank_impulse") return CourseRailRideEventType::BankImpulse;
    if (text == "rumble") return CourseRailRideEventType::Rumble;
    if (text == "drop") return CourseRailRideEventType::Drop;
    if (text == "landing") return CourseRailRideEventType::Landing;
    return CourseRailRideEventType::RailJoint;
}

const char* ToCourseRailRideBankModeString(
    CourseRailRideBankMode mode) noexcept {
    switch (mode) {
    case CourseRailRideBankMode::None: return "none";
    case CourseRailRideBankMode::Additive: return "additive";
    case CourseRailRideBankMode::Override: return "override";
    }
    return "none";
}

CourseRailRideBankMode ParseCourseRailRideBankMode(
    std::string_view text) noexcept {
    if (text == "additive") return CourseRailRideBankMode::Additive;
    if (text == "override") return CourseRailRideBankMode::Override;
    return CourseRailRideBankMode::None;
}
