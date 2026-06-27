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

struct CourseAsset {
    std::string name = "Untitled Course";
    std::vector<RailPathControlPoint> railPoints;
    std::vector<CourseCameraKey> cameraKeys;
    std::vector<CourseSection> sections;
    std::vector<CourseEventMarker> events;

    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
    bool SaveToFile(const std::string& path, std::string* errorMessage = nullptr) const;
    void BuildFallbackCanyon(float corridorRadius);
    void SortForRuntime();
    void ApplyToRailPath(RailPath& railPath) const;
    CourseCameraKey EvaluateCamera(float distance) const;
    const CourseSection* FindSection(float distance) const;
    bool IsValid() const { return railPoints.size() >= 2; }
};

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
