#include "RailVehicleDamageCoordinator.h"

#include <algorithm>
#include <cmath>

namespace {

void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

bool ValidState(const RailVehicleDamageRuntimeState& state) noexcept {
    return state.initialized &&
        std::isfinite(state.repeatedContactCooldownSeconds) &&
        state.repeatedContactCooldownSeconds >= 0.0f;
}

} // namespace

RailVehicleDamageCoordinator::RailVehicleDamageCoordinator() {
    (void)Initialize(RailVehicleHitboxProfile::MineCartDefaults(), nullptr);
}

bool RailVehicleDamageCoordinator::Initialize(
    const RailVehicleHitboxProfile& profile,
    std::string* errorMessage) {
    if (!profile.Validate(errorMessage)) return false;
    profile_ = profile;
    initialized_ = true;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleDamageCoordinator::Reset() {
    state_ = {};
    state_.initialized = initialized_;
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
}

bool RailVehicleDamageCoordinator::RestoreState(
    const RailVehicleDamageRuntimeState& state,
    std::string* errorMessage) {
    if (!initialized_ || !ValidState(state) ||
        state.repeatedContactCooldownSeconds >
            profile_.repeatedContactIntervalSeconds + 0.001f) {
        SetError(errorMessage, "Rail vehicle damage checkpoint is invalid.");
        return false;
    }
    state_ = state;
    ++state_.revision;
    frame_ = {};
    frame_.state = state_;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const RailVehicleDamageCoordinatorFrame&
RailVehicleDamageCoordinator::Update(
    const RailVehicleDamageCoordinatorInput& input) {
    frame_ = {};
    const float dt = std::isfinite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f)
        : 0.0f;
    state_.repeatedContactCooldownSeconds = (std::max)(
        0.0f,
        state_.repeatedContactCooldownSeconds - dt);
    if (!initialized_ || input.collisionSystem == nullptr ||
        input.vehicleMovement == nullptr) {
        ++state_.revision;
        frame_.state = state_;
        return frame_;
    }

    const RailVehicleBodyCollisionFrame* body = input.bodyCollision;
    if (body != nullptr) {
        frame_.sourceBodyCollisionRevision = body->revision;
    }
    const bool damagingContact = input.gameplayActive && body != nullptr &&
        body->valid && body->contact && body->blocking &&
        body->kind != RailVehicleBodyContactKind::QueryBudgetExceeded &&
        body->impactSpeed >= profile_.minimumDamageImpactSpeed &&
        DamageFor(body->kind) > 0.0f;
    const bool newContact = damagingContact &&
        body->contactSequence != state_.lastBodyContactSequence;
    const bool repeatReady = damagingContact &&
        state_.repeatedContactCooldownSeconds <= 0.0f;
    if (damagingContact && (newContact || repeatReady)) {
        PlayerHitRequest request{};
        request.kind = body->kind == RailVehicleBodyContactKind::DynamicObstacle
            ? PlayerHitKind::ObstacleContact
            : PlayerHitKind::TerrainContact;
        request.sourceActorId = body->hitActorId;
        request.sourceId = body->hitStableId;
        request.impactEffectId = "hit_ring";
        request.rawDamage = DamageFor(body->kind);
        request.postHitInvulnerabilitySeconds =
            profile_.postContactInvulnerabilitySeconds;
        request.railDistance = body->impactDistance;
        request.lateralOffset = body->impactLateralOffset;
        request.verticalOffset = body->impactVerticalOffset;
        request.impactWorldPosition = body->impactWorldPosition;
        request.impactNormalWorld = body->impactNormalWorld;
        request.hasWorldImpact = true;
        frame_.damageResult =
            input.collisionSystem->ApplyPlayerHit(request);
        frame_.submittedContactDamage = true;
        frame_.acceptedContactDamage = frame_.damageResult.accepted;
        frame_.lethal = frame_.damageResult.lethal;
        state_.lastBodyContactSequence = body->contactSequence;
        if (frame_.damageResult.accepted) {
            state_.lastAcceptedDamageSequence = frame_.damageResult.sequence;
            state_.repeatedContactCooldownSeconds =
                profile_.repeatedContactIntervalSeconds;
        }
    }

    const PlayerDamageRuntimeState& playerDamage =
        input.collisionSystem->PlayerDamage().State();
    const RailVehicleRuntimeState& vehicle = input.vehicleMovement->State();
    frame_.vehicleHitPointsBefore = vehicle.hitPoints;
    frame_.playerHealthNormalized = playerDamage.initialized &&
        playerDamage.maximumHitPoints > 0.0f
        ? (std::clamp)(
            playerDamage.hitPoints / playerDamage.maximumHitPoints,
            0.0f, 1.0f)
        : 1.0f;
    const float targetVehicleHitPoints =
        input.vehicleMovement->Definition().maximumHitPoints *
        frame_.playerHealthNormalized;
    input.vehicleMovement->SynchronizeHitPoints(targetVehicleHitPoints);
    frame_.vehicleHitPointsAfter = input.vehicleMovement->State().hitPoints;
    frame_.vehicleHealthSynchronized =
        std::abs(frame_.vehicleHitPointsAfter -
                 frame_.vehicleHitPointsBefore) > 0.0001f;
    ++state_.revision;
    frame_.state = state_;
    return frame_;
}

float RailVehicleDamageCoordinator::DamageFor(
    RailVehicleBodyContactKind kind) const noexcept {
    switch (kind) {
    case RailVehicleBodyContactKind::DynamicObstacle:
        return profile_.obstacleContactDamage;
    case RailVehicleBodyContactKind::TerrainPlacement:
        return profile_.terrainContactDamage;
    case RailVehicleBodyContactKind::ProceduralTerrain:
        return profile_.proceduralTerrainContactDamage;
    case RailVehicleBodyContactKind::None:
    case RailVehicleBodyContactKind::QueryBudgetExceeded:
        return 0.0f;
    }
    return 0.0f;
}
