#include "EnemyAttackLaneTelegraphRenderer.h"

#include "../EffectRuntime.h"
#include "../diagnostics/DebugDrawSystem.h"
#include "../terrain/RailPath.h"

#include <algorithm>
#include <cmath>

namespace {
Vector3 Add(Vector3 a, Vector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vector3 Subtract(Vector3 a, Vector3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vector3 Scale(Vector3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}
Vector3 Lerp(Vector3 a, Vector3 b, float t) noexcept {
    return Add(a, Scale(Subtract(b, a), t));
}
float PhaseOpacity(EnemyAttackTelegraphPhase phase) noexcept {
    switch (phase) {
    case EnemyAttackTelegraphPhase::Warming: return 0.34f;
    case EnemyAttackTelegraphPhase::Tracking: return 0.52f;
    case EnemyAttackTelegraphPhase::Imminent: return 0.92f;
    case EnemyAttackTelegraphPhase::Fired: return 0.68f;
    case EnemyAttackTelegraphPhase::None: return 0.0f;
    }
    return 0.0f;
}
Vector4 TrajectoryColor(EnemyProjectileTrajectory trajectory, float alpha) noexcept {
    switch (trajectory) {
    case EnemyProjectileTrajectory::Direct: return {1.0f, 0.08f, 0.72f, alpha};
    case EnemyProjectileTrajectory::Predictive: return {1.0f, 0.72f, 0.08f, alpha};
    case EnemyProjectileTrajectory::Homing: return {0.92f, 0.12f, 1.0f, alpha};
    case EnemyProjectileTrajectory::Arc: return {1.0f, 0.22f, 0.10f, alpha};
    }
    return {1.0f, 0.08f, 0.72f, alpha};
}
EnemyAttackLaneShape ResolveShape(const EnemyAttackTelegraphCue& cue) noexcept {
    if (cue.projectileTrajectory == EnemyProjectileTrajectory::Homing) {
        return EnemyAttackLaneShape::Homing;
    }
    if (cue.projectileTrajectory == EnemyProjectileTrajectory::Arc) {
        return EnemyAttackLaneShape::Arc;
    }
    if (cue.projectileCount >= 3 ||
        cue.attackPattern == CourseEnemyFirePattern::Spread ||
        cue.attackPattern == CourseEnemyFirePattern::BossArc) {
        return EnemyAttackLaneShape::Fan;
    }
    return EnemyAttackLaneShape::Line;
}
void UpdateMarker(
    EffectRuntime& runtime,
    uint32_t id,
    const Vector3& position,
    const Vector4& color,
    float radius) {
    EffectInstance* instance = runtime.FindInstance(id);
    if (instance == nullptr) return;
    instance->transform.translate = position;
    instance->transform.scale = {radius, radius, radius};
    instance->color = color;
    instance->attached = true;
    instance->previewLoop = true;
}
} // namespace

size_t EnemyAttackLaneTelegraphRenderer::LaneKeyHash::operator()(
    const LaneKey& key) const noexcept {
    const size_t first = std::hash<uint32_t>{}(key.actorId);
    const size_t second = std::hash<uint64_t>{}(key.attackIntentSequence);
    return first ^ (second + 0x9e3779b9u + (first << 6u) + (first >> 2u));
}

void EnemyAttackLaneTelegraphRenderer::Reset(EffectRuntime* effectRuntime) {
    if (effectRuntime != nullptr) {
        for (auto& [key, markers] : managedMarkers_) {
            (void)key;
            StopMarkers(markers, effectRuntime);
        }
    }
    managedMarkers_.clear();
    frame_ = {};
    revision_ = 0;
}

void EnemyAttackLaneTelegraphRenderer::Update(
    const EnemyAttackLaneTelegraphRenderInput& input) {
    frame_ = {};
    const uint64_t revision = ++revision_;
    if (!input.settings.enabled || !input.gameplayActive ||
        input.telegraph == nullptr || input.railPath == nullptr ||
        input.railPath->Length() <= 0.0f) {
        StopUntouched(input.effectRuntime, revision);
        frame_.revision = revision;
        return;
    }

    const size_t budget = (std::max)(
        static_cast<size_t>(1), input.settings.maximumVisibleLanes);
    frame_.lanes.reserve((std::min)(budget, input.telegraph->cues.size()));
    for (const EnemyAttackTelegraphCue& cue : input.telegraph->cues) {
        if (frame_.lanes.size() >= budget) {
            ++frame_.droppedByBudget;
            continue;
        }
        if (cue.phase == EnemyAttackTelegraphPhase::None) continue;

        const RailPathSample targetSample = input.railPath->Evaluate(
            cue.targetRailDistance);
        const Vector3 target = Add(
            Add(
                targetSample.position,
                Scale(targetSample.right, cue.targetLateralOffset)),
            Scale(targetSample.up, cue.targetVerticalOffset));
        const float opacity = PhaseOpacity(cue.phase);
        const float urgencyScale = cue.phase == EnemyAttackTelegraphPhase::Imminent
            ? input.settings.imminentScale
            : 1.0f;
        const float pulse = 1.0f + 0.08f * std::sin(
            input.elapsedTime * 11.0f + static_cast<float>(cue.actorId % 13u));

        EnemyAttackLaneTelegraphProxy proxy{};
        proxy.actorId = cue.actorId;
        proxy.attackIntentSequence = cue.attackIntentSequence;
        proxy.attackTokenId = cue.attackTokenId;
        proxy.shape = ResolveShape(cue);
        proxy.phase = cue.phase;
        proxy.trajectory = cue.projectileTrajectory;
        proxy.startWorld = cue.worldPosition;
        proxy.targetWorld = target;
        proxy.railRight = targetSample.right;
        proxy.railUp = targetSample.up;
        proxy.opacity = opacity;
        proxy.color = TrajectoryColor(cue.projectileTrajectory, opacity);
        proxy.laneWidth = input.settings.baseLaneWidth * urgencyScale;
        proxy.sourceRadius = input.settings.sourceMarkerRadius *
            urgencyScale * pulse;
        proxy.targetRadius = input.settings.targetMarkerRadius *
            urgencyScale * pulse;
        proxy.projectileCount = (std::max)(1, cue.projectileCount);

        const LaneKey key{cue.actorId, cue.attackIntentSequence};
        ManagedMarkers& markers = managedMarkers_[key];
        markers.touchedRevision = revision;
        if (input.settings.effectRuntimeEnabled && input.effectRuntime != nullptr) {
            if (markers.sourceInstanceId == 0) {
                markers.sourceInstanceId = input.effectRuntime->PlayEffectWithParams(
                    input.settings.markerEffectId,
                    proxy.startWorld,
                    proxy.color,
                    {proxy.sourceRadius, proxy.sourceRadius, proxy.sourceRadius});
                if (markers.sourceInstanceId != 0) {
                    input.effectRuntime->SetEffectPreviewLoop(
                        markers.sourceInstanceId, true);
                }
            }
            if (markers.targetInstanceId == 0) {
                markers.targetInstanceId = input.effectRuntime->PlayEffectWithParams(
                    input.settings.markerEffectId,
                    proxy.targetWorld,
                    proxy.color,
                    {proxy.targetRadius, proxy.targetRadius, proxy.targetRadius});
                if (markers.targetInstanceId != 0) {
                    input.effectRuntime->SetEffectPreviewLoop(
                        markers.targetInstanceId, true);
                }
            }
            UpdateMarker(
                *input.effectRuntime,
                markers.sourceInstanceId,
                proxy.startWorld,
                proxy.color,
                proxy.sourceRadius);
            UpdateMarker(
                *input.effectRuntime,
                markers.targetInstanceId,
                proxy.targetWorld,
                proxy.color,
                proxy.targetRadius);
        }
        proxy.sourceEffectInstanceId = markers.sourceInstanceId;
        proxy.targetEffectInstanceId = markers.targetInstanceId;
        frame_.effectBackedMarkers +=
            (markers.sourceInstanceId != 0 ? 1u : 0u) +
            (markers.targetInstanceId != 0 ? 1u : 0u);
        frame_.lanes.push_back(std::move(proxy));
        ++frame_.productionSubmittedLanes;
    }

    StopUntouched(input.effectRuntime, revision);
    frame_.sourceTelegraphRevision = input.telegraph->revision;
    frame_.revision = revision;
}

void EnemyAttackLaneTelegraphRenderer::AppendProductionWorldPrimitives(
    ge3::debug::DebugDrawSystem& productionDraw) const {
    for (const EnemyAttackLaneTelegraphProxy& lane : frame_.lanes) {
        Vector4 faint = lane.color;
        faint.w *= 0.28f;
        productionDraw.AddLine(lane.startWorld, lane.targetWorld, faint, lane.color);
        productionDraw.AddCircle(
            lane.startWorld,
            lane.railRight,
            lane.railUp,
            lane.sourceRadius,
            lane.color,
            20);
        productionDraw.AddCircle(
            lane.targetWorld,
            lane.railRight,
            lane.railUp,
            lane.targetRadius,
            lane.color,
            24);

        if (lane.shape == EnemyAttackLaneShape::Fan) {
            const int count = (std::clamp)(lane.projectileCount, 3, 5);
            for (int index = 0; index < count; ++index) {
                const float centered = static_cast<float>(index) -
                    static_cast<float>(count - 1) * 0.5f;
                const Vector3 endpoint = Add(
                    lane.targetWorld,
                    Scale(lane.railRight, centered * lane.targetRadius * 2.4f));
                productionDraw.AddLine(lane.startWorld, endpoint, faint, lane.color);
            }
        } else if (lane.shape == EnemyAttackLaneShape::Homing) {
            productionDraw.AddCircle(
                lane.targetWorld,
                lane.railRight,
                lane.railUp,
                lane.targetRadius * 1.55f,
                faint,
                24);
        } else if (lane.shape == EnemyAttackLaneShape::Arc) {
            Vector3 previous = lane.startWorld;
            for (int segment = 1; segment <= 10; ++segment) {
                const float t = static_cast<float>(segment) / 10.0f;
                Vector3 point = Lerp(lane.startWorld, lane.targetWorld, t);
                point = Add(
                    point,
                    Scale(lane.railUp, std::sin(t * 3.14159265f) *
                        lane.targetRadius * 3.0f));
                productionDraw.AddLine(previous, point, faint, lane.color);
                previous = point;
            }
        }
    }
}

void EnemyAttackLaneTelegraphRenderer::AppendWorldPrimitives(
    ge3::debug::DebugDrawSystem& debugDraw) const {
    AppendProductionWorldPrimitives(debugDraw);
}

bool EnemyAttackLaneTelegraphRenderer::WasSubmitted(
    uint32_t actorId,
    uint64_t attackIntentSequence) const noexcept {
    return std::any_of(
        frame_.lanes.begin(),
        frame_.lanes.end(),
        [actorId, attackIntentSequence](
            const EnemyAttackLaneTelegraphProxy& lane) {
            return lane.actorId == actorId &&
                lane.attackIntentSequence == attackIntentSequence;
        });
}

void EnemyAttackLaneTelegraphRenderer::StopMarkers(
    ManagedMarkers& markers,
    EffectRuntime* runtime) {
    if (runtime != nullptr) {
        if (markers.sourceInstanceId != 0) runtime->StopEffect(markers.sourceInstanceId);
        if (markers.targetInstanceId != 0) runtime->StopEffect(markers.targetInstanceId);
    }
    markers.sourceInstanceId = 0;
    markers.targetInstanceId = 0;
}

void EnemyAttackLaneTelegraphRenderer::StopUntouched(
    EffectRuntime* runtime,
    uint64_t revision) {
    for (auto it = managedMarkers_.begin(); it != managedMarkers_.end();) {
        if (it->second.touchedRevision == revision) {
            ++it;
            continue;
        }
        StopMarkers(it->second, runtime);
        it = managedMarkers_.erase(it);
    }
}

const char* ToString(EnemyAttackLaneShape shape) noexcept {
    switch (shape) {
    case EnemyAttackLaneShape::Line: return "Line";
    case EnemyAttackLaneShape::Fan: return "Fan";
    case EnemyAttackLaneShape::Homing: return "Homing";
    case EnemyAttackLaneShape::Arc: return "Arc";
    }
    return "Unknown";
}
