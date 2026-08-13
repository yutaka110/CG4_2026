#pragma once

#include "EditorProductionMeshAsset.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorObjProductionImportRequest {
    std::string sourceAssetGuid;
    std::string outputAssetName;
    EditorMeshBuildSettings settings{};
};

struct EditorObjProductionImportResult {
    bool succeeded = false;
    bool reimported = false;
    EditorAssetRecord record{};
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    uint32_t materialSlotCount = 0;
    uint32_t lodCount = 0;
    std::size_t artifactBytes = 0;
    std::string message;
    std::vector<std::string> diagnostics;
};

class EditorObjProductionImportBridge {
public:
    EditorObjProductionImportBridge(
        EditorAssetRegistry& registry,
        EditorProductionMeshRuntimeCache* runtimeCache,
        std::filesystem::path projectRoot = std::filesystem::current_path());

    static bool CanImport(const EditorAssetRecord& source);
    static std::string DefaultOutputAssetName(const EditorAssetRecord& source);
    static bool IsProductionForSource(
        const EditorAssetRecord& production,
        const EditorAssetRecord& source,
        const std::filesystem::path& projectRoot = std::filesystem::current_path());
    // Read-only half of Production Import. This exposes the exact source-to-
    // authoring-geometry conversion used by ImportAndBake without writing an
    // Asset, changing the Registry, or publishing a runtime generation.
    static bool LoadSourceGeometry(
        const EditorAssetRecord& source,
        const std::filesystem::path& projectRoot,
        EditorGeometryMesh& geometry,
        uint64_t* sourceFileHash = nullptr,
        uint32_t* materialSlotCount = nullptr,
        std::vector<std::string>* diagnostics = nullptr,
        std::string* errorMessage = nullptr);

    EditorObjProductionImportResult ImportAndBake(
        const EditorObjProductionImportRequest& request);

private:
    EditorAssetRegistry& registry_;
    EditorProductionMeshRuntimeCache* runtimeCache_ = nullptr;
    std::filesystem::path projectRoot_;
};

} // namespace editor
