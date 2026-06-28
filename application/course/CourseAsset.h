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

struct CourseAsset {
    std::string name = "Untitled Course";
    std::vector<RailPathControlPoint> railPoints;
    std::vector<CourseCameraKey> cameraKeys;
    std::vector<CourseSection> sections;
    std::vector<CourseEventMarker> events;
    std::vector<CourseTerrainPlacement> terrainPlacements;

    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
    bool SaveToFile(const std::string& path, std::string* errorMessage = nullptr) const;
    void BuildFallbackCanyon(float corridorRadius);
    void SortForRuntime();
    void ApplyToRailPath(RailPath& railPath) const;
    CourseCameraKey EvaluateCamera(float distance) const;
    const CourseSection* FindSection(float distance) const;
    bool IsValid() const { return railPoints.size() >= 2; }
};

const char* ToCourseTerrainLayerString(CourseTerrainLayer layer);
const char* ToCourseTerrainCollisionModeString(CourseTerrainCollisionMode mode);
CourseTerrainLayer ParseCourseTerrainLayer(const std::string& text);
CourseTerrainCollisionMode ParseCourseTerrainCollisionMode(const std::string& text);

class CourseRuntime {
public:
    void Bind(const CourseAsset* asset);
    void Reset(float distance = 0.0f);
    std::vector<CourseEventMarker> Advance(float deltaTime, const RailPath& railPath);

    float Distance() const { return distance_; }
    const CourseSection* CurrentSection() const;

private:
    const CourseAsset* asset_ = nullptr;
    float distance_ = 0.0f;
    size_t nextEventIndex_ = 0;
};
