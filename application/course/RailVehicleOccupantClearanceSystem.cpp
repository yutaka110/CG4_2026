#include "RailVehicleOccupantClearanceSystem.h"

#include "../terrain/RailPath.h"
#include "../terrain/TerrainGenerationSettings.h"
#include "../terrain/TerrainVolumeField.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

struct RailLocalPoint final {
    float distance = 0.0f;
    float lateral = 0.0f;
    float vertical = 0.0f;
};

bool Finite(float value) noexcept { return std::isfinite(value); }

void SetError(std::string* errorMessage, const std::string& message) {
    if (errorMessage != nullptr) *errorMessage = message;
}

float Lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

RailLocalPoint Lerp(
    const RailLocalPoint& a,
    const RailLocalPoint& b,
    float t) noexcept {
    return {
        Lerp(a.distance, b.distance, t),
        Lerp(a.lateral, b.lateral, t),
        Lerp(a.vertical, b.vertical, t)};
}

bool SegmentExpandedAabb(
    const RailLocalPoint& start,
    const RailLocalPoint& end,
    const RailLocalPoint& center,
    const Vector3& halfExtents,
    float expansion,
    float& outEnterFraction,
    bool& outStartedInside) noexcept {
    const float startValues[3] = {start.distance, start.lateral, start.vertical};
    const float endValues[3] = {end.distance, end.lateral, end.vertical};
    const float centerValues[3] = {center.distance, center.lateral, center.vertical};
    const float extentValues[3] = {
        (std::max)(std::abs(halfExtents.z), 0.01f) + expansion,
        (std::max)(std::abs(halfExtents.x), 0.01f) + expansion,
        (std::max)(std::abs(halfExtents.y), 0.01f) + expansion};
    float enter = 0.0f;
    float leave = 1.0f;
    outStartedInside = true;
    for (uint32_t axis = 0; axis < 3; ++axis) {
        const float minimum = centerValues[axis] - extentValues[axis];
        const float maximum = centerValues[axis] + extentValues[axis];
        const float origin = startValues[axis];
        const float delta = endValues[axis] - origin;
        if (origin < minimum || origin > maximum) outStartedInside = false;
        if (std::abs(delta) <= 0.000001f) {
            if (origin < minimum || origin > maximum) return false;
            continue;
        }
        float first = (minimum - origin) / delta;
        float second = (maximum - origin) / delta;
        if (first > second) std::swap(first, second);
        enter = (std::max)(enter, first);
        leave = (std::min)(leave, second);
        if (enter > leave) return false;
    }
    if (leave < 0.0f || enter > 1.0f) return false;
    outEnterFraction = (std::clamp)(enter, 0.0f, 1.0f);
    return true;
}

bool OverlapsSegmentBounds(
    const RailLocalPoint& start,
    const RailLocalPoint& end,
    const RailLocalPoint& center,
    const Vector3& halfExtents,
    float expansion) noexcept {
    const float minimumDistance = (std::min)(start.distance, end.distance);
    const float maximumDistance = (std::max)(start.distance, end.distance);
    const float minimumLateral = (std::min)(start.lateral, end.lateral);
    const float maximumLateral = (std::max)(start.lateral, end.lateral);
    const float minimumVertical = (std::min)(start.vertical, end.vertical);
    const float maximumVertical = (std::max)(start.vertical, end.vertical);
    return center.distance + std::abs(halfExtents.z) + expansion >= minimumDistance &&
        center.distance - std::abs(halfExtents.z) - expansion <= maximumDistance &&
        center.lateral + std::abs(halfExtents.x) + expansion >= minimumLateral &&
        center.lateral - std::abs(halfExtents.x) - expansion <= maximumLateral &&
        center.vertical + std::abs(halfExtents.y) + expansion >= minimumVertical &&
        center.vertical - std::abs(halfExtents.y) - expansion <= maximumVertical;
}

} // namespace

RailVehicleOccupantClearanceDefinition
RailVehicleOccupantClearanceDefinition::MineCartDefaults() {
    return {};
}

