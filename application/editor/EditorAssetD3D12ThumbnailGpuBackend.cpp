#include "EditorAssetD3D12ThumbnailGpuBackend.h"

#include "EditorAssetFallbackIconAtlas.h"
#include "EditorAssetMeshThumbnailPreviewRenderer.h"
#include "EditorAssetPreviewSceneRenderer.h"
#include "EditorAssetThumbnailTextureLoader.h"
#include "mesh/EditorProductionMeshAsset.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <d3dcompiler.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace editor {
namespace {

constexpr uint32_t kMaxPreviewMaterialSlots = 4;
constexpr uint32_t kPreviewMaterialTextureRoleCount = 4;
constexpr uint32_t kMaxPreviewMaterialTextures = kMaxPreviewMaterialSlots * kPreviewMaterialTextureRoleCount;
constexpr size_t kMaxPreviewMaterialTexturePixelCacheEntries = 128;

enum class PreviewMaterialTextureRole : uint32_t {
    Albedo = 0,
    Normal = 1,
    Roughness = 2,
    Metallic = 3,
};

bool SupportsDirectPreviewSceneSvr(const EditorAssetGpuThumbnailAllocationRequest& request) {
    return request.previewKind == EditorAssetPreviewKind::Mesh || request.kind == EditorAssetKind::Mesh;
}

void FillPreviewClearColor(uint32_t swatchRgba, float (&outColor)[4]) {
    const float r = static_cast<float>(swatchRgba & 0xffu) / 255.0f;
    const float g = static_cast<float>((swatchRgba >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>((swatchRgba >> 16) & 0xffu) / 255.0f;
    outColor[0] = 0.18f + r * 0.45f;
    outColor[1] = 0.18f + g * 0.45f;
    outColor[2] = 0.20f + b * 0.45f;
    outColor[3] = 1.0f;
}

void FillColor(uint32_t rgba, float brightness, float (&outColor)[4]) {
    outColor[0] = (std::clamp)((static_cast<float>(rgba & 0xffu) / 255.0f) * brightness, 0.0f, 1.0f);
    outColor[1] = (std::clamp)((static_cast<float>((rgba >> 8) & 0xffu) / 255.0f) * brightness, 0.0f, 1.0f);
    outColor[2] = (std::clamp)((static_cast<float>((rgba >> 16) & 0xffu) / 255.0f) * brightness, 0.0f, 1.0f);
    outColor[3] = 1.0f;
}

struct PreviewMeshVertex {
    float position[3]{};
    float normal[3]{};
    float color[3]{};
    float texcoord[2]{};
    float textureWeight = 0.0f;
    float textureIndex = 0.0f;
    float roughness = 0.58f;
    float metallic = 0.0f;
    float normalTextureWeight = 0.0f;
    float roughnessTextureWeight = 0.0f;
    float metallicTextureWeight = 0.0f;
};

struct PreviewMeshConstants {
    float center[3]{};
    float invRadius = 1.0f;
    float lightDirection[3]{0.35f, -0.85f, 0.38f};
    float materialSlots = 1.0f;
    float baseColor[3]{0.55f, 0.68f, 0.48f};
    float cameraDistance = 3.0f;
    float textureCount = 0.0f;
    float pbrStrength = 1.0f;
    float averageRoughness = 0.58f;
    float averageMetallic = 0.0f;
};

struct PreviewMeshPayload {
    std::vector<PreviewMeshVertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t materialSlotCount = 0;
    uint32_t materialTextureCount = 0;
    bool proceduralFallback = false;
    bool productionLoader = false;
    bool materialTextureBound = false;
    bool materialTextureFallback = false;
    std::vector<std::string> materialTexturePaths;
    uint32_t normalTextureCount = 0;
    uint32_t roughnessTextureCount = 0;
    uint32_t metallicTextureCount = 0;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct PreviewMaterialInfo {
    std::array<float, 3> color{};
    std::string texturePath;
    std::string normalTexturePath;
    std::string roughnessTexturePath;
    std::string metallicTexturePath;
    bool hasTexture = false;
    uint32_t textureIndex = UINT32_MAX;
    bool hasNormalTexture = false;
    bool hasRoughnessTexture = false;
    bool hasMetallicTexture = false;
    uint32_t normalTextureIndex = UINT32_MAX;
    uint32_t roughnessTextureIndex = UINT32_MAX;
    uint32_t metallicTextureIndex = UINT32_MAX;
    float roughness = 0.58f;
    float metallic = 0.0f;
};

Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(
    ID3D12Device* device,
    uint64_t sizeInBytes);

Vec3 Subtract(Vec3 a, Vec3 b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Cross(Vec3 a, Vec3 b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

Vec3 Normalize(Vec3 value) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.00001f) {
        return Vec3{0.0f, 1.0f, 0.0f};
    }
    return Vec3{value.x / length, value.y / length, value.z / length};
}

void AccumulateBounds(Vec3 point, Vec3& minPoint, Vec3& maxPoint, bool& hasBounds) {
    if (!hasBounds) {
        minPoint = point;
        maxPoint = point;
        hasBounds = true;
        return;
    }
    minPoint.x = (std::min)(minPoint.x, point.x);
    minPoint.y = (std::min)(minPoint.y, point.y);
    minPoint.z = (std::min)(minPoint.z, point.z);
    maxPoint.x = (std::max)(maxPoint.x, point.x);
    maxPoint.y = (std::max)(maxPoint.y, point.y);
    maxPoint.z = (std::max)(maxPoint.z, point.z);
}

std::array<float, 3> MaterialColor(uint32_t rgba, uint32_t slot, uint32_t slotCount) {
    const float r = static_cast<float>(rgba & 0xffu) / 255.0f;
    const float g = static_cast<float>((rgba >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>((rgba >> 16) & 0xffu) / 255.0f;
    const float t = slotCount > 1 ? static_cast<float>(slot) / static_cast<float>(slotCount - 1u) : 0.0f;
    return {
        (std::clamp)(r * (0.82f + 0.18f * t) + 0.08f * t, 0.0f, 1.0f),
        (std::clamp)(g * (0.76f + 0.24f * (1.0f - t)) + 0.06f * t, 0.0f, 1.0f),
        (std::clamp)(b * (0.86f + 0.14f * t) + 0.10f * (1.0f - t), 0.0f, 1.0f)};
}

std::string Lowercase(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
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

uint32_t PreviewMaterialTextureDescriptorIndex(
    PreviewMaterialTextureRole role,
    uint32_t slot) {
    if (slot >= kMaxPreviewMaterialSlots) {
        return UINT32_MAX;
    }
    return static_cast<uint32_t>(role) * kMaxPreviewMaterialSlots + slot;
}

uint32_t RegisterPreviewMaterialTexturePath(
    PreviewMeshPayload& payload,
    PreviewMaterialTextureRole role,
    uint32_t slot,
    const std::string& texturePath) {
    const uint32_t descriptorIndex = PreviewMaterialTextureDescriptorIndex(role, slot);
    if (texturePath.empty() || descriptorIndex == UINT32_MAX) {
        payload.materialTextureFallback = true;
        return UINT32_MAX;
    }
    if (payload.materialTexturePaths.size() < kMaxPreviewMaterialTextures) {
        payload.materialTexturePaths.resize(kMaxPreviewMaterialTextures);
    }
    payload.materialTexturePaths[descriptorIndex] = texturePath;
    return descriptorIndex;
}

uint32_t CountPreviewMaterialTexturePaths(const std::vector<std::string>& texturePaths) {
    return static_cast<uint32_t>(std::count_if(
        texturePaths.begin(),
        texturePaths.end(),
        [](const std::string& path) { return !path.empty(); }));
}

bool TryRegisterAssimpMaterialTexture(
    const aiMaterial* material,
    const std::filesystem::path& meshPath,
    aiTextureType primaryType,
    aiTextureType fallbackType,
    PreviewMeshPayload& payload,
    PreviewMaterialTextureRole role,
    uint32_t slot,
    std::string& outTexturePath,
    uint32_t& outTextureIndex) {
    if (material == nullptr) {
        return false;
    }

    aiString texturePath;
    bool hasTexture = material->GetTexture(primaryType, 0, &texturePath) == AI_SUCCESS;
    if (!hasTexture && fallbackType != primaryType) {
        hasTexture = material->GetTexture(fallbackType, 0, &texturePath) == AI_SUCCESS;
    }
    if (!hasTexture) {
        return false;
    }

    ++payload.materialTextureCount;
    const std::filesystem::path resolvedTexture = ResolveMaterialTexturePath(meshPath, texturePath);
    std::error_code existsError;
    if (resolvedTexture.empty() || !std::filesystem::exists(resolvedTexture, existsError)) {
        payload.materialTextureFallback = true;
        return false;
    }

    outTexturePath = resolvedTexture.generic_string();
    outTextureIndex = RegisterPreviewMaterialTexturePath(payload, role, slot, outTexturePath);
    if (outTextureIndex == UINT32_MAX) {
        payload.materialTextureFallback = true;
        outTexturePath.clear();
        return false;
    }
    return true;
}

PreviewMaterialInfo MaterialInfoFromAssimp(
    const aiMaterial* material,
    const std::filesystem::path& meshPath,
    uint32_t fallbackRgba,
    uint32_t slot,
    uint32_t slotCount,
    PreviewMeshPayload& payload) {
    PreviewMaterialInfo info{};
    info.color = MaterialColor(fallbackRgba, slot, slotCount);
    if (material == nullptr) {
        return info;
    }

    aiColor3D diffuse{};
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
        info.color = {
            (std::clamp)(diffuse.r, 0.0f, 1.0f),
            (std::clamp)(diffuse.g, 0.0f, 1.0f),
            (std::clamp)(diffuse.b, 0.0f, 1.0f)};
    }

    float roughness = info.roughness;
    if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
        info.roughness = (std::clamp)(roughness, 0.04f, 1.0f);
    } else {
        float shininess = 0.0f;
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 0.0f) {
            info.roughness = (std::clamp)(std::sqrt(2.0f / (shininess + 2.0f)), 0.04f, 1.0f);
        }
    }
    float metallic = info.metallic;
    if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
        info.metallic = (std::clamp)(metallic, 0.0f, 1.0f);
    }

    info.hasTexture = TryRegisterAssimpMaterialTexture(
        material,
        meshPath,
        aiTextureType_DIFFUSE,
        aiTextureType_BASE_COLOR,
        payload,
        PreviewMaterialTextureRole::Albedo,
        slot,
        info.texturePath,
        info.textureIndex);
    info.hasNormalTexture = TryRegisterAssimpMaterialTexture(
        material,
        meshPath,
        aiTextureType_NORMALS,
        aiTextureType_NORMAL_CAMERA,
        payload,
        PreviewMaterialTextureRole::Normal,
        slot,
        info.normalTexturePath,
        info.normalTextureIndex);
    if (info.hasNormalTexture) {
        ++payload.normalTextureCount;
    }
    info.hasRoughnessTexture = TryRegisterAssimpMaterialTexture(
        material,
        meshPath,
        aiTextureType_DIFFUSE_ROUGHNESS,
        aiTextureType_DIFFUSE_ROUGHNESS,
        payload,
        PreviewMaterialTextureRole::Roughness,
        slot,
        info.roughnessTexturePath,
        info.roughnessTextureIndex);
    if (info.hasRoughnessTexture) {
        ++payload.roughnessTextureCount;
    }
    info.hasMetallicTexture = TryRegisterAssimpMaterialTexture(
        material,
        meshPath,
        aiTextureType_METALNESS,
        aiTextureType_METALNESS,
        payload,
        PreviewMaterialTextureRole::Metallic,
        slot,
        info.metallicTexturePath,
        info.metallicTextureIndex);
    if (info.hasMetallicTexture) {
        ++payload.metallicTextureCount;
    }
    return info;
}

int ParseObjPositionIndex(std::string_view token, size_t positionCount) {
    if (token.empty()) {
        return -1;
    }
    const size_t slash = token.find('/');
    const std::string indexText(token.substr(0, slash));
    if (indexText.empty()) {
        return -1;
    }
    int index = 0;
    try {
        index = std::stoi(indexText);
    } catch (...) {
        return -1;
    }
    if (index > 0) {
        --index;
    } else if (index < 0) {
        index = static_cast<int>(positionCount) + index;
    } else {
        return -1;
    }
    if (index < 0 || static_cast<size_t>(index) >= positionCount) {
        return -1;
    }
    return index;
}

void AppendPreviewTriangle(
    PreviewMeshPayload& payload,
    Vec3 a,
    Vec3 b,
    Vec3 c,
    const std::array<float, 3>& color,
    Vec2 uvA,
    Vec2 uvB,
    Vec2 uvC,
    float textureWeight,
    uint32_t textureIndex,
    float roughness,
    float metallic,
    float normalTextureWeight,
    float roughnessTextureWeight,
    float metallicTextureWeight,
    Vec3& minPoint,
    Vec3& maxPoint,
    bool& hasBounds) {
    const Vec3 normal = Normalize(Cross(Subtract(b, a), Subtract(c, a)));
    const uint32_t baseIndex = static_cast<uint32_t>(payload.vertices.size());
    const Vec3 points[3] = {a, b, c};
    const Vec2 uvs[3] = {uvA, uvB, uvC};
    for (size_t i = 0; i < 3; ++i) {
        const Vec3 point = points[i];
        PreviewMeshVertex vertex{};
        vertex.position[0] = point.x;
        vertex.position[1] = point.y;
        vertex.position[2] = point.z;
        vertex.normal[0] = normal.x;
        vertex.normal[1] = normal.y;
        vertex.normal[2] = normal.z;
        vertex.color[0] = color[0];
        vertex.color[1] = color[1];
        vertex.color[2] = color[2];
        vertex.texcoord[0] = uvs[i].x;
        vertex.texcoord[1] = uvs[i].y;
        vertex.textureWeight = textureWeight;
        vertex.textureIndex = static_cast<float>(textureIndex);
        vertex.roughness = (std::clamp)(roughness, 0.04f, 1.0f);
        vertex.metallic = (std::clamp)(metallic, 0.0f, 1.0f);
        vertex.normalTextureWeight = normalTextureWeight;
        vertex.roughnessTextureWeight = roughnessTextureWeight;
        vertex.metallicTextureWeight = metallicTextureWeight;
        payload.vertices.push_back(vertex);
        AccumulateBounds(point, minPoint, maxPoint, hasBounds);
    }
    payload.indices.push_back(baseIndex + 0u);
    payload.indices.push_back(baseIndex + 1u);
    payload.indices.push_back(baseIndex + 2u);
}

bool BuildProductionPreviewMeshPayload(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    PreviewMeshPayload& outPayload,
    Vec3& outMinPoint,
    Vec3& outMaxPoint) {
    if (request.sourcePath.empty()) {
        return false;
    }
    const std::filesystem::path sourcePath(request.sourcePath);
    const std::string extension = Lowercase(sourcePath.extension().string());
    if (extension != ".obj" &&
        extension != ".fbx" &&
        extension != ".gltf" &&
        extension != ".glb" &&
        extension != ".mesh") {
        return false;
    }

    if (extension == ".mesh") {
        std::error_code sourceSizeError;
        const std::uintmax_t sourceSize = std::filesystem::file_size(sourcePath, sourceSizeError);
        std::ifstream source;
        if (!sourceSizeError && sourceSize <= 64u * 1024u * 1024u) {
            source.open(sourcePath, std::ios::binary);
        }
        std::ostringstream text;
        if (source) text << source.rdbuf();
        EditorProductionMeshAssetDocument document{};
        if (source && EditorProductionMeshAssetDocument::Deserialize(
                text.str(), document, nullptr)) {
            constexpr std::size_t kMaxBakedPreviewTriangles = 4096;
            outPayload = {};
            outPayload.productionLoader = true;
            bool hasBounds = false;
            Vec3 minimum{};
            Vec3 maximum{};
            outPayload.vertices.reserve(document.geometry.vertices.size());
            for (const EditorGeometryVertex& sourceVertex : document.geometry.vertices) {
                PreviewMeshVertex vertex{};
                vertex.position[0] = sourceVertex.position.x;
                vertex.position[1] = sourceVertex.position.y;
                vertex.position[2] = sourceVertex.position.z;
                vertex.normal[0] = sourceVertex.normal.x;
                vertex.normal[1] = sourceVertex.normal.y;
                vertex.normal[2] = sourceVertex.normal.z;
                vertex.texcoord[0] = sourceVertex.u;
                vertex.texcoord[1] = sourceVertex.v;
                const std::array<float, 3> color = MaterialColor(request.swatchRgba, 0, 1);
                vertex.color[0] = color[0];
                vertex.color[1] = color[1];
                vertex.color[2] = color[2];
                vertex.roughness = 0.58f;
                outPayload.vertices.push_back(vertex);
                AccumulateBounds(
                    {sourceVertex.position.x, sourceVertex.position.y, sourceVertex.position.z},
                    minimum, maximum, hasBounds);
            }
            const std::size_t triangleCount = (std::min)(
                document.geometry.triangles.size(), kMaxBakedPreviewTriangles);
            uint32_t maxSlot = 0;
            outPayload.indices.reserve(triangleCount * 3);
            for (std::size_t index = 0; index < triangleCount; ++index) {
                const EditorGeometryTriangle& triangle = document.geometry.triangles[index];
                outPayload.indices.insert(outPayload.indices.end(),
                    {triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]});
                maxSlot = (std::max)(maxSlot, triangle.materialSlot);
            }
            outPayload.materialSlotCount = maxSlot + 1;
            outMinPoint = minimum;
            outMaxPoint = maximum;
            return hasBounds && !outPayload.indices.empty();
        }
    }

    Assimp::Importer importer;
    const unsigned flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_ImproveCacheLocality |
        aiProcess_FindDegenerates |
        aiProcess_SortByPType;
    const aiScene* scene = importer.ReadFile(sourcePath.string(), flags);
    if (scene == nullptr || scene->mNumMeshes == 0) {
        return false;
    }

    constexpr size_t kMaxPreviewTriangles = 4096;
    std::unordered_map<unsigned, uint32_t> materialSlotByIndex;
    std::unordered_map<unsigned, PreviewMaterialInfo> materialInfoByIndex;
    const uint32_t swatch = request.swatchRgba == 0 ? 0xff88a766u : request.swatchRgba;
    bool hasBounds = false;
    Vec3 minPoint{};
    Vec3 maxPoint{};

    outPayload = {};
    outPayload.productionLoader = true;
    for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (mesh == nullptr || mesh->mNumVertices == 0 || mesh->mNumFaces == 0) {
            continue;
        }

        const unsigned materialIndex = mesh->mMaterialIndex;
        const uint32_t slot = [&]() {
            const auto existing = materialSlotByIndex.find(materialIndex);
            if (existing != materialSlotByIndex.end()) {
                return existing->second;
            }
            const uint32_t newSlot = static_cast<uint32_t>(materialSlotByIndex.size());
            materialSlotByIndex.emplace(materialIndex, newSlot);
            return newSlot;
        }();
        const uint32_t slotCountHint = (std::max)(1u, (std::max)(request.materialSlotCount, static_cast<uint32_t>(scene->mNumMaterials)));
        PreviewMaterialInfo materialInfo{};
        const auto materialInfoIt = materialInfoByIndex.find(materialIndex);
        if (materialInfoIt != materialInfoByIndex.end()) {
            materialInfo = materialInfoIt->second;
        } else {
            const aiMaterial* material =
                scene->HasMaterials() && materialIndex < scene->mNumMaterials
                    ? scene->mMaterials[materialIndex]
                    : nullptr;
            materialInfo = MaterialInfoFromAssimp(
                material,
                sourcePath,
                swatch,
                slot,
                slotCountHint,
                outPayload);
            materialInfoByIndex.emplace(materialIndex, materialInfo);
        }
        const bool materialUsesTexture =
            materialInfo.hasTexture &&
            materialInfo.textureIndex != UINT32_MAX &&
            materialInfo.textureIndex < kMaxPreviewMaterialSlots;
        const uint32_t materialTextureSlot =
            slot < kMaxPreviewMaterialSlots ? slot : 0u;

        for (unsigned faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            if (outPayload.indices.size() / 3u >= kMaxPreviewTriangles) {
                break;
            }
            const aiFace& face = mesh->mFaces[faceIndex];
            if (face.mNumIndices != 3) {
                continue;
            }
            const auto pointAt = [&](unsigned index) {
                const aiVector3D& p = mesh->mVertices[index];
                return Vec3{p.x, p.y, p.z};
            };
            const auto uvAt = [&](unsigned index) {
                if (mesh->HasTextureCoords(0) && mesh->mTextureCoords[0] != nullptr) {
                    const aiVector3D& uv = mesh->mTextureCoords[0][index];
                    return Vec2{uv.x, 1.0f - uv.y};
                }
                return Vec2{};
            };
            AppendPreviewTriangle(
                outPayload,
                pointAt(face.mIndices[0]),
                pointAt(face.mIndices[1]),
                pointAt(face.mIndices[2]),
                materialInfo.color,
                uvAt(face.mIndices[0]),
                uvAt(face.mIndices[1]),
                uvAt(face.mIndices[2]),
                materialUsesTexture ? 1.0f : 0.0f,
                materialTextureSlot,
                materialInfo.roughness,
                materialInfo.metallic,
                materialInfo.hasNormalTexture ? 1.0f : 0.0f,
                materialInfo.hasRoughnessTexture ? 1.0f : 0.0f,
                materialInfo.hasMetallicTexture ? 1.0f : 0.0f,
                minPoint,
                maxPoint,
                hasBounds);
        }
        if (outPayload.indices.size() / 3u >= kMaxPreviewTriangles) {
            break;
        }
    }

    if (outPayload.vertices.empty() || !hasBounds) {
        outPayload = {};
        return false;
    }
    outPayload.materialSlotCount = static_cast<uint32_t>(materialSlotByIndex.size());
    outMinPoint = minPoint;
    outMaxPoint = maxPoint;
    return true;
}

bool BuildObjPreviewMeshPayload(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    PreviewMeshPayload& outPayload,
    Vec3& outMinPoint,
    Vec3& outMaxPoint) {
    if (request.sourcePath.empty() ||
        Lowercase(std::filesystem::path(request.sourcePath).extension().string()) != ".obj") {
        return false;
    }

    std::ifstream file(request.sourcePath);
    if (!file.is_open()) {
        return false;
    }

    constexpr size_t kMaxSourcePositions = 16384;
    constexpr size_t kMaxPreviewTriangles = 4096;
    std::vector<Vec3> positions;
    positions.reserve((std::min)(static_cast<uint32_t>(kMaxSourcePositions), (std::max)(request.vertexCount, 64u)));
    std::unordered_map<std::string, uint32_t> materialSlots;
    uint32_t currentSlot = 0;
    bool hasBounds = false;
    Vec3 minPoint{};
    Vec3 maxPoint{};
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("v ", 0) == 0) {
            if (positions.size() >= kMaxSourcePositions) {
                continue;
            }
            std::istringstream stream(line.substr(2));
            Vec3 point{};
            if (stream >> point.x >> point.y >> point.z) {
                positions.push_back(point);
            }
        } else if (line.rfind("usemtl ", 0) == 0) {
            std::string name = line.substr(7);
            if (name.empty()) {
                currentSlot = 0;
                continue;
            }
            auto [it, inserted] = materialSlots.emplace(std::move(name), static_cast<uint32_t>(materialSlots.size()));
            currentSlot = it->second;
            (void)inserted;
        } else if (line.rfind("f ", 0) == 0) {
            std::istringstream stream(line.substr(2));
            std::vector<int> faceIndices;
            std::string token;
            while (stream >> token) {
                const int index = ParseObjPositionIndex(token, positions.size());
                if (index >= 0) {
                    faceIndices.push_back(index);
                }
            }
            if (faceIndices.size() < 3) {
                continue;
            }
            const uint32_t slotCount = (std::max)(1u, (std::max)(request.materialSlotCount, static_cast<uint32_t>(materialSlots.size())));
            const std::array<float, 3> color = MaterialColor(
                request.swatchRgba == 0 ? 0xff88a766u : request.swatchRgba,
                currentSlot % slotCount,
                slotCount);
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                if (outPayload.indices.size() / 3u >= kMaxPreviewTriangles) {
                    break;
                }
                AppendPreviewTriangle(
                    outPayload,
                    positions[static_cast<size_t>(faceIndices[0])],
                    positions[static_cast<size_t>(faceIndices[i])],
                    positions[static_cast<size_t>(faceIndices[i + 1])],
                    color,
                    Vec2{},
                    Vec2{},
                    Vec2{},
                    0.0f,
                    0u,
                    0.58f,
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                    minPoint,
                    maxPoint,
                    hasBounds);
            }
        }
        if (outPayload.indices.size() / 3u >= kMaxPreviewTriangles) {
            break;
        }
    }

    if (outPayload.vertices.empty() || !hasBounds) {
        outPayload = {};
        return false;
    }
    outPayload.materialSlotCount = static_cast<uint32_t>(materialSlots.size());
    outMinPoint = minPoint;
    outMaxPoint = maxPoint;
    return true;
}

