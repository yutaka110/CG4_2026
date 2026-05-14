#include "ModelLoaderAssimp.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstdint>

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

} // namespace

Node ReadNode(aiNode* node);

ModelData LoadObjFile_Assimp(
    const std::string& directoryPath,
    const std::string& filename) {
    ModelData modelData{};
    modelData.vertices.clear();
    modelData.indices.clear();
    modelData.material.textureFilePath.clear();
    modelData.rootNode.localMatrix = MakeIdentity4x4();

    const std::string filePath = JoinPath(directoryPath, filename);

    Assimp::Importer importer;
    const unsigned flags =
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals;

    const aiScene* scene = importer.ReadFile(filePath.c_str(), flags);
    if (!scene || !scene->HasMeshes()) {
        return modelData;
    }

    for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh || !mesh->HasNormals() || !mesh->HasTextureCoords(0)) {
            continue;
        }

        const uint32_t baseVertex = static_cast<uint32_t>(modelData.vertices.size());
        modelData.vertices.reserve(modelData.vertices.size() + mesh->mNumVertices);
        for (unsigned vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            const aiVector3D& p = mesh->mVertices[vertexIndex];
            const aiVector3D& n = mesh->mNormals[vertexIndex];
            const aiVector3D& uv = mesh->mTextureCoords[0][vertexIndex];

            VertexData v{};
            v.position = { p.x, p.y, p.z, 1.0f };
            v.texcoord = { uv.x, uv.y };
            v.normal = { n.x, n.y, n.z };
            modelData.vertices.push_back(v);
        }

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

        if (scene->HasMaterials()) {
            const unsigned matIndex = mesh->mMaterialIndex;
            if (matIndex < scene->mNumMaterials) {
                const aiMaterial* mat = scene->mMaterials[matIndex];
                if (mat && mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                    aiString texPath;
                    if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                        modelData.material.textureFilePath =
                            JoinPath(directoryPath, texPath.C_Str());
                    }
                }
            }
        }
    }

    if (scene->mRootNode != nullptr) {
        modelData.rootNode = ReadNode(scene->mRootNode);
    }

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
