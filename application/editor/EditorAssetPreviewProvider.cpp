#include "EditorAssetPreviewProvider.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_set>

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

std::string ExtensionOf(const std::string& path) {
    return ToLower(std::filesystem::path(path).extension().string());
}

std::string UpperFormat(std::string extension) {
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
    return extension.empty() ? std::string("ASSET") : extension;
}

std::string ByteText(uint64_t bytes) {
    return std::to_string(static_cast<unsigned long long>(bytes)) + " B";
}

uint64_t TryFileSize(const std::filesystem::path& path) {
    std::error_code error;
    const uint64_t size = static_cast<uint64_t>(std::filesystem::file_size(path, error));
    return error ? 0 : size;
}

uint64_t TryFileTimestamp(const std::filesystem::path& path) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(path, error);
    return error ? 0ull : static_cast<uint64_t>(time.time_since_epoch().count());
}

bool FileExists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

uint32_t ReadLe16(const unsigned char* data) {
    return static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8);
}

uint32_t ReadLe32(const unsigned char* data) {
    return static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8) |
        (static_cast<uint32_t>(data[2]) << 16) |
        (static_cast<uint32_t>(data[3]) << 24);
}

uint32_t ReadBe16(const unsigned char* data) {
    return (static_cast<uint32_t>(data[0]) << 8) |
        static_cast<uint32_t>(data[1]);
}

uint32_t ReadBe32(const unsigned char* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]);
}

bool ParsePngSize(const std::filesystem::path& path, uint32_t& width, uint32_t& height) {
    std::array<unsigned char, 24> bytes{};
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return false;
    }
    const std::array<unsigned char, 8> signature{{137, 80, 78, 71, 13, 10, 26, 10}};
    if (!std::equal(signature.begin(), signature.end(), bytes.begin())) {
        return false;
    }
    width = ReadBe32(bytes.data() + 16);
    height = ReadBe32(bytes.data() + 20);
    return width > 0 && height > 0;
}

bool ParseBmpSize(const std::filesystem::path& path, uint32_t& width, uint32_t& height) {
    std::array<unsigned char, 26> bytes{};
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return false;
    }
    if (bytes[0] != 'B' || bytes[1] != 'M') {
        return false;
    }
    width = ReadLe32(bytes.data() + 18);
    height = ReadLe32(bytes.data() + 22);
    return width > 0 && height > 0;
}

bool ParseDdsSize(const std::filesystem::path& path, uint32_t& width, uint32_t& height) {
    std::array<unsigned char, 20> bytes{};
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return false;
    }
    if (bytes[0] != 'D' || bytes[1] != 'D' || bytes[2] != 'S' || bytes[3] != ' ') {
        return false;
    }
    height = ReadLe32(bytes.data() + 12);
    width = ReadLe32(bytes.data() + 16);
    return width > 0 && height > 0;
}

bool ParseTgaSize(const std::filesystem::path& path, uint32_t& width, uint32_t& height) {
    std::array<unsigned char, 18> bytes{};
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return false;
    }
    width = ReadLe16(bytes.data() + 12);
    height = ReadLe16(bytes.data() + 14);
    return width > 0 && height > 0;
}

