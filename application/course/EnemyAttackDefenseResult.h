#pragma once

#include <cstdint>

enum class EnemyAttackDefenseMethod : uint8_t {
    None,
    Interrupt,
    ShootDown,
    LeanLeft,
    LeanRight,
    Duck,
};

enum class EnemyAttackDefenseOutcome : uint8_t {
    Success,
    Failed,
};

enum class EnemyAttackDefenseGrade : uint8_t {
    None,
    Late,
    Good,
    Perfect,
};

// Authoritative, one-result-per-attack/projectile defense outcome. Gameplay,
// score and presentation consume this record instead of inferring success from
// an animation, prompt, near-miss or projectile disappearance.
struct EnemyAttackDefenseResult final {
    uint64_t sequence = 0;
    uint32_t actorId = 0;
    uint64_t attackIntentSequence = 0;
    uint64_t attackTokenId = 0;
    uint64_t projectileId = 0;
    EnemyAttackDefenseMethod method = EnemyAttackDefenseMethod::None;
    EnemyAttackDefenseOutcome outcome = EnemyAttackDefenseOutcome::Failed;
    EnemyAttackDefenseGrade grade = EnemyAttackDefenseGrade::None;
    float timingMarginSeconds = 0.0f;
    float closeness = 0.0f;
    float actionStrength = 0.0f;
    float railDistance = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    uint32_t scoreAwarded = 0;
    uint32_t chainAfter = 0;
    bool accepted = false;
};

const char* ToString(EnemyAttackDefenseMethod method) noexcept;
const char* ToString(EnemyAttackDefenseOutcome outcome) noexcept;
const char* ToString(EnemyAttackDefenseGrade grade) noexcept;
