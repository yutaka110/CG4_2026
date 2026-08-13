#pragma once

#include "utils/math/Vector.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

inline constexpr uint32_t kCourseTerrainMapAssetSchemaVersion = 1;
inline constexpr uint32_t kCourseTerrainMapBakerVersion = 1;

struct CourseTerrainMapVertex final {
    Vector3 position{};
    Vector3 normal{0.0f, 1.0f, 0.0f};
};

struct CourseTerrainMapTile final {
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    Vector3 worldMinimum{};
    Vector3 worldMaximum{};
    std::vector<CourseTerrainMapVertex> vertices;
    std::vector<uint32_t> indices;
};

struct CourseTerrainMapLod final {
    uint32_t level = 0;
    uint32_t longitudinalSegments = 0;
    uint32_t radialSegments = 0;
    std::vector<CourseTerrainMapTile> tiles;
};

// Durable, editor-derived representation of the procedural canyon shell.
// It is rebuilt from TerrainVolumeField rather than copied from transient GPU
// chunks, so it covers the complete course and remains deterministic.
struct CourseTerrainMapAsset final {
    uint32_t schemaVersion = kCourseTerrainMapAssetSchemaVersion;
    uint32_t bakerVersion = kCourseTerrainMapBakerVersion;
    std::string sourceCourseName;
    uint64_t sourceRailHash = 0;
    uint64_t sourceTerrainSettingsHash = 0;
    uint64_t sourceTerrainEditHash = 0;
    uint64_t bakeSettingsHash = 0;
    uint64_t sourceFingerprint = 0;
    uint64_t contentRevision = 0;
    float railLength = 0.0f;
    Vector3 worldMinimum{};
    Vector3 worldMaximum{};
    std::vector<CourseTerrainMapLod> lods;

    bool Empty() const noexcept;
    const CourseTerrainMapLod* FindLod(uint32_t level) const noexcept;
    bool Validate(std::string* errorMessage = nullptr) const;
    bool IsSourceCurrent(uint64_t expectedFingerprint) const noexcept;
    bool SaveToString(std::string* text, std::string* errorMessage = nullptr) const;
    bool LoadFromString(std::string_view text, std::string* errorMessage = nullptr);
    bool SaveToFile(const std::string& path, std::string* errorMessage = nullptr) const;
    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
};

} // namespace editor
