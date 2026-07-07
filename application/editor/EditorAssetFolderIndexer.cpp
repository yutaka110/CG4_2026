#include "EditorAssetFolderIndexer.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>
#include <utility>

namespace editor {
namespace {

std::string ToLower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

std::string NormalizePath(std::filesystem::path path) {
    std::string text = path.generic_string();
    if (text.rfind("./", 0) == 0) {
        text.erase(0, 2);
    }
    return text;
}

EditorAssetKind KindForExtension(
    const std::string& extension,
    const EditorAssetFolderIndexOptions& options) {
    const std::string ext = ToLower(extension);
    if (options.includeMeshes &&
        (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx")) {
        return EditorAssetKind::Mesh;
    }
    if (options.includeEffects && ext == ".effect") {
        return EditorAssetKind::Effect;
    }
    if (options.includeCourseAssets &&
        (ext == ".course" ||
            ext == ".actor" ||
            ext == ".wave" ||
            ext == ".pattern" ||
            ext == ".obstacle" ||
            ext == ".terrainpreset" ||
            ext == ".postpreset")) {
        return EditorAssetKind::Course;
    }
    if (options.includeTextures &&
        (ext == ".png" || ext == ".bmp" || ext == ".dds" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")) {
        return EditorAssetKind::Texture;
    }
    if (options.includeAudio &&
        (ext == ".wav" || ext == ".mp3" || ext == ".ogg")) {
        return EditorAssetKind::Audio;
    }
    return EditorAssetKind::Unknown;
}

std::string BuildAssetId(
    EditorAssetKind kind,
    const std::filesystem::path& relativePath) {
    std::filesystem::path idPath = relativePath;
    idPath.replace_extension();

    if (kind == EditorAssetKind::Mesh) {
        return idPath.filename().generic_string();
    }

    return NormalizePath(idPath);
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
        const std::filesystem::path path = it->path();
        const EditorAssetKind kind = KindForExtension(path.extension().string(), options);
        if (kind == EditorAssetKind::Unknown) {
            ++result.skippedFiles;
            continue;
        }

        std::filesystem::path relativePath = std::filesystem::relative(path, rootPath, error);
        if (error) {
            error.clear();
            relativePath = path.filename();
        }

        EditorAssetRecord record{};
        record.kind = kind;
        record.id = BuildAssetId(kind, relativePath);
        record.displayName = path.stem().string();
        record.sourcePath = NormalizePath(std::filesystem::path("Resources") / relativePath);
        record.referenceable = kind != EditorAssetKind::Mesh;

        if (registry.Register(std::move(record))) {
            ++result.registeredAssets;
        } else {
            ++result.skippedFiles;
        }
    }

    return result;
}

} // namespace editor
