#include "EditorAssetFolderIndexer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
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

std::string Trim(std::string value) {
    const auto first = std::find_if(
        value.begin(),
        value.end(),
        [](unsigned char ch) {
            return !std::isspace(ch);
        });
    const auto last = std::find_if(
        value.rbegin(),
        value.rend(),
        [](unsigned char ch) {
            return !std::isspace(ch);
        }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::vector<std::string> SplitList(const std::string& text) {
    std::vector<std::string> values;
    std::string token;
    std::stringstream stream(text);
    while (std::getline(stream, token, ',')) {
        const std::string trimmed = Trim(token);
        if (!trimmed.empty()) {
            values.push_back(trimmed);
        }
    }
    return values;
}

bool ReadAssetMetadata(
    const std::filesystem::path& physicalMetaPath,
    EditorAssetRecord& record) {
    std::ifstream file(physicalMetaPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        if (key == "guid") {
            record.guid = value;
            record.provisionalGuid = false;
        } else if (key == "logicalPath") {
            record.logicalPath = value;
        } else if (key == "tags") {
            record.tags = SplitList(value);
        } else if (key == "dependencies") {
            record.dependencies = SplitList(value);
        }
    }

    record.hasMetadata = true;
    return true;
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
        record.logicalPath = record.sourcePath;
        record.metadataPath = record.sourcePath + ".meta";
        record.missing = !std::filesystem::exists(path, error);
        error.clear();
        record.referenceable = kind != EditorAssetKind::Mesh;
        const std::filesystem::path physicalMetaPath = path.string() + ".meta";
        ReadAssetMetadata(physicalMetaPath, record);

        if (registry.Register(std::move(record))) {
            ++result.registeredAssets;
        } else {
            ++result.skippedFiles;
        }
    }

    return result;
}

} // namespace editor