bool ParseJpegSize(const std::filesystem::path& path, uint32_t& width, uint32_t& height) {
    std::ifstream file(path, std::ios::binary);
    unsigned char first = 0;
    unsigned char second = 0;
    if (!file.read(reinterpret_cast<char*>(&first), 1) ||
        !file.read(reinterpret_cast<char*>(&second), 1) ||
        first != 0xff ||
        second != 0xd8) {
        return false;
    }

    while (file) {
        unsigned char markerPrefix = 0;
        if (!file.read(reinterpret_cast<char*>(&markerPrefix), 1)) {
            return false;
        }
        if (markerPrefix != 0xff) {
            continue;
        }
        unsigned char marker = 0;
        do {
            if (!file.read(reinterpret_cast<char*>(&marker), 1)) {
                return false;
            }
        } while (marker == 0xff);

        if (marker == 0xd9 || marker == 0xda) {
            return false;
        }
        unsigned char lengthBytes[2]{};
        if (!file.read(reinterpret_cast<char*>(lengthBytes), 2)) {
            return false;
        }
        const uint32_t segmentLength = ReadBe16(lengthBytes);
        if (segmentLength < 2) {
            return false;
        }
        const bool isStartOfFrame =
            (marker >= 0xc0 && marker <= 0xc3) ||
            (marker >= 0xc5 && marker <= 0xc7) ||
            (marker >= 0xc9 && marker <= 0xcb) ||
            (marker >= 0xcd && marker <= 0xcf);
        if (isStartOfFrame) {
            unsigned char frame[5]{};
            if (!file.read(reinterpret_cast<char*>(frame), 5)) {
                return false;
            }
            height = ReadBe16(frame + 1);
            width = ReadBe16(frame + 3);
            return width > 0 && height > 0;
        }
        file.seekg(static_cast<std::streamoff>(segmentLength - 2), std::ios::cur);
    }
    return false;
}

bool ParseTextureSize(
    const std::filesystem::path& path,
    const std::string& extension,
    uint32_t& width,
    uint32_t& height) {
    if (extension == ".png") {
        return ParsePngSize(path, width, height);
    }
    if (extension == ".bmp") {
        return ParseBmpSize(path, width, height);
    }
    if (extension == ".dds") {
        return ParseDdsSize(path, width, height);
    }
    if (extension == ".tga") {
        return ParseTgaSize(path, width, height);
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return ParseJpegSize(path, width, height);
    }
    return false;
}

bool IsSupportedTextureExtension(const std::string& extension) {
    return extension == ".png" ||
        extension == ".bmp" ||
        extension == ".dds" ||
        extension == ".jpg" ||
        extension == ".jpeg" ||
        extension == ".tga";
}

bool IsSupportedMeshExtension(const std::string& extension) {
    return extension == ".mesh" ||
        extension == ".obj" ||
        extension == ".gltf" ||
        extension == ".glb" ||
        extension == ".fbx";
}

bool IsSupportedTextAssetExtension(const std::string& extension) {
    return extension == ".course" ||
        extension == ".effect" ||
        extension == ".json" ||
        extension == ".preset" ||
        extension == ".txt";
}

bool IsSupportedAudioExtension(const std::string& extension) {
    return extension == ".wav" ||
        extension == ".mp3" ||
        extension == ".ogg" ||
        extension == ".flac";
}

uint32_t CountTextLines(const std::filesystem::path& path) {
    std::ifstream file(path);
    uint32_t lines = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lines;
    }
    return lines;
}

