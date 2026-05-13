#include "AppSceneResources.h"

#include <cassert>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <numbers>
#include <vector>

#include "AppRenderResources.h"
#include "AppRuntimeUtils.h"
#include "ModelLoaderAssimp.h"

using Microsoft::WRL::ComPtr;

namespace {

    struct SphereVertex {
        float position[4];
        float texcoord[2];
        float normal[3];
    };

    struct SkyboxVertex {
        Vector4 position;
    };

    struct RingVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    std::vector<SphereVertex> BuildSphereVertices(uint32_t stackCount, uint32_t sliceCount) {
        std::vector<SphereVertex> v;
        v.reserve(stackCount * sliceCount * 6);

        for (uint32_t y = 0; y < stackCount; ++y) {
            float v0 = (float)y / (float)stackCount;
            float v1 = (float)(y + 1) / (float)stackCount;
            float phi0 = v0 * std::numbers::pi_v<float>;
            float phi1 = v1 * std::numbers::pi_v<float>;

            for (uint32_t x = 0; x < sliceCount; ++x) {
                float u0 = (float)x / (float)sliceCount;
                float u1 = (float)(x + 1) / (float)sliceCount;
                float theta0 = u0 * (std::numbers::pi_v<float> *2.0f);
                float theta1 = u1 * (std::numbers::pi_v<float> *2.0f);

                auto MakeV = [](float phi, float theta, float u, float vTex) {
                    SphereVertex sv{};
                    float sx = std::sin(phi) * std::cos(theta);
                    float sy = std::cos(phi);
                    float sz = std::sin(phi) * std::sin(theta);

                    sv.position[0] = sx;
                    sv.position[1] = sy;
                    sv.position[2] = sz;
                    sv.position[3] = 1.0f;

                    sv.texcoord[0] = u;
                    sv.texcoord[1] = vTex;

                    sv.normal[0] = sx;
                    sv.normal[1] = sy;
                    sv.normal[2] = sz;
                    return sv;
                    };

                SphereVertex a = MakeV(phi0, theta0, u0, v0);
                SphereVertex b = MakeV(phi0, theta1, u1, v0);
                SphereVertex c = MakeV(phi1, theta0, u0, v1);
                SphereVertex d = MakeV(phi1, theta1, u1, v1);

                v.push_back(a);
                v.push_back(c);
                v.push_back(b);
                v.push_back(b);
                v.push_back(c);
                v.push_back(d);
            }
        }

        return v;
    }

    std::vector<SkyboxVertex> BuildSkyboxVertices() {
        constexpr float k = 1.0f;
        const Vector4 p000{-k, -k, -k, 1.0f};
        const Vector4 p001{-k, -k,  k, 1.0f};
        const Vector4 p010{-k,  k, -k, 1.0f};
        const Vector4 p011{-k,  k,  k, 1.0f};
        const Vector4 p100{ k, -k, -k, 1.0f};
        const Vector4 p101{ k, -k,  k, 1.0f};
        const Vector4 p110{ k,  k, -k, 1.0f};
        const Vector4 p111{ k,  k,  k, 1.0f};

        return {
            {p100}, {p110}, {p111}, {p100}, {p111}, {p101},
            {p000}, {p001}, {p011}, {p000}, {p011}, {p010},
            {p010}, {p011}, {p111}, {p010}, {p111}, {p110},
            {p000}, {p100}, {p101}, {p000}, {p101}, {p001},
            {p001}, {p101}, {p111}, {p001}, {p111}, {p011},
            {p000}, {p010}, {p110}, {p000}, {p110}, {p100},
        };
    }

    std::vector<RingVertex> BuildRingVertices(uint32_t divide) {
        std::vector<RingVertex> vertices;
        divide = (std::max)(uint32_t{3}, divide);
        vertices.reserve(static_cast<size_t>(divide) * 6);
        const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divide);

        auto makeVertex = [](float sinValue, float cosValue, float v, float u) {
            RingVertex vertex{};
            vertex.position = {sinValue, cosValue, 0.0f, 1.0f};
            vertex.texcoord = {u, v};
            vertex.normal = {0.0f, 0.0f, -1.0f};
            return vertex;
        };

