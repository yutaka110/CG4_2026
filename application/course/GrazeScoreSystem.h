#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "PlayerNearMissSystem.h"

struct GrazeScoreDefinition final {
    uint32_t baseScore = 60;
    uint32_t maximumClosenessBonus = 140;
    float closenessExponent = 1.55f;
    float predictiveMultiplier = 1.10f;
    float homingMultiplier = 1.25f;
    float arcMultiplier = 1.16f;
    float chainWindowSeconds = 2.6f;
    float chainStepMultiplier = 0.08f;
    float maximumChainMultiplier = 2.5f;
    float adrenalineGainBase = 0.08f;
    float adrenalineGainCloseness = 0.18f;
    float adrenalineDecayPerSecond = 0.07f;
    size_t maximumResultsPerFrame = 16;

    bool Validate(std::string* errorMessage = nullptr) const;
};

struct GrazeScoreResult final {
    uint64_t sequence = 0;
    uint64_t nearMissSequence = 0;
    uint64_t projectileId = 0;
    EnemyProjectileTrajectory trajectory = EnemyProjectileTrajectory::Direct;
    uint32_t scoreAwarded = 0;
    uint32_t chainAfter = 0;
    float closeness = 0.0f;
    float scoreMultiplier = 1.0f;
    float adrenalineAfter = 0.0f;
    float railDistance = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    bool accepted = false;
};

struct GrazeScoreRuntimeState final {
    uint64_t nextSequence = 1;
    uint64_t totalScore = 0;
    uint64_t lastConsumedNearMissSequence = 0;
    uint32_t acceptedGrazes = 0;
    uint32_t chain = 0;
    uint32_t maximumChain = 0;
    float chainRemainingSeconds = 0.0f;
    float adrenalineNormalized = 0.0f;
    uint64_t revision = 0;
};

struct GrazeScoreFrameInput final {
    float deltaTime = 0.0f;
    std::span<const PlayerNearMissResult> nearMissResults{};
    bool gameplayActive = true;
};

// Authoritative scoring policy for accepted near misses. Collision owns whether
// a graze occurred; this system alone owns its reward and chain state.
class GrazeScoreSystem final {
public:
    bool Initialize(
        const GrazeScoreDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    void Update(const GrazeScoreFrameInput& input);
    bool RestoreState(
        const GrazeScoreRuntimeState& state,
        std::string* errorMessage = nullptr);

    const GrazeScoreDefinition& Definition() const noexcept {
        return definition_;
    }
    const GrazeScoreRuntimeState& State() const noexcept { return state_; }
    const std::vector<GrazeScoreResult>& ResultsThisFrame() const noexcept {
        return resultsThisFrame_;
    }

private:
    float TrajectoryMultiplier(EnemyProjectileTrajectory trajectory) const noexcept;

    GrazeScoreDefinition definition_{};
    GrazeScoreRuntimeState state_{};
    std::vector<GrazeScoreResult> resultsThisFrame_;
    bool initialized_ = false;
};
