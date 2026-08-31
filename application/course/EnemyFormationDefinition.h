#pragma once

#include <cstdint>
#include <string>

enum class EnemyFormationPattern : uint8_t {
    Authored,
    V,
    LineAbreast,
    Column,
    EchelonLeft,
    EchelonRight,
    Ring,
};

enum class EnemyEntranceStyle : uint8_t {
    FromAhead,
    SweepLeft,
    SweepRight,
    Dive,
    Rise,
    RearChase,
};

enum class EnemyExitStyle : uint8_t {
    ForwardBreak,
    SplitSides,
    Climb,
    Dive,
    RearRetreat,
};

// Immutable contract shared by formation motion and entrance/exit staging.
// Authored slots remain the default so existing Course assets do not move.
struct EnemyFormationDefinition final {
    std::string definitionId;
    EnemyFormationPattern pattern = EnemyFormationPattern::Authored;
    EnemyEntranceStyle entranceStyle = EnemyEntranceStyle::FromAhead;
    EnemyExitStyle exitStyle = EnemyExitStyle::SplitSides;
    float slotSpacing = 5.5f;
    float verticalSpacing = 1.4f;
    float cohesionResponse = 7.5f;
    float maximumCorrection = 8.0f;
    float attackStaggerSeconds = 0.12f;
    float entranceDurationSeconds = 0.72f;
    float entranceStaggerSeconds = 0.08f;
    float entranceForwardDistance = 22.0f;
    float entranceSideDistance = 14.0f;
    float exitDurationSeconds = 0.62f;
    float exitForwardDistance = 28.0f;
    float exitSideDistance = 18.0f;
    bool preserveAuthoredSlots = true;
    bool commercialFormation = true;

    static EnemyFormationDefinition CommercialDefault(
        std::string formationId = {});
    bool Validate(std::string* errorMessage = nullptr) const;
};

bool TryParseEnemyFormationPattern(
    const std::string& text,
    EnemyFormationPattern& pattern) noexcept;
bool TryParseEnemyEntranceStyle(
    const std::string& text,
    EnemyEntranceStyle& style) noexcept;
bool TryParseEnemyExitStyle(
    const std::string& text,
    EnemyExitStyle& style) noexcept;
const char* ToString(EnemyFormationPattern pattern) noexcept;
const char* ToString(EnemyEntranceStyle style) noexcept;
const char* ToString(EnemyExitStyle style) noexcept;
