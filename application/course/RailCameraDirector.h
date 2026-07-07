#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "CourseAsset.h"
#include "RailLockOnTypes.h"
#include "../terrain/RailPath.h"
#include "utils/math/Vector.h"

class CourseSpawnRuntime;

enum class RailCameraDirectorMode {
    Chase,
    Combat,
    AimFocus,
    HighSpeed,
    Tunnel,
    Boss,
    Setpiece,
    Cinematic,
    EventAccent,
    Recovery,
};

struct RailCameraComfortSettings {
    bool enabled = true;
    float stableAngularVelocityDeg = 42.0f;
    float stableAngularAccelerationDeg = 720.0f;
    float stableFovChangeRateDeg = 28.0f;
    float stableRollDeg = 10.0f;
    float stableShakeAmount = 0.60f;
    float hardTransitionAngularVelocityDeg = 105.0f;
    float hardTransitionFovChangeRateDeg = 55.0f;
    float hardTransitionRollDeg = 18.0f;
};

struct RailCameraAimFocusSettings {
    bool enabled = true;
    float blendInRate = 9.0f;
    float blendOutRate = 5.5f;
    float maxReticleVelocityForFullFocus = 720.0f;
    float fovOffsetDeg = -2.0f;
    float maxLockFovOffsetDeg = -1.0f;
    float rollSuppression = 0.82f;
    float shakeSuppression = 0.78f;
    float lateralSuppression = 0.22f;
    float lookAheadBoost = 7.0f;
    float backDistanceBoost = 1.8f;
};

enum class RailCameraLookAtPolicy {
    RailLookAhead,
    LockToken,
    ThreatCenter,
    BossThreat,
    Obstacle,
};

struct RailCameraLookAtSettings {
    bool enabled = true;
    float blendRate = 7.5f;
    float releaseBlendRate = 4.2f;
    float maxForwardDistance = 160.0f;
    float minForwardDistance = -18.0f;
    float lockTokenWeight = 4.8f;
    float enemyWeight = 1.0f;
    float bossWeight = 3.2f;
    float obstacleWeight = 0.42f;
    float centerRetention = 0.34f;
    float maxTargetOffset = 46.0f;
};

struct RailCameraCompositionSafetySettings {
    bool enabled = true;
    float aimableZoneWidth = 0.58f;
    float aimableZoneHeight = 0.58f;
    float readabilityZoneWidth = 0.82f;
    float readabilityZoneHeight = 0.78f;
    float minForwardDistance = 4.0f;
    float maxForwardDistance = 170.0f;
    float blendInRate = 5.8f;
    float blendOutRate = 3.2f;
    float maxTargetCorrection = 18.0f;
    float correctionGain = 0.72f;
    float fovExpandDeg = 4.5f;
    float maxFovDeg = 74.0f;
    float lockTokenWeight = 4.2f;
    float bossWeight = 3.0f;
    float enemyWeight = 1.0f;
    float obstacleWeight = 0.45f;
    float fireBlockRisk = 0.62f;
};

struct RailCameraLineOfSightSettings {
    bool enabled = true;
    float minForwardDistance = 4.0f;
    float maxForwardDistance = 180.0f;
    float obstaclePadding = 0.80f;
    float targetReleaseStrength = 0.46f;
    float fovExpandDeg = 3.0f;
    float maxFovDeg = 76.0f;
    bool blockEnemyFireWhenOccluded = true;
    bool preferBaseTargetWhenOccluded = true;
};

struct RailCameraCollisionProtectionSettings {
    bool enabled = true;
    float obstaclePadding = 0.95f;
    float minClearance = 1.35f;
    float nearClipClearanceMultiplier = 6.0f;
    float maxPushDistance = 5.5f;
    float targetCompensation = 0.34f;
    float fovExpandDeg = 2.6f;
    float maxFovDeg = 76.0f;
    bool blockEnemyFireWhenUnsafe = true;
};

