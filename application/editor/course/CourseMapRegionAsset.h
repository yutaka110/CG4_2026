#pragma once

#include "CourseMapVisualAsset.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr uint32_t kCourseMapRegionAssetSchemaVersion = 2;
inline constexpr uint32_t kCourseMapCartographyBakerVersion = 3;

struct CourseMapRegion final {
    std::string stableId;
    std::string sourceStableId;
    std::string sourceMeshId;
    CourseMapVisualLayer layer = CourseMapVisualLayer::HeroLandmark;
    std::vector<Vector3> worldVertices;
    std::vector<uint32_t> indices;
    std::vector<Vector3> footprint;
    Vector3 worldCentroid{};
    float minimumHeight = 0.0f;
    float maximumHeight = 0.0f;
    float projectedArea = 0.0f;
    bool exactSourceGeometry = false;
    bool locked = false;
};

struct CourseMapRegionTile final {
    int32_t x = 0;
    int32_t z = 0;
    Vector3 worldMinimum{};
    Vector3 worldMaximum{};
    std::vector<uint32_t> regionIndices;
};

struct CourseMapRegionAsset final {
    uint32_t schemaVersion = kCourseMapRegionAssetSchemaVersion;
    uint32_t bakerVersion = kCourseMapCartographyBakerVersion;
    std::string sourceCourseName;
    uint64_t sourceVisualFingerprint = 0;
    uint64_t sourceGeometryHash = 0;
    uint64_t bakeSettingsHash = 0;
    // Cheap durable dependency key used by the UI path. It is computed from
    // Asset Registry mesh records and never requires parsing Production Mesh
    // geometry merely to decide whether the persisted map can be reused.
    uint64_t sourceMeshRegistryFingerprint = 0;
    uint64_t sourceFingerprint = 0;
    uint64_t contentRevision = 0;
    Vector3 worldMinimum{};
    Vector3 worldMaximum{};
    float tileWorldSize = 256.0f;
    std::vector<CourseMapRegion> regions;
    std::vector<CourseMapRegionTile> tiles;

    bool Empty() const noexcept { return regions.empty(); }
    bool Validate(std::string* errorMessage = nullptr) const;
    bool IsSourceCurrent(uint64_t expectedFingerprint) const noexcept;
    bool SaveToString(std::string* text, std::string* errorMessage = nullptr) const;
    bool LoadFromString(std::string_view text, std::string* errorMessage = nullptr);
    bool SaveToFile(const std::string& path, std::string* errorMessage = nullptr) const;
    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
};

} // namespace editor
