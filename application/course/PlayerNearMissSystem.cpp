#include "PlayerNearMissSystem.h"

#include <algorithm>
#include <cmath>

namespace {

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

bool Finite(float value) noexcept {
    return std::isfinite(value);
}

} // namespace

void PlayerNearMissSystem::Reset() {
    state_ = {};
    resultsThisFrame_.clear();
}

void PlayerNearMissSystem::Update(float deltaTime) {
    resultsThisFrame_.clear();
    (void)deltaTime;
}

PlayerNearMissResult PlayerNearMissSystem::Submit(
    const PlayerNearMissRequest& request) {
    PlayerNearMissResult result{};
    result.request = request;
    const bool valid = request.projectileId != 0 &&
        Finite(request.closeness) && Finite(request.surfaceSeparation) &&
        Finite(request.railDistance) && Finite(request.lateralOffset) &&
        Finite(request.verticalOffset) && request.closeness > 0.0f &&
        request.surfaceSeparation > 0.0f;
    if (!valid || WasProcessed(request.projectileId) ||
        resultsThisFrame_.size() >= settings_.maximumResultsPerFrame) {
        return result;
    }

    result.sequence = state_.nextSequence++;
    if (state_.nextSequence == 0) state_.nextSequence = 1;
    result.accepted = true;
    ++state_.acceptedNearMisses;
    Remember(request.projectileId);
    ++state_.revision;
    resultsThisFrame_.push_back(result);
    return result;
}

bool PlayerNearMissSystem::RestoreState(
    const PlayerNearMissRuntimeState& state,
    std::string* errorMessage) {
    if (state.processedProjectileIds.size() >
            settings_.projectileHistoryCapacity) {
        SetError(errorMessage, "Player near-miss checkpoint is invalid.");
        return false;
    }
    state_ = state;
    resultsThisFrame_.clear();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool PlayerNearMissSystem::WasProcessed(uint64_t projectileId) const noexcept {
    return projectileId != 0 && std::find(
        state_.processedProjectileIds.begin(),
        state_.processedProjectileIds.end(),
        projectileId) != state_.processedProjectileIds.end();
}

void PlayerNearMissSystem::Remember(uint64_t projectileId) {
    if (projectileId == 0 || settings_.projectileHistoryCapacity == 0 ||
        WasProcessed(projectileId)) {
        return;
    }
    state_.processedProjectileIds.push_back(projectileId);
    if (state_.processedProjectileIds.size() >
        settings_.projectileHistoryCapacity) {
        state_.processedProjectileIds.erase(
            state_.processedProjectileIds.begin(),
            state_.processedProjectileIds.begin() +
                static_cast<std::ptrdiff_t>(
                    state_.processedProjectileIds.size() -
                    settings_.projectileHistoryCapacity));
    }
}