void BuildProceduralPreviewMeshPayload(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    PreviewMeshPayload& outPayload,
    Vec3& outMinPoint,
    Vec3& outMaxPoint) {
    outPayload = {};
    outPayload.proceduralFallback = true;
    const float extent = (std::max)(0.5f, request.boundsRadius > 0.0f ? request.boundsRadius : 1.0f);
    const Vec3 top{0.0f, extent, 0.0f};
    const Vec3 bottom{0.0f, -extent, 0.0f};
    const Vec3 left{-extent, 0.0f, 0.0f};
    const Vec3 right{extent, 0.0f, 0.0f};
    const Vec3 front{0.0f, 0.0f, -extent};
    const Vec3 back{0.0f, 0.0f, extent};
    const uint32_t slotCount = (std::max)(1u, request.materialSlotCount);
    bool hasBounds = false;
    Vec3 minPoint{};
    Vec3 maxPoint{};
    const uint32_t swatch = request.swatchRgba == 0 ? 0xff88a766u : request.swatchRgba;
    AppendPreviewTriangle(outPayload, top, right, front, MaterialColor(swatch, 0, slotCount), Vec2{}, Vec2{}, Vec2{}, 0.0f, 0u, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f, minPoint, maxPoint, hasBounds);
    AppendPreviewTriangle(outPayload, top, front, left, MaterialColor(swatch, 1 % slotCount, slotCount), Vec2{}, Vec2{}, Vec2{}, 0.0f, 0u, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f, minPoint, maxPoint, hasBounds);
    AppendPreviewTriangle(outPayload, top, left, back, MaterialColor(swatch, 2 % slotCount, slotCount), Vec2{}, Vec2{}, Vec2{}, 0.0f, 0u, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f, minPoint, maxPoint, hasBounds);
    AppendPreviewTriangle(outPayload, top, back, right, MaterialColor(swatch, 3 % slotCount, slotCount), Vec2{}, Vec2{}, Vec2{}, 0.0f, 0u, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f, minPoint, maxPoint, hasBounds);
    AppendPreviewTriangle(outPayload, bottom, front, right, MaterialColor(swatch, 0, slotCount), Vec2{}, Vec2{}, Vec2{}, 0.0f, 0u, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f, minPoint, maxPoint, hasBounds);
    AppendPreviewTriangle(outPayload, bottom, left, front, MaterialColor(swatch, 1 % slotCount, slotCount), Vec2{}, Vec2{}, Vec2{}, 0.0f, 0u, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f, minPoint, maxPoint, hasBounds);
    AppendPreviewTriangle(outPayload, bottom, back, left, MaterialColor(swatch, 2 % slotCount, slotCount), Vec2{}, Vec2{}, Vec2{}, 0.0f, 0u, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f, minPoint, maxPoint, hasBounds);
    AppendPreviewTriangle(outPayload, bottom, right, back, MaterialColor(swatch, 3 % slotCount, slotCount), Vec2{}, Vec2{}, Vec2{}, 0.0f, 0u, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f, minPoint, maxPoint, hasBounds);
    outPayload.materialSlotCount = slotCount;
    outMinPoint = minPoint;
    outMaxPoint = maxPoint;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBufferWithData(
    ID3D12Device* device,
    const void* source,
    uint64_t byteSize) {
    if (device == nullptr || source == nullptr || byteSize == 0) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer = CreateUploadBuffer(device, byteSize);
    if (buffer == nullptr) {
        return nullptr;
    }
    uint8_t* mapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    if (FAILED(buffer->Map(0, &readRange, reinterpret_cast<void**>(&mapped))) || mapped == nullptr) {
        return nullptr;
    }
    std::copy(
        static_cast<const uint8_t*>(source),
        static_cast<const uint8_t*>(source) + byteSize,
        mapped);
    buffer->Unmap(0, nullptr);
    return buffer;
}

uint64_t Align256(uint64_t value) {
    return (value + 255ull) & ~255ull;
}

void ClearPreviewRect(
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    uint32_t width,
    uint32_t height,
    LONG left,
    LONG top,
    LONG right,
    LONG bottom,
    const float (&color)[4]) {
    D3D12_RECT rect{};
    rect.left = (std::clamp)(left, 0l, static_cast<LONG>(width));
    rect.top = (std::clamp)(top, 0l, static_cast<LONG>(height));
    rect.right = (std::clamp)(right, rect.left, static_cast<LONG>(width));
    rect.bottom = (std::clamp)(bottom, rect.top, static_cast<LONG>(height));
    if (rect.left >= rect.right || rect.top >= rect.bottom) {
        return;
    }
    commandList->ClearRenderTargetView(rtv, color, 1, &rect);
}

void DrawPreviewSceneProxyGeometry(
    ID3D12GraphicsCommandList* commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    const EditorAssetGpuThumbnailAllocationRequest& request,
    uint32_t width,
    uint32_t height) {
    const LONG centerX = static_cast<LONG>(width / 2u);
    const LONG centerY = static_cast<LONG>(height / 2u);
    const float radius = request.boundsRadius > 0.0f ? request.boundsRadius : 1.0f;
    const LONG extent = (std::clamp)(
        28l + static_cast<LONG>(radius * 6.0f) + static_cast<LONG>((request.faceCount + request.vertexCount) % 13u),
        28l,
        static_cast<LONG>((std::min)(width, height) / 2u - 14u));
    const uint32_t swatch = request.swatchRgba == 0 ? 0xff88a766u : request.swatchRgba;

    float shadow[4]{};
    float left[4]{};
    float right[4]{};
    float top[4]{};
    float edge[4]{};
    FillColor(swatch, 0.28f, shadow);
    FillColor(swatch, 0.72f, left);
    FillColor(swatch, 0.95f, right);
    FillColor(swatch, 1.18f, top);
    FillColor(0xffffffffu, 0.72f, edge);

    ClearPreviewRect(commandList, rtv, width, height, centerX - extent + 5, centerY + extent - 5, centerX + extent + 8, centerY + extent + 2, shadow);
    ClearPreviewRect(commandList, rtv, width, height, centerX - extent, centerY - extent / 2, centerX + 2, centerY + extent, left);
    ClearPreviewRect(commandList, rtv, width, height, centerX - 2, centerY - extent, centerX + extent, centerY + extent / 2, right);
    ClearPreviewRect(commandList, rtv, width, height, centerX - extent / 2, centerY - extent, centerX + extent / 2, centerY - extent + 5, top);
    ClearPreviewRect(commandList, rtv, width, height, centerX - extent, centerY - 2, centerX + extent, centerY + 2, edge);
    ClearPreviewRect(commandList, rtv, width, height, centerX - 2, centerY - extent, centerX + 2, centerY + extent, edge);

    const uint32_t materialSlots = request.hasMaterialBinding
        ? (std::min)(5u, (std::max)(1u, request.materialSlotCount))
        : 0u;
    for (uint32_t slot = 0; slot < materialSlots; ++slot) {
        float materialColor[4]{};
        FillColor(swatch + slot * 0x00112233u, 0.8f + static_cast<float>(slot) * 0.08f, materialColor);
        const LONG x = 18 + static_cast<LONG>(slot) * 18;
        ClearPreviewRect(commandList, rtv, width, height, x, static_cast<LONG>(height) - 18, x + 12, static_cast<LONG>(height) - 8, materialColor);
    }
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateThumbnailTextureResource(
    ID3D12Device* device,
    uint32_t width,
    uint32_t height) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    const HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture));
    return SUCCEEDED(hr) ? texture : nullptr;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(
    ID3D12Device* device,
    uint64_t sizeInBytes) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeInBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    Microsoft::WRL::ComPtr<ID3D12Resource> upload;
    const HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&upload));
    return SUCCEEDED(hr) ? upload : nullptr;
}