std::string ReadSmallTextFile(const std::filesystem::path& path) {
    constexpr std::uintmax_t kMaxPreviewScanBytes = 2u * 1024u * 1024u;
    std::error_code error;
    if (!std::filesystem::exists(path, error) ||
        !std::filesystem::is_regular_file(path, error) ||
        std::filesystem::file_size(path, error) > kMaxPreviewScanBytes) {
        return {};
    }
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

std::filesystem::path ResolveMaterialTexturePath(
    const std::filesystem::path& meshPath,
    const aiString& texturePath) {
    if (texturePath.length == 0 || texturePath.C_Str()[0] == '*') {
        return {};
    }
    std::filesystem::path path(texturePath.C_Str());
    if (path.is_relative()) {
        path = meshPath.parent_path() / path;
    }
    return path.lexically_normal();
}

struct MeshPreviewSourceSummary {
    uint32_t vertices = 0;
    uint32_t faces = 0;
    uint32_t materialSlots = 0;
    uint32_t materialTextures = 0;
    uint64_t materialTextureTimestamp = 0;
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
    float radius = 0.0f;
    bool hasBounds = false;
    bool hasMaterialBinding = false;
};

void AccumulateBounds(MeshPreviewSourceSummary& summary, float x, float y, float z) {
    if (!summary.hasBounds) {
        summary.minX = summary.maxX = x;
        summary.minY = summary.maxY = y;
        summary.minZ = summary.maxZ = z;
        summary.hasBounds = true;
        return;
    }
    summary.minX = (std::min)(summary.minX, x);
    summary.minY = (std::min)(summary.minY, y);
    summary.minZ = (std::min)(summary.minZ, z);
    summary.maxX = (std::max)(summary.maxX, x);
    summary.maxY = (std::max)(summary.maxY, y);
    summary.maxZ = (std::max)(summary.maxZ, z);
}

void FinalizeBounds(MeshPreviewSourceSummary& summary) {
    if (!summary.hasBounds) {
        return;
    }
    const float dx = summary.maxX - summary.minX;
    const float dy = summary.maxY - summary.minY;
    const float dz = summary.maxZ - summary.minZ;
    summary.radius = (std::max)(0.01f, std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f);
}

MeshPreviewSourceSummary ParseObjMeshPreviewSummary(const std::filesystem::path& path) {
    MeshPreviewSourceSummary summary{};
    std::unordered_set<std::string> materialNames;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("v ", 0) == 0) {
            ++summary.vertices;
            std::istringstream stream(line.substr(2));
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (stream >> x >> y >> z) {
                AccumulateBounds(summary, x, y, z);
            }
        } else if (line.rfind("f ", 0) == 0) {
            ++summary.faces;
        } else if (line.rfind("usemtl ", 0) == 0) {
            const std::string material = line.substr(7);
            if (!material.empty()) {
                materialNames.insert(material);
            }
        } else if (line.rfind("mtllib ", 0) == 0) {
            summary.hasMaterialBinding = true;
        }
    }
    summary.materialSlots = static_cast<uint32_t>(materialNames.size());
    summary.hasMaterialBinding = summary.hasMaterialBinding || summary.materialSlots > 0;
    FinalizeBounds(summary);
    return summary;
}

MeshPreviewSourceSummary ParseAssimpMeshPreviewSummary(const std::filesystem::path& path) {
    MeshPreviewSourceSummary summary{};
    Assimp::Importer importer;
    const unsigned flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_ImproveCacheLocality |
        aiProcess_FindDegenerates |
        aiProcess_SortByPType;
    const aiScene* scene = importer.ReadFile(path.string(), flags);
    if (scene == nullptr || scene->mNumMeshes == 0) {
        return summary;
    }

    std::unordered_set<unsigned> materialIndices;
    for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (mesh == nullptr || mesh->mNumVertices == 0) {
            continue;
        }
        summary.vertices += mesh->mNumVertices;
        summary.faces += mesh->mNumFaces;
        materialIndices.insert(mesh->mMaterialIndex);
        for (unsigned vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            const aiVector3D& p = mesh->mVertices[vertexIndex];
            AccumulateBounds(summary, p.x, p.y, p.z);
        }
    }

    if (scene->HasMaterials()) {
        for (unsigned materialIndex : materialIndices) {
            if (materialIndex >= scene->mNumMaterials) {
                continue;
            }
            const aiMaterial* material = scene->mMaterials[materialIndex];
            if (material == nullptr) {
                continue;
            }

            aiString texturePath;
            bool hasTexture = material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS;
            if (!hasTexture) {
                hasTexture = material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == AI_SUCCESS;
            }
            if (!hasTexture) {
                continue;
            }

            ++summary.materialTextures;
            const std::filesystem::path resolved = ResolveMaterialTexturePath(path, texturePath);
            if (!resolved.empty()) {
                summary.materialTextureTimestamp =
                    (std::max)(summary.materialTextureTimestamp, TryFileTimestamp(resolved));
            }
        }
    }

    summary.materialSlots = static_cast<uint32_t>(materialIndices.size());
    summary.hasMaterialBinding = summary.materialSlots > 0 || summary.materialTextures > 0;
    FinalizeBounds(summary);
    return summary;
}

uint32_t CountTokenOccurrences(const std::string& text, std::string_view token) {
    if (token.empty()) {
        return 0;
    }
    uint32_t count = 0;
    std::size_t offset = 0;
    while ((offset = text.find(token, offset)) != std::string::npos) {
        ++count;
        offset += token.size();
    }
    return count;
}

