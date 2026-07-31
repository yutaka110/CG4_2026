#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ge3::level {

inline constexpr uint32_t kBlenderLevelSchemaVersion = 1;

enum class BlenderSpawnKind : uint32_t {
    None = 0,
    Player = 1,
    Enemy = 2,
};

enum class BlenderEnemyType : uint32_t {
    None = 0,
    Drone = 1,
    Turret = 2,
    Boss = 3,
};

struct BlenderLevelVector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct BlenderLevelTransform {
    BlenderLevelVector3 translation{};
    BlenderLevelVector3 rotationDegrees{};
    BlenderLevelVector3 scaling{1.0, 1.0, 1.0};
};

struct BlenderLevelCollider {
    std::string type;
    BlenderLevelVector3 center{};
    BlenderLevelVector3 size{2.0, 2.0, 2.0};
};

struct BlenderLevelObject {
    std::string guid;
    std::string blenderType;
    std::string name;
    BlenderSpawnKind spawnKind = BlenderSpawnKind::None;
    BlenderEnemyType enemyType = BlenderEnemyType::None;
    BlenderLevelTransform transform{};
    std::optional<std::string> fileName;
    std::optional<BlenderLevelCollider> collider;
    std::vector<BlenderLevelObject> children;
};

struct BlenderLevelCoordinateSystem {
    std::string handedness;
    std::string upAxis;
    std::string forwardAxis;
    double unitScaleMeters = 1.0;
    std::string transformSpace;
    std::string rotationUnit;
    std::string rotationOrder;
};

struct BlenderLevelData {
    uint32_t schemaVersion = 0;
    std::string sceneGuid;
    std::string name;
    BlenderLevelCoordinateSystem coordinateSystem{};
    std::vector<BlenderLevelObject> objects;

    std::size_t ObjectCount() const noexcept;
    std::size_t PlayerSpawnCount() const noexcept;
    std::size_t EnemySpawnCount() const noexcept;
    const BlenderLevelObject* FindObject(std::string_view guid) const noexcept;
};

enum class BlenderLevelLoadErrorCode : uint32_t {
    None = 0,
    FileOpenFailed,
    FileReadFailed,
    FileTooLarge,
    InvalidUtf8,
    JsonSyntax,
    SchemaViolation,
    ResourceLimit,
};

struct BlenderLevelLoadError {
    BlenderLevelLoadErrorCode code = BlenderLevelLoadErrorCode::None;
    std::filesystem::path source;
    std::string jsonPath;
    std::size_t line = 0;
    std::size_t column = 0;
    std::string message;
};

struct BlenderLevelLoadResult {
    std::optional<BlenderLevelData> data;
    std::optional<BlenderLevelLoadError> error;

    bool Succeeded() const noexcept {
        return data.has_value() && !error.has_value();
    }
};

struct BlenderLevelJsonLimits {
    std::size_t maximumFileBytes = 64u * 1024u * 1024u;
    std::size_t maximumStringBytes = 1u * 1024u * 1024u;
    std::size_t maximumJsonNodes = 1'000'000u;
    std::size_t maximumObjects = 100'000u;
    std::size_t maximumHierarchyDepth = 128u;
};

class BlenderLevelJsonLoader {
public:
    explicit BlenderLevelJsonLoader(BlenderLevelJsonLimits limits = {});

    BlenderLevelLoadResult LoadFile(const std::filesystem::path& path) const;
    BlenderLevelLoadResult LoadJsonString(
        std::string_view json,
        std::filesystem::path source = {}) const;

    const BlenderLevelJsonLimits& Limits() const noexcept { return limits_; }

private:
    BlenderLevelJsonLimits limits_{};
};

const char* ToString(BlenderSpawnKind value) noexcept;
const char* ToString(BlenderEnemyType value) noexcept;
const char* ToString(BlenderLevelLoadErrorCode value) noexcept;

} // namespace ge3::level
