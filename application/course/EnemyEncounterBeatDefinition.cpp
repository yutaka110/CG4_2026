#include "EnemyEncounterBeatDefinition.h"

#include <cmath>

namespace {
bool SafeToken(const std::string& value, bool allowEmpty = true) {
    if ((!allowEmpty && value.empty()) || value.size() > 128) return false;
    for (unsigned char character : value) {
        if (character == '|' || character == '\n' || character == '\r') {
            return false;
        }
    }
    return true;
}

bool FiniteRange(float value, float minimum, float maximum) noexcept {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}
} // namespace

bool EnemyEncounterBeatDefinition::Validate(
    std::string* errorMessage) const {
    const auto reject = [errorMessage](const char* message) {
        if (errorMessage != nullptr) *errorMessage = message;
        return false;
    };
    if (!SafeToken(editorGuid, false) || !SafeToken(encounterId, false) ||
        !SafeToken(displayName) || !SafeToken(waveGuid, false) ||
        !SafeToken(cameraShotId)) {
        return reject("Encounter Beat requires safe GUID, encounter, Wave and camera tokens.");
    }
    if (!FiniteRange(triggerRailDistance, 0.0f, 1000000.0f) ||
        !FiniteRange(endRailDistance, 0.01f, 1000000.0f) ||
        endRailDistance <= triggerRailDistance ||
        !FiniteRange(prewarmDistance, 0.0f, 10000.0f)) {
        return reject("Encounter Beat rail trigger or prewarm distance is invalid.");
    }
    if (!FiniteRange(establishMinimumSeconds, 0.0f, 30.0f) ||
        !FiniteRange(establishMaximumSeconds, 0.05f, 60.0f) ||
        establishMaximumSeconds < establishMinimumSeconds ||
        !FiniteRange(threatenMinimumSeconds, 0.0f, 30.0f) ||
        !FiniteRange(threatenMaximumSeconds, 0.05f, 60.0f) ||
        threatenMaximumSeconds < threatenMinimumSeconds ||
        !FiniteRange(attackMinimumSeconds, 0.0f, 60.0f) ||
        !FiniteRange(attackMaximumSeconds, 0.05f, 300.0f) ||
        attackMaximumSeconds < attackMinimumSeconds ||
        !FiniteRange(recoverySeconds, 0.0f, 30.0f) ||
        !FiniteRange(resolveTimeoutSeconds, 0.1f, 120.0f)) {
        return reject("Encounter Beat phase timing is outside commercial limits.");
    }
    if (!FiniteRange(requiredReadableRatio, 0.0f, 1.0f) ||
        maximumConcurrentAttackers == 0 || maximumConcurrentAttackers > 16 ||
        !FiniteRange(maximumThreatBudget, 0.1f, 64.0f)) {
        return reject("Encounter Beat readability or attack budget is invalid.");
    }
    if (!FiniteRange(cameraWeight, 0.0f, 2.0f) ||
        !FiniteRange(cameraFocusWeight, 0.0f, 1.0f) ||
        !FiniteRange(cameraFovOffsetDegrees, -12.0f, 18.0f) ||
        !FiniteRange(cameraBackDistanceOffset, -8.0f, 18.0f) ||
        priority < -1000 || priority > 1000) {
        return reject("Encounter Beat camera composition or priority is invalid.");
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

float EnemyEncounterBeatDefinition::AuthoredDurationSeconds() const noexcept {
    return establishMaximumSeconds + threatenMaximumSeconds +
        attackMaximumSeconds + recoverySeconds + resolveTimeoutSeconds;
}

const char* ToString(EnemyEncounterBeatPhase phase) noexcept {
    switch (phase) {
    case EnemyEncounterBeatPhase::Dormant: return "Dormant";
    case EnemyEncounterBeatPhase::Establish: return "Establish";
    case EnemyEncounterBeatPhase::Threaten: return "Threaten";
    case EnemyEncounterBeatPhase::Attack: return "Attack";
    case EnemyEncounterBeatPhase::Recovery: return "Recovery";
    case EnemyEncounterBeatPhase::ExitResolve: return "ExitResolve";
    case EnemyEncounterBeatPhase::Complete: return "Complete";
    case EnemyEncounterBeatPhase::Failed: return "Failed";
    }
    return "Dormant";
}