EditorAssetPreviewInfo MakeMissingPreview(const EditorAssetRecord& record) {
    EditorAssetPreviewInfo info{};
    info.kind = EditorAssetPreviewKind::Icon;
    info.readiness = EditorAssetPreviewReadiness::Missing;
    info.label = ToString(record.kind);
    info.detail = "Source file is missing; using missing-asset fallback.";
    info.fallbackIcon = true;
    return info;
}

EditorAssetPreviewInfo MakeUnsupportedPreview(const EditorAssetRecord& record, const std::string& reason) {
    EditorAssetPreviewInfo info{};
    info.kind = EditorAssetPreviewKind::Icon;
    info.readiness = EditorAssetPreviewReadiness::Unsupported;
    info.label = ToString(record.kind);
    info.detail = reason.empty()
        ? std::string(ToString(record.kind)) + " uses the kind icon fallback."
        : reason;
    info.fallbackIcon = true;
    return info;
}

EditorAssetPreviewInfo BuildTexturePreview(const EditorAssetRecord& record, const std::string& extension) {
    EditorAssetPreviewInfo info{};
    info.kind = EditorAssetPreviewKind::Texture;
    info.label = "TEX";
    info.format = UpperFormat(extension);
    info.fallbackIcon = true;
    if (!IsSupportedTextureExtension(extension)) {
        info.readiness = EditorAssetPreviewReadiness::Failed;
        info.detail = extension.empty()
            ? "Texture preview cannot be generated without a source extension."
            : "Texture preview does not support source extension " + extension + ".";
        return info;
    }

    const std::filesystem::path source(record.sourcePath);
    if (!FileExists(source)) {
        info.readiness = EditorAssetPreviewReadiness::Ready;
        info.fallbackIcon = false;
        info.detail = info.format + " texture preview metadata inferred from source extension.";
        return info;
    }

    info.byteSize = TryFileSize(source);
    if (!ParseTextureSize(source, extension, info.width, info.height)) {
        info.readiness = EditorAssetPreviewReadiness::Failed;
        info.detail = info.format + " texture preview metadata could not be read from source file.";
        return info;
    }

    info.readiness = EditorAssetPreviewReadiness::Ready;
    info.fallbackIcon = false;
    info.label = info.format + " " + std::to_string(info.width) + "x" + std::to_string(info.height);
    info.detail =
        info.format +
        " texture preview " +
        std::to_string(info.width) +
        "x" +
        std::to_string(info.height) +
        ", " +
        ByteText(info.byteSize) +
        ".";
    return info;
}

