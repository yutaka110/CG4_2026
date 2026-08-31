#pragma once

#include <cstdint>
#include <string>
#include <string_view>

enum class EnemyEncounterBeatPhase : uint8_t {
    Dormant,
    Establish,
    Threaten,
    Attack,
    Recovery,
    ExitResolve,
    Complete,
    Failed,
};

// Schema-v13 persistent encounter choreography. A Beat references the same
// stable Wave GUID used by enemy placements; mutable phase/timer state lives
// only in EnemyEncounterPacingDirector.
struct EnemyEncounterBeatDefinition final {
    std::string editorGuid;
    std::string encounterId;
    std::string displayName = "Enemy Encounter Beat";
    std::string waveGuid;
    float triggerRailDistance = 0.0f;
    float endRailDistance = 120.0f;
    float prewarmDistance = 60.0f;
    float establishMinimumSeconds = 0.55f;
    float establishMaximumSeconds = 2.00f;
    float threatenMinimumSeconds = 0.45f;
    float threatenMaximumSeconds = 2.50f;
    float attackMinimumSeconds = 0.80f;
    float attackMaximumSeconds = 12.0f;
    float recoverySeconds = 0.55f;
    float resolveTimeoutSeconds = 5.0f;
    float requiredReadableRatio = 0.65f;
    uint32_t maximumConcurrentAttackers = 2;
    float maximumThreatBudget = 2.6f;
    std::string cameraShotId;
    float cameraWeight = 0.85f;
    float cameraFocusWeight = 0.72f;
    float cameraFovOffsetDegrees = 2.5f;
    float cameraBackDistanceOffset = 1.8f;
    int priority = 0;
    bool exitSurvivorsOnResolve = true;
    bool requireCombatTruthForCompletion = true;
    bool enabled = true;
    bool editorVisible = true;
    bool editorLocked = false;

    bool Validate(std::string* errorMessage = nullptr) const;
    float AuthoredDurationSeconds() const noexcept;
};

const char* ToString(EnemyEncounterBeatPhase phase) noexcept;