        for (uint32_t index = 0; index < divide; ++index) {
            const float angle = static_cast<float>(index) * radianPerDivide;
            const float nextAngle = static_cast<float>(index + 1) * radianPerDivide;
            const float sin0 = std::sin(angle);
            const float cos0 = std::cos(angle);
            const float sin1 = std::sin(nextAngle);
            const float cos1 = std::cos(nextAngle);
            const float u0 = static_cast<float>(index) / static_cast<float>(divide);
            const float u1 = static_cast<float>(index + 1) / static_cast<float>(divide);

            const RingVertex outer0 = makeVertex(sin0, cos0, 0.0f, u0);
            const RingVertex outer1 = makeVertex(sin1, cos1, 0.0f, u1);
            const RingVertex inner0 = makeVertex(sin0, cos0, 1.0f, u0);
            const RingVertex inner1 = makeVertex(sin1, cos1, 1.0f, u1);

            vertices.push_back(outer0);
            vertices.push_back(outer1);
            vertices.push_back(inner0);
            vertices.push_back(inner0);
            vertices.push_back(outer1);
            vertices.push_back(inner1);
        }
        return vertices;
    }

} // namespace

bool AppSceneResources::Initialize(
    ComPtr<ID3D12Device> device,
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
    uint32_t descriptorSizeSRV) {

    // =========================================================
    // Sprite geometry
    // =========================================================
    indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
    vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);

    transformationMatrixResourceSprite =
        CreateBufferResource(device, sizeof(TransformationMatrix));
    transformationMatrixResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&transformationMatrixDataSprite));
    transformationMatrixDataSprite->WVP = MakeIdentity4x4();
    transformationMatrixDataSprite->World = MakeIdentity4x4();
    transformationMatrixDataSprite->WorldInverseTranspose = MakeIdentity4x4();

    indexBufferViewSprite.BufferLocation =
        indexResourceSprite->GetGPUVirtualAddress();
    indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

    vertexBufferViewSprite.BufferLocation =
        vertexResourceSprite->GetGPUVirtualAddress();
    vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

    uint32_t* indexDataSprite = nullptr;
    indexResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&indexDataSprite));
    indexDataSprite[0] = 0;
    indexDataSprite[1] = 1;
    indexDataSprite[2] = 2;
    indexDataSprite[3] = 3;
    indexDataSprite[4] = 4;
    indexDataSprite[5] = 5;

    VertexData* vertexDataSprite = nullptr;
    vertexResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&vertexDataSprite));

    vertexDataSprite[0].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[0].texcoord = { 0.0f, 1.0f };

    vertexDataSprite[1].position = { -0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[1].texcoord = { 0.0f, 0.0f };

    vertexDataSprite[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[2].texcoord = { 1.0f, 1.0f };

    vertexDataSprite[3].position = { -0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[3].texcoord = { 0.0f, 0.0f };

    vertexDataSprite[4].position = { 0.5f,  0.5f, 0.0f, 1.0f };
    vertexDataSprite[4].texcoord = { 1.0f, 0.0f };

    vertexDataSprite[5].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertexDataSprite[5].texcoord = { 1.0f, 1.0f };

    // =========================================================
    // Materials
    // =========================================================
    materialResource = CreateBufferResource(device, sizeof(Material));
    materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
    materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData->enableLighting = true;
    materialData->shininess = 3.0f;
    materialData->environmentCoefficient = 0.3f;
    materialData->uvTransform = MakeIdentity4x4();

    materialResourceSprite = CreateBufferResource(device, sizeof(Material));
    materialResourceSprite->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&materialDataSprite));
    materialDataSprite->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    materialDataSprite->enableLighting = false;
    materialDataSprite->shininess = 1.0f;
    materialDataSprite->environmentCoefficient = 0.0f;
    materialDataSprite->uvTransform = MakeIdentity4x4();

    // =========================================================
    // Lights
    // =========================================================
    directionalLightData.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 dir = { 0.0f, -1.0f, 0.0f };
    dir = Normalize(dir);
    directionalLightData.direction = dir;
    directionalLightData.intensity = 1.5f;

    directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));
    directionalLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedLight));
    *mappedLight = directionalLightData;

    pointLightData.color = { 1,1,1,1 };
    pointLightData.position = { 0.0f, 2.0f, 0.0f };
    pointLightData.intensity = 1.0f;
    pointLightData.radius = 10.0f;
    pointLightData.decay = 2.0f;

    pointLightResource = CreateBufferResource(device, sizeof(PointLight));
    pointLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedPointLight));
    *mappedPointLight = pointLightData;

    spotLight.color = { 1,1,1,1 };
    spotLight.position = { 2.0f, 1.25f, 0.0f };
    spotLight.direction = Normalize(Vector3{ -1.0f, -1.0f, 0.0f });
    spotLight.distance = 7.0f;
    spotLight.intensity = 5.0f;
    spotLight.decay = 2.0f;
    spotLight.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);

    spotLightResource = CreateBufferResource(device, sizeof(SpotLight));
    spotLightResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&mappedSpotLight));
    *mappedSpotLight = spotLight;

    // =========================================================
    // Camera
    // =========================================================
    cameraResource = CreateBufferResource(device, sizeof(CameraForGPU));
    cameraResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedCamera));
    mappedCamera->worldPosition = Vector3{ 0.0f, 0.0f, -5.0f };
    mappedCamera->padding = 0.0f;

    // =========================================================
    // Texture 2枚
    // slot 1, 2 を使用（slot 0 は ImGui 用の前提）
    // =========================================================
    DirectX::ScratchImage mipImages = AppRenderResources::LoadTexture("resources/monsterBall.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    textureResource = AppRenderResources::CreateTextureResource(device, metadata);
    AppRenderResources::UploadTextureData(textureResource, mipImages);

    DirectX::ScratchImage mipImages2 = AppRenderResources::LoadTexture("resources/monsterBall.png");
    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    textureResource2 = AppRenderResources::CreateTextureResource(device, metadata2);
    AppRenderResources::UploadTextureData(textureResource2, mipImages2);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    textureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
    textureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
    device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    textureSrvHandleCPU2 = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    textureSrvHandleGPU2 = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
    device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

    const std::string circle2TexturePath =
        std::filesystem::exists("Resources/circle2.png") ? "Resources/circle2.png" : "resources/monsterBall.png";
    DirectX::ScratchImage circle2Images = AppRenderResources::LoadTexture(circle2TexturePath);
    const DirectX::TexMetadata& circle2Metadata = circle2Images.GetMetadata();
    circle2TextureResource = AppRenderResources::CreateTextureResource(device, circle2Metadata);
    AppRenderResources::UploadTextureData(circle2TextureResource, circle2Images);

    D3D12_SHADER_RESOURCE_VIEW_DESC circle2SrvDesc{};
    circle2SrvDesc.Format = circle2Metadata.format;
    circle2SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    circle2SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    circle2SrvDesc.Texture2D.MipLevels = UINT(circle2Metadata.mipLevels);

    circle2TextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 5);
    circle2TextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 5);
    device->CreateShaderResourceView(circle2TextureResource.Get(), &circle2SrvDesc, circle2TextureSrvHandleCPU);

    const std::string gradationLineTexturePath =
        std::filesystem::exists("Resources/gradationLine.png") ? "Resources/gradationLine.png" : circle2TexturePath;
    DirectX::ScratchImage gradationLineImages = AppRenderResources::LoadTexture(gradationLineTexturePath);
    const DirectX::TexMetadata& gradationLineMetadata = gradationLineImages.GetMetadata();
    gradationLineTextureResource = AppRenderResources::CreateTextureResource(device, gradationLineMetadata);
    AppRenderResources::UploadTextureData(gradationLineTextureResource, gradationLineImages);

    D3D12_SHADER_RESOURCE_VIEW_DESC gradationLineSrvDesc{};
    gradationLineSrvDesc.Format = gradationLineMetadata.format;
    gradationLineSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    gradationLineSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    gradationLineSrvDesc.Texture2D.MipLevels = UINT(gradationLineMetadata.mipLevels);

    gradationLineTextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 6);
    gradationLineTextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 6);
    device->CreateShaderResourceView(
        gradationLineTextureResource.Get(),
        &gradationLineSrvDesc,
        gradationLineTextureSrvHandleCPU);

    const std::string skyboxTexturePath = "Resources/rostock_laage_airport_4k.dds";
    if (std::filesystem::exists(skyboxTexturePath)) {
        DirectX::ScratchImage skyboxImages = AppRenderResources::LoadTexture(skyboxTexturePath);
        const DirectX::TexMetadata& skyboxMetadata = skyboxImages.GetMetadata();
        if (skyboxMetadata.IsCubemap()) {
            skyboxTextureResource = AppRenderResources::CreateTextureResource(device, skyboxMetadata);
            AppRenderResources::UploadTextureData(skyboxTextureResource, skyboxImages);

            D3D12_SHADER_RESOURCE_VIEW_DESC skyboxSrvDesc{};
            skyboxSrvDesc.Format = skyboxMetadata.format;
            skyboxSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            skyboxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            skyboxSrvDesc.TextureCube.MostDetailedMip = 0;
            skyboxSrvDesc.TextureCube.MipLevels = UINT(skyboxMetadata.mipLevels);
            skyboxSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

            skyboxTextureSrvHandleCPU = AppRenderResources::GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
            skyboxTextureSrvHandleGPU = AppRenderResources::GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 3);
            device->CreateShaderResourceView(skyboxTextureResource.Get(), &skyboxSrvDesc, skyboxTextureSrvHandleCPU);
        } else {
            OutputDebugStringA("[AppSceneResources] Skybox texture exists but is not a cubemap DDS.\n");
        }
    } else {
        OutputDebugStringA("[AppSceneResources] Skybox DDS not found. Skybox pass will be skipped.\n");
    }

    // =========================================================
    // Sphere mesh
    // =========================================================
    {
        const uint32_t sphereStacks = 32;
        const uint32_t sphereSlices = 64;
        std::vector<SphereVertex> sphereVerts =
            BuildSphereVertices(sphereStacks, sphereSlices);

        sphere.vertexCount = (UINT)sphereVerts.size();
        sphere.vertexResource =
            CreateBufferResource(device, sizeof(SphereVertex) * sphere.vertexCount);

        SphereVertex* mappedVB = nullptr;
        sphere.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, sphereVerts.data(), sizeof(SphereVertex) * sphere.vertexCount);

        sphere.vbv.BufferLocation = sphere.vertexResource->GetGPUVirtualAddress();
        sphere.vbv.SizeInBytes = (UINT)(sizeof(SphereVertex) * sphere.vertexCount);
        sphere.vbv.StrideInBytes = sizeof(SphereVertex);

        sphere.cbvResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        sphere.cbvResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&sphere.mappedCBV));
        sphere.mappedCBV->WVP = MakeIdentity4x4();
        sphere.mappedCBV->World = MakeIdentity4x4();
        sphere.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }

    // =========================================================
    // Skybox mesh
    // =========================================================
    {
        const std::vector<SkyboxVertex> skyboxVerts = BuildSkyboxVertices();
        skybox.vertexCount = UINT(skyboxVerts.size());
        skybox.vertexResource =
            CreateBufferResource(device, sizeof(SkyboxVertex) * skybox.vertexCount);

        SkyboxVertex* mappedVB = nullptr;
        skybox.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, skyboxVerts.data(), sizeof(SkyboxVertex) * skybox.vertexCount);

        skybox.vbv.BufferLocation = skybox.vertexResource->GetGPUVirtualAddress();
        skybox.vbv.SizeInBytes = UINT(sizeof(SkyboxVertex) * skybox.vertexCount);
        skybox.vbv.StrideInBytes = sizeof(SkyboxVertex);

        skybox.cbvResource = CreateBufferResource(device, sizeof(TransformationMatrix));
        skybox.cbvResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&skybox.mappedCBV));
        skybox.mappedCBV->WVP = MakeIdentity4x4();
        skybox.mappedCBV->World = MakeIdentity4x4();
        skybox.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }

    // =========================================================
    // VFX Ring mesh
    // =========================================================
    {
        const std::vector<RingVertex> ringVerts = BuildRingVertices(128);
        ring.vertexCount = UINT(ringVerts.size());
        ring.vertexResource =
            CreateBufferResource(device, sizeof(RingVertex) * ring.vertexCount);

        RingVertex* mappedVB = nullptr;
        ring.vertexResource->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedVB));
        memcpy(mappedVB, ringVerts.data(), sizeof(RingVertex) * ring.vertexCount);

        ring.vbv.BufferLocation = ring.vertexResource->GetGPUVirtualAddress();
        ring.vbv.SizeInBytes = UINT(sizeof(RingVertex) * ring.vertexCount);
        ring.vbv.StrideInBytes = sizeof(RingVertex);
    }

    // =========================================================
    // Assimp model VB
    // =========================================================
    modelData = LoadObjFile_Assimp("Resources/ball", "ball.obj");
    assert(!modelData.vertices.empty());

    modelVertexResource =
        CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());

    VertexData* mapped = nullptr;
    modelVertexResource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
    memcpy(mapped, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

    modelVBV.BufferLocation = modelVertexResource->GetGPUVirtualAddress();
    modelVBV.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
    modelVBV.StrideInBytes = sizeof(VertexData);

    modelVertexCount = UINT(modelData.vertices.size());

    return true;
}

