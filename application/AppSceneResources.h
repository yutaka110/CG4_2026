#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <string>

#include "../../externals/DirectXTex/DirectXTex.h"
#include "AppRuntimeState.h"
#include "AnimationClip.h"
#include "utils/math/MathUtils.h"
#include "utils/math/Vector.h"
#include "utils/dx12/BufferHelper.h"
#include "ModelLoaderAssimp.h"
struct SphereMeshData {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    Microsoft::WRL::ComPtr<ID3D12Resource> cbvResource;
    TransformationMatrix* mappedCBV = nullptr;
    UINT vertexCount = 0;
};

struct SkyboxMeshData {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    Microsoft::WRL::ComPtr<ID3D12Resource> cbvResource;
    TransformationMatrix* mappedCBV = nullptr;
    UINT vertexCount = 0;
};

struct RingMeshData {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    UINT vertexCount = 0;
};

struct CylinderMeshData {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    UINT vertexCount = 0;
};

struct CameraForGPU {
    Vector3 worldPosition;
    float padding;
};

class AppSceneResources {
public:
    bool Initialize(
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
        uint32_t descriptorSizeSRV);

    void UpdateCameraWorldPosition(const Vector3& worldPosition);
    void UpdateTransforms(
        const AppRuntimeState& runtimeState,
        Matrix4x4* wvpData,
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projMatrix,
        uint32_t windowWidth,
        uint32_t windowHeight);
    void SyncRuntimeState(AppRuntimeState& runtimeState, float deltaTime);

public:
    // Sprite
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
    D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite;
    TransformationMatrix* transformationMatrixDataSprite = nullptr;

    // Material
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    Material* materialData = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite;
    Material* materialDataSprite = nullptr;

    // Lights
    DirectionalLight directionalLightData{};
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
    DirectionalLight* mappedLight = nullptr;

    PointLight pointLightData{};
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource;
    PointLight* mappedPointLight = nullptr;

    SpotLight spotLight{};
    Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource;
    SpotLight* mappedSpotLight = nullptr;

    // Camera
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource;
    CameraForGPU* mappedCamera = nullptr;

    // Texture
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2;
    Microsoft::WRL::ComPtr<ID3D12Resource> circle2TextureResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> gradationLineTextureResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> skyboxTextureResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> animatedCubeTextureResource;
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2{};
    D3D12_GPU_DESCRIPTOR_HANDLE circle2TextureSrvHandleGPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE gradationLineTextureSrvHandleGPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxTextureSrvHandleGPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE animatedCubeTextureSrvHandleGPU{};
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU{};
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2{};
    D3D12_CPU_DESCRIPTOR_HANDLE circle2TextureSrvHandleCPU{};
    D3D12_CPU_DESCRIPTOR_HANDLE gradationLineTextureSrvHandleCPU{};
    D3D12_CPU_DESCRIPTOR_HANDLE skyboxTextureSrvHandleCPU{};
    D3D12_CPU_DESCRIPTOR_HANDLE animatedCubeTextureSrvHandleCPU{};

    // Sphere
    SphereMeshData sphere{};

    // Skybox
    SkyboxMeshData skybox{};

    // VFX primitives
    RingMeshData ring{};
    CylinderMeshData cylinder{};

    // Model
    ModelData modelData{};
    Microsoft::WRL::ComPtr<ID3D12Resource> modelVertexResource;
    D3D12_VERTEX_BUFFER_VIEW modelVBV{};
    UINT modelVertexCount = 0;

    ModelData animatedCubeData{};
    AnimationClip animatedCubeAnimation{};
    Microsoft::WRL::ComPtr<ID3D12Resource> animatedCubeVertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> animatedCubeTransformResource;
    D3D12_VERTEX_BUFFER_VIEW animatedCubeVBV{};
    TransformationMatrix* animatedCubeTransformData = nullptr;
    UINT animatedCubeVertexCount = 0;
};
