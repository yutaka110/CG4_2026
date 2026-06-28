#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "CourseAsset.h"

class CourseSpawnRuntime;

struct EncounterDirectorSettings {
    size_t maxActiveEnemies = 10;
    size_t maxActiveObstacles = 5;
    size_t bossEntryEnemyLimit = 1;
    size_t maxQueuedEvents = 32;
    float majorEncounterSpacing = 1.15f;
    float maxQueueDelay = 4.0f;
};

struct EncounterDirectorFrameInput {
    float deltaTime = 0.016f;
    float currentDistance = 0.0f;
    std::vector<CourseEventMarker> triggeredEvents;
    const CourseSpawnRuntime* spawnRuntime = nullptr;
};

struct EncounterDirectorFrameOutput {
    std::vector<CourseEventMarker> dispatchEvents;
};

struct EncounterDirectorStats {
    size_t pendingEvents = 0;
    size_t activeEnemies = 0;
    size_t activeBullets = 0;
    size_t activeObstacles = 0;
    uint32_t releasedThisFrame = 0;
    uint32_t delayedThisFrame = 0;
    uint32_t suppressedThisFrame = 0;
    std::string lastDecision;
};

class EncounterDirector {
public:
    void Reset();
    EncounterDirectorFrameOutput Update(const EncounterDirectorFrameInput& input);
    const EncounterDirectorStats& LastStats() const { return lastStats_; }
    EncounterDirectorSettings& Settings() { return settings_; }
    const EncounterDirectorSettings& Settings() const { return settings_; }

private:
    struct PendingEvent {
        CourseEventMarker event;
        float age = 0.0f;
    };

    bool CanRelease(
        const CourseEventMarker& event,
        float age,
        const EncounterDirectorStats& stats,
        std::string& reason) const;
    bool IsMajorEncounter(const CourseEventMarker& event) const;
    bool IsCriticalEvent(const CourseEventMarker& event) const;

    EncounterDirectorSettings settings_{};
    EncounterDirectorStats lastStats_{};
    std::vector<PendingEvent> pendingEvents_;
    float timeSinceMajorRelease_ = 999.0f;
};
