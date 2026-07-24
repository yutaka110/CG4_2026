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
#include <utility>
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
    std::string sourcePath;
    uint64_t sourceTimestamp = 0;
    uint64_t generation = 0;
    EditorCookedMeshArtifact mesh;
    EditorCookedCollisionArtifact collision;
};

enum class EditorMeshAssetChangeKind {
    Added,
    Modified,
    Removed,
};

struct EditorMeshAssetChange {
    EditorMeshAssetChangeKind kind = EditorMeshAssetChangeKind::Added;
    std::string assetGuid;
    uint64_t previousSourceTimestamp = 0;
    uint64_t currentSourceTimestamp = 0;
};

struct EditorMeshAssetChangeSet {
    uint32_t registryRevision = 0;
    uint64_t sequence = 0;
    std::vector<EditorMeshAssetChange> changes;

    bool Empty() const noexcept { return changes.empty(); }
};

class EditorMeshAssetChangeTracker {
public:
    explicit EditorMeshAssetChangeTracker(
        std::filesystem::path projectRoot = std::filesystem::current_path())
        : projectRoot_(std::move(projectRoot)) {}

    EditorMeshAssetChangeSet Poll(const EditorAssetRegistry& registry);
    void Reset();

    uint64_t Sequence() const noexcept { return sequence_; }
    std::size_t TrackedAssetCount() const noexcept { return snapshots_.size(); }

private:
    struct ArtifactStamp {
        bool exists = false;
        uint64_t byteSize = 0;
        uint64_t writeTime = 0;

        bool operator==(const ArtifactStamp&) const = default;
    };

    struct Snapshot {
        std::string sourcePath;
        uint64_t sourceTimestamp = 0;
        bool missing = false;
        ArtifactStamp source;
        ArtifactStamp cooked;
        ArtifactStamp collision;

        bool operator==(const Snapshot&) const = default;
    };

    std::unordered_map<std::string, Snapshot> snapshots_;
    std::filesystem::path projectRoot_;
    uint64_t sequence_ = 0;
};

struct EditorProductionMeshRuntimeHandle {
    std::string assetGuid;
    uint64_t generation = 0;

    bool Valid() const noexcept { return !assetGuid.empty() && generation != 0; }
};

struct EditorProductionMeshRuntimeReconcileResult {
    uint64_t cacheRevision = 0;
    uint32_t loaded = 0;
    uint32_t updated = 0;
    uint32_t refreshed = 0;
    uint32_t removed = 0;
    uint32_t skippedCold = 0;
    uint32_t failed = 0;
    std::vector<std::string> diagnostics;

    bool Succeeded() const noexcept { return failed == 0; }
    bool Changed() const noexcept {
        return loaded != 0 || updated != 0 || refreshed != 0 || removed != 0;
    }
};

class EditorProductionMeshRuntimeCache {
public:
    explicit EditorProductionMeshRuntimeCache(
        std::filesystem::path projectRoot = std::filesystem::current_path())
        : projectRoot_(std::move(projectRoot)) {}

    bool Load(
        const EditorAssetRecord& record,
        std::string* errorMessage = nullptr);
    EditorProductionMeshRuntimeReconcileResult ReconcileAssets(
        const EditorAssetRegistry& registry,
        const EditorMeshAssetChangeSet& changes);
    void Invalidate(std::string_view assetGuid);
    void Clear();

    const EditorProductionMeshRuntimeResource* Find(std::string_view assetGuid) const;
    EditorProductionMeshRuntimeHandle Handle(std::string_view assetGuid) const;
    const EditorProductionMeshRuntimeResource* Resolve(
        const EditorProductionMeshRuntimeHandle& handle) const;
    EditorMeshRendererResourceView ResolveForRenderer(
        std::string_view assetGuid,
        uint32_t lodIndex) const;
    EditorMeshPhysicsResourceView ResolveForPhysics(std::string_view assetGuid) const;
    uint64_t Revision() const noexcept { return revision_; }
    std::size_t Count() const noexcept { return resources_.size(); }

private:
    enum class PublishResult {
        Loaded,
        Updated,
        Refreshed,
        Unchanged,
    };

    bool BuildResource(
        const EditorAssetRecord& record,
        EditorProductionMeshRuntimeResource& output,
        std::string* errorMessage) const;
    PublishResult Publish(EditorProductionMeshRuntimeResource resource);

    std::unordered_map<std::string, EditorProductionMeshRuntimeResource> resources_;
    std::filesystem::path projectRoot_;
    uint64_t nextGeneration_ = 1;
    uint64_t revision_ = 0;
};

const char* ToString(EditorMeshCollisionBuildMode mode) noexcept;
bool ParseEditorMeshCollisionBuildMode(
    std::string_view text,
    EditorMeshCollisionBuildMode& output) noexcept;

} // namespace editor
