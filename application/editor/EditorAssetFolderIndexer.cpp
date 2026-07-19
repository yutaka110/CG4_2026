#include "EditorAssetFolderIndexer.h"

#include "EditorAssetImportService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace editor {
namespace {

std::string NormalizedPathKey(const std::filesystem::path& path) {
    std::string value = path.lexically_normal().generic_string();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

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

    std::vector<std::filesystem::path> discoveredFiles;
    std::unordered_set<std::string> metadataPaths;
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

        discoveredFiles.push_back(it->path());
        if (it->path().extension() == ".meta") {
            metadataPaths.insert(NormalizedPathKey(it->path()));
        }
    }

    EditorAssetImportService importService(registry);
    for (const std::filesystem::path& discoveredPath : discoveredFiles) {
        ++result.scannedFiles;
        const EditorAssetKind kind =
            EditorAssetKindForImportPath(discoveredPath, importOptions);
        if (kind == EditorAssetKind::Unknown) {
            ++result.skippedFiles;
            continue;
        }

        const std::filesystem::path relativePath = discoveredPath.lexically_relative(rootPath);
        if (relativePath.empty() || relativePath.is_absolute()) {
            ++result.skippedFiles;
            continue;
        }
        const std::string id = BuildEditorAssetIdForImportPath(kind, relativePath);
        if (const EditorAssetRecord* existing = registry.Find(kind, id)) {
            const std::filesystem::path existingPath = existing->sourcePath;
            if (existingPath.lexically_normal() != discoveredPath.lexically_normal() &&
                existingPath.lexically_normal() !=
                    (rootPath / relativePath).lexically_normal()) {
                ++result.identityCollisions;
                ++result.skippedFiles;
                continue;
            }
        }

        EditorAssetImportOptions indexedOptions = importOptions;
        indexedOptions.sourceKnownPresent = true;
        indexedOptions.readMetadata = metadataPaths.contains(
            NormalizedPathKey(discoveredPath.string() + ".meta"));
        const EditorAssetImportResult importResult = importService.ImportIndexed(
            discoveredPath, relativePath, indexedOptions);
        if (importResult.succeeded) {
            ++result.registeredAssets;
        } else {
            ++result.skippedFiles;
        }
    }

    return result;
}

} // namespace editor