bool UploadThumbnailPixels(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* texture,
    const EditorAssetThumbnailPixelData& pixels,
    Microsoft::WRL::ComPtr<ID3D12Resource>& outUpload,
    uint64_t& outUploadBytes,
    std::string& outError) {
    if (device == nullptr || commandList == nullptr || texture == nullptr || pixels.rgba8.empty()) {
        outError = "Texture thumbnail upload inputs are invalid.";
        return false;
    }

    D3D12_RESOURCE_DESC textureDesc = texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(
        &textureDesc,
        0,
        1,
        0,
        &footprint,
        &numRows,
        &rowSizeInBytes,
        &uploadSize);

    outUpload = CreateUploadBuffer(device, uploadSize);
    if (outUpload == nullptr) {
        outError = "Texture thumbnail upload buffer allocation failed.";
        return false;
    }
    outUploadBytes = uploadSize;

    uint8_t* mapped = nullptr;
    D3D12_RANGE readRange{};
    if (FAILED(outUpload->Map(0, &readRange, reinterpret_cast<void**>(&mapped))) || mapped == nullptr) {
        outError = "Texture thumbnail upload buffer map failed.";
        return false;
    }

    const uint8_t* source = pixels.rgba8.data();
    uint8_t* target = mapped + footprint.Offset;
    for (uint32_t y = 0; y < pixels.height; ++y) {
        std::copy(
            source + static_cast<size_t>(pixels.rowPitch) * y,
            source + static_cast<size_t>(pixels.rowPitch) * y + pixels.rowPitch,
            target + static_cast<size_t>(footprint.Footprint.RowPitch) * y);
    }
    outUpload->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = texture;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = outUpload.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;

    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);
    return true;
}

