#pragma once

#include <filesystem>

#include "EditorAssetRegistry.h"

namespace editor {

struct EditorAssetFolderIndexOptions {
    bool includeMeshes = true;
    bool includeEffects = true;
    bool includeCourseAssets = true;
    bool includePrefabs = true;
    bool includeMaterialGraphs = true;
    bool includeTextures = true;
    bool includeAudio = true;
};

struct EditorAssetFolderIndexResult {
    uint32_t scannedFiles = 0;
    uint32_t registeredAssets = 0;
    uint32_t skippedFiles = 0;
    uint32_t identityCollisions = 0;
};

EditorAssetFolderIndexResult IndexEditorAssetsFromFolder(
    EditorAssetRegistry& registry,
    const std::filesystem::path& rootPath,
    const EditorAssetFolderIndexOptions& options = {});

} // namespace editor
