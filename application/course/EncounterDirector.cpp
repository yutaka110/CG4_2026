#include "EncounterDirector.h"

#include "CourseSpawnRuntime.h"

#include <algorithm>
#include <utility>

namespace {
bool IsType(const CourseEventMarker& event, const char* type) {
    return event.type == type;
}
} // namespace

void EncounterDirector::Reset() {
    pendingEvents_.clear();
    timeSinceMajorRelease_ = 999.0f;
    lastStats_ = {};
    lastStats_.lastDecision = "Reset";
}

EncounterDirectorFrameOutput EncounterDirector::Update(const EncounterDirectorFrameInput& input) {
    EncounterDirectorFrameOutput output{};
    const float dt = (std::max)(0.0f, input.deltaTime);
    timeSinceMajorRelease_ += dt;

    lastStats_ = {};
    if (input.spawnRuntime != nullptr) {
        lastStats_.activeEnemies = input.spawnRuntime->ActiveEnemyCount();
        lastStats_.activeBullets = input.spawnRuntime->ActiveBulletCount();
        lastStats_.activeObstacles = input.spawnRuntime->ActiveObstacleCount();
    }

    for (const CourseEventMarker& event : input.triggeredEvents) {
        if (pendingEvents_.size() >= settings_.maxQueuedEvents && !IsCriticalEvent(event)) {
            ++lastStats_.suppressedThisFrame;
            lastStats_.lastDecision = "Suppressed non-critical event because encounter queue is full.";
            continue;
        }
        pendingEvents_.push_back({event, 0.0f});
    }

    std::vector<PendingEvent> stillPending;
    stillPending.reserve(pendingEvents_.size());
    for (PendingEvent& pending : pendingEvents_) {
        pending.age += dt;

        std::string reason;
        if (CanRelease(pending.event, pending.age, lastStats_, reason)) {
            output.dispatchEvents.push_back(pending.event);
            ++lastStats_.releasedThisFrame;
            lastStats_.lastDecision = reason;
            if (IsMajorEncounter(pending.event)) {
                timeSinceMajorRelease_ = 0.0f;
            }
        } else {
            stillPending.push_back(pending);
            ++lastStats_.delayedThisFrame;
            lastStats_.lastDecision = reason;
        }
    }

    pendingEvents_ = std::move(stillPending);
    lastStats_.pendingEvents = pendingEvents_.size();
    if (lastStats_.lastDecision.empty()) {
        lastStats_.lastDecision = output.dispatchEvents.empty()
            ? "No encounter events this frame."
            : "Released encounter events.";
    }
    return output;
}

bool EncounterDirector::CanRelease(
    const CourseEventMarker& event,
    float age,
    const EncounterDirectorStats& stats,
    std::string& reason) const {
    if (age >= settings_.maxQueueDelay) {
        reason = "Released event after maximum queue delay.";
        return true;
    }

    if (IsType(event, "enemy_wave")) {
        if (stats.activeEnemies >= settings_.maxActiveEnemies) {
            reason = "Delayed enemy wave because active enemy budget is full.";
            return false;
        }
        if (timeSinceMajorRelease_ < settings_.majorEncounterSpacing) {
            reason = "Delayed enemy wave to preserve encounter spacing.";
            return false;
        }
        reason = "Released enemy wave.";
        return true;
    }

    if (IsType(event, "boss")) {
        if (stats.activeEnemies > settings_.bossEntryEnemyLimit) {
            reason = "Delayed boss entry until previous enemies clear.";
            return false;
        }
        reason = "Released boss entry.";
        return true;
    }

    if (IsType(event, "boss_phase")) {
        if (timeSinceMajorRelease_ < settings_.majorEncounterSpacing * 0.75f) {
            reason = "Delayed boss phase to preserve readability.";
            return false;
        }
        reason = "Released boss phase.";
        return true;
    }

    if (IsType(event, "obstacle")) {
        if (stats.activeObstacles >= settings_.maxActiveObstacles) {
            reason = "Delayed obstacle because active obstacle budget is full.";
            return false;
        }
        reason = "Released obstacle.";
        return true;
    }

    reason = "Released non-combat event.";
    return true;
}

bool EncounterDirector::IsMajorEncounter(const CourseEventMarker& event) const {
    return IsType(event, "enemy_wave") || IsType(event, "boss") || IsType(event, "boss_phase");
}

bool EncounterDirector::IsCriticalEvent(const CourseEventMarker& event) const {
    return IsMajorEncounter(event) || IsType(event, "checkpoint");
}
