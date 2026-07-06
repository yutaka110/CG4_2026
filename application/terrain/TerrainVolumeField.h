#pragma once

#include "RailPath.h"
#include "TerrainGenerationSettings.h"
#include "utils/math/Vector.h"

struct TerrainVolumeLocalSample {
    float sdf = 0.0f;
    float noise = 0.0f;
    float archMask = 0.0f;
    float carveMask = 0.0f;
    float openingMask = 0.0f;
    float openCanyonBlend = 0.0f;
    float radiusScale = 1.0f;
};

class TerrainVolumeField {
public:
    TerrainVolumeField(
        const RailPath& railPath,
        const TerrainGenerationSettings& settings);

    TerrainVolumeLocalSample SampleLocal(
        float distance,
        float lateral,
        float vertical) const;
    Vector3 SurfacePoint(
        float distance,
        float angle,
        Vector3* outNormal = nullptr) const;
    float OpeningMask(float distance, float angle) const;
    float OpenCanyonBlend(float distance) const;
    Vector3 EstimateNormal(
        float distance,
        float lateral,
        float vertical) const;

private:
    float Noise3(float distance, float lateral, float vertical, float scale) const;
    float ArchMask(float distance, float angle) const;
    float SubtractiveCarveMask(float distance, float angle) const;
    float RadiusScale(float distance, float angle) const;

    const RailPath& railPath_;
    const TerrainGenerationSettings& settings_;
};
