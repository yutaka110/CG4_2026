#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../terrain/RailPath.h"
#include "utils/math/Vector.h"

struct CourseCameraKey {
    float distance = 0.0f;
    float backDistance = 18.0f;
    float verticalOffset = 6.0f;
    float lateralOffset = 0.0f;
    float lookAheadDistance = 54.0f;
    float lookUpOffset = 2.2f;
    float lookForwardOffset = 8.0f;
    float fovY = 0.30f * 3.14159265358979323846f;
    float roll = 0.0f;
};

struct CourseSection {
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    std::string name;
    std::string category;
};

struct CourseEventMarker {
    float distance = 0.0f;
    std::string type;
    std::string id;
    std::string payload;
};

enum class CourseTerrainLayer {
    GameplayCollision,
    HeroLandmark,
    VistaBackground,
};

enum class CourseTerrainCollisionMode {
    None,
    Proxy,
    Solid,
};

enum class CourseRockClusterType {
    AttachedDebris,
    HeroFracture,
    FallingDebris,
    VistaSilhouette,
};

enum class CourseRockClusterAnchor {
    LeftWall,
    RightWall,
    Floor,
    CeilingBreak,
    VistaWall,
};

struct CourseTerrainPlacement {
    float distance = 0.0f;
    CourseTerrainLayer layer = CourseTerrainLayer::HeroLandmark;
    std::string id;
    std::string meshId = "animated_cube";
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float forwardOffset = 0.0f;
    Vector3 scale = {1.0f, 1.0f, 1.0f};
    Vector3 rotation = {0.0f, 0.0f, 0.0f};
    CourseTerrainCollisionMode collisionMode = CourseTerrainCollisionMode::None;
    int renderPriority = 0;
    float cullBehindDistance = -1.0f;
    float cullAheadDistance = -1.0f;
};

struct CourseRockCluster {
    float distance = 0.0f;
    std::string id;
    std::string meshId = "curved_canyon_wall";
    CourseRockClusterAnchor anchor = CourseRockClusterAnchor::LeftWall;
    CourseRockClusterType type = CourseRockClusterType::AttachedDebris;
    uint32_t count = 0;
    float minScale = 0.5f;
    float maxScale = 2.0f;
    Vector3 spread = {10.0f, 4.0f, 8.0f};
    Vector3 rotation = {0.0f, 0.0f, 0.0f};
    float clearLaneRadius = 18.0f;
    float cullBehindDistance = 120.0f;
    float cullAheadDistance = 260.0f;
    struct InstanceTransformOverride {
        uint32_t index = 0;
        Vector3 localOffset = {0.0f, 0.0f, 0.0f};
        Vector3 scale = {1.0f, 1.0f, 1.0f};
        Vector3 rotation = {0.0f, 0.0f, 0.0f};
    };
    std::vector<InstanceTransformOverride> instanceOverrides;
};

struct CourseLightingPreset {
    float distance = 0.0f;
    std::string id;
    float blendDistance = 80.0f;
    Vector4 sunColor = {1.0f, 0.74f, 0.46f, 1.0f};
    Vector3 sunDirection = {-0.38f, -0.52f, 0.76f};
    float sunIntensity = 2.4f;
    Vector4 clearColor = {0.32f, 0.29f, 0.28f, 1.0f};
    Vector3 fogColor = {0.42f, 0.38f, 0.36f};
    float fogIntensity = 0.48f;
    float fogStart = 135.0f;
    float fogEnd = 1450.0f;
    float fogDensity = 0.26f;
    float backlitFogLift = 0.34f;
    float openingGlowStrength = 0.62f;
    float foregroundSilhouetteStrength = 0.58f;
    float lowFogLayerStrength = 0.30f;
    float coolFloorHazeStrength = 0.24f;
};

struct CourseCinematicCameraShot {
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    std::string id;
    std::string mode;
    std::string presetId;
    std::string blendAssetId;
    float blendInDistance = 24.0f;
    float blendOutDistance = 24.0f;
    float weightScale = 1.0f;
    float backDistanceOffset = 0.0f;
    float verticalOffset = 0.0f;
    float lateralOffset = 0.0f;
    float lookAheadOffset = 0.0f;
    float lookUpOffset = 0.0f;
    float lookForwardOffset = 0.0f;
    float fovOffset = 0.0f;
    float rollOffset = 0.0f;
    float shakeAmount = 0.0f;
};

struct CourseCameraShotPreset {
    std::string id;
    std::string mode;
    float backDistanceOffset = 0.0f;
    float verticalOffset = 0.0f;
    float lateralOffset = 0.0f;
    float lookAheadOffset = 0.0f;
    float lookUpOffset = 0.0f;
    float lookForwardOffset = 0.0f;
    float fovOffset = 0.0f;
    float rollOffset = 0.0f;
    float shakeAmount = 0.0f;
};

