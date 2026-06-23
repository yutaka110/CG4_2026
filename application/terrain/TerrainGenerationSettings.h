#pragma once

#include <cstdint>

#include "utils/math/Vector.h"

enum class TerrainDisplayMode : uint32_t {
    Lit = 0,
    Unlit = 1,
    Wireframe = 2,
    Debug = 3,
    DetailNormal = 4,
};

struct TerrainGenerationSettings {
    uint32_t seed = 1337;
    float chunkLength = 80.0f;
    uint32_t visibleAheadChunks = 8;
    uint32_t visibleBehindChunks = 2;
    float corridorRadius = 18.0f;
    float canyonHalfWidth = 42.0f;
    float wallHeight = 35.0f;
    float noiseStrength = 14.0f;
    float volumeRoughness = 0.42f;
    float volumeArchScale = 0.55f;
    float sdfCarveDensity = 0.48f;
    float sdfCarveStrength = 0.42f;
    float sdfCarveScale = 1.0f;
    uint32_t surfaceLongitudinalSteps = 36;
    uint32_t surfaceRadialSegments = 56;
    float rockPillarDensity = 0.35f;
    float rockScatterDensity = 0.42f;
    float rockScatterScale = 1.0f;
    float rockEmbedStrength = 0.78f;
    float rockContactPebbleDensity = 0.62f;
    float rockClusterStrength = 0.72f;
    float rockRootShadowStrength = 0.78f;
    float rockMotherBlendStrength = 0.68f;
    float rockMaterialVariation = 0.55f;
    float motherRockErosionStrength = 0.68f;
    float largeScaleErosionStrength = 0.72f;
    float archDensity = 0.25f;
    float dustZoneDensity = 0.45f;
};

struct TerrainAuthoringState {
    bool enabled = true;
    bool autoAdvancePreview = false;
    bool showDebugDraw = false;
    bool showRailPath = true;
    bool showCorridor = false;
    bool showChunks = false;
    bool showSpawnCandidates = false;
    bool showRockScatter = false;
    bool showVfxZones = false;
    bool showVolumeSlice = false;
    bool showSdfSamples = false;
    bool showCascadeBounds = false;
    bool showShadowDebugView = true;
    bool autoReloadPreset = false;
    bool requestSavePreset = false;
    bool requestLoadPreset = false;
    bool requestReloadPreset = false;
    TerrainDisplayMode displayMode = TerrainDisplayMode::Lit;
    Vector4 materialBaseColor = {1.18f, 1.08f, 0.94f, 1.0f};
    float materialBrightness = 1.0f;
    float materialNoiseStrength = 1.0f;
    float materialStrataStrength = 1.0f;
    float materialStrataBreakupStrength = 0.68f;
    float materialSpecularStrength = 0.035f;
    float materialRimLightStrength = 0.45f;
    float materialBacklightRimBoost = 0.28f;
    float materialFloorSandShadowStrength = 0.38f;
    float materialDetailNormalStrength = 0.72f;
    float materialMicroDetailStrength = 0.62f;
    bool useDetailTextureCache = true;
    float materialDetailCacheScale = 1.0f;
    float materialDetailTileWorldSize = 96.0f;
    float materialDetailNearScale = 1.35f;
    float materialDetailFarScale = 0.55f;
    float materialDetailDistanceBlend = 180.0f;
    bool useDetailNormalMap = true;
    float materialDetailNormalMapStrength = 0.58f;
    float materialDetailHybridBlend = 0.52f;
    bool invertDetailNormalY = false;
    float materialCavityAoStrength = 0.58f;
    float materialSkyFillStrength = 0.30f;
    bool useCanyonSunLighting = true;
    Vector4 canyonSunColor = {1.0f, 0.74f, 0.46f, 1.0f};
    Vector3 canyonSunDirection = {-0.38f, -0.52f, 0.76f};
    float canyonSunIntensity = 2.4f;
    bool cascadeShadowEnabled = true;
    float cascadeShadowBias = 0.0018f;
    float cascadeShadowStrength = 0.68f;
    float cascadeShadowSplit0 = 120.0f;
    float cascadeShadowSplit1 = 260.0f;
    float cascadeShadowSplit2 = 520.0f;
    float cascadeShadowSplit3 = 960.0f;
    int shadowDebugCascade = 0;
    float previewDistance = 0.0f;
    float previewSpeed = 32.0f;
    TerrainGenerationSettings settings{};
};
