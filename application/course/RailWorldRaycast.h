#pragma once

#include "RailAimState.h"

class RailPath;
class CourseSpawnRuntime;
struct CourseAsset;
struct TerrainGenerationSettings;
class TerrainEditLayer;

// Read-only scene inputs used to resolve the authoritative aim ray. All hit
// candidates are compared in world-ray distance, so the result also provides
// deterministic weapon occlusion.
struct RailWorldRaycastInput {
    const RailAimState* aim = nullptr;
    const RailPath* railPath = nullptr;
    const CourseSpawnRuntime* spawnRuntime = nullptr;
    const CourseAsset* course = nullptr;
    const TerrainGenerationSettings* terrainSettings = nullptr;
    const TerrainEditLayer* terrainEdits = nullptr;
    const TerrainEditLayer* terrainPreview = nullptr;
    float playerDistance = 0.0f;
    float collisionPadding = 0.0f;
    bool includeProceduralTerrain = true;
};

class RailWorldRaycast {
public:
    static RailAimHit Query(const RailWorldRaycastInput& input);
};
