#pragma once

#include "../EditorAssetRegistry.h"
#include "../geometry/EditorGeometryMesh.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace editor {

enum class CourseTerrainMeshResolutionSource : uint8_t {
    None,
    ProductionAsset,
    ImportedAuthoringSource,
};

struct CourseTerrainProductionMeshResolution final {
    CourseTerrainMeshResolutionSource source =
        CourseTerrainMeshResolutionSource::None;
    EditorAssetReferenceResolutionSource referenceSource =
        EditorAssetReferenceResolutionSource::None;
    std::string requestedMeshId;
    std::string canonicalMeshId;
    std::string resolvedAssetId;
    std::string resolvedAssetGuid;
    std::filesystem::path sourcePath;
    EditorGeometryMesh geometry;
    uint64_t sourceFileHash = 0;
    uint64_t sourceGeometryHash = 0;
    bool normalizedAlias = false;
    bool ambiguous = false;
    std::string message;

    bool Resolved() const noexcept {
        return source != CourseTerrainMeshResolutionSource::None &&
            !geometry.vertices.empty() && !geometry.triangles.empty();
    }
};

struct CourseTerrainProductionMeshResolverStats final {
    uint64_t resolutions = 0;
    uint64_t cacheHits = 0;
    uint64_t directReferences = 0;
    uint64_t normalizedAliases = 0;
    uint64_t productionAssets = 0;
    uint64_t importedAuthoringSources = 0;
    uint64_t unresolved = 0;
    uint64_t ambiguous = 0;
};

// Resolves the compact mesh IDs stored by Course authoring to the durable
// Asset Registry. A generated .mesh is preferred. When a legacy imported
// OBJ/glTF/GLB/FBX has not been baked yet, the resolver uses the exact same
// read-only conversion as Production Import, yielding Production Geometry
// without mutating the Registry or writing an Asset during map rendering.
class CourseTerrainProductionMeshResolver final {
public:
    explicit CourseTerrainProductionMeshResolver(
        std::filesystem::path projectRoot = std::filesystem::current_path());

    const CourseTerrainProductionMeshResolution& Resolve(
        const EditorAssetRegistry& assets,
        std::string_view courseMeshId,
        bool* cacheHit = nullptr) const;

    void Invalidate() const;
    const CourseTerrainProductionMeshResolverStats& Stats() const noexcept {
        return stats_;
    }

private:
    CourseTerrainProductionMeshResolution ResolveUncached(
        const EditorAssetRegistry& assets,
        std::string_view courseMeshId) const;

    std::filesystem::path projectRoot_;
    mutable const EditorAssetRegistry* cachedRegistry_ = nullptr;
    mutable uint32_t cachedRegistryRevision_ = 0;
    mutable std::unordered_map<std::string,
        CourseTerrainProductionMeshResolution> cache_;
    mutable CourseTerrainProductionMeshResolverStats stats_{};
};

const char* ToString(CourseTerrainMeshResolutionSource source) noexcept;

} // namespace editor
