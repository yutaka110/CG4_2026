#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "PlayerDamageSystem.h"

class CourseSpawnRuntime;
struct EnemyAttackTelegraphFrame;
struct EnemyProjectilePresentationFrame;
struct EnemyAttackDefensePresentationFrame;

enum class CombatTruthBlocker : uint32_t {
    None = 0,
    HostileActor = 1u << 0,
    AttackTelegraph = 1u << 1,
    HostileProjectile = 1u << 2,
    DefenseWindow = 1u << 3,
    ActiveWave = 1u << 4,
    RecentDamage = 1u << 5,
};

constexpr CombatTruthBlocker operator|(
    CombatTruthBlocker left, CombatTruthBlocker right) noexcept {
    return static_cast<CombatTruthBlocker>(
        static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

constexpr bool HasCombatTruthBlocker(
    CombatTruthBlocker value, CombatTruthBlocker blocker) noexcept {
    return (static_cast<uint32_t>(value) &
            static_cast<uint32_t>(blocker)) != 0;
}

struct CombatTruthGateSettings final {
    float clearConfirmationSeconds = 0.50f;
    float damageClearHoldSeconds = 0.80f;
    bool activeWaveBlocksClear = true;
};

struct CombatTruthGateInput final {
    const CourseSpawnRuntime* runtime = nullptr;
    const EnemyAttackTelegraphFrame* telegraph = nullptr;
    const EnemyProjectilePresentationFrame* projectiles = nullptr;
    const EnemyAttackDefensePresentationFrame* defense = nullptr;
    std::span<const PlayerDamageResult> damageResults{};
    uint32_t activeWaves = 0;
    float deltaTime = 0.016f;
    bool gameplayActive = true;
};

struct CombatTruthGateFrame final {
    CombatTruthBlocker blockers = CombatTruthBlocker::None;
    uint32_t activeHostiles = 0;
    uint32_t activeTelegraphs = 0;
    uint32_t activeHostileProjectiles = 0;
    uint32_t unresolvedDefenseWindows = 0;
    uint32_t acceptedDamageResults = 0;
    uint32_t activeWaves = 0;
    float clearCandidateSeconds = 0.0f;
    float recentDamageHoldSeconds = 0.0f;
    bool combatActive = false;
    bool safeToAnnounceClear = false;
    bool safeToResolveSession = false;
    bool damageWithoutKnownThreat = false;
    std::string statusText;
    uint64_t revision = 0;
};

// Produces the sole player-facing answer to "is combat actually clear?".
// Wave state alone is deliberately insufficient: warnings, projectiles,
// defense windows and recent accepted damage all participate in the gate.
class CombatTruthGate final {
public:
    void Reset();
    void Update(
        const CombatTruthGateInput& input,
        const CombatTruthGateSettings& settings = {});

    const CombatTruthGateFrame& Frame() const noexcept { return frame_; }

private:
    CombatTruthGateFrame frame_{};
    float clearCandidateSeconds_ = 0.0f;
    float recentDamageHoldSeconds_ = 0.0f;
    uint64_t revision_ = 0;
};

const char* ToString(CombatTruthBlocker blocker) noexcept;
