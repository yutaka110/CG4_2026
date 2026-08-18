#include "GrazeScoreSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool Finite(float value) noexcept {
    return std::isfinite(value);
}

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

} // namespace

bool GrazeScoreDefinition::Validate(std::string* errorMessage) const {
    const bool valid = baseScore > 0 && maximumClosenessBonus > 0 &&
        Finite(closenessExponent) && closenessExponent > 0.0f &&
        Finite(predictiveMultiplier) && predictiveMultiplier > 0.0f &&
        Finite(homingMultiplier) && homingMultiplier > 0.0f &&
        Finite(arcMultiplier) && arcMultiplier > 0.0f &&
        Finite(chainWindowSeconds) && chainWindowSeconds >= 0.1f &&
        Finite(chainStepMultiplier) && chainStepMultiplier >= 0.0f &&
        Finite(maximumChainMultiplier) && maximumChainMultiplier >= 1.0f &&
        Finite(adrenalineGainBase) && adrenalineGainBase >= 0.0f &&
        Finite(adrenalineGainCloseness) && adrenalineGainCloseness >= 0.0f &&
        Finite(adrenalineDecayPerSecond) && adrenalineDecayPerSecond >= 0.0f &&
        maximumResultsPerFrame > 0;
    if (!valid) {
        SetError(errorMessage, "Graze score definition contains invalid values.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool GrazeScoreSystem::Initialize(
    const GrazeScoreDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    initialized_ = true;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void GrazeScoreSystem::Reset() {
    state_ = {};
    resultsThisFrame_.clear();
}

void GrazeScoreSystem::Update(const GrazeScoreFrameInput& input) {
    resultsThisFrame_.clear();
    if (!initialized_) {
        std::string ignored;
        (void)Initialize(GrazeScoreDefinition{}, &ignored);
    }

    const float dt = Finite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    if (input.gameplayActive) {
        const uint32_t oldChain = state_.chain;
        const float oldChainTime = state_.chainRemainingSeconds;
        const float oldAdrenaline = state_.adrenalineNormalized;
        state_.chainRemainingSeconds = (std::max)(
            0.0f,
            state_.chainRemainingSeconds - dt);
        if (state_.chainRemainingSeconds <= 0.0f) state_.chain = 0;
        state_.adrenalineNormalized = (std::max)(
            0.0f,
            state_.adrenalineNormalized -
                definition_.adrenalineDecayPerSecond * dt);
        if (oldChain != state_.chain ||
            std::abs(oldChainTime - state_.chainRemainingSeconds) > 0.0001f ||
            std::abs(oldAdrenaline - state_.adrenalineNormalized) > 0.0001f) {
            ++state_.revision;
        }
    }

    if (!input.gameplayActive) return;
    for (const PlayerNearMissResult& nearMiss : input.nearMissResults) {
        if (!nearMiss.accepted || nearMiss.sequence == 0 ||
            nearMiss.sequence <= state_.lastConsumedNearMissSequence ||
            resultsThisFrame_.size() >= definition_.maximumResultsPerFrame) {
            continue;
        }

        const float closeness = (std::clamp)(
            nearMiss.request.closeness,
            0.0f,
            1.0f);
        ++state_.chain;
        state_.maximumChain = (std::max)(state_.maximumChain, state_.chain);
        state_.chainRemainingSeconds = definition_.chainWindowSeconds;
        const float chainMultiplier = (std::min)(
            definition_.maximumChainMultiplier,
            1.0f + static_cast<float>(state_.chain - 1u) *
                definition_.chainStepMultiplier);
        const float trajectoryMultiplier =
            TrajectoryMultiplier(nearMiss.request.trajectory);
        const float closenessCurve = std::pow(
            closeness,
            definition_.closenessExponent);
        const float rawScore =
            (static_cast<float>(definition_.baseScore) +
             closenessCurve *
                 static_cast<float>(definition_.maximumClosenessBonus)) *
            chainMultiplier * trajectoryMultiplier;
        const double boundedScore = (std::min)(
            static_cast<double>((std::numeric_limits<uint32_t>::max)()),
            (std::max)(1.0, std::round(static_cast<double>(rawScore))));
        const uint32_t score = static_cast<uint32_t>(boundedScore);

        state_.adrenalineNormalized = (std::clamp)(
            state_.adrenalineNormalized + definition_.adrenalineGainBase +
                closeness * definition_.adrenalineGainCloseness,
            0.0f,
            1.0f);
        state_.totalScore += score;
        ++state_.acceptedGrazes;
        state_.lastConsumedNearMissSequence = nearMiss.sequence;

        GrazeScoreResult result{};
        result.sequence = state_.nextSequence++;
        if (state_.nextSequence == 0) state_.nextSequence = 1;
        result.nearMissSequence = nearMiss.sequence;
        result.projectileId = nearMiss.request.projectileId;
        result.trajectory = nearMiss.request.trajectory;
        result.scoreAwarded = score;
        result.chainAfter = state_.chain;
        result.closeness = closeness;
        result.scoreMultiplier = chainMultiplier * trajectoryMultiplier;
        result.adrenalineAfter = state_.adrenalineNormalized;
        result.railDistance = nearMiss.request.railDistance;
        result.lateralOffset = nearMiss.request.lateralOffset;
        result.verticalOffset = nearMiss.request.verticalOffset;
        result.accepted = true;
        resultsThisFrame_.push_back(result);
        ++state_.revision;
    }
}

bool GrazeScoreSystem::RestoreState(
    const GrazeScoreRuntimeState& state,
    std::string* errorMessage) {
    if (!Finite(state.chainRemainingSeconds) ||
        state.chainRemainingSeconds < 0.0f ||
        !Finite(state.adrenalineNormalized) ||
        state.adrenalineNormalized < 0.0f ||
        state.adrenalineNormalized > 1.0f ||
        state.chain > state.maximumChain) {
        SetError(errorMessage, "Graze score checkpoint is invalid.");
        return false;
    }
    state_ = state;
    resultsThisFrame_.clear();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

float GrazeScoreSystem::TrajectoryMultiplier(
    EnemyProjectileTrajectory trajectory) const noexcept {
    switch (trajectory) {
    case EnemyProjectileTrajectory::Predictive:
        return definition_.predictiveMultiplier;
    case EnemyProjectileTrajectory::Homing:
        return definition_.homingMultiplier;
    case EnemyProjectileTrajectory::Arc:
        return definition_.arcMultiplier;
    case EnemyProjectileTrajectory::Direct:
    default:
        return 1.0f;
    }
}