struct RailCameraSegmentTransitionSettings {
    bool enabled = true;
    float duration = 0.72f;
    float minDuration = 0.18f;
    float highSpeedDuration = 0.48f;
    float bossDuration = 0.92f;
    float tunnelDuration = 0.62f;
    float maxPositionBlendDistance = 34.0f;
    float maxTargetBlendDistance = 48.0f;
    float rollBlendStrength = 0.82f;
    float fovBlendStrength = 0.86f;
    float shakeDampen = 0.35f;
    float enemyFireHold = 0.20f;
    float comfortGraceMultiplier = 1.35f;
};

struct RailCameraEncounterFramingSettings {
    bool enabled = true;
    float blendInRate = 6.8f;
    float blendOutRate = 3.4f;
    float waveHoldDuration = 1.15f;
    float bossHoldDuration = 2.20f;
    float obstacleHoldDuration = 0.70f;
    float minForwardDistance = -6.0f;
    float maxForwardDistance = 175.0f;
    float minActiveEnemyFocus = 2.0f;
    float enemyCountForFullWide = 6.0f;
    float enemySpreadForFullWide = 14.0f;
    float bossFocusBoost = 0.42f;
    float fovExpandDeg = 5.0f;
    float bossFovExpandDeg = 3.0f;
    float maxFovDeg = 76.0f;
    float lookAheadBoost = 4.0f;
    float backDistanceBoost = 2.0f;
    float lateralDampen = 0.70f;
    float rollDampen = 0.65f;
    float fireHoldDuration = 0.28f;
};

struct RailCameraDirectorFrameInput {
    const CourseAsset* course = nullptr;
    const RailPath* railPath = nullptr;
    const CourseSection* section = nullptr;
    float distance = 0.0f;
    float deltaTime = 0.016f;
    float railSpeed = 0.0f;
    bool lockHeld = false;
    bool lockPressed = false;
    bool lockReleased = false;
    bool lockAimFeelActive = false;
    int lockTokenCount = 0;
    int maxLockCount = 1;
    Vector2 reticleVelocity{};
    const CourseSpawnRuntime* spawnRuntime = nullptr;
    const std::vector<RailLockToken>* lockTokens = nullptr;
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;
    float nearClipDistance = 0.10f;
};

struct RailCameraDirectorFrame {
    CourseCameraKey rig{};
    Vector3 position{};
    Vector3 target{};
    Vector3 baseTarget{};
    Vector3 threatCenter{};
    Vector3 up{0.0f, 1.0f, 0.0f};
    Vector3 forward{0.0f, 0.0f, 1.0f};
    float fovY = 0.30f * 3.14159265358979323846f;
    float shakeAmount = 0.0f;
    float railSpeed = 0.0f;
    float angularVelocityDeg = 0.0f;
    float angularAccelerationDeg = 0.0f;
    float fovChangeRateDeg = 0.0f;
    float rollDeg = 0.0f;
    float linearSpeed = 0.0f;
    float stabilityScore = 1.0f;
    float aimFocusBlend = 0.0f;
    float aimFocusStrength = 0.0f;
    float lookAtBlend = 0.0f;
    float lookAtWeight = 0.0f;
    float compositionSafetyBlend = 0.0f;
    float compositionRisk = 0.0f;
    float compositionFovOffsetDeg = 0.0f;
    float lineOfSightFovOffsetDeg = 0.0f;
    float cameraCollisionPushDistance = 0.0f;
    float cameraCollisionClosestDistance = 9999.0f;
    float cameraCollisionFovOffsetDeg = 0.0f;
    float segmentTransitionBlend = 1.0f;
    float segmentTransitionRemaining = 0.0f;
    float encounterFramingBlend = 0.0f;
    float encounterFramingFovOffsetDeg = 0.0f;
    float encounterFramingThreatSpread = 0.0f;
    float encounterFramingRemaining = 0.0f;
    float cinematicShotWeight = 0.0f;
    Vector2 compositionCorrection{};
    int lookAtCandidateCount = 0;
    int compositionCandidateCount = 0;
    int compositionOutOfAimableCount = 0;
    int compositionOutOfReadabilityCount = 0;
    int lineOfSightCandidateCount = 0;
    int lineOfSightBlockedCount = 0;
    int cameraCollisionObstacleCount = 0;
    int encounterFramingEnemyCount = 0;
    int encounterFramingBossCount = 0;
    uint32_t lineOfSightOccluderActorId = 0;
    uint32_t cameraCollisionObstacleActorId = 0;
    bool lockCameraStabilized = false;
    bool compositionSafeForAiming = true;
    bool lineOfSightSafeForAiming = true;
    bool cameraCollisionSafe = true;
    bool segmentTransitionActive = false;
    bool encounterFramingActive = false;
    bool stableForAiming = true;
    bool hardTransition = false;
    bool allowEnemyFire = true;
    RailCameraDirectorMode modeKind = RailCameraDirectorMode::Chase;
    RailCameraLookAtPolicy lookAtPolicy = RailCameraLookAtPolicy::RailLookAhead;
    std::string mode = "Default";
    std::string comfortReason = "stable";
    std::string lookAtReason = "rail look-ahead";
    std::string compositionReason = "no composition pressure";
    std::string lineOfSightReason = "clear";
    std::string cameraCollisionReason = "clear";
    std::string segmentTransitionReason = "stable section";
    std::string encounterFramingReason = "no encounter framing";
    std::string cinematicShotId = "-";
    std::string cinematicShotPresetId = "-";
    std::string cinematicShotBlendAssetId = "-";
    std::string cinematicShotBlendCurve = "-";
    std::string previousSectionName = "-";
    std::string currentSectionName = "-";
};

