#include "EditorAssetFolderIndexer.h"

#include "EditorAssetImportService.h"

#include <filesystem>
#include <system_error>

namespace editor {

EditorAssetFolderIndexResult IndexEditorAssetsFromFolder(
    EditorAssetRegistry& registry,
    const std::filesystem::path& rootPath,
    const EditorAssetFolderIndexOptions& options) {
    EditorAssetFolderIndexResult result{};

    std::error_code error;
    if (!std::filesystem::exists(rootPath, error) ||
        !std::filesystem::is_directory(rootPath, error)) {
        return result;
    }

    EditorAssetImportOptions importOptions{};
    importOptions.resourcesRoot = rootPath;
    importOptions.createMetadata = false;
    importOptions.scanDependencies = false;
    importOptions.replaceExisting = true;
    importOptions.includeMeshes = options.includeMeshes;
    importOptions.includeEffects = options.includeEffects;
    importOptions.includeCourseAssets = options.includeCourseAssets;
    importOptions.includePrefabs = options.includePrefabs;
    importOptions.includeMaterialGraphs = options.includeMaterialGraphs;
    importOptions.includeTextures = options.includeTextures;
    importOptions.includeAudio = options.includeAudio;

    EditorAssetImportService importService(registry);
    const std::filesystem::recursive_directory_iterator end;
    for (std::filesystem::recursive_directory_iterator it(
             rootPath,
             std::filesystem::directory_options::skip_permission_denied,
             error);
         it != end;
         it.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        if (!it->is_regular_file(error)) {
            error.clear();
            continue;
        }

        ++result.scannedFiles;
        const EditorAssetKind kind =
            EditorAssetKindForImportPath(it->path(), importOptions);
        if (kind == EditorAssetKind::Unknown) {
            ++result.skippedFiles;
            continue;
        }

        const std::filesystem::path relativePath =
            std::filesystem::relative(it->path(), rootPath, error);
        if (error) {
            error.clear();
            ++result.skippedFiles;
            continue;
        }
        const std::string id = BuildEditorAssetIdForImportPath(kind, relativePath);
        if (const EditorAssetRecord* existing = registry.Find(kind, id)) {
            const std::filesystem::path existingPath = existing->sourcePath;
            if (existingPath.lexically_normal() != it->path().lexically_normal() &&
                existingPath.lexically_normal() !=
                    (rootPath / relativePath).lexically_normal()) {
                ++result.identityCollisions;
                ++result.skippedFiles;
                continue;
            }
        }

        const EditorAssetImportResult importResult =
            importService.Import(it->path(), importOptions);
        if (importResult.succeeded) {
            ++result.registeredAssets;
        } else {
            ++result.skippedFiles;
        }
    }

    return result;
}

} // namespace editor