EditorAssetPreviewInfo BuildMeshPreview(const EditorAssetRecord& record, const std::string& extension) {
    if (!IsSupportedMeshExtension(extension)) {
        return MakeUnsupportedPreview(
            record,
            extension.empty()
                ? "Mesh preview cannot be generated without a source extension."
                : "Mesh preview does not support source extension " + extension + ".");
    }

    EditorAssetPreviewInfo info{};
    info.kind = EditorAssetPreviewKind::Mesh;
    info.readiness = EditorAssetPreviewReadiness::Ready;
    info.label = "MESH";
    info.format = UpperFormat(extension);
    info.fallbackIcon = true;

    const std::filesystem::path source(record.sourcePath);
    if (FileExists(source)) {
        info.byteSize = TryFileSize(source);
        const MeshPreviewSourceSummary productionSummary = ParseAssimpMeshPreviewSummary(source);
        if (productionSummary.hasBounds && productionSummary.faces > 0) {
            info.vertexCount = productionSummary.vertices;
            info.faceCount = productionSummary.faces;
            info.materialSlotCount = productionSummary.materialSlots;
            info.materialTextureCount = productionSummary.materialTextures;
            info.materialTextureTimestamp = productionSummary.materialTextureTimestamp;
            info.boundsMin[0] = productionSummary.minX;
            info.boundsMin[1] = productionSummary.minY;
            info.boundsMin[2] = productionSummary.minZ;
            info.boundsMax[0] = productionSummary.maxX;
            info.boundsMax[1] = productionSummary.maxY;
            info.boundsMax[2] = productionSummary.maxZ;
            info.boundsRadius = productionSummary.radius;
            info.previewCameraDistance = productionSummary.radius > 0.0f ? productionSummary.radius * 2.8f : 3.0f;
            info.previewLightDirection[0] = 0.38f;
            info.previewLightDirection[1] = -0.82f;
            info.previewLightDirection[2] = 0.42f;
            info.hasPreviewGeometry = true;
            info.hasMaterialBinding = productionSummary.hasMaterialBinding;
        } else if (extension == ".obj") {
            const MeshPreviewSourceSummary summary = ParseObjMeshPreviewSummary(source);
            info.vertexCount = summary.vertices;
            info.faceCount = summary.faces;
            info.materialSlotCount = summary.materialSlots;
            info.materialTextureCount = summary.materialTextures;
            info.materialTextureTimestamp = summary.materialTextureTimestamp;
            info.boundsMin[0] = summary.minX;
            info.boundsMin[1] = summary.minY;
            info.boundsMin[2] = summary.minZ;
            info.boundsMax[0] = summary.maxX;
            info.boundsMax[1] = summary.maxY;
            info.boundsMax[2] = summary.maxZ;
            info.boundsRadius = summary.radius;
            info.previewCameraDistance = summary.radius > 0.0f ? summary.radius * 2.8f : 3.0f;
            info.previewLightDirection[0] = 0.38f;
            info.previewLightDirection[1] = -0.82f;
            info.previewLightDirection[2] = 0.42f;
            info.hasPreviewGeometry = summary.hasBounds && summary.faces > 0;
            info.hasMaterialBinding = summary.hasMaterialBinding;
        } else if (extension == ".mesh" || extension == ".gltf") {
            info.lineCount = CountTextLines(source);
            const std::string text = ReadSmallTextFile(source);
            info.materialSlotCount =
                (std::max)(CountTokenOccurrences(text, "material"), CountTokenOccurrences(text, "Material"));
            info.hasMaterialBinding = info.materialSlotCount > 0;
            info.previewCameraDistance = 3.0f;
        } else if (extension == ".glb" || extension == ".fbx") {
            info.previewCameraDistance = 3.0f;
        }
    }
    if (info.previewCameraDistance <= 0.0f) {
        info.previewCameraDistance = info.boundsRadius > 0.0f ? info.boundsRadius * 2.8f : 3.0f;
    }

    std::ostringstream detail;
    detail << info.format << " mesh preview metadata";
    if (info.vertexCount > 0 || info.faceCount > 0) {
        detail << ": " << info.vertexCount << " vertices, " << info.faceCount << " faces";
    } else if (info.lineCount > 0) {
        detail << ": " << info.lineCount << " source lines";
    } else {
        detail << " is available";
    }
    if (info.hasPreviewGeometry) {
        detail << ", bounds radius " << info.boundsRadius;
    }
    if (info.hasMaterialBinding || info.materialSlotCount > 0) {
        detail << ", material slots " << (std::max)(1u, info.materialSlotCount);
    }
    if (info.materialTextureCount > 0) {
        detail << ", material textures " << info.materialTextureCount;
    }
    detail << ", preview camera " << info.previewCameraDistance;
    if (info.byteSize > 0) {
        detail << ", " << ByteText(info.byteSize);
    }
    detail << ".";
    info.detail = detail.str();
    return info;
}

EditorAssetPreviewInfo BuildTextPreview(
    const EditorAssetRecord& record,
    const std::string& extension,
    EditorAssetPreviewKind kind,
    const char* label) {
    if (!IsSupportedTextAssetExtension(extension)) {
        return MakeUnsupportedPreview(
            record,
            extension.empty()
                ? std::string(label) + " preview cannot be generated without a source extension."
                : std::string(label) + " preview does not support source extension " + extension + ".");
    }

    EditorAssetPreviewInfo info{};
    info.kind = kind;
    info.readiness = EditorAssetPreviewReadiness::Ready;
    info.label = label;
    info.format = UpperFormat(extension);
    info.fallbackIcon = true;
    const std::filesystem::path source(record.sourcePath);
    if (FileExists(source)) {
        info.byteSize = TryFileSize(source);
        info.lineCount = CountTextLines(source);
    }
    std::ostringstream detail;
    detail << info.format << " authoring preview metadata";
    if (info.lineCount > 0) {
        detail << ": " << info.lineCount << " lines";
    } else {
        detail << " is available";
    }
    if (info.byteSize > 0) {
        detail << ", " << ByteText(info.byteSize);
    }
    detail << ".";
    info.detail = detail.str();
    return info;
}

