#pragma once

#include <cstdint>
#include <string>

#include "CourseAsset.h"
#include "CourseSpawnRuntime.h"

class RailPath;
struct TerrainGenerationSettings;
class TerrainEditLayer;

enum class RailVehicleClearanceHitKind : uint8_t {
    None,
    DynamicObstacle,
    TerrainPlacement,
    ProceduralTerrain,
    QueryBudgetExceeded,
};

struct RailVehicleOccupantClearanceDefinition final {
    float occupantRadius = 0.82f;
    float safetyMargin = 0.12f;
    float contactFractionEpsilon = 0.002f;
    uint32_t maximumObstacleCandidates = 128;
    uint32_t maximumTerrainCandidates = 256;
    uint32_t proceduralSweepSamples = 16;
    uint32_t proceduralRefinementSteps = 6;
    bool includeDynamicObstacles = true;
    bool includeTerrainPlacements = true;
    bool includeProceduralTerrain = true;

    static RailVehicleOccupantClearanceDefinition MineCartDefaults();
    bool Validate(std::string* errorMessage = nullptr) const;
};

struct RailVehicleOccupantClearanceInput final {
    const CourseSpawnRuntime* spawnRuntime = nullptr;
    const CourseAsset* course = nullptr;
    const RailPath* railPath = nullptr;
    const TerrainGenerationSettings* terrainSettings = nullptr;
    const TerrainEditLayer* terrainEdits = nullptr;
    const TerrainEditLayer* terrainPreview = nullptr;
    float startDistance = 0.0f;
    float startLateralOffset = 0.0f;
    float startVerticalOffset = 0.0f;
    float targetDistance = 0.0f;
    float targetLateralOffset = 0.0f;
    float targetVerticalOffset = 0.0f;
    bool gameplayActive = true;
};

// Authoritative rail-local swept-sphere result. The query deliberately uses
// gameplay collision proxies rather than presentation meshes so replay,
// checkpoint restore and different render LODs produce the same safe offset.
struct RailVehicleOccupantClearanceFrame final {
    bool valid = false;
    bool blocked = false;
    bool startedPenetrating = false;
    bool budgetExceeded = false;
    float safeFraction = 1.0f;
    float requestedDistance = 0.0f;
    float requestedLateralOffset = 0.0f;
    float requestedVerticalOffset = 0.0f;
    float resolvedDistance = 0.0f;
    float resolvedLateralOffset = 0.0f;
    float resolvedVerticalOffset = 0.0f;
    RailVehicleClearanceHitKind hitKind = RailVehicleClearanceHitKind::None;
    uint32_t hitActorId = 0;
    std::string hitStableId;
    uint32_t obstacleCandidatesTested = 0;
    uint32_t terrainCandidatesTested = 0;
    uint32_t proceduralSamplesTested = 0;
    uint64_t revision = 0;
};

class RailVehicleOccupantClearanceSystem final {
public:
    RailVehicleOccupantClearanceSystem();

    bool Initialize(
        const RailVehicleOccupantClearanceDefinition& definition,
        std::string* errorMessage = nullptr);
    void Reset();
    const RailVehicleOccupantClearanceFrame& Update(
        const RailVehicleOccupantClearanceInput& input);

    const RailVehicleOccupantClearanceDefinition& Definition() const noexcept {
        return definition_;
    }
    const RailVehicleOccupantClearanceFrame& Frame() const noexcept {
        return frame_;
    }

private:
    RailVehicleOccupantClearanceDefinition definition_{};
    RailVehicleOccupantClearanceFrame frame_{};
    uint64_t revision_ = 0;
    bool initialized_ = false;
};

const char* ToString(RailVehicleClearanceHitKind kind) noexcept;