bool RailVehicleOccupantClearanceDefinition::Validate(
    std::string* errorMessage) const {
    const bool valid = Finite(occupantRadius) && Finite(safetyMargin) &&
        Finite(contactFractionEpsilon) && occupantRadius > 0.0f &&
        safetyMargin >= 0.0f && contactFractionEpsilon >= 0.0f &&
        contactFractionEpsilon <= 0.05f && maximumObstacleCandidates > 0 &&
        maximumTerrainCandidates > 0 && proceduralSweepSamples >= 2 &&
        proceduralSweepSamples <= 128 && proceduralRefinementSteps <= 16;
    if (!valid) {
        SetError(errorMessage, "Rail vehicle occupant clearance definition is invalid.");
        return false;
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

RailVehicleOccupantClearanceSystem::RailVehicleOccupantClearanceSystem() {
    (void)Initialize(
        RailVehicleOccupantClearanceDefinition::MineCartDefaults(), nullptr);
}

bool RailVehicleOccupantClearanceSystem::Initialize(
    const RailVehicleOccupantClearanceDefinition& definition,
    std::string* errorMessage) {
    if (!definition.Validate(errorMessage)) return false;
    definition_ = definition;
    initialized_ = true;
    Reset();
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

void RailVehicleOccupantClearanceSystem::Reset() {
    frame_ = {};
    frame_.safeFraction = 1.0f;
    frame_.revision = ++revision_;
}

const RailVehicleOccupantClearanceFrame&
RailVehicleOccupantClearanceSystem::Update(
    const RailVehicleOccupantClearanceInput& input) {
    frame_ = {};
    frame_.safeFraction = 1.0f;
    frame_.revision = ++revision_;
    const bool finiteInput = Finite(input.startDistance) &&
        Finite(input.startLateralOffset) && Finite(input.startVerticalOffset) &&
        Finite(input.targetDistance) && Finite(input.targetLateralOffset) &&
        Finite(input.targetVerticalOffset);
    if (!initialized_ || !input.gameplayActive || !finiteInput) return frame_;

    frame_.valid = true;
    frame_.requestedDistance = input.targetDistance;
    frame_.requestedLateralOffset = input.targetLateralOffset;
    frame_.requestedVerticalOffset = input.targetVerticalOffset;
    const RailLocalPoint start{
        input.startDistance,
        input.startLateralOffset,
        input.startVerticalOffset};
    const RailLocalPoint target{
        input.targetDistance,
        input.targetLateralOffset,
        input.targetVerticalOffset};
    float bestFraction = 1.0f;
    const float expansion = definition_.occupantRadius + definition_.safetyMargin;

    const auto considerHit = [&] (
        float fraction,
        bool startedInside,
        RailVehicleClearanceHitKind kind,
        uint32_t actorId,
        const std::string& stableId) {
        if (fraction > bestFraction) return;
        const bool replaces = fraction < bestFraction ||
            frame_.hitKind == RailVehicleClearanceHitKind::None;
        if (!replaces) return;
        bestFraction = fraction;
        frame_.startedPenetrating = startedInside;
        frame_.hitKind = kind;
        frame_.hitActorId = actorId;
        frame_.hitStableId = stableId;
    };

    if (definition_.includeDynamicObstacles && input.spawnRuntime != nullptr) {
        for (const CourseObstacleActor& obstacle : input.spawnRuntime->Obstacles()) {
            const RailLocalPoint center{
                obstacle.desc.spawnDistance + obstacle.desc.distanceOffset,
                obstacle.desc.lateralOffset,
                obstacle.desc.verticalOffset};
            if (!OverlapsSegmentBounds(
                    start, target, center, obstacle.desc.halfExtents,
                    expansion)) {
                continue;
            }
            if (frame_.obstacleCandidatesTested >=
                definition_.maximumObstacleCandidates) {
                frame_.budgetExceeded = true;
                break;
            }
            ++frame_.obstacleCandidatesTested;
            float fraction = 1.0f;
            bool startedInside = false;
            if (SegmentExpandedAabb(
                    start, target, center, obstacle.desc.halfExtents,
                    expansion, fraction, startedInside)) {
                considerHit(
                    fraction, startedInside,
                    RailVehicleClearanceHitKind::DynamicObstacle,
                    obstacle.actorId, obstacle.desc.id);
            }
        }
    }

    if (definition_.includeTerrainPlacements && input.course != nullptr) {
        for (const CourseTerrainPlacement& placement :
             input.course->terrainPlacements) {
            if (placement.layer != CourseTerrainLayer::GameplayCollision ||
                placement.collisionMode == CourseTerrainCollisionMode::None) {
                continue;
            }
            const RailLocalPoint center{
                placement.distance + placement.forwardOffset,
                placement.lateralOffset,
                placement.verticalOffset};
            if (!OverlapsSegmentBounds(
                    start, target, center, placement.scale, expansion)) {
                continue;
            }
            if (frame_.terrainCandidatesTested >=
                definition_.maximumTerrainCandidates) {
                frame_.budgetExceeded = true;
                break;
            }
            ++frame_.terrainCandidatesTested;
            float fraction = 1.0f;
            bool startedInside = false;
            if (SegmentExpandedAabb(
                    start, target, center, placement.scale,
                    expansion, fraction, startedInside)) {
                considerHit(
                    fraction, startedInside,
                    RailVehicleClearanceHitKind::TerrainPlacement,
                    0u,
                    placement.editorGuid.empty()
                        ? placement.id : placement.editorGuid);
            }
        }
    }

    if (definition_.includeProceduralTerrain && input.railPath != nullptr &&
        input.terrainSettings != nullptr && input.railPath->Length() > 0.0f) {
        TerrainVolumeField field(
            *input.railPath,
            *input.terrainSettings,
            input.terrainEdits,
            input.terrainPreview);
        const auto unsafe = [&] (float fraction) {
            const RailLocalPoint point = Lerp(start, target, fraction);
            ++frame_.proceduralSamplesTested;
            const TerrainVolumeLocalSample sample = field.SampleLocal(
                point.distance, point.lateral, point.vertical);
            if (sample.sdf >= 0.0f) return true;
            const float normalizedRadius = sample.sdf + 1.0f;
            const float radialDistance = std::sqrt(
                point.lateral * point.lateral +
                point.vertical * point.vertical);
            float surfaceClearance = (std::min)(
                input.terrainSettings->canyonHalfWidth,
                input.terrainSettings->wallHeight);
            if (normalizedRadius > 0.0001f) {
                surfaceClearance = radialDistance *
                    (1.0f / normalizedRadius - 1.0f);
            }
            return surfaceClearance <= expansion;
        };
        if (unsafe(0.0f)) {
            considerHit(
                0.0f, true,
                RailVehicleClearanceHitKind::ProceduralTerrain,
                0u, "procedural_terrain");
        } else {
            float previousFraction = 0.0f;
            for (uint32_t sample = 1;
                 sample <= definition_.proceduralSweepSamples;
                 ++sample) {
                const float fraction = static_cast<float>(sample) /
                    static_cast<float>(definition_.proceduralSweepSamples);
                const bool currentUnsafe = unsafe(fraction);
                if (currentUnsafe) {
                    float low = previousFraction;
                    float high = fraction;
                    for (uint32_t refinement = 0;
                         refinement < definition_.proceduralRefinementSteps;
                         ++refinement) {
                        const float middle = (low + high) * 0.5f;
                        if (unsafe(middle)) high = middle;
                        else low = middle;
                    }
                    considerHit(
                        high, false,
                        RailVehicleClearanceHitKind::ProceduralTerrain,
                        0u, "procedural_terrain");
                    break;
                }
                previousFraction = fraction;
            }
        }
    }

    if (frame_.budgetExceeded) {
        bestFraction = 0.0f;
        frame_.startedPenetrating = false;
        frame_.hitKind = RailVehicleClearanceHitKind::QueryBudgetExceeded;
        frame_.hitActorId = 0;
        frame_.hitStableId = "clearance_query_budget";
    }

    if (frame_.hitKind != RailVehicleClearanceHitKind::None) {
        frame_.safeFraction = (std::clamp)(
            bestFraction - definition_.contactFractionEpsilon,
            0.0f, 1.0f);
        frame_.blocked = frame_.safeFraction < 0.9999f;
    }
    frame_.resolvedDistance = Lerp(
        input.startDistance, input.targetDistance, frame_.safeFraction);
    frame_.resolvedLateralOffset = Lerp(
        input.startLateralOffset,
        input.targetLateralOffset,
        frame_.safeFraction);
    frame_.resolvedVerticalOffset = Lerp(
        input.startVerticalOffset,
        input.targetVerticalOffset,
        frame_.safeFraction);
    return frame_;
}

const char* ToString(RailVehicleClearanceHitKind kind) noexcept {
    switch (kind) {
    case RailVehicleClearanceHitKind::None: return "None";
    case RailVehicleClearanceHitKind::DynamicObstacle: return "DynamicObstacle";
    case RailVehicleClearanceHitKind::TerrainPlacement: return "TerrainPlacement";
    case RailVehicleClearanceHitKind::ProceduralTerrain: return "ProceduralTerrain";
    case RailVehicleClearanceHitKind::QueryBudgetExceeded: return "QueryBudgetExceeded";
    }
    return "Unknown";
}