EditorAssetPreviewInfo BuildAudioPreview(const EditorAssetRecord& record, const std::string& extension) {
    if (!IsSupportedAudioExtension(extension)) {
        return MakeUnsupportedPreview(
            record,
            extension.empty()
                ? "Audio preview cannot be generated without a source extension."
                : "Audio preview does not support source extension " + extension + ".");
    }

    EditorAssetPreviewInfo info{};
    info.kind = EditorAssetPreviewKind::Audio;
    info.readiness = EditorAssetPreviewReadiness::Ready;
    info.label = "AUD";
    info.format = UpperFormat(extension);
    info.fallbackIcon = true;
    const std::filesystem::path source(record.sourcePath);
    if (FileExists(source)) {
        info.byteSize = TryFileSize(source);
    }
    info.detail =
        info.format +
        " audio preview metadata" +
        (info.byteSize > 0 ? std::string(": ") + ByteText(info.byteSize) : std::string(" is available")) +
        ".";
    return info;
}

} // namespace

EditorAssetPreviewInfo EditorAssetPreviewProvider::BuildPreview(const EditorAssetRecord& record) const {
    if (record.missing) {
        return MakeMissingPreview(record);
    }

    const std::string extension = ExtensionOf(record.sourcePath);
    switch (record.kind) {
    case EditorAssetKind::Texture:
        return BuildTexturePreview(record, extension);
    case EditorAssetKind::Mesh:
        return BuildMeshPreview(record, extension);
    case EditorAssetKind::Course:
        return BuildTextPreview(record, extension, EditorAssetPreviewKind::Text, "CRS");
    case EditorAssetKind::Prefab:
        return BuildTextPreview(record, extension, EditorAssetPreviewKind::Text, "PFB");
    case EditorAssetKind::MaterialGraph:
        return BuildTextPreview(record, extension, EditorAssetPreviewKind::Text, "MAT");
    case EditorAssetKind::VfxGraph:
        return BuildTextPreview(record, extension, EditorAssetPreviewKind::Text, "VFXG");
    case EditorAssetKind::AnimationStateMachine:
        return BuildTextPreview(record, extension, EditorAssetPreviewKind::Text, "ASM");
    case EditorAssetKind::GameplayVisualScript:
        return BuildTextPreview(record, extension, EditorAssetPreviewKind::Text, "GVS");
    case EditorAssetKind::Effect:
        return BuildTextPreview(record, extension, EditorAssetPreviewKind::Text, "FX");
    case EditorAssetKind::Audio:
        return BuildAudioPreview(record, extension);
    case EditorAssetKind::Unknown:
        break;
    }
    return MakeUnsupportedPreview(record, {});
}

const char* ToString(EditorAssetPreviewKind kind) {
    switch (kind) {
    case EditorAssetPreviewKind::Unknown:
        return "Unknown";
    case EditorAssetPreviewKind::Icon:
        return "Icon";
    case EditorAssetPreviewKind::Texture:
        return "Texture";
    case EditorAssetPreviewKind::Mesh:
        return "Mesh";
    case EditorAssetPreviewKind::Text:
        return "Text";
    case EditorAssetPreviewKind::Audio:
        return "Audio";
    }
    return "Unknown";
}

const char* ToString(EditorAssetPreviewReadiness readiness) {
    switch (readiness) {
    case EditorAssetPreviewReadiness::Missing:
        return "Missing";
    case EditorAssetPreviewReadiness::Unsupported:
        return "Unsupported";
    case EditorAssetPreviewReadiness::Ready:
        return "Ready";
    case EditorAssetPreviewReadiness::Failed:
        return "Failed";
    }
    return "Unknown";
}

} // namespace editor
