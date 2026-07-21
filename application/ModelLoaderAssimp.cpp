#include "ModelLoaderAssimp.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstdint>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#include "utils/math/MathUtils.h"
#include "utils/math/Vector.h"

namespace {

std::string JoinPath(const std::string& dir, const std::string& file) {
    if (dir.empty()) {
        return file;
    }
    if (dir.back() == '/' || dir.back() == '\\') {
        return dir + file;
    }
    return dir + "/" + file;
}

std::string ResolveTexturePath(
    const std::string& directoryPath,
    const aiString& importedPath) {
    const std::string pathText = importedPath.C_Str();
    if (pathText.empty() || pathText.front() == '*') {
        // Embedded textures require a separate upload path. Fail closed to the
        // renderer fallback until that resource type is implemented.
        return {};
    }
    const std::filesystem::path path(pathText);
    if (path.is_absolute()) {
        return path.lexically_normal().generic_string();
    }
    return (std::filesystem::path(directoryPath) / path)
        .lexically_normal()
        .generic_string();
}

std::string ReadFirstTexturePath(
    const aiMaterial* material,
    const std::string& directoryPath,
    std::initializer_list<aiTextureType> textureTypes) {
    if (material == nullptr) {
        return {};
    }
    for (const aiTextureType textureType : textureTypes) {
        if (material->GetTextureCount(textureType) == 0) {
            continue;
        }
        aiString importedPath;
        if (material->GetTexture(textureType, 0, &importedPath) == AI_SUCCESS) {
            return ResolveTexturePath(directoryPath, importedPath);
        }
    }
    return {};
}

MaterialData ReadMaterialData(
    const aiMaterial* material,
    const std::string& directoryPath,
    uint32_t materialIndex) {
    MaterialData result;
    result.name = "material_" + std::to_string(materialIndex);
    if (material == nullptr) {
        return result;
    }

    aiString materialName;
    if (material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS &&
        materialName.length > 0) {
        result.name = materialName.C_Str();
    }
    result.textureFilePath = ReadFirstTexturePath(
        material,
        directoryPath,
        {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE});
    result.normalTextureFilePath = ReadFirstTexturePath(
        material,
        directoryPath,
        {aiTextureType_NORMALS, aiTextureType_HEIGHT});

    aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
    if (aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &baseColor) != AI_SUCCESS) {
        aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &baseColor);
    }
    result.baseColorFactor = {
        baseColor.r,
        baseColor.g,
        baseColor.b,
        baseColor.a,
    };
    return result;
}

Matrix4x4 MakeInverseBindPoseMatrix(const aiMatrix4x4& offsetMatrix) {
    aiMatrix4x4 bindPoseMatrixAssimp = offsetMatrix;
    bindPoseMatrixAssimp.Inverse();

    aiVector3D scale{};
    aiVector3D translate{};
    aiQuaternion rotate{};
    bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

    Matrix4x4 bindPoseMatrix = MakeAffineMatrix(
        Vector3{ scale.x, scale.y, scale.z },
        Normalize(Quaternion{ rotate.x, rotate.y, rotate.z, rotate.w }),
        Vector3{ translate.x, translate.y, translate.z });
    return Inverse(bindPoseMatrix);
}

