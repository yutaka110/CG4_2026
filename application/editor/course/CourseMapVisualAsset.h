#pragma once

#include "utils/math/Vector.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr uint32_t kCourseMapVisualAssetSchemaVersion = 1;
inline constexpr uint32_t kCourseMapVisualBakerVersion = 1;

enum class CourseMapVisualLayer : uint8_t {
    GameplayTerrain,
    HeroLandmark,
    VistaBackground,
    RockMass,
    SceneStructure,
};

struct CourseMapVisualPrimitive final {
    std::string stableId;
    std::string sourceMeshId;
    CourseMapVisualLayer layer = CourseMapVisualLayer::HeroLandmark;
    std::vector<Vector3> worldCorners;
    Vector3 worldCenter{};
    float minimumHeight = 0.0f;
    float maximumHeight = 0.0f;
    bool locked = false;
};

struct CourseMapVisualContour final {
    std::string stableId;
    std::vector<Vector3> worldPoints;
    float height = 0.0f;
    bool major = false;
};

struct CourseMapVisualLandmark final {
    std::string stableId;
    std::string label;
    Vector3 worldPosition{};
    uint16_t priority = 0;
};

struct CourseMapVisualTile final {
    int32_t x = 0;
    int32_t z = 0;
    Vector3 worldMinimum{};
    Vector3 worldMaximum{};
    std::vector<uint32_t> primitiveIndices;
    std::vector<uint32_t> contourIndices;
};

struct CourseMapVisualAsset final {
    uint32_t schemaVersion = kCourseMapVisualAssetSchemaVersion;
    uint32_t bakerVersion = kCourseMapVisualBakerVersion;
    std::string sourceCourseName;
    uint64_t sourceCourseHash = 0;
    uint64_t sourceSceneHash = 0;
    uint64_t bakeSettingsHash = 0;
    uint64_t sourceFingerprint = 0;
    uint64_t contentRevision = 0;
    Vector3 worldMinimum{};
    Vector3 worldMaximum{};
    float tileWorldSize = 256.0f;
    std::vector<CourseMapVisualPrimitive> primitives;
    std::vector<CourseMapVisualContour> contours;
    std::vector<CourseMapVisualLandmark> landmarks;
    std::vector<CourseMapVisualTile> tiles;

    bool Empty() const noexcept { return primitives.empty(); }
    bool Validate(std::string* errorMessage = nullptr) const;
    bool IsSourceCurrent(uint64_t expectedFingerprint) const noexcept;
    bool SaveToString(std::string* text, std::string* errorMessage = nullptr) const;
    bool LoadFromString(std::string_view text, std::string* errorMessage = nullptr);
    bool SaveToFile(const std::string& path, std::string* errorMessage = nullptr) const;
    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
};

const char* ToString(CourseMapVisualLayer layer) noexcept;

} // namespace editor
