#pragma once

#include <cstdint>

#include "EnemyProjectileDefinitionAsset.h"

class CourseSpawnRuntime;
struct CourseEnemyActor;

struct EnemyTargetingRuntimeState final {
    uint64_t attackIntentSequence = 0;
    uint64_t attackTokenId = 0;
    uint64_t revision = 0;
    float originDistance = 0.0f;
    float originLateralOffset = 0.0f;
    float originVerticalOffset = 0.0f;
    float targetDistance = 0.0f;
    float targetLateralOffset = 0.0f;
    float targetVerticalOffset = 0.0f;
    float playerForwardVelocity = 0.0f;
    float playerLateralVelocity = 0.0f;
    float playerVerticalVelocity = 0.0f;
    float predictedFlightSeconds = 0.0f;
    bool initialized = false;
    bool solutionLocked = false;
};

struct EnemyTargetingFrameInput final {
    float deltaTime = 0.0f;
    float playerDistance = 0.0f;
    float playerLateralOffset = 0.0f;
    float playerVerticalOffset = 4.0f;
};

struct EnemyTargetingFrame final {
    uint32_t activeReservations = 0;
    uint32_t solutionsLockedThisFrame = 0;
    uint32_t predictiveSolutions = 0;
    uint32_t homingSolutions = 0;
    uint64_t revision = 0;
};

// Captures one fair, checkpoint-safe aim solution per admitted attack token.
// Predictive aim is frozen before Telegraph presentation, so later dodging is
// meaningful; only Homing projectiles may adjust after launch, at a turn cap.
class EnemyTargetingSystem final {
public:
    void Reset();
    void Update(
        CourseSpawnRuntime& runtime,
        const EnemyTargetingFrameInput& input);

    const EnemyTargetingFrame& Frame() const noexcept { return frame_; }

private:
    void LockSolution(
        CourseEnemyActor& actor,
        const EnemyTargetingFrameInput& input,
        float playerForwardVelocity,
        float playerLateralVelocity,
        float playerVerticalVelocity);

    EnemyTargetingFrame frame_{};
    float previousPlayerDistance_ = 0.0f;
    float previousPlayerLateralOffset_ = 0.0f;
    float previousPlayerVerticalOffset_ = 4.0f;
    bool hasPreviousPlayerSample_ = false;
    uint64_t revision_ = 0;
};