struct CourseCameraBlendAsset {
    std::string id;
    float blendInDistance = 24.0f;
    float blendOutDistance = 24.0f;
    std::string curve = "smoothstep";
    float weightScale = 1.0f;
};

struct CourseCameraShotState {
    float weight = 0.0f;
    CourseCinematicCameraShot shot{};
    std::string presetId;
    std::string blendAssetId;
    std::string blendCurve = "linear";
};

struct CourseTerrainMaterialPreset {
    float distance = 0.0f;
    std::string id;
    float blendDistance = 80.0f;
    Vector4 baseColor = {1.02f, 0.96f, 0.90f, 1.0f};
    float brightness = 1.0f;
    float noiseStrength = 1.0f;
    float strataStrength = 1.0f;
    float strataBreakupStrength = 0.92f;
    float specularStrength = 0.135f;
    float rimLightStrength = 0.84f;
    float backlightRimBoost = 0.72f;
    float floorSandShadowStrength = 0.38f;
    float detailNormalStrength = 1.18f;
    float microDetailStrength = 1.16f;
    float cavityAoStrength = 0.82f;
    float skyFillStrength = 0.30f;
};

struct CourseCinematicShotSet {
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    std::string id;
    std::string label;
    std::vector<std::string> heroLandmarkIds;
    std::vector<std::string> vistaLandmarkIds;
    std::string lightingPresetId;
    std::string cameraShotId;
    std::string terrainMaterialId;
    std::string fogMood;
    std::string compositionNotes;
};

struct CourseAsset {
    std::string name = "Untitled Course";
    std::vector<RailPathControlPoint> railPoints;
    std::vector<CourseCameraKey> cameraKeys;
    std::vector<CourseSection> sections;
    std::vector<CourseEventMarker> events;
    std::vector<CourseTerrainPlacement> terrainPlacements;
    std::vector<CourseRockCluster> rockClusters;
    std::vector<CourseLightingPreset> lightingPresets;
    std::vector<CourseCameraShotPreset> cameraShotPresets;
    std::vector<CourseCameraBlendAsset> cameraBlendAssets;
    std::vector<CourseCinematicCameraShot> cinematicCameraShots;
    std::vector<CourseTerrainMaterialPreset> terrainMaterialPresets;
    std::vector<CourseCinematicShotSet> cinematicShotSets;

    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
    bool SaveToFile(const std::string& path, std::string* errorMessage = nullptr) const;
    void BuildFallbackCanyon(float corridorRadius);
    void SortForRuntime();
    void ApplyToRailPath(RailPath& railPath) const;
    CourseCameraKey EvaluateCamera(float distance) const;
    CourseLightingPreset EvaluateLightingPreset(float distance) const;
    CourseCameraShotState EvaluateCinematicCameraShot(float distance) const;
    CourseTerrainMaterialPreset EvaluateTerrainMaterialPreset(float distance) const;
    const CourseCinematicShotSet* FindCinematicShotSet(float distance) const;
    const CourseSection* FindSection(float distance) const;
    bool IsValid() const { return railPoints.size() >= 2; }
};

const char* ToCourseTerrainLayerString(CourseTerrainLayer layer);
const char* ToCourseTerrainCollisionModeString(CourseTerrainCollisionMode mode);
const char* ToCourseRockClusterTypeString(CourseRockClusterType type);
const char* ToCourseRockClusterAnchorString(CourseRockClusterAnchor anchor);
CourseTerrainLayer ParseCourseTerrainLayer(const std::string& text);
CourseTerrainCollisionMode ParseCourseTerrainCollisionMode(const std::string& text);
CourseRockClusterType ParseCourseRockClusterType(const std::string& text);
CourseRockClusterAnchor ParseCourseRockClusterAnchor(const std::string& text);

class CourseRuntime {
public:
    void Bind(const CourseAsset* asset);
    void Reset(float distance = 0.0f);
    std::vector<CourseEventMarker> Advance(float deltaTime, const RailPath& railPath);
    std::vector<CourseEventMarker> Advance(float deltaTime, const RailPath& railPath, float speedOverride);

    float Distance() const { return distance_; }
    const CourseSection* CurrentSection() const;

private:
    std::vector<CourseEventMarker> AdvanceInternal(
        float deltaTime,
        const RailPath& railPath,
        const float* speedOverride);

    const CourseAsset* asset_ = nullptr;
    float distance_ = 0.0f;
    size_t nextEventIndex_ = 0;
};
