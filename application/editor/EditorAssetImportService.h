#pragma once

#include "EditorAssetRegistry.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class EditorAssetThumbnailService;

enum class EditorAssetExternalImportCollisionPolicy {
    Skip,
    Overwrite,
    Rename,
};

struct EditorAssetImportOptions {
    std::filesystem::path resourcesRoot = "Resources";
    bool createMetadata = true;
    bool scanDependencies = true;
    bool replaceExisting = true;
    bool includeMeshes = true;
    bool includeEffects = true;
    bool includeCourseAssets = true;
    bool includeTextures = true;
    bool includeAudio = true;
};

struct EditorAssetExternalImportPolicy {
    std::filesystem::path resourcesRoot = "Resources";
    std::filesystem::path destinationFolder = "Imported";
    EditorAssetExternalImportCollisionPolicy collisionPolicy =
        EditorAssetExternalImportCollisionPolicy::Rename;
    bool copySidecarMetadata = true;
    bool createMetadata = true;
    bool scanDependencies = true;
    bool replaceExisting = true;
    std::uintmax_t maxFileBytes = 256ull * 1024ull * 1024ull;
};

struct EditorAssetImportResult {
    bool succeeded = false;
    bool warning = false;
    std::string message;
    EditorAssetRecord record{};
    uint32_t importedCount = 0;
    uint32_t migratedCount = 0;
    uint32_t skippedCount = 0;
};

class EditorAssetImportService {
public:
    EditorAssetImportService(
        EditorAssetRegistry& registry,
        EditorAssetThumbnailService* thumbnails = nullptr);

    EditorAssetImportResult Import(
        const std::filesystem::path& sourcePath,
        const EditorAssetImportOptions& options = {});
    EditorAssetImportResult ImportExternal(
        const std::filesystem::path& externalSourcePath,
        const EditorAssetExternalImportPolicy& policy = {});
    EditorAssetImportResult ImportExternalBatch(
        const std::vector<std::filesystem::path>& externalSourcePaths,
        const EditorAssetExternalImportPolicy& policy = {});
    EditorAssetImportResult Reimport(
        EditorAssetKind kind,
        std::string_view id,
        const EditorAssetImportOptions& options = {});
    EditorAssetImportResult BatchMigrateMetadata(
        const EditorAssetImportOptions& options = {});

private:
    EditorAssetImportResult ImportInternal(
        const std::filesystem::path& sourcePath,
        const EditorAssetImportOptions& options,
        bool finalizeChange);
    EditorAssetImportResult ImportExternalInternal(
        const std::filesystem::path& externalSourcePath,
        const EditorAssetExternalImportPolicy& policy,
        bool finalizeChange);
    void FinalizeAssetRegistryChange(const EditorAssetImportOptions& options);

    EditorAssetRegistry& registry_;
    EditorAssetThumbnailService* thumbnails_ = nullptr;
};

EditorAssetKind EditorAssetKindForImportPath(
    const std::filesystem::path& path,
    const EditorAssetImportOptions& options = {});
std::string BuildEditorAssetIdForImportPath(
    EditorAssetKind kind,
    const std::filesystem::path& relativePath);
const char* ToString(EditorAssetExternalImportCollisionPolicy policy);

} // namespace editor
