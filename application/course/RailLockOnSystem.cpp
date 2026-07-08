#include "RailLockOnSystem.h"

#include "../diagnostics/DebugDrawSystem.h"

#include <algorithm>

void RailLockOnSystem::Reset() {
    reticle_.Reset();
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
    registryInput.cameraPosition = input.cameraPosition;
    registry_.Build(registryInput);

    RailReticleFrameInput reticleInput{};
    reticleInput.hwnd = input.hwnd;
    reticleInput.deltaTime = input.deltaTime;
    reticleInput.viewportWidth = input.viewportWidth;
    reticleInput.viewportHeight = input.viewportHeight;
    reticleInput.hasCursorPosition = input.hasCursorPosition;
    reticleInput.cursorPosition = input.cursorPosition;
    reticleInput.anchors = &registry_.Anchors();
    reticleInput.viewProjection = input.viewProjection;
    reticleInput.settings = settings_;
    reticle_.Update(reticleInput);

    RailLockResolverFrameInput resolverInput{};
    resolverInput.anchors = &registry_.Anchors();
    resolverInput.reticle = &reticle_.State();
    resolverInput.viewProjection = input.viewProjection;
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