constexpr float kGeometryEpsilonSquared = 1.0e-12f;

Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) noexcept {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vector3 Cross(const Vector3& lhs, const Vector3& rhs) noexcept {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

float Dot(const Vector3& lhs, const Vector3& rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float LengthSquared(const Vector3& value) noexcept {
    return Dot(value, value);
}

bool IsFinite(const Vector3& value) noexcept {
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

Vector3 Position3(const VertexData& vertex) noexcept {
    return {vertex.position.x, vertex.position.y, vertex.position.z};
}

Vector3 Add(const Vector3& lhs, const Vector3& rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

} // namespace

ModelGeometryOrientationStats RepairModelGeometryOrientation(
    ModelData& modelData) noexcept {
    ModelGeometryOrientationStats stats{};
    std::vector<Vector3> generatedNormals(modelData.vertices.size(), Vector3{});
    std::vector<bool> needsGeneratedNormal(modelData.vertices.size(), false);

    for (size_t vertexIndex = 0; vertexIndex < modelData.vertices.size(); ++vertexIndex) {
        VertexData& vertex = modelData.vertices[vertexIndex];
        if (!IsFinite(vertex.normal) ||
            LengthSquared(vertex.normal) <= kGeometryEpsilonSquared) {
            vertex.normal = {};
            needsGeneratedNormal[vertexIndex] = true;
        } else {
            vertex.normal = Normalize(vertex.normal);
        }
    }

    for (SubMeshData& subMesh : modelData.subMeshes) {
        const uint64_t rangeEnd =
            static_cast<uint64_t>(subMesh.indexStart) + subMesh.indexCount;
        if (rangeEnd > modelData.indices.size()) {
            continue;
        }
        for (uint32_t offset = 0; offset + 2 < subMesh.indexCount; offset += 3) {
            const uint32_t indexOffset = subMesh.indexStart + offset;
            const uint32_t index0 = modelData.indices[indexOffset];
            uint32_t index1 = modelData.indices[indexOffset + 1];
            uint32_t index2 = modelData.indices[indexOffset + 2];
            if (index0 >= modelData.vertices.size() ||
                index1 >= modelData.vertices.size() ||
                index2 >= modelData.vertices.size()) {
                continue;
            }

            ++stats.triangleCount;
            const Vector3 edge1 = Subtract(
                Position3(modelData.vertices[index1]),
                Position3(modelData.vertices[index0]));
            const Vector3 edge2 = Subtract(
                Position3(modelData.vertices[index2]),
                Position3(modelData.vertices[index0]));
            Vector3 faceNormal = Cross(edge1, edge2);
            if (!IsFinite(faceNormal) ||
                LengthSquared(faceNormal) <= kGeometryEpsilonSquared) {
                ++stats.degenerateTriangleCount;
                continue;
            }

            const Vector3 authoredNormal = Add(
                Add(modelData.vertices[index0].normal, modelData.vertices[index1].normal),
                modelData.vertices[index2].normal);
            if (LengthSquared(authoredNormal) > kGeometryEpsilonSquared &&
                Dot(faceNormal, authoredNormal) < 0.0f) {
                std::swap(
                    modelData.indices[indexOffset + 1],
                    modelData.indices[indexOffset + 2]);
                std::swap(index1, index2);
                faceNormal = {-faceNormal.x, -faceNormal.y, -faceNormal.z};
                ++stats.repairedWindingCount;
            }

            faceNormal = Normalize(faceNormal);
            for (const uint32_t vertexIndex : {index0, index1, index2}) {
                if (needsGeneratedNormal[vertexIndex]) {
                    generatedNormals[vertexIndex] = Add(
                        generatedNormals[vertexIndex], faceNormal);
                }
            }
        }
    }

    for (size_t vertexIndex = 0; vertexIndex < modelData.vertices.size(); ++vertexIndex) {
        if (!needsGeneratedNormal[vertexIndex]) {
            continue;
        }
        const Vector3 generated = generatedNormals[vertexIndex];
        if (LengthSquared(generated) > kGeometryEpsilonSquared) {
            modelData.vertices[vertexIndex].normal = Normalize(generated);
            ++stats.regeneratedNormalCount;
        }
    }
    return stats;
}

bool ValidateModelGeometryOrientation(const ModelData& modelData) noexcept {
    for (const VertexData& vertex : modelData.vertices) {
        if (!IsFinite(vertex.normal) ||
            std::fabs(LengthSquared(vertex.normal) - 1.0f) > 0.01f) {
            return false;
        }
    }
    for (const SubMeshData& subMesh : modelData.subMeshes) {
        const uint64_t rangeEnd =
            static_cast<uint64_t>(subMesh.indexStart) + subMesh.indexCount;
        if (rangeEnd > modelData.indices.size()) {
            return false;
        }
        for (uint32_t offset = 0; offset + 2 < subMesh.indexCount; offset += 3) {
            const uint32_t index0 = modelData.indices[subMesh.indexStart + offset];
            const uint32_t index1 = modelData.indices[subMesh.indexStart + offset + 1];
            const uint32_t index2 = modelData.indices[subMesh.indexStart + offset + 2];
            if (index0 >= modelData.vertices.size() ||
                index1 >= modelData.vertices.size() ||
                index2 >= modelData.vertices.size()) {
                return false;
            }
            const Vector3 faceNormal = Cross(
                Subtract(Position3(modelData.vertices[index1]), Position3(modelData.vertices[index0])),
                Subtract(Position3(modelData.vertices[index2]), Position3(modelData.vertices[index0])));
            if (LengthSquared(faceNormal) <= kGeometryEpsilonSquared) {
                continue;
            }
            const Vector3 vertexNormal = Add(
                Add(modelData.vertices[index0].normal, modelData.vertices[index1].normal),
                modelData.vertices[index2].normal);
            if (LengthSquared(vertexNormal) <= kGeometryEpsilonSquared ||
                Dot(faceNormal, vertexNormal) <= 0.0f) {
                return false;
            }
        }
    }
    return true;
}

Node ReadNode(aiNode* node);

ModelData LoadObjFile_Assimp(
    const std::string& directoryPath,
    const std::string& filename) {
    ModelData modelData{};
    modelData.vertices.clear();
    modelData.indices.clear();
    modelData.skinClusterData.clear();
    modelData.subMeshes.clear();
    modelData.materials.clear();
    modelData.material.textureFilePath.clear();
    modelData.rootNode.localMatrix = MakeIdentity4x4();

    const std::string filePath = JoinPath(directoryPath, filename);

    Assimp::Importer importer;
    const unsigned flags =
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenSmoothNormals;

    const aiScene* scene = importer.ReadFile(filePath.c_str(), flags);
    if (!scene || !scene->HasMeshes()) {
        return modelData;
    }

    if (scene->HasMaterials()) {
        modelData.materials.reserve(scene->mNumMaterials);
        for (uint32_t materialIndex = 0;
             materialIndex < scene->mNumMaterials;
             ++materialIndex) {
            modelData.materials.push_back(ReadMaterialData(
                scene->mMaterials[materialIndex],
                directoryPath,
                materialIndex));
        }
    }
    if (modelData.materials.empty()) {
        modelData.materials.push_back(MaterialData{"material_0"});
    }

    for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh || !mesh->HasNormals()) {
            continue;
        }

        const uint32_t baseVertex = static_cast<uint32_t>(modelData.vertices.size());
        modelData.vertices.reserve(modelData.vertices.size() + mesh->mNumVertices);
        for (unsigned vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            const aiVector3D& p = mesh->mVertices[vertexIndex];
            const aiVector3D& n = mesh->mNormals[vertexIndex];
            const aiVector3D uv = mesh->HasTextureCoords(0)
                ? mesh->mTextureCoords[0][vertexIndex]
                : aiVector3D{};

            VertexData v{};
            v.position = { p.x, p.y, p.z, 1.0f };
            v.texcoord = { uv.x, uv.y };
            v.normal = { n.x, n.y, n.z };
            modelData.vertices.push_back(v);
        }

        const uint32_t indexStart = static_cast<uint32_t>(modelData.indices.size());
        modelData.indices.reserve(modelData.indices.size() + mesh->mNumFaces * 3);
        for (unsigned faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace& face = mesh->mFaces[faceIndex];
            if (face.mNumIndices != 3) {
                continue;
            }

            for (unsigned element = 0; element < face.mNumIndices; ++element) {
                modelData.indices.push_back(baseVertex + face.mIndices[element]);
            }
        }
        const uint32_t indexCount =
            static_cast<uint32_t>(modelData.indices.size()) - indexStart;
        if (indexCount > 0) {
            const uint32_t materialIndex = mesh->mMaterialIndex < modelData.materials.size()
                ? mesh->mMaterialIndex
                : 0;
            modelData.subMeshes.push_back({
                mesh->mName.length > 0
                    ? std::string(mesh->mName.C_Str())
                    : "submesh_" + std::to_string(meshIndex),
                indexStart,
                indexCount,
                materialIndex,
            });
        }

        for (unsigned boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            const aiBone* bone = mesh->mBones[boneIndex];
            if (bone == nullptr) {
                continue;
            }

            const std::string jointName = bone->mName.C_Str();
            JointWeightData& jointWeightData = modelData.skinClusterData[jointName];
            jointWeightData.inverseBindPoseMatrix =
                MakeInverseBindPoseMatrix(bone->mOffsetMatrix);

            for (unsigned weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                const aiVertexWeight& weight = bone->mWeights[weightIndex];
                jointWeightData.vertexWeights.push_back(VertexWeightData{
                    weight.mWeight,
                    baseVertex + weight.mVertexId,
                });
            }
        }
    }

    if (scene->mRootNode != nullptr) {
        modelData.rootNode = ReadNode(scene->mRootNode);
    }

    EnsureModelDataMaterialLayout(modelData);
    RepairModelGeometryOrientation(modelData);

    return modelData;
}

Node ReadNode(aiNode* node) {
    Node result;

    aiVector3D scale{};
    aiVector3D translate{};
    aiQuaternion rotate{};
    node->mTransformation.Decompose(scale, rotate, translate);

    result.transform.scale = { scale.x, scale.y, scale.z };
    result.transform.rotate = Normalize(Quaternion{ rotate.x, rotate.y, rotate.z, rotate.w });
    result.transform.translate = { translate.x, translate.y, translate.z };
    result.localMatrix = MakeAffineMatrix(
        result.transform.scale,
        result.transform.rotate,
        result.transform.translate);

    result.name = node->mName.C_Str();

    result.children.resize(node->mNumChildren);
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        result.children[i] = ReadNode(node->mChildren[i]);
    }

    return result;
}