bool BuildThumbnailPixelPayload(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outDetail,
    bool& outFallbackUsed,
    std::string& outError) {
    outFallbackUsed = false;
    if (request.previewKind == EditorAssetPreviewKind::Texture && !request.sourcePath.empty()) {
        std::string decodeError;
        if (LoadEditorAssetTextureThumbnailPixels(request.sourcePath, 96, outPixels, decodeError)) {
            outDetail = "Texture thumbnail pixels decoded and uploaded to GPU SRV.";
            return true;
        }
        if (!BuildEditorAssetFallbackIconPixels(
                request.kind,
                request.previewKind,
                request.swatchRgba,
                outPixels)) {
            outError = decodeError.empty() ? "Texture thumbnail decode failed." : decodeError;
            return false;
        }
        outFallbackUsed = true;
        outDetail = "Texture decode failed; fallback icon atlas uploaded to GPU SRV.";
        return true;
    }

    if (request.previewKind == EditorAssetPreviewKind::Mesh || request.kind == EditorAssetKind::Mesh) {
        if (!RenderEditorAssetPreviewScenePass(request, outPixels, outDetail, outError)) {
            return false;
        }
        return true;
    }

    if (!BuildEditorAssetFallbackIconPixels(
            request.kind,
            request.previewKind,
            request.swatchRgba,
            outPixels)) {
        outError = "Fallback icon atlas generation failed.";
        return false;
    }
    outFallbackUsed = true;
    outDetail = "Fallback icon atlas tile uploaded to GPU SRV.";
    return true;
}

} // namespace

bool EditorAssetD3D12ThumbnailGpuBackend::Initialize(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvHeap,
    uint32_t descriptorSize,
    uint32_t firstDescriptorIndex,
    uint32_t descriptorCapacity) {
    if (device == nullptr || srvHeap == nullptr || descriptorSize == 0 || descriptorCapacity == 0) {
        return false;
    }
    device_ = device;
    srvHeap_ = srvHeap;
    descriptorSize_ = descriptorSize;
    firstDescriptorIndex_ = firstDescriptorIndex;
    descriptorCapacity_ = descriptorCapacity;
    nextLocalIndex_ = 0;
    allocatedCount_ = 0;
    freeDescriptorIndices_.clear();
    keyToDescriptorIndex_.clear();
    keyToTextureResource_.clear();
    keyToPreviewRtvIndex_.clear();
    keyToPreviewMaterialDescriptorIndex_.clear();
    keyToPreviewMaterialDescriptorCount_.clear();
    keyToPreviewMaterialTextureResources_.clear();
    previewMaterialTexturePixelCache_.clear();
    previewRenderTargets_.Initialize(device_, descriptorCapacity_);
    uploadRetirementQueue_.Clear();
    telemetry_ = {};
    telemetry_.descriptorCapacity = descriptorCapacity_;
    return true;
}

void EditorAssetD3D12ThumbnailGpuBackend::SetUploadCommandList(
    ID3D12GraphicsCommandList* commandList) {
    uploadCommandList_ = commandList;
}

void EditorAssetD3D12ThumbnailGpuBackend::SetFrameFenceValues(
    uint64_t completedFenceValue,
    uint64_t scheduledFenceValue) {
    completedFenceValue_ = completedFenceValue;
    scheduledFenceValue_ = scheduledFenceValue;
    RetireCompletedUploads(completedFenceValue_);
}

void EditorAssetD3D12ThumbnailGpuBackend::ConfigureCache(
    EditorAssetThumbnailCachePolicy policy) {
    cacheStore_.Configure(std::move(policy));
}

