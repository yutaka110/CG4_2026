#include "EnemyTargetingSystem.h"

#include "CourseSpawnRuntime.h"

#include <algorithm>
#include <cmath>

void EnemyTargetingSystem::Reset() {
    frame_ = {};
    previousPlayerDistance_ = 0.0f;
    previousPlayerLateralOffset_ = 0.0f;
    previousPlayerVerticalOffset_ = 4.0f;
    hasPreviousPlayerSample_ = false;
    revision_ = 0;
}

void EnemyTargetingSystem::Update(
    CourseSpawnRuntime& runtime,
    const EnemyTargetingFrameInput& input) {
    const float dt = (std::max)(0.0f, input.deltaTime);
    float forwardVelocity = 0.0f;
    float lateralVelocity = 0.0f;
    float verticalVelocity = 0.0f;
    if (hasPreviousPlayerSample_ && dt > 0.000001f) {
        forwardVelocity =
            (input.playerDistance - previousPlayerDistance_) / dt;
        lateralVelocity =
            (input.playerLateralOffset - previousPlayerLateralOffset_) / dt;
        verticalVelocity =
            (input.playerVerticalOffset - previousPlayerVerticalOffset_) / dt;
    }
    forwardVelocity = (std::clamp)(forwardVelocity, -120.0f, 120.0f);
    lateralVelocity = (std::clamp)(lateralVelocity, -40.0f, 40.0f);
    verticalVelocity = (std::clamp)(verticalVelocity, -40.0f, 40.0f);
    previousPlayerDistance_ = input.playerDistance;
    previousPlayerLateralOffset_ = input.playerLateralOffset;
    previousPlayerVerticalOffset_ = input.playerVerticalOffset;
    hasPreviousPlayerSample_ = true;

    frame_ = {};
    for (CourseEnemyActor& actor : runtime.MutableEnemies()) {
        if (!actor.behaviorDefinition.commercialBehavior ||
            !actor.attackState.tokenReserved ||
            actor.attackState.intentSequence == 0) {
            continue;
        }
        ++frame_.activeReservations;
        EnemyTargetingRuntimeState& state = actor.targetingState;
        if (!state.solutionLocked ||
            state.attackIntentSequence != actor.attackState.intentSequence ||
            state.attackTokenId != actor.attackState.tokenId) {
            LockSolution(
                actor,
                input,
                forwardVelocity,
                lateralVelocity,
                verticalVelocity);
            ++frame_.solutionsLockedThisFrame;
        }
        if (actor.desc.projectileDefinition.trajectory ==
            EnemyProjectileTrajectory::Predictive) {
            ++frame_.predictiveSolutions;
        } else if (actor.desc.projectileDefinition.trajectory ==
                   EnemyProjectileTrajectory::Homing) {
            ++frame_.homingSolutions;
        }
    }
    frame_.revision = revision_;
}

void EnemyTargetingSystem::LockSolution(
    CourseEnemyActor& actor,
    const EnemyTargetingFrameInput& input,
    float playerForwardVelocity,
    float playerLateralVelocity,
    float playerVerticalVelocity) {
    const EnemyProjectileDefinitionAsset& definition =
        actor.desc.projectileDefinition;
    EnemyTargetingRuntimeState& state = actor.targetingState;
    state = {};
    state.attackIntentSequence = actor.attackState.intentSequence;
    state.attackTokenId = actor.attackState.tokenId;
    state.originDistance = actor.desc.spawnDistance + actor.desc.distanceOffset -
        actor.desc.radius * 1.5f;
    state.originLateralOffset = actor.desc.lateralOffset;
    state.originVerticalOffset = actor.desc.verticalOffset;
    state.playerForwardVelocity = playerForwardVelocity;
    state.playerLateralVelocity = playerLateralVelocity;
    state.playerVerticalVelocity = playerVerticalVelocity;
    const float distanceToPlayer = (std::max)(
        0.0f, state.originDistance - input.playerDistance);
    const float speed = (std::max)(1.0f, definition.initialSpeed);
    state.predictedFlightSeconds = (std::clamp)(
        distanceToPlayer / speed,
        0.0f,
        (std::max)(0.0f, definition.maximumPredictionSeconds));
    const bool predictive =
        definition.trajectory == EnemyProjectileTrajectory::Predictive ||
        definition.trajectory == EnemyProjectileTrajectory::Homing;
    const float predictionSeconds = predictive
        ? state.predictedFlightSeconds * definition.predictionScale
        : 0.0f;
    state.targetDistance = input.playerDistance +
        playerForwardVelocity * predictionSeconds;
    state.targetLateralOffset = input.playerLateralOffset +
        playerLateralVelocity * predictionSeconds;
    state.targetVerticalOffset = input.playerVerticalOffset +
        playerVerticalVelocity * predictionSeconds;
    state.initialized = true;
    state.solutionLocked = true;
    state.revision = ++revision_;
}
