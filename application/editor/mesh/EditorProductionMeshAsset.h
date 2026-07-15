#pragma once

#include "../EditorAssetRegistry.h"
#include "../geometry/EditorGeometryMesh.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor {

inline constexpr std::string_view kEditorBakedMeshGuidProperty = "bakedMeshGuid";
inline constexpr std::string_view kEditorBakedMeshSourceHashProperty = "bakedMeshSourceHash";
inline constexpr std::string_view kEditorBakedMeshBuildHashProperty = "bakedMeshBuildHash";

enum class EditorMeshCollisionBuildMode : uint32_t {
    None = 0,
    Box = 1,
    TriangleMesh = 2,
};

struct EditorMeshBuildSettings {
    static constexpr uint32_t kMaxLods = 4;

    uint32_t lodCount = 3;
    std::array<float, kMaxLods> lodRatios{1.0f, 0.5f, 0.25f, 0.125f};
    EditorMeshCollisionBuildMode collisionMode = EditorMeshCollisionBuildMode::Box;

    bool Validate(std::string* errorMessage = nullptr) const;
    uint64_t ContentHash() const noexcept;
};

struct EditorCookedMeshVertex {
    Vector3 position{};
    Vector3 normal{};
    float u = 0.0f;
    float v = 0.0f;
};

struct EditorCookedMeshLod {
    float sourceRatio = 1.0f;
    std::vector<EditorCookedMeshVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> materialSlots;
};

struct EditorCookedMeshArtifact {
    static constexpr uint32_t kSchemaVersion = 1;
    static constexpr std::size_t kMaxArtifactBytes = 128u * 1024u * 1024u;

    uint64_t sourceGeometryHash = 0;
    uint64_t buildSettingsHash = 0;
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    std::vector<EditorCookedMeshLod> lods;

    bool Validate(std::string* errorMessage = nullptr) const;
    bool Serialize(std::vector<uint8_t>& output, std::string* errorMessage = nullptr) const;
    static bool Deserialize(
        const std::vector<uint8_t>& input,
        EditorCookedMeshArtifact& output,
        std::string* errorMessage = nullptr);
};

struct EditorCookedCollisionArtifact {
    static constexpr uint32_t kSchemaVersion = 1;

    EditorMeshCollisionBuildMode mode = EditorMeshCollisionBuildMode::None;
    uint64_t sourceGeometryHash = 0;
    Vector3 center{};
    Vector3 extents{};
    std::vector<Vector3> vertices;
    std::vector<uint32_t> indices;

    bool Validate(std::string* errorMessage = nullptr) const;
    bool Serialize(std::vector<uint8_t>& output, std::string* errorMessage = nullptr) const;
    static bool Deserialize(
        const std::vector<uint8_t>& input,
        EditorCookedCollisionArtifact& output,
        std::string* errorMessage = nullptr);
};

struct EditorProductionMeshAssetDocument {
    static constexpr uint32_t kSchemaVersion = 1;

    std::string assetGuid;
    std::string assetId;
    uint64_t sourceGeometryHash = 0;
    EditorMeshBuildSettings settings{};
    EditorGeometryMesh geometry{};

    bool Validate(std::string* errorMessage = nullptr) const;
    bool Serialize(std::string& output, std::string* errorMessage = nullptr) const;
    static bool Deserialize(
        std::string_view input,
        EditorProductionMeshAssetDocument& output,
        std::string* errorMessage = nullptr);
};

bool BuildEditorCookedMeshArtifacts(
    const EditorGeometryMesh& geometry,
    const EditorGeneratedCollision* authoredCollision,
    const EditorMeshBuildSettings& settings,
    EditorCookedMeshArtifact& mesh,
    EditorCookedCollisionArtifact& collision,
    std::string* errorMessage = nullptr);

std::filesystem::path EditorCookedMeshPath(const std::filesystem::path& sourcePath);
std::filesystem::path EditorCookedCollisionPath(const std::filesystem::path& sourcePath);

struct EditorMeshRendererResourceView {
    const EditorCookedMeshVertex* vertices = nullptr;
    std::size_t vertexCount = 0;
    const uint32_t* indices = nullptr;
    std::size_t indexCount = 0;
    const uint32_t* materialSlots = nullptr;
    std::size_t materialSlotCount = 0;
    uint32_t lodIndex = 0;

    bool Valid() const noexcept {
        return vertices != nullptr && vertexCount > 0 && indices != nullptr && indexCount >= 3;
    }
};

struct EditorMeshPhysicsResourceView {
    const EditorCookedCollisionArtifact* collision = nullptr;
    bool Valid() const noexcept {
        return collision != nullptr && collision->mode != EditorMeshCollisionBuildMode::None;
    }
};

struct EditorProductionMeshRuntimeResource {
    std::string assetGuid;
    uint64_t sourceTimestamp = 0;
    EditorCookedMeshArtifact mesh;
    EditorCookedCollisionArtifact collision;
};

class EditorProductionMeshRuntimeCache {
public:
    bool Load(
        const EditorAssetRecord& record,
        std::string* errorMessage = nullptr);
    void Invalidate(std::string_view assetGuid);
    void Clear();

    const EditorProductionMeshRuntimeResource* Find(std::string_view assetGuid) const;
    EditorMeshRendererResourceView ResolveForRenderer(
        std::string_view assetGuid,
        uint32_t lodIndex) const;
    EditorMeshPhysicsResourceView ResolveForPhysics(std::string_view assetGuid) const;

private:
    std::unordered_map<std::string, EditorProductionMeshRuntimeResource> resources_;
};

const char* ToString(EditorMeshCollisionBuildMode mode) noexcept;
bool ParseEditorMeshCollisionBuildMode(
    std::string_view text,
    EditorMeshCollisionBuildMode& output) noexcept;

} // namespace editor