class RailCameraDirector {
public:
    void Reset();
    void NotifyCourseEvents(const std::vector<CourseEventMarker>& events);
    void AddFeedbackImpulse(float shakeAmplitude, float fovKick, float rollKick);
    RailCameraDirectorFrame Evaluate(const RailCameraDirectorFrameInput& input);
    const RailCameraDirectorFrame& LastFrame() const { return lastFrame_; }
    const RailCameraComfortSettings& ComfortSettings() const { return comfortSettings_; }
    RailCameraComfortSettings& MutableComfortSettings() { return comfortSettings_; }
    const RailCameraAimFocusSettings& AimFocusSettings() const { return aimFocusSettings_; }
    RailCameraAimFocusSettings& MutableAimFocusSettings() { return aimFocusSettings_; }
    const RailCameraLookAtSettings& LookAtSettings() const { return lookAtSettings_; }
    RailCameraLookAtSettings& MutableLookAtSettings() { return lookAtSettings_; }
    const RailCameraCompositionSafetySettings& CompositionSafetySettings() const { return compositionSafetySettings_; }
    RailCameraCompositionSafetySettings& MutableCompositionSafetySettings() { return compositionSafetySettings_; }
    const RailCameraLineOfSightSettings& LineOfSightSettings() const { return lineOfSightSettings_; }
    RailCameraLineOfSightSettings& MutableLineOfSightSettings() { return lineOfSightSettings_; }
    const RailCameraCollisionProtectionSettings& CollisionProtectionSettings() const { return collisionProtectionSettings_; }
    RailCameraCollisionProtectionSettings& MutableCollisionProtectionSettings() { return collisionProtectionSettings_; }
    const RailCameraSegmentTransitionSettings& SegmentTransitionSettings() const { return segmentTransitionSettings_; }
    RailCameraSegmentTransitionSettings& MutableSegmentTransitionSettings() { return segmentTransitionSettings_; }
    const RailCameraEncounterFramingSettings& EncounterFramingSettings() const { return encounterFramingSettings_; }
    RailCameraEncounterFramingSettings& MutableEncounterFramingSettings() { return encounterFramingSettings_; }

private:
    CourseCameraKey SmoothRig(const CourseCameraKey& target, float deltaTime);
    float UpdateAimFocusBlend(const RailCameraDirectorFrameInput& input);
    void ApplyAimFocusStabilization(
        CourseCameraKey& rig,
        const RailCameraDirectorFrameInput& input,
        std::string& mode,
        RailCameraDirectorFrame& frame);
    void ApplyEncounterFramingRules(
        CourseCameraKey& rig,
        const RailCameraDirectorFrameInput& input,
        std::string& mode,
        RailCameraDirectorFrame& frame);
    void UpdateLookAtTarget(
        RailCameraDirectorFrame& frame,
        const RailCameraDirectorFrameInput& input,
        const RailPathSample& cameraSample);
    void ApplyCompositionSafety(
        RailCameraDirectorFrame& frame,
        const RailCameraDirectorFrameInput& input,
        const RailPathSample& cameraSample,
        std::string& mode);
    void ApplyLineOfSightSafety(
        RailCameraDirectorFrame& frame,
        const RailCameraDirectorFrameInput& input,
        std::string& mode);
    void ApplyCameraCollisionProtection(
        RailCameraDirectorFrame& frame,
        const RailCameraDirectorFrameInput& input,
        const RailPathSample& cameraSample,
        std::string& mode);
    void BeginSegmentTransitionIfNeeded(const RailCameraDirectorFrameInput& input);
    void ApplySegmentTransitionPolish(
        RailCameraDirectorFrame& frame,
        const RailCameraDirectorFrameInput& input,
        std::string& mode);
    void ApplySectionDirecting(CourseCameraKey& rig, const CourseSection* section, std::string& mode) const;
    void ApplyCinematicShotDirecting(
        CourseCameraKey& rig,
        const CourseAsset* course,
        float distance,
        std::string& mode,
        RailCameraDirectorFrame& frame) const;
    void ApplyEventDirecting(CourseCameraKey& rig, float deltaTime, std::string& mode);
    void UpdateComfortMetrics(RailCameraDirectorFrame& frame, const RailCameraDirectorFrameInput& input);

