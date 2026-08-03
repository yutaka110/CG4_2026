#include "RailLockOnSystem.h"

#include "../diagnostics/DebugDrawSystem.h"

#include <algorithm>
#include <cmath>

void RailLockOnSystem::Reset() {
    reticle_.Reset();
    aimAssist_.Reset();
    resolver_.Reset();
    debugFrame_ = {};
    elapsedTime_ = 0.0f;
}

void RailLockOnSystem::Update(const RailLockOnFrameInput& input) {
    elapsedTime_ += (std::max)(0.0f, input.deltaTime);

    RailTargetRegistryFrameInput registryInput{};
    registryInput.spawnRuntime = input.spawnRuntime;
    registryInput.railPath = input.railPath;
    registryInput.playerDistance = input.playerDistance;
    registryInput.settings = settings_;
    registryInput.cameraPosition = input.gameplayCameraPosition;
    registry_.Build(registryInput);

    RailReticleFrameInput reticleInput{};
    reticleInput.hwnd = input.hwnd;
    reticleInput.deltaTime = input.deltaTime;
    reticleInput.viewportWidth = input.viewportWidth;
    reticleInput.viewportHeight = input.viewportHeight;
    reticleInput.hasCursorPosition = input.hasCursorPosition;
    reticleInput.cursorPosition = input.cursorPosition;
    reticleInput.anchors = &registry_.Anchors();
    reticleInput.gameplayViewProjection = input.gameplayViewProjection;
    reticleInput.gameplayCameraPosition = input.gameplayCameraPosition;
    reticleInput.aimRayMaxDistance = input.aimRayMaxDistance;
    reticleInput.gamepadConnected = input.gamepadConnected;
    reticleInput.gamepadAim = input.gamepadAim;
    reticleInput.aimFrictionScale = aimAssist_.Frame().inputFrictionScale;
    reticleInput.settings = settings_;
    reticle_.Update(reticleInput);

    RailWorldRaycastInput raycastInput{};
    raycastInput.railPath = input.railPath;
    raycastInput.spawnRuntime = input.spawnRuntime;
    raycastInput.course = input.course;
    raycastInput.terrainSettings = input.terrainSettings;
    raycastInput.terrainEdits = input.terrainEdits;
    raycastInput.terrainPreview = input.terrainPreview;
    raycastInput.playerDistance = input.playerDistance;

    const Vector2 reticleVelocity = reticle_.State().velocity;
    RailAimAssistFrameInput aimAssistInput{};
    aimAssistInput.rawAim = &reticle_.State().aim;
    aimAssistInput.anchors = &registry_.Anchors();
    aimAssistInput.visibilityQuery = &raycastInput;
    aimAssistInput.inputDevice = reticle_.InputDeviceState().activeDevice;
    aimAssistInput.settings = aimAssistSettings_;
    aimAssistInput.deltaTime = input.deltaTime;
    aimAssistInput.reticleSpeedPixelsPerSecond = std::sqrt(
        reticleVelocity.x * reticleVelocity.x + reticleVelocity.y * reticleVelocity.y);
    aimAssistInput.enabled = input.aimAssistEnabled;
    aimAssistInput.lockModeActive = reticle_.State().lockHeld;
    aimAssist_.Update(aimAssistInput);
    reticle_.SetAim(aimAssist_.AssistedAim());

    raycastInput.aim = &reticle_.State().aim;
    reticle_.ApplyAimHit(RailWorldRaycast::Query(raycastInput));

    RailLockResolverFrameInput resolverInput{};
    resolverInput.anchors = &registry_.Anchors();
    resolverInput.reticle = &reticle_.State();
    resolverInput.viewProjection = input.gameplayViewProjection;
    resolverInput.viewportWidth = input.viewportWidth;
    resolverInput.viewportHeight = input.viewportHeight;
    resolverInput.elapsedTime = elapsedTime_;
    resolverInput.settings = settings_;
    resolver_.Update(resolverInput);

    lastRelease_ = {};
    if (reticle_.State().lockReleased && !resolver_.Tokens().empty()) {
        lastRelease_.releaseTime = elapsedTime_;
        lastRelease_.tokens = resolver_.ConsumeTokens();
    }

    debugFrame_.candidates = resolver_.Candidates();
    debugFrame_.tokens = resolver_.Tokens();
    debugFrame_.acquiredTokens = resolver_.AcceptedTokensThisFrame();
    debugFrame_.releasedTokens = lastRelease_.tokens;
    debugFrame_.reticle = reticle_.State();
    debugFrame_.elapsedTime = elapsedTime_;
    debugFrame_.anchorCount = static_cast<int>(registry_.Anchors().size());
    debugFrame_.acceptedThisFrame = resolver_.AcceptedThisFrame();
    debugFrame_.releasedThisFrame = static_cast<int>(lastRelease_.tokens.size());
}

void RailLockOnSystem::AppendDebugDraw(ge3::debug::DebugDrawSystem& debugDraw) const {
    const RailAimState& aim = reticle_.State().aim;
    if (aim.valid) {
        const Vector4 rayColor = aim.hasWorldHit
            ? Vector4{1.0f, 0.32f, 0.12f, 0.92f}
            : Vector4{0.24f, 0.72f, 1.0f, 0.56f};
        debugDraw.AddLine(aim.worldRayOrigin, aim.worldAimPoint, rayColor);
        debugDraw.AddPoint(aim.worldAimPoint, aim.hasWorldHit ? 0.55f : 0.24f, rayColor);
        if (aim.hasWorldHit) {
            debugDraw.AddLine(
                aim.worldAimPoint,
                {
                    aim.worldAimPoint.x + aim.worldAimNormal.x * 2.0f,
                    aim.worldAimPoint.y + aim.worldAimNormal.y * 2.0f,
                    aim.worldAimPoint.z + aim.worldAimNormal.z * 2.0f},
                {0.25f, 1.0f, 0.42f, 0.92f});
        }
    }

    const RailAimAssistFrame& assist = aimAssist_.Frame();
    if (assist.active) {
        debugDraw.AddLine(
            assist.rawAim.worldRayOrigin,
            {
                assist.rawAim.worldRayOrigin.x +
                    assist.rawAim.worldRayDirection.x * assist.rawAim.maxDistance,
                assist.rawAim.worldRayOrigin.y +
                    assist.rawAim.worldRayDirection.y * assist.rawAim.maxDistance,
                assist.rawAim.worldRayOrigin.z +
                    assist.rawAim.worldRayDirection.z * assist.rawAim.maxDistance},
            {0.35f, 0.55f, 1.0f, 0.28f});
        debugDraw.AddPoint(
            assist.targetWorldPosition,
            0.8f,
            {1.0f, 0.82f, 0.18f, 0.95f});
    }

    for (const RailLockCandidate& candidate : debugFrame_.candidates) {
        Vector4 color{0.25f, 0.65f, 1.0f, 0.75f};
        if (candidate.lockable) {
            color = {0.1f, 1.0f, 0.45f, 1.0f};
        } else if (candidate.rejectReason == RailLockRejectReason::AlreadyLocked) {
            color = {1.0f, 0.86f, 0.2f, 1.0f};
        } else if (candidate.rejectReason != RailLockRejectReason::NotSwept &&
                   candidate.rejectReason != RailLockRejectReason::LockModeInactive) {
            color = {1.0f, 0.25f, 0.18f, 0.75f};
        }
        debugDraw.AddPoint(candidate.anchor.worldPosition, 1.1f, color);
        debugDraw.AddCircle(
            candidate.anchor.worldPosition,
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            1.5f,
            color,
            20);
    }
}
