#include "RailVehicleMountedDefenseSystem.h"

#include <algorithm>
#include <cmath>

namespace {
void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) *errorMessage = message;
}
}

RailVehicleMountedDefenseDefinition
RailVehicleMountedDefenseDefinition::MineCartDefaults() {
    return {};
}

bool RailVehicleMountedDefenseDefinition::Validate(
    std::string* errorMessage) const {
    const bool valid = std::isfinite(leanHitboxRadiusScale) &&
        std::isfinite(duckHitboxRadiusScale) &&
        std::isfinite(minimumHitboxRadiusScale) &&
        std::isfinite(actionInputThreshold) &&
        leanHitboxRadiusScale > 0.0f && leanHitboxRadiusScale <= 1.0f &&
        duckHitboxRadiusScale > 0.0f && duckHitboxRadiusScale <= 1.0f &&
        minimumHitboxRadiusScale > 0.0f &&
        minimumHitboxRadiusScale <= leanHitboxRadiusScale &&
        minimumHitboxRadiusScale <= duckHitboxRadiusScale &&
        actionInputThreshold >= 0.0f && actionInputThreshold <= 1.0f;
    if (!valid || grantInvulnerability) {
        SetError(errorMessage,
            "Mounted defense must use a valid hitbox response and cannot grant blanket invulnerability.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailVehicleMountedDefenseSystem::RailVehicleMountedDefenseSystem() {
    (void)Initialize(RailVehicleMountedDefenseDefinition::MineCartDefaults());
}

bool RailVehicleMountedDefenseSystem::Initialize(
    const RailVehicleMountedDefenseDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    initialized_ = true;
    Reset();
    return true;
}

void RailVehicleMountedDefenseSystem::Reset() {
    state_ = {};
    state_.hitboxRadiusScale = 1.0f;
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
}

const RailVehicleMountedDefenseFrame& RailVehicleMountedDefenseSystem::Update(
    const RailVehicleMountedDefenseInput& input) {
    frame_ = {};
    frame_.vehicleRailPosePreserved = true;
    const bool mounted = initialized_ && input.occupantMounted &&
        input.vehicleDefinition != nullptr && input.vehicleState != nullptr &&
        input.vehicleState->initialized && input.occupantMotion != nullptr &&
        input.vehicleDefinition->mountedMovementMode ==
            RailVehicleMountedMovementMode::VehicleMounted;
    RailVehicleMountedDefenseAction nextAction =
        RailVehicleMountedDefenseAction::None;
    float strength = 0.0f;
    if (mounted && (input.occupantMotion->active ||
        input.occupantMotion->state.phase ==
            RailVehicleMountedEvasionPhase::Recovering)) {
        const float x = input.occupantMotion->state.directionX;
        const float y = input.occupantMotion->state.directionY;
        strength = (std::clamp)(
            input.occupantMotion->normalizedStrength, 0.0f, 1.0f);
        if (std::abs(y) > std::abs(x)) {
            nextAction = RailVehicleMountedDefenseAction::Duck;
        } else if (x < -definition_.actionInputThreshold) {
            nextAction = RailVehicleMountedDefenseAction::LeanLeft;
        } else if (x > definition_.actionInputThreshold) {
            nextAction = RailVehicleMountedDefenseAction::LeanRight;
        }
    }

    if (nextAction != state_.action &&
        nextAction != RailVehicleMountedDefenseAction::None) {
        ++state_.actionSequence;
    }
    state_.action = nextAction;
    state_.actionStrength = strength;
    state_.active = nextAction != RailVehicleMountedDefenseAction::None &&
        input.occupantMotion != nullptr && input.occupantMotion->active;
    const float authoredScale = nextAction == RailVehicleMountedDefenseAction::Duck
        ? definition_.duckHitboxRadiusScale
        : nextAction == RailVehicleMountedDefenseAction::LeanLeft ||
              nextAction == RailVehicleMountedDefenseAction::LeanRight
            ? definition_.leanHitboxRadiusScale
            : 1.0f;
    state_.hitboxRadiusScale = (std::max)(
        definition_.minimumHitboxRadiusScale,
        1.0f - (1.0f - authoredScale) * strength);
    state_.sourceEvasionSequence = input.occupantMotion != nullptr
        ? input.occupantMotion->state.eventSequence : 0;
    ++state_.revision;

    frame_.state = state_;
    frame_.occupantLateralOffset = input.occupantMotion != nullptr
        ? input.occupantMotion->state.lateralOffset : 0.0f;
    frame_.occupantVerticalOffset = input.occupantMotion != nullptr
        ? input.occupantMotion->state.verticalOffset : 0.0f;
    frame_.invulnerable = false;
    frame_.sourceVehicleRevision = mounted
        ? input.vehicleState->revision : 0;
    return frame_;
}

const char* ToString(RailVehicleMountedDefenseAction action) noexcept {
    switch (action) {
    case RailVehicleMountedDefenseAction::None: return "None";
    case RailVehicleMountedDefenseAction::LeanLeft: return "LeanLeft";
    case RailVehicleMountedDefenseAction::LeanRight: return "LeanRight";
    case RailVehicleMountedDefenseAction::Duck: return "Duck";
    }
    return "Unknown";
}
