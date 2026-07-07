#include "RailSpeedDirector.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
bool ContainsInsensitive(std::string value, const char* token) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::string needle = token;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value.find(needle) != std::string::npos;
}

float Approach(float current, float target, float maxDelta) {
    if (current < target) {
        return (std::min)(current + maxDelta, target);
    }
    return (std::max)(current - maxDelta, target);
}
} // namespace

const char* ToRailSpeedZoneModeString(RailSpeedZoneMode mode) {
    switch (mode) {
    case RailSpeedZoneMode::Cruise:
        return "Cruise";
    case RailSpeedZoneMode::Combat:
        return "Combat";
    case RailSpeedZoneMode::HighSpeed:
        return "High Speed";
    case RailSpeedZoneMode::Tunnel:
        return "Tunnel";
    case RailSpeedZoneMode::Boss:
        return "Boss";
    case RailSpeedZoneMode::Setpiece:
        return "Setpiece";
    case RailSpeedZoneMode::Cinematic:
        return "Cinematic";
    case RailSpeedZoneMode::Recovery:
        return "Recovery";
    }
    return "Cruise";
}

void RailSpeedDirector::Reset(float speed) {
    smoothedSpeed_ = (std::max)(0.0f, speed);
    hasSmoothedSpeed_ = speed > 0.0f;
    eventSlowTimer_ = 0.0f;
    eventBoostTimer_ = 0.0f;
    lastFrame_ = {};
    lastFrame_.smoothedSpeed = smoothedSpeed_;
}

void RailSpeedDirector::NotifyCourseEvents(const std::vector<CourseEventMarker>& events) {
    for (const CourseEventMarker& event : events) {
        const std::string key = event.type + " " + event.id + " " + event.payload;
        if (ContainsInsensitive(key, "boss") ||
            ContainsInsensitive(key, "setpiece") ||
            ContainsInsensitive(key, "obstacle") ||
            ContainsInsensitive(key, "checkpoint")) {
            eventSlowTimer_ = (std::max)(eventSlowTimer_, settings_.eventBlendDuration);
        }
        if (ContainsInsensitive(key, "escape") ||
            ContainsInsensitive(key, "boost") ||
            ContainsInsensitive(key, "recovery")) {
            eventBoostTimer_ = (std::max)(eventBoostTimer_, settings_.eventBlendDuration);
        }
    }
}

RailSpeedDirectorFrame RailSpeedDirector::Evaluate(const RailSpeedDirectorFrameInput& input) {
    RailSpeedDirectorFrame frame{};
    frame.distance = input.distance;
    frame.enabled = settings_.enabled;
    if (input.section != nullptr) {
        frame.sectionName = input.section->name;
    }

    if (input.railPath == nullptr || input.railPath->Length() <= 0.0f) {
        lastFrame_ = frame;
        return frame;
    }

    const float dt = (std::max)(0.0f, input.deltaTime);
    const RailPathSample sample = input.railPath->Evaluate(input.distance);
    frame.baseSpeed = (std::max)(settings_.minSpeed, sample.speed);
    frame.mode = ResolveMode(input.section);
    frame.modeName = ToRailSpeedZoneModeString(frame.mode);
    frame.zoneMultiplier = settings_.enabled ? MultiplierForMode(frame.mode) : 1.0f;

    eventSlowTimer_ = (std::max)(0.0f, eventSlowTimer_ - dt);
    eventBoostTimer_ = (std::max)(0.0f, eventBoostTimer_ - dt);
    const float slowWeight = settings_.eventBlendDuration > 0.001f
        ? (std::clamp)(eventSlowTimer_ / settings_.eventBlendDuration, 0.0f, 1.0f)
        : 0.0f;
    const float boostWeight = settings_.eventBlendDuration > 0.001f
        ? (std::clamp)(eventBoostTimer_ / settings_.eventBlendDuration, 0.0f, 1.0f)
        : 0.0f;
    const float slowMultiplier = 1.0f + (settings_.eventSlowMultiplier - 1.0f) * slowWeight;
    const float boostMultiplier = 1.0f + (settings_.eventBoostMultiplier - 1.0f) * boostWeight;
    frame.eventMultiplier = slowMultiplier * boostMultiplier;

    frame.targetSpeed = (std::clamp)(
        frame.baseSpeed * frame.zoneMultiplier * frame.eventMultiplier,
        settings_.minSpeed,
        settings_.maxSpeed);
    if (!hasSmoothedSpeed_) {
        smoothedSpeed_ = frame.targetSpeed;
        hasSmoothedSpeed_ = true;
    } else {
        const float rate = frame.targetSpeed >= smoothedSpeed_
            ? settings_.acceleration
            : settings_.deceleration;
        smoothedSpeed_ = Approach(smoothedSpeed_, frame.targetSpeed, (std::max)(0.0f, rate) * dt);
    }
    frame.smoothedSpeed = settings_.enabled ? smoothedSpeed_ : frame.baseSpeed;
    frame.reason = settings_.enabled ? "zone/event director" : "rail speed passthrough";

    lastFrame_ = frame;
    return frame;
}

RailSpeedZoneMode RailSpeedDirector::ResolveMode(const CourseSection* section) const {
    if (section == nullptr) {
        return RailSpeedZoneMode::Cruise;
    }

    const std::string key = section->name + " " + section->category;
    if (ContainsInsensitive(key, "boss")) {
        return RailSpeedZoneMode::Boss;
    }
    if (ContainsInsensitive(key, "tunnel") || ContainsInsensitive(key, "obstacle")) {
        return RailSpeedZoneMode::Tunnel;
    }
    if (ContainsInsensitive(key, "escape") || ContainsInsensitive(key, "high speed")) {
        return RailSpeedZoneMode::HighSpeed;
    }
    if (ContainsInsensitive(key, "setpiece") || ContainsInsensitive(key, "falling")) {
        return RailSpeedZoneMode::Setpiece;
    }
    if (ContainsInsensitive(key, "finale") || ContainsInsensitive(key, "cinematic")) {
        return RailSpeedZoneMode::Cinematic;
    }
    if (ContainsInsensitive(key, "combat") || ContainsInsensitive(key, "contact") ||
        ContainsInsensitive(key, "crossfire") || ContainsInsensitive(key, "pressure")) {
        return RailSpeedZoneMode::Combat;
    }
    if (ContainsInsensitive(key, "recovery")) {
        return RailSpeedZoneMode::Recovery;
    }
    return RailSpeedZoneMode::Cruise;
}

float RailSpeedDirector::MultiplierForMode(RailSpeedZoneMode mode) const {
    switch (mode) {
    case RailSpeedZoneMode::Cruise:
        return settings_.cruiseMultiplier;
    case RailSpeedZoneMode::Combat:
        return settings_.combatMultiplier;
    case RailSpeedZoneMode::HighSpeed:
        return settings_.highSpeedMultiplier;
    case RailSpeedZoneMode::Tunnel:
        return settings_.tunnelMultiplier;
    case RailSpeedZoneMode::Boss:
        return settings_.bossMultiplier;
    case RailSpeedZoneMode::Setpiece:
        return settings_.setpieceMultiplier;
    case RailSpeedZoneMode::Cinematic:
        return settings_.cinematicMultiplier;
    case RailSpeedZoneMode::Recovery:
        return settings_.recoveryMultiplier;
    }
    return settings_.cruiseMultiplier;
}
