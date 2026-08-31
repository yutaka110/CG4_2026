#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include "EnemyAttackDefenseResult.h"
#include "EnemyAttackInterruptSystem.h"
#include "EnemyProjectileShootDownSystem.h"
#include "PlayerDamageSystem.h"
#include "PlayerNearMissSystem.h"
#include "RailVehicleMountedDefenseSystem.h"

class CourseSpawnRuntime;

struct EnemyAttackDefenseResolutionSettings final {
    bool enabled = true;
    uint32_t interruptScore = 180;
    uint32_t shootDownScore = 140;
    uint32_t poseScore = 160;
    float perfectTimingSeconds = 0.36f;
    float goodTimingSeconds = 0.16f;
    float chainWindowSeconds = 2.2f;
    float chainBonusPerStep = 0.08f;
    float maximumChainMultiplier = 2.0f;
    size_t historyCapacity = 512;
    size_t maximumResultsPerFrame = 16;
};

struct EnemyAttackDefenseResolutionInput final {
    const CourseSpawnRuntime* runtime = nullptr;
    const EnemyProjectileShootDownFrame* shootDown = nullptr;
    const EnemyAttackInterruptFrame* interrupt = nullptr;
    std::span<const PlayerNearMissResult> nearMissResults{};
    std::span<const PlayerDamageResult> damageResults{};
    const RailVehicleMountedDefenseFrame* mountedDefense = nullptr;
    float playerDistance = 0.0f;
    float deltaTime = 0.016f;
    bool gameplayActive = true;
};

struct EnemyAttackDefenseResolutionState final {
    uint64_t nextSequence = 1;
    uint64_t totalScore = 0;
    uint32_t chain = 0;
    uint32_t maximumChain = 0;
    float chainRemainingSeconds = 0.0f;
    uint64_t revision = 0;
};

struct EnemyAttackDefenseResolutionFrame final {
    std::vector<EnemyAttackDefenseResult> results;
    uint32_t successes = 0;
    uint32_t failures = 0;
    uint32_t scoreAwarded = 0;
    uint64_t revision = 0;
};

class EnemyAttackDefenseResolutionSystem final {
public:
    void Reset();
    void Update(
        const EnemyAttackDefenseResolutionInput& input,
        const EnemyAttackDefenseResolutionSettings& settings = {});

    const EnemyAttackDefenseResolutionState& State() const noexcept {
        return state_;
    }
    const EnemyAttackDefenseResolutionFrame& Frame() const noexcept {
        return frame_;
    }

private:
    void Resolve(EnemyAttackDefenseResult result,
                 const EnemyAttackDefenseResolutionSettings& settings);
    bool ProjectileResolved(uint64_t id) const noexcept;
    bool TokenResolved(uint64_t id) const noexcept;
    void RememberProjectile(uint64_t id, size_t capacity);
    void RememberToken(uint64_t id, size_t capacity);

    EnemyAttackDefenseResolutionState state_{};
    EnemyAttackDefenseResolutionFrame frame_{};
    std::unordered_set<uint64_t> resolvedProjectiles_;
    std::unordered_set<uint64_t> resolvedTokens_;
    std::vector<uint64_t> projectileHistory_;
    std::vector<uint64_t> tokenHistory_;
};