uint32_t EditorAssetD3D12ThumbnailGpuBackend::RetireCompletedUploads(
    uint64_t completedFenceValue) {
    const uint32_t retired = uploadRetirementQueue_.RetireCompleted(completedFenceValue);
    const EditorThumbnailUploadRetirementTelemetry& uploadTelemetry =
        uploadRetirementQueue_.Telemetry();
    telemetry_.pendingUploadBytes = uploadTelemetry.pendingBytes;
    telemetry_.retiredUploadBytes = uploadTelemetry.retiredBytes;
    telemetry_.pendingUploadCount = uploadTelemetry.pendingCount;
    telemetry_.retainedUploadResources = uploadTelemetry.pendingCount;
    return retired;
}

bool EditorAssetD3D12ThumbnailGpuBackend::AllocateThumbnail(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    EditorAssetGpuThumbnailAllocation& outAllocation,
    std::string& outError) {
    ++telemetry_.allocationRequests;
    if (device_ == nullptr || srvHeap_ == nullptr || request.key.empty()) {
        outError = "GPU thumbnail backend is not initialized.";
        return false;
    }

    ReleaseThumbnail(request.key, 0);

    const uint32_t descriptorIndex = AllocateDescriptorIndex();
    if (descriptorIndex == UINT32_MAX) {
        outError = "GPU thumbnail SRV descriptor range is exhausted.";
        return false;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = CpuHandle(descriptorIndex);
    const D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = GpuHandle(descriptorIndex);

    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
    std::string detail = "GPU thumbnail fallback SRV descriptor is allocated.";
    uint32_t textureWidth = request.width;
    uint32_t textureHeight = request.height;
    bool directPreviewScenePublished = false;

    if (SupportsDirectPreviewSceneSvr(request)) {
        std::string previewSceneError;
        directPreviewScenePublished = TryPublishPreviewSceneRenderTarget(
            request,
            descriptorIndex,
            cpuHandle,
            textureResource,
            textureWidth,
            textureHeight,
            detail,
            previewSceneError);
        if (!directPreviewScenePublished) {
            ++telemetry_.previewSceneFallback;
            if (!previewSceneError.empty()) {
                detail = "Preview scene direct SRV fallback: " + previewSceneError;
            }
        }
    }

    if (!directPreviewScenePublished) {
        EditorAssetThumbnailPixelData pixels;
        std::string payloadDetail;
        std::string cacheDetail;
        bool fallbackUsed = false;
        const EditorAssetThumbnailCacheKey cacheKey =
            BuildEditorAssetThumbnailCacheKey(request, cacheStore_.Policy().previewVersion);
        if (cacheStore_.TryLoad(cacheKey, pixels, cacheDetail)) {
            payloadDetail = cacheDetail;
        } else if (BuildThumbnailPixelPayload(request, pixels, payloadDetail, fallbackUsed, outError)) {
            std::string storeDetail;
            cacheStore_.Store(cacheKey, pixels, fallbackUsed, storeDetail);
            if (fallbackUsed) {
                ++telemetry_.fallbackUploads;
            }
        }

        const EditorAssetThumbnailCacheTelemetry& cacheTelemetry = cacheStore_.Telemetry();
        telemetry_.cacheHits = cacheTelemetry.hits;
        telemetry_.cacheMisses = cacheTelemetry.misses;
        telemetry_.cacheStores = cacheTelemetry.stores;
        telemetry_.cacheEvictions = cacheTelemetry.evictions;

        if (!pixels.rgba8.empty()) {
            if (uploadCommandList_ == nullptr) {
                FreeDescriptorIndex(descriptorIndex);
                outError = "Thumbnail upload command list is not available.";
                return false;
            }

            textureResource = CreateThumbnailTextureResource(device_, pixels.width, pixels.height);
            if (textureResource == nullptr) {
                FreeDescriptorIndex(descriptorIndex);
                outError = "Thumbnail GPU resource allocation failed.";
                return false;
            }

            Microsoft::WRL::ComPtr<ID3D12Resource> uploadResource;
            uint64_t uploadBytes = 0;
            if (!UploadThumbnailPixels(
                    device_,
                    uploadCommandList_,
                    textureResource.Get(),
                    pixels,
                    uploadResource,
                    uploadBytes,
                    outError)) {
                FreeDescriptorIndex(descriptorIndex);
                return false;
            }
            RetainUploadResource(uploadResource, uploadBytes);
            textureWidth = pixels.width;
            textureHeight = pixels.height;
            telemetry_.uploadBytes += uploadBytes;
            detail = payloadDetail;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        device_->CreateShaderResourceView(textureResource.Get(), &srvDesc, cpuHandle);
    }

    keyToDescriptorIndex_[request.key] = descriptorIndex;
    if (textureResource != nullptr) {
        keyToTextureResource_[request.key] = textureResource;
    }
    ++allocatedCount_;
    ++telemetry_.allocated;
    telemetry_.residentCount = allocatedCount_;
    telemetry_.retainedUploadResources = uploadRetirementQueue_.PendingCount();
    telemetry_.pendingUploadBytes = uploadRetirementQueue_.PendingBytes();

    outAllocation.resourceId = gpuHandle.ptr;
    outAllocation.displayTextureId = gpuHandle.ptr;
    outAllocation.descriptorIndex = descriptorIndex;
    outAllocation.width = textureWidth;
    outAllocation.height = textureHeight;
    outAllocation.shaderResourceView = true;
    outAllocation.detail = detail + " (" + std::to_string(textureWidth) + "x" +
        std::to_string(textureHeight) + ")";
    return true;
}

void EditorAssetD3D12ThumbnailGpuBackend::ReleaseThumbnail(
    std::string_view key,
    uint64_t resourceId) {
    (void)resourceId;
    if (key.empty()) {
        return;
    }
    const auto it = keyToDescriptorIndex_.find(std::string(key));
    if (it == keyToDescriptorIndex_.end()) {
        return;
    }
    FreeDescriptorIndex(it->second);
    keyToDescriptorIndex_.erase(it);
    keyToTextureResource_.erase(std::string(key));
    const auto rtvIt = keyToPreviewRtvIndex_.find(std::string(key));
    if (rtvIt != keyToPreviewRtvIndex_.end()) {
        previewRenderTargets_.Release(rtvIt->second);
        keyToPreviewRtvIndex_.erase(rtvIt);
    }
    const auto materialDescriptorIt = keyToPreviewMaterialDescriptorIndex_.find(std::string(key));
    if (materialDescriptorIt != keyToPreviewMaterialDescriptorIndex_.end()) {
        const auto countIt = keyToPreviewMaterialDescriptorCount_.find(std::string(key));
        const uint32_t descriptorCount = countIt != keyToPreviewMaterialDescriptorCount_.end()
            ? countIt->second
            : 1u;
        FreeDescriptorRange(materialDescriptorIt->second, descriptorCount);
        keyToPreviewMaterialDescriptorIndex_.erase(materialDescriptorIt);
        if (countIt != keyToPreviewMaterialDescriptorCount_.end()) {
            keyToPreviewMaterialDescriptorCount_.erase(countIt);
        }
    }
    keyToPreviewMaterialTextureResources_.erase(std::string(key));
    telemetry_.previewSceneMaterialTextureSrvDescriptors = ActivePreviewMaterialDescriptorCount();
    if (allocatedCount_ > 0) {
        --allocatedCount_;
    }
    ++telemetry_.released;
    telemetry_.residentCount = allocatedCount_;
}

EditorAssetGpuThumbnailBackendTelemetry EditorAssetD3D12ThumbnailGpuBackend::Telemetry() const {
    EditorAssetGpuThumbnailBackendTelemetry telemetry = telemetry_;
    telemetry.residentCount = allocatedCount_;
    telemetry.descriptorCapacity = descriptorCapacity_;
    const EditorThumbnailUploadRetirementTelemetry& uploadTelemetry =
        uploadRetirementQueue_.Telemetry();
    telemetry.retainedUploadResources = uploadTelemetry.pendingCount;
    telemetry.pendingUploadBytes = uploadTelemetry.pendingBytes;
    telemetry.retiredUploadBytes = uploadTelemetry.retiredBytes;
    telemetry.pendingUploadCount = uploadTelemetry.pendingCount;
    telemetry.previewSceneMaterialTextureSrvDescriptors = ActivePreviewMaterialDescriptorCount();
    return telemetry;
}

uint32_t EditorAssetD3D12ThumbnailGpuBackend::AllocateDescriptorIndex() {
    if (!freeDescriptorIndices_.empty()) {
        const uint32_t descriptorIndex = freeDescriptorIndices_.back();
        freeDescriptorIndices_.pop_back();
        return descriptorIndex;
    }
    if (nextLocalIndex_ >= descriptorCapacity_) {
        return UINT32_MAX;
    }
    return firstDescriptorIndex_ + nextLocalIndex_++;
}

uint32_t EditorAssetD3D12ThumbnailGpuBackend::AllocateDescriptorRange(uint32_t count) {
    if (count == 0 || count > descriptorCapacity_) {
        return UINT32_MAX;
    }
    if (count == 1) {
        return AllocateDescriptorIndex();
    }

    std::sort(freeDescriptorIndices_.begin(), freeDescriptorIndices_.end());
    for (size_t begin = 0; begin + count <= freeDescriptorIndices_.size(); ++begin) {
        const uint32_t first = freeDescriptorIndices_[begin];
        bool contiguous = true;
        for (uint32_t i = 1; i < count; ++i) {
            if (freeDescriptorIndices_[begin + i] != first + i) {
                contiguous = false;
                break;
            }
        }
        if (!contiguous) {
            continue;
        }
        freeDescriptorIndices_.erase(
            freeDescriptorIndices_.begin() + static_cast<std::ptrdiff_t>(begin),
            freeDescriptorIndices_.begin() + static_cast<std::ptrdiff_t>(begin + count));
        return first;
    }

    if (nextLocalIndex_ + count > descriptorCapacity_) {
        return UINT32_MAX;
    }
    const uint32_t first = firstDescriptorIndex_ + nextLocalIndex_;
    nextLocalIndex_ += count;
    return first;
}

void EditorAssetD3D12ThumbnailGpuBackend::FreeDescriptorIndex(uint32_t descriptorIndex) {
    if (descriptorIndex < firstDescriptorIndex_ ||
        descriptorIndex >= firstDescriptorIndex_ + descriptorCapacity_) {
        return;
    }
    freeDescriptorIndices_.push_back(descriptorIndex);
}

void EditorAssetD3D12ThumbnailGpuBackend::FreeDescriptorRange(
    uint32_t firstDescriptorIndex,
    uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        FreeDescriptorIndex(firstDescriptorIndex + i);
    }
}

uint32_t EditorAssetD3D12ThumbnailGpuBackend::ActivePreviewMaterialDescriptorCount() const {
    uint32_t count = 0;
    for (const auto& [key, descriptorCount] : keyToPreviewMaterialDescriptorCount_) {
        (void)key;
        count += descriptorCount;
    }
    return count;
}

void EditorAssetD3D12ThumbnailGpuBackend::RetainUploadResource(
    Microsoft::WRL::ComPtr<ID3D12Resource> resource,
    uint64_t byteSize) {
    if (resource == nullptr) {
        return;
    }
    const uint64_t retireFenceValue = scheduledFenceValue_ != 0
        ? scheduledFenceValue_
        : completedFenceValue_ + 1;
    uploadRetirementQueue_.Enqueue(std::move(resource), byteSize, retireFenceValue);
    const EditorThumbnailUploadRetirementTelemetry& uploadTelemetry =
        uploadRetirementQueue_.Telemetry();
    telemetry_.retainedUploadResources = uploadTelemetry.pendingCount;
    telemetry_.pendingUploadBytes = uploadTelemetry.pendingBytes;
    telemetry_.pendingUploadCount = uploadTelemetry.pendingCount;
}

bool EditorAssetD3D12ThumbnailGpuBackend::TryCreatePreviewMaterialTextureTable(
    const std::string& key,
    const std::vector<std::string>& texturePaths,
    PreviewMaterialTextureTable& outTable,
    std::string& outError) {
    outTable = {};
    if (device_ == nullptr || uploadCommandList_ == nullptr || srvHeap_ == nullptr || key.empty() || texturePaths.empty()) {
        outError = "Preview material texture SRV table inputs are invalid.";
        return false;
    }

    const uint32_t descriptorCount = kMaxPreviewMaterialTextures;
    const uint32_t baseDescriptorIndex = AllocateDescriptorRange(descriptorCount);
    if (baseDescriptorIndex == UINT32_MAX) {
        outError = "Preview material texture SRV descriptor range is exhausted.";
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    outTable.baseDescriptorIndex = baseDescriptorIndex;
    outTable.descriptorCount = descriptorCount;
    outTable.gpuHandle = GpuHandle(baseDescriptorIndex);
    outTable.boundSlots.assign(descriptorCount, false);
    outTable.textureResources.reserve(descriptorCount);

    for (uint32_t i = 0; i < descriptorCount; ++i) {
        device_->CreateShaderResourceView(nullptr, &srvDesc, CpuHandle(baseDescriptorIndex + i));
    }

    std::string firstFailure;
    const uint32_t inputCount = (std::min)(descriptorCount, static_cast<uint32_t>(texturePaths.size()));
    for (uint32_t i = 0; i < inputCount; ++i) {
        const std::string& texturePath = texturePaths[i];
        if (texturePath.empty()) {
            continue;
        }

        EditorAssetThumbnailPixelData pixels;
        std::string decodeError;
        const auto cacheIt = previewMaterialTexturePixelCache_.find(texturePath);
        if (cacheIt != previewMaterialTexturePixelCache_.end()) {
            pixels = cacheIt->second;
            ++telemetry_.previewSceneProductionMaterialCacheHits;
        } else {
            ++telemetry_.previewSceneProductionMaterialCacheMisses;
            if (!LoadEditorAssetTextureThumbnailPixels(texturePath, 256, pixels, decodeError) ||
                pixels.rgba8.empty()) {
                ++outTable.fallbackCount;
                if (firstFailure.empty()) {
                    firstFailure = decodeError.empty()
                        ? "Preview material texture decode failed."
                        : decodeError;
                }
                continue;
            }
            if (previewMaterialTexturePixelCache_.size() >= kMaxPreviewMaterialTexturePixelCacheEntries) {
                previewMaterialTexturePixelCache_.erase(previewMaterialTexturePixelCache_.begin());
            }
            previewMaterialTexturePixelCache_.emplace(texturePath, pixels);
        }
        if (pixels.rgba8.empty()) {
            ++outTable.fallbackCount;
            if (firstFailure.empty()) {
                firstFailure = "Preview material texture cache entry was empty.";
            }
            continue;
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> textureResource =
            CreateThumbnailTextureResource(device_, pixels.width, pixels.height);
        if (textureResource == nullptr) {
            ++outTable.fallbackCount;
            if (firstFailure.empty()) {
                firstFailure = "Preview material texture resource allocation failed.";
            }
            continue;
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> uploadResource;
        uint64_t uploadBytes = 0;
        std::string uploadError;
        if (!UploadThumbnailPixels(
                device_,
                uploadCommandList_,
                textureResource.Get(),
                pixels,
                uploadResource,
                uploadBytes,
                uploadError)) {
            ++outTable.fallbackCount;
            if (firstFailure.empty()) {
                firstFailure = uploadError.empty()
                    ? "Preview material texture upload failed."
                    : uploadError;
            }
            continue;
        }

        device_->CreateShaderResourceView(textureResource.Get(), &srvDesc, CpuHandle(baseDescriptorIndex + i));
        RetainUploadResource(uploadResource, uploadBytes);
        telemetry_.uploadBytes += uploadBytes;
        outTable.boundSlots[i] = true;
        ++outTable.boundCount;
        outTable.textureResources.push_back(textureResource);
    }

    if (outTable.boundCount == 0) {
        FreeDescriptorRange(baseDescriptorIndex, descriptorCount);
        outTable = {};
        outError = firstFailure.empty()
            ? "Preview material texture SRV table produced no bound textures."
            : firstFailure;
        return false;
    }

    keyToPreviewMaterialDescriptorIndex_[key] = baseDescriptorIndex;
    keyToPreviewMaterialDescriptorCount_[key] = descriptorCount;
    keyToPreviewMaterialTextureResources_[key] = outTable.textureResources;
    telemetry_.previewSceneMaterialTextureSrvDescriptors = ActivePreviewMaterialDescriptorCount();
    if (outTable.fallbackCount > 0 && !firstFailure.empty()) {
        outError = firstFailure;
    }
    return true;
}

bool EditorAssetD3D12ThumbnailGpuBackend::EnsurePreviewMeshPipeline(std::string& outError) {
    if (previewMeshRootSignature_ != nullptr && previewMeshPipelineState_ != nullptr) {
        return true;
    }
    if (device_ == nullptr) {
        outError = "Preview mesh pipeline cannot be created without a D3D12 device.";
        return false;
    }

    D3D12_DESCRIPTOR_RANGE materialTextureRange{};
    materialTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    materialTextureRange.NumDescriptors = kMaxPreviewMaterialTextures;
    materialTextureRange.BaseShaderRegister = 0;
    materialTextureRange.RegisterSpace = 0;
    materialTextureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &materialTextureRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC materialSampler{};
    materialSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    materialSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    materialSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    materialSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    materialSampler.MipLODBias = 0.0f;
    materialSampler.MaxAnisotropy = 1;
    materialSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    materialSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    materialSampler.MinLOD = 0.0f;
    materialSampler.MaxLOD = D3D12_FLOAT32_MAX;
    materialSampler.ShaderRegister = 0;
    materialSampler.RegisterSpace = 0;
    materialSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &materialSampler;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(hr) || signatureBlob == nullptr) {
        outError = "Preview mesh root signature serialization failed.";
        return false;
    }
    hr = device_->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&previewMeshRootSignature_));
    if (FAILED(hr) || previewMeshRootSignature_ == nullptr) {
        outError = "Preview mesh root signature creation failed.";
        return false;
    }

    if (!previewShaderCompiler_.Initialize()) {
        outError = "Preview mesh shader compiler initialization failed.";
        previewMeshRootSignature_.Reset();
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShader =
        previewShaderCompiler_.CompileFromFile(
            L"Resources/EditorAssetThumbnailPreview.VS.hlsl",
            L"VSMain",
            L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShader =
        previewShaderCompiler_.CompileFromFile(
            L"Resources/EditorAssetThumbnailPreview.PS.hlsl",
            L"PSMain",
            L"ps_6_0");
    if (vertexShader == nullptr || pixelShader == nullptr) {
        outError = "Preview mesh shader compilation failed.";
        previewMeshRootSignature_.Reset();
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXWEIGHT", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXINDEX", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"ROUGHNESS", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"METALLIC", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMALTEXWEIGHT", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"ROUGHTEXWEIGHT", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"METALTEXWEIGHT", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterDesc.DepthClipEnable = TRUE;

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    depthDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = previewMeshRootSignature_.Get();
    psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.InputLayout = {inputElements, _countof(inputElements)};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&previewMeshPipelineState_));
    if (FAILED(hr) || previewMeshPipelineState_ == nullptr) {
        outError = "Preview mesh pipeline state creation failed.";
        previewMeshRootSignature_.Reset();
        return false;
    }
    return true;
}

bool EditorAssetD3D12ThumbnailGpuBackend::TryDrawRendererBackedMeshPreview(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    D3D12_GPU_DESCRIPTOR_HANDLE fallbackSrvHandle,
    uint32_t width,
    uint32_t height,
    bool& outProceduralFallback,
    std::string& outDetail,
    std::string& outError) {
    outProceduralFallback = false;
    outDetail.clear();
    if (device_ == nullptr || uploadCommandList_ == nullptr) {
        outError = "Preview mesh draw inputs are invalid.";
        return false;
    }
    if (!EnsurePreviewMeshPipeline(outError)) {
        return false;
    }

    PreviewMeshPayload payload;
    Vec3 minPoint{};
    Vec3 maxPoint{};
    bool usedProductionLoader = BuildProductionPreviewMeshPayload(request, payload, minPoint, maxPoint);
    bool usedObjSource = false;
    if (!usedProductionLoader) {
        usedObjSource = BuildObjPreviewMeshPayload(request, payload, minPoint, maxPoint);
    }
    if (!usedProductionLoader && !usedObjSource) {
        BuildProceduralPreviewMeshPayload(request, payload, minPoint, maxPoint);
        outProceduralFallback = true;
    }
    if (payload.vertices.empty() || payload.indices.empty()) {
        outError = "Preview mesh payload is empty.";
        return false;
    }

    PreviewMaterialTextureTable materialTextureTable;
    std::string materialTextureSrvError;
    const uint32_t expectedMaterialTextureCount = CountPreviewMaterialTexturePaths(payload.materialTexturePaths);
    const bool materialTextureSrvBound =
        expectedMaterialTextureCount > 0 &&
        TryCreatePreviewMaterialTextureTable(
            request.key,
            payload.materialTexturePaths,
            materialTextureTable,
            materialTextureSrvError);
    if (materialTextureSrvBound) {
        payload.materialTextureBound = true;
        payload.materialTextureFallback = payload.materialTextureFallback || materialTextureTable.fallbackCount > 0;
        for (PreviewMeshVertex& vertex : payload.vertices) {
            const uint32_t textureIndex = static_cast<uint32_t>((std::max)(0.0f, vertex.textureIndex));
            const uint32_t albedoIndex = PreviewMaterialTextureDescriptorIndex(PreviewMaterialTextureRole::Albedo, textureIndex);
            const uint32_t normalIndex = PreviewMaterialTextureDescriptorIndex(PreviewMaterialTextureRole::Normal, textureIndex);
            const uint32_t roughnessIndex = PreviewMaterialTextureDescriptorIndex(PreviewMaterialTextureRole::Roughness, textureIndex);
            const uint32_t metallicIndex = PreviewMaterialTextureDescriptorIndex(PreviewMaterialTextureRole::Metallic, textureIndex);
            if (albedoIndex == UINT32_MAX ||
                albedoIndex >= materialTextureTable.boundSlots.size() ||
                !materialTextureTable.boundSlots[albedoIndex]) {
                vertex.textureWeight = 0.0f;
            }
            if (normalIndex == UINT32_MAX ||
                normalIndex >= materialTextureTable.boundSlots.size() ||
                !materialTextureTable.boundSlots[normalIndex]) {
                vertex.normalTextureWeight = 0.0f;
            }
            if (roughnessIndex == UINT32_MAX ||
                roughnessIndex >= materialTextureTable.boundSlots.size() ||
                !materialTextureTable.boundSlots[roughnessIndex]) {
                vertex.roughnessTextureWeight = 0.0f;
            }
            if (metallicIndex == UINT32_MAX ||
                metallicIndex >= materialTextureTable.boundSlots.size() ||
                !materialTextureTable.boundSlots[metallicIndex]) {
                vertex.metallicTextureWeight = 0.0f;
            }
        }
    } else if (expectedMaterialTextureCount > 0) {
        payload.materialTextureFallback = true;
        for (PreviewMeshVertex& vertex : payload.vertices) {
            vertex.textureWeight = 0.0f;
            vertex.normalTextureWeight = 0.0f;
            vertex.roughnessTextureWeight = 0.0f;
            vertex.metallicTextureWeight = 0.0f;
        }
    }
    const auto cleanupMaterialTextureSrv = [&]() {
        if (materialTextureTable.baseDescriptorIndex != UINT32_MAX) {
            FreeDescriptorRange(materialTextureTable.baseDescriptorIndex, materialTextureTable.descriptorCount);
            keyToPreviewMaterialDescriptorIndex_.erase(request.key);
            keyToPreviewMaterialDescriptorCount_.erase(request.key);
            keyToPreviewMaterialTextureResources_.erase(request.key);
            for (Microsoft::WRL::ComPtr<ID3D12Resource>& textureResource : materialTextureTable.textureResources) {
                if (textureResource != nullptr) {
                    RetainUploadResource(textureResource, 0);
                }
            }
            materialTextureTable = {};
            telemetry_.previewSceneMaterialTextureSrvDescriptors = ActivePreviewMaterialDescriptorCount();
        }
    };

    const uint64_t vertexBytes = static_cast<uint64_t>(payload.vertices.size() * sizeof(PreviewMeshVertex));
    const uint64_t indexBytes = static_cast<uint64_t>(payload.indices.size() * sizeof(uint32_t));
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer =
        CreateUploadBufferWithData(device_, payload.vertices.data(), vertexBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer =
        CreateUploadBufferWithData(device_, payload.indices.data(), indexBytes);
    if (vertexBuffer == nullptr || indexBuffer == nullptr) {
        cleanupMaterialTextureSrv();
        outError = "Preview mesh vertex/index buffer allocation failed.";
        return false;
    }

    PreviewMeshConstants constants{};
    constants.center[0] = (minPoint.x + maxPoint.x) * 0.5f;
    constants.center[1] = (minPoint.y + maxPoint.y) * 0.5f;
    constants.center[2] = (minPoint.z + maxPoint.z) * 0.5f;
    const float dx = maxPoint.x - minPoint.x;
    const float dy = maxPoint.y - minPoint.y;
    const float dz = maxPoint.z - minPoint.z;
    const float radius = (std::max)(0.01f, std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f);
    constants.invRadius = 1.0f / radius;
    constants.lightDirection[0] = request.previewLightDirection[0];
    constants.lightDirection[1] = request.previewLightDirection[1];
    constants.lightDirection[2] = request.previewLightDirection[2];
    constants.materialSlots = static_cast<float>((std::max)(1u, (std::max)(request.materialSlotCount, payload.materialSlotCount)));
    std::array<float, 3> baseColor =
        MaterialColor(request.swatchRgba == 0 ? 0xff88a766u : request.swatchRgba, 0, static_cast<uint32_t>(constants.materialSlots));
    if (!payload.vertices.empty()) {
        double r = 0.0;
        double g = 0.0;
        double b = 0.0;
        double roughness = 0.0;
        double metallic = 0.0;
        for (const PreviewMeshVertex& vertex : payload.vertices) {
            r += vertex.color[0];
            g += vertex.color[1];
            b += vertex.color[2];
            roughness += vertex.roughness;
            metallic += vertex.metallic;
        }
        const double inv = 1.0 / static_cast<double>(payload.vertices.size());
        baseColor = {
            static_cast<float>(r * inv),
            static_cast<float>(g * inv),
            static_cast<float>(b * inv)};
        constants.averageRoughness = static_cast<float>(roughness * inv);
        constants.averageMetallic = static_cast<float>(metallic * inv);
    }
    constants.baseColor[0] = baseColor[0];
    constants.baseColor[1] = baseColor[1];
    constants.baseColor[2] = baseColor[2];
    constants.cameraDistance = request.previewCameraDistance > 0.0f
        ? request.previewCameraDistance
        : radius * 2.8f;
    constants.textureCount = materialTextureSrvBound
        ? static_cast<float>(materialTextureTable.boundCount)
        : 0.0f;
    constants.pbrStrength = usedProductionLoader ? 1.0f : 0.35f;

    const uint64_t constantBytes = Align256(sizeof(PreviewMeshConstants));
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer = CreateUploadBuffer(device_, constantBytes);
    if (constantBuffer == nullptr) {
        cleanupMaterialTextureSrv();
        outError = "Preview mesh constant buffer allocation failed.";
        return false;
    }
    uint8_t* constantMapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    if (FAILED(constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&constantMapped))) ||
        constantMapped == nullptr) {
        cleanupMaterialTextureSrv();
        outError = "Preview mesh constant buffer map failed.";
        return false;
    }
    std::fill(constantMapped, constantMapped + constantBytes, uint8_t{0});
    std::copy(
        reinterpret_cast<const uint8_t*>(&constants),
        reinterpret_cast<const uint8_t*>(&constants) + sizeof(constants),
        constantMapped);
    constantBuffer->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vbv.SizeInBytes = static_cast<UINT>(vertexBytes);
    vbv.StrideInBytes = sizeof(PreviewMeshVertex);

    D3D12_INDEX_BUFFER_VIEW ibv{};
    ibv.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    ibv.SizeInBytes = static_cast<UINT>(indexBytes);
    ibv.Format = DXGI_FORMAT_R32_UINT;

    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = static_cast<LONG>(width);
    scissor.bottom = static_cast<LONG>(height);

    uploadCommandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    uploadCommandList_->RSSetViewports(1, &viewport);
    uploadCommandList_->RSSetScissorRects(1, &scissor);
    ID3D12DescriptorHeap* descriptorHeaps[] = {srvHeap_};
    uploadCommandList_->SetDescriptorHeaps(1, descriptorHeaps);
    uploadCommandList_->SetGraphicsRootSignature(previewMeshRootSignature_.Get());
    uploadCommandList_->SetPipelineState(previewMeshPipelineState_.Get());
    uploadCommandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    uploadCommandList_->IASetVertexBuffers(0, 1, &vbv);
    uploadCommandList_->IASetIndexBuffer(&ibv);
    uploadCommandList_->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
    uploadCommandList_->SetGraphicsRootDescriptorTable(
        1,
        materialTextureSrvBound ? materialTextureTable.gpuHandle : fallbackSrvHandle);
    uploadCommandList_->DrawIndexedInstanced(static_cast<UINT>(payload.indices.size()), 1, 0, 0, 0);

    RetainUploadResource(vertexBuffer, vertexBytes);
    RetainUploadResource(indexBuffer, indexBytes);
    RetainUploadResource(constantBuffer, constantBytes);

    if (usedProductionLoader) {
        outDetail = "renderer-backed production mesh loader draw";
    } else if (usedObjSource) {
        outDetail = "renderer-backed legacy OBJ mesh draw";
    } else {
        outDetail = "renderer-backed procedural mesh fallback draw";
    }
    outDetail += " (" + std::to_string(payload.vertices.size()) + " vertices, " +
        std::to_string(payload.indices.size() / 3u) + " triangles)";
    if (payload.materialSlotCount > 0) {
        outDetail += ", material slots " + std::to_string(payload.materialSlotCount);
    }
    if (payload.materialTextureBound) {
        outDetail += ", material texture SRV table bound " +
            std::to_string(materialTextureTable.boundCount) + "/" +
            std::to_string(expectedMaterialTextureCount);
        const auto countBoundRole = [&](PreviewMaterialTextureRole role) {
            const uint32_t baseIndex = static_cast<uint32_t>(role) * kMaxPreviewMaterialSlots;
            uint32_t count = 0;
            for (uint32_t i = 0; i < kMaxPreviewMaterialSlots && baseIndex + i < materialTextureTable.boundSlots.size(); ++i) {
                if (materialTextureTable.boundSlots[baseIndex + i]) {
                    ++count;
                }
            }
            return count;
        };
        const uint32_t boundNormalMaps = countBoundRole(PreviewMaterialTextureRole::Normal);
        const uint32_t boundRoughnessMaps = countBoundRole(PreviewMaterialTextureRole::Roughness);
        const uint32_t boundMetallicMaps = countBoundRole(PreviewMaterialTextureRole::Metallic);
        if (boundNormalMaps > 0) {
            outDetail += ", normal map bound " + std::to_string(boundNormalMaps);
        }
        if (boundRoughnessMaps > 0) {
            outDetail += ", roughness map bound " + std::to_string(boundRoughnessMaps);
        }
        if (boundMetallicMaps > 0) {
            outDetail += ", metallic map bound " + std::to_string(boundMetallicMaps);
        }
        if (materialTextureTable.fallbackCount > 0) {
            outDetail += ", material texture SRV partial fallback " +
                std::to_string(materialTextureTable.fallbackCount);
        }
    } else if (payload.materialTextureFallback || payload.materialTextureCount > 0) {
        outDetail += ", material texture SRV fallback";
        if (!materialTextureSrvError.empty()) {
            outDetail += " (" + materialTextureSrvError + ")";
        }
    }
    if (usedProductionLoader && payload.materialSlotCount > 0) {
        outDetail += ", PBR preview roughness " + std::to_string(constants.averageRoughness) +
            " metallic " + std::to_string(constants.averageMetallic);
    }
    return true;
}

bool EditorAssetD3D12ThumbnailGpuBackend::TryPublishPreviewSceneRenderTarget(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    uint32_t descriptorIndex,
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle,
    Microsoft::WRL::ComPtr<ID3D12Resource>& outResource,
    uint32_t& outWidth,
    uint32_t& outHeight,
    std::string& outDetail,
    std::string& outError) {
    if (!SupportsDirectPreviewSceneSvr(request)) {
        return false;
    }
    if (uploadCommandList_ == nullptr) {
        outError = "Preview scene command list is not available.";
        return false;
    }

    const uint32_t extent = (std::max)(128u, (std::min)(256u, (std::max)(request.width, request.height)));
    EditorAssetPreviewRenderTargetAllocation allocation{};
    if (!previewRenderTargets_.Allocate(extent, extent, allocation, outError)) {
        return false;
    }

    if (allocation.initialStateShaderResource) {
        D3D12_RESOURCE_BARRIER toRenderTarget{};
        toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toRenderTarget.Transition.pResource = allocation.resource.Get();
        toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        uploadCommandList_->ResourceBarrier(1, &toRenderTarget);
    }

    float clearColor[4]{};
    FillPreviewClearColor(request.swatchRgba, clearColor);
    uploadCommandList_->ClearRenderTargetView(allocation.rtvCpuHandle, clearColor, 0, nullptr);
    bool rendererProceduralFallback = false;
    std::string rendererDrawDetail;
    std::string rendererDrawError;
    const bool rendererBackedDraw = TryDrawRendererBackedMeshPreview(
        request,
        allocation.rtvCpuHandle,
        GpuHandle(descriptorIndex),
        allocation.width,
        allocation.height,
        rendererProceduralFallback,
        rendererDrawDetail,
        rendererDrawError);
    if (rendererBackedDraw) {
        ++telemetry_.previewSceneRendererDraws;
        if (rendererDrawDetail.find("production mesh loader") != std::string::npos) {
            ++telemetry_.previewSceneProductionMeshDraws;
        }
        if (rendererDrawDetail.find("material texture SRV table bound") != std::string::npos) {
            ++telemetry_.previewSceneMaterialTextureBound;
            ++telemetry_.previewSceneMaterialTextureSrvBound;
            ++telemetry_.previewSceneMaterialTextureTables;
        }
        if (rendererDrawDetail.find("PBR preview") != std::string::npos) {
            ++telemetry_.previewSceneMaterialPbrPreviews;
        }
        if (rendererDrawDetail.find("normal map bound") != std::string::npos) {
            ++telemetry_.previewSceneMaterialNormalMapBound;
        }
        if (rendererDrawDetail.find("roughness map bound") != std::string::npos) {
            ++telemetry_.previewSceneMaterialRoughnessMapBound;
        }
        if (rendererDrawDetail.find("metallic map bound") != std::string::npos) {
            ++telemetry_.previewSceneMaterialMetallicMapBound;
        }
        if (rendererDrawDetail.find("material texture SRV fallback") != std::string::npos ||
            rendererDrawDetail.find("material texture SRV partial fallback") != std::string::npos) {
            ++telemetry_.previewSceneMaterialTextureFallback;
            ++telemetry_.previewSceneMaterialTextureSrvFallback;
        }
        if (rendererProceduralFallback) {
            ++telemetry_.previewSceneProceduralFallback;
        }
    } else {
        ++telemetry_.previewSceneProxyGeometry;
        DrawPreviewSceneProxyGeometry(
            uploadCommandList_,
            allocation.rtvCpuHandle,
            request,
            allocation.width,
            allocation.height);
    }

    D3D12_RESOURCE_BARRIER toShaderResource{};
    toShaderResource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toShaderResource.Transition.pResource = allocation.resource.Get();
    toShaderResource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toShaderResource.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toShaderResource.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    uploadCommandList_->ResourceBarrier(1, &toShaderResource);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    device_->CreateShaderResourceView(allocation.resource.Get(), &srvDesc, srvCpuHandle);

    keyToPreviewRtvIndex_[request.key] = allocation.rtvIndex;
    outResource = allocation.resource;
    outWidth = allocation.width;
    outHeight = allocation.height;
    outDetail = "Preview scene render target published directly as GPU SRV";
    if (rendererBackedDraw) {
        outDetail += " with " + rendererDrawDetail;
    } else if (!rendererDrawError.empty()) {
        outDetail += " with clear-rect proxy fallback after renderer draw failed: " + rendererDrawError;
    } else {
        outDetail += " with clear-rect proxy fallback";
    }
    if (request.hasPreviewGeometry) {
        outDetail += ", real mesh bounds";
    } else {
        outDetail += ", deterministic mesh proxy bounds";
    }
    if (request.hasMaterialBinding || request.materialSlotCount > 0) {
        outDetail += ", material slots " + std::to_string((std::max)(1u, request.materialSlotCount));
        ++telemetry_.previewSceneMaterialBound;
    }
    if (request.previewCameraDistance > 0.0f) {
        outDetail += ", camera " + std::to_string(request.previewCameraDistance);
    }
    outDetail += ", light (" +
        std::to_string(request.previewLightDirection[0]) + "," +
        std::to_string(request.previewLightDirection[1]) + "," +
        std::to_string(request.previewLightDirection[2]) + ").";
    ++telemetry_.previewSceneRendered;
    if (allocation.reused) {
        ++telemetry_.previewRenderTargetReused;
    }
    if (allocation.resized) {
        ++telemetry_.previewRenderTargetResized;
    }
    (void)descriptorIndex;
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE EditorAssetD3D12ThumbnailGpuBackend::CpuHandle(
    uint32_t descriptorIndex) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize_) * descriptorIndex;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE EditorAssetD3D12ThumbnailGpuBackend::GpuHandle(
    uint32_t descriptorIndex) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = srvHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(descriptorSize_) * descriptorIndex;
    return handle;
}

} // namespace editor