    CourseCameraKey smoothedRig_{};
    RailCameraComfortSettings comfortSettings_{};
    RailCameraAimFocusSettings aimFocusSettings_{};
    RailCameraLookAtSettings lookAtSettings_{};
    RailCameraCompositionSafetySettings compositionSafetySettings_{};
    RailCameraLineOfSightSettings lineOfSightSettings_{};
    RailCameraCollisionProtectionSettings collisionProtectionSettings_{};
    RailCameraSegmentTransitionSettings segmentTransitionSettings_{};
    RailCameraEncounterFramingSettings encounterFramingSettings_{};
    RailCameraDirectorFrame lastFrame_{};
    bool hasSmoothedRig_ = false;
    bool hasPreviousComfortFrame_ = false;
    Vector3 previousPosition_{};
    Vector3 previousForward_{0.0f, 0.0f, 1.0f};
    float previousFovY_ = 0.30f * 3.14159265358979323846f;
    float previousRoll_ = 0.0f;
    float previousAngularVelocityDeg_ = 0.0f;
    float aimFocusBlend_ = 0.0f;
    float encounterFramingBlend_ = 0.0f;
    float encounterFramingHoldRemaining_ = 0.0f;
    float encounterFireHoldRemaining_ = 0.0f;
    std::string encounterFramingReason_ = "no encounter";
    float lookAtBlend_ = 0.0f;
    float compositionSafetyBlend_ = 0.0f;
    Vector2 smoothedCompositionCorrection_{};
    Vector3 smoothedLookAtTarget_{};
    bool hasSmoothedLookAtTarget_ = false;
    RailCameraDirectorFrame segmentTransitionStartFrame_{};
    bool hasSegmentTransitionStartFrame_ = false;
    std::string currentSectionSignature_;
    std::string previousSectionSignature_;
    float segmentTransitionElapsed_ = 0.0f;
    float segmentTransitionDuration_ = 0.0f;
    float fovKick_ = 0.0f;
    float rollKick_ = 0.0f;
    float shakeTime_ = 0.0f;
    float shakeAmplitude_ = 0.0f;
    float directorTime_ = 0.0f;
};

const char* ToRailCameraDirectorModeString(RailCameraDirectorMode mode);
const char* ToRailCameraLookAtPolicyString(RailCameraLookAtPolicy policy);
