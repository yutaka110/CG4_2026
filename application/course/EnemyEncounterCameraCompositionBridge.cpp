#include "EnemyEncounterCameraCompositionBridge.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "CourseSpawnRuntime.h"
#include "../terrain/RailPath.h"

namespace {
Vector3 Add(const Vector3& a, const Vector3& b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vector3 Scale(const Vector3& value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}
float PhaseWeight(EnemyEncounterBeatPhase phase) noexcept {
    switch (phase) {
    case EnemyEncounterBeatPhase::Establish: return 0.72f;
    case EnemyEncounterBeatPhase::Threaten: return 1.00f;
    case EnemyEncounterBeatPhase::Attack: return 0.86f;
    case EnemyEncounterBeatPhase::Recovery: return 0.48f;
    case EnemyEncounterBeatPhase::ExitResolve: return 0.58f;
    default: return 0.0f;
    }
}
} // namespace

void EnemyEncounterCameraCompositionBridge::Reset() {
    frame_ = {};
    blend_ = 0.0f;
    revision_ = 0;
}

const EnemyEncounterCameraCompositionFrame&
EnemyEncounterCameraCompositionBridge::Update(
    const EnemyEncounterCameraCompositionInput& input) {
    EnemyEncounterCameraCompositionFrame next{};
    next.revision = ++revision_;
    const bool requested = input.gameplayActive && input.pacing != nullptr &&
        input.pacing->active && input.pacing->cameraCompositionRequested;
    const float dt = std::isfinite(input.deltaTime)
        ? (std::clamp)(input.deltaTime, 0.0f, 0.25f) : 0.0f;
    const float target = requested ? 1.0f : 0.0f;
    const float rate = requested ? 6.5f : 3.8f;
    const float t = 1.0f - std::exp(-dt * rate);
    blend_ += (target - blend_) * (std::clamp)(t, 0.0f, 1.0f);
    blend_ = (std::clamp)(blend_, 0.0f, 1.0f);
    next.blend = blend_;
    if (input.pacing == nullptr || blend_ <= 0.001f) {
        frame_ = std::move(next);
        return frame_;
    }

    const EnemyEncounterPacingFrame& pacing = *input.pacing;
    const float phaseWeight = PhaseWeight(pacing.phase);
    next.active = requested || blend_ > 0.01f;
    next.phase = pacing.phase;
    next.beatGuid = pacing.activeBeatGuid;
    next.preferredCameraShotId = pacing.definition.cameraShotId;
    next.preferredCameraShotWeight =
        pacing.definition.cameraWeight * phaseWeight * blend_;
    next.focusWeight = (std::clamp)(
        pacing.definition.cameraFocusWeight * phaseWeight * blend_,
        0.0f, 1.0f);
    constexpr float kPi = 3.14159265358979323846f;
    next.fovOffsetRadians =
        pacing.definition.cameraFovOffsetDegrees * kPi / 180.0f *
        phaseWeight * blend_;
    next.backDistanceOffset =
        pacing.definition.cameraBackDistanceOffset * phaseWeight * blend_;

    if (input.runtime != nullptr && input.railPath != nullptr &&
        input.railPath->Length() > 0.0f) {
        Vector3 center{};
        for (const CourseEnemyActor& actor : input.runtime->Enemies()) {
            if (actor.desc.waveId != pacing.waveGuid) continue;
            const RailPathSample sample = input.railPath->Evaluate(
                actor.desc.spawnDistance + actor.desc.distanceOffset);
            const Vector3 world = Add(
                Add(sample.position,
                    Scale(sample.right, actor.desc.lateralOffset)),
                Scale(sample.up, actor.desc.verticalOffset));
            center = Add(center, world);
            ++next.focusActorCount;
        }
        if (next.focusActorCount > 0) {
            next.focusWorld = Scale(
                center, 1.0f / static_cast<float>(next.focusActorCount));
            next.hasFocusWorld = true;
        }
    }
    if (!next.hasFocusWorld && input.railPath != nullptr &&
        input.railPath->Length() > 0.0f) {
        const RailPathSample fallback = input.railPath->Evaluate(
            input.playerDistance + 42.0f);
        next.focusWorld = fallback.position;
        next.hasFocusWorld = true;
    }
    frame_ = std::move(next);
    return frame_;
}