void AppSceneResources::UpdateCameraWorldPosition(const Vector3& worldPosition) {
    if (mappedCamera == nullptr) {
        return;
    }

    mappedCamera->worldPosition = worldPosition;
}

void AppSceneResources::UpdateTransforms(
    const AppRuntimeState& runtimeState,
    Matrix4x4* wvpData,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projMatrix,
    uint32_t windowWidth,
    uint32_t windowHeight) {
    if (wvpData != nullptr) {
        Matrix4x4 worldMatrix = MakeAffineMatrix(
            runtimeState.transform.scale,
            runtimeState.transform.rotate,
            runtimeState.transform.translate);
        *wvpData = Multiply(worldMatrix, Multiply(viewMatrix, projMatrix));
    }

    if (transformationMatrixDataSprite != nullptr) {
        Matrix4x4 worldMatrixSprite = MakeAffineMatrix(
            runtimeState.transformSprite.scale,
            runtimeState.transformSprite.rotate,
            runtimeState.transformSprite.translate);
        Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(
            0.0f,
            0.0f,
            float(windowWidth),
            float(windowHeight),
            0.0f,
            100.0f);
        transformationMatrixDataSprite->World = worldMatrixSprite;
        transformationMatrixDataSprite->WVP = Multiply(
            worldMatrixSprite,
            Multiply(MakeIdentity4x4(), projectionMatrixSprite));
        transformationMatrixDataSprite->WorldInverseTranspose =
            Transpose(Inverse(worldMatrixSprite));
    }

    if (materialDataSprite != nullptr) {
        Matrix4x4 uvTransformMatrix = MakeScaleMatrix(runtimeState.uvTransformSprite.scale);
        uvTransformMatrix = Multiply(
            uvTransformMatrix,
            MakeRoateZMatrix(runtimeState.uvTransformSprite.rotate.z));
        uvTransformMatrix = Multiply(
            uvTransformMatrix,
            MakeTranslateMatrix(runtimeState.uvTransformSprite.translate));
        materialDataSprite->uvTransform = uvTransformMatrix;
    }

    if (sphere.mappedCBV != nullptr) {
        Matrix4x4 worldMatrixSphere = MakeAffineMatrix(
            runtimeState.transform.scale,
            runtimeState.transform.rotate,
            runtimeState.transform.translate);
        const Matrix4x4& rootLocal = modelData.rootNode.localMatrix;
        Matrix4x4 worldWithNode = Multiply(rootLocal, worldMatrixSphere);
        Matrix4x4 wvpWithNode = Multiply(worldWithNode, Multiply(viewMatrix, projMatrix));
        sphere.mappedCBV->World = worldWithNode;
        sphere.mappedCBV->WVP = wvpWithNode;
        sphere.mappedCBV->WorldInverseTranspose = Transpose(Inverse(worldWithNode));
    }

    if (skybox.mappedCBV != nullptr) {
        Matrix4x4 skyboxView = viewMatrix;
        skyboxView.m[3][0] = 0.0f;
        skyboxView.m[3][1] = 0.0f;
        skyboxView.m[3][2] = 0.0f;
        skybox.mappedCBV->World = MakeIdentity4x4();
        skybox.mappedCBV->WVP = Multiply(skyboxView, projMatrix);
        skybox.mappedCBV->WorldInverseTranspose = MakeIdentity4x4();
    }
}

void AppSceneResources::SyncRuntimeState(AppRuntimeState& runtimeState, float deltaTime) {
    directionalLightData = runtimeState.directionalLightData;
    directionalLightData.direction = Normalize(directionalLightData.direction);
    runtimeState.directionalLightData.direction = directionalLightData.direction;
    if (mappedLight != nullptr) {
        *mappedLight = directionalLightData;
    }

    pointLightData = runtimeState.pointLightData;
    pointLightData.position.x = sinf(deltaTime) * 2.0f;
    runtimeState.pointLightData.position = pointLightData.position;
    if (mappedPointLight != nullptr) {
        *mappedPointLight = pointLightData;
    }

    spotLight = runtimeState.spotLight;
    spotLight.direction = Normalize(spotLight.direction);
    runtimeState.spotLight.direction = spotLight.direction;
    if (mappedSpotLight != nullptr) {
        *mappedSpotLight = spotLight;
    }

    if (materialData != nullptr) {
        *materialData = runtimeState.materialData;
    }
}
