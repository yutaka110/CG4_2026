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
        if (EditorAssetKindForImportPath(it->path(), importOptions) == EditorAssetKind::Unknown) {
            ++result.skippedFiles;
            continue;
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
