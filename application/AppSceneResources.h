#pragma once

#include <array>
#include <d3d12.h>
#include <span>
#include <wrl.h>
#include <string>
#include <vector>

#include "../../externals/DirectXTex/DirectXTex.h"
#include "AppRuntimeState.h"
#include "AnimationClip.h"
#include "Skeleton.h"
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

struct SpearMeshData {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    UINT vertexCount = 0;
};

struct OrbitRibbonMeshData {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    UINT vertexCount = 0;
};

struct GpuMeshResource {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW ibv{};
    UINT vertexCount = 0;
    UINT indexCount = 0;
};

static constexpr uint32_t kNumMaxInfluence = 4;

struct VertexInfluence {
    std::array<float, kNumMaxInfluence> weights{};
    std::array<int32_t, kNumMaxInfluence> jointIndices{};
};

struct JointPaletteEntry {
    Matrix4x4 skeletonSpaceMatrix{};
    Matrix4x4 skeletonSpaceInverseTransposeMatrix{};
};

struct SkinningInformation {
    uint32_t numVertices = 0;
    uint32_t padding[3]{};
};

struct SkinCluster {
    std::vector<Matrix4x4> inverseBindPoseMatrices;
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView{};
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
    D3D12_RESOURCE_STATES paletteState = D3D12_RESOURCE_STATE_COPY_DEST;
    std::vector<JointPaletteEntry> paletteEntries;
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteUploadResource;
    JointPaletteEntry* mappedPaletteUpload = nullptr;
    bool paletteDirty = false;
    Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexResource;
    D3D12_VERTEX_BUFFER_VIEW skinnedVertexBufferView{};
    Microsoft::WRL::ComPtr<ID3D12Resource> skinningInfoResource;
    SkinningInformation* mappedSkinningInfo = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE vertexSrvCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE vertexSrvGpu{};
    D3D12_CPU_DESCRIPTOR_HANDLE influenceSrvCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE influenceSrvGpu{};
    D3D12_CPU_DESCRIPTOR_HANDLE paletteSrvCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE paletteSrvGpu{};
    D3D12_CPU_DESCRIPTOR_HANDLE skinnedVertexUavCpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE skinnedVertexUavGpu{};
    D3D12_RESOURCE_STATES skinnedVertexState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
};

struct SkinnedModelInstance {
    std::string name;
    std::string directory;
    std::string filename;
    ModelData model;
    AnimationClip animation;
    Skeleton skeleton;
    Animator animator;
    GpuMeshResource mesh;
    SkinCluster skinCluster;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
    TransformationMatrix* transformData = nullptr;
    Transform transform{};
    bool visible = false;
    bool loaded = false;
};

struct SkeletonDebugLineVertex {
    Vector4 position;
    Vector4 color;
};

struct CameraForGPU {
    Vector3 worldPosition;
    float padding;
};

struct AppManagedTextureResource {
    std::string name;
    std::string path;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    uint32_t descriptorIndex = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct AppManagedModelResource {
    std::string name;
    std::string directory;
    std::string filename;
    ModelData model;
    GpuMeshResource mesh;
    D3D12_GPU_DESCRIPTOR_HANDLE textureGpu{};
    bool loaded = false;
};

struct AppModelObjectInstance {
    std::string name;
    uint32_t modelIndex = 0;
    Transform transform{};
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
    TransformationMatrix* transformData = nullptr;
    bool visible = true;
};

class AppSceneResources {
public:
    bool Initialize(
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        ID3D12GraphicsCommandList* uploadCommandList,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
        uint32_t descriptorSizeSRV);
    void ReleaseInitialUploadResources();

    void UpdateCameraWorldPosition(const Vector3& worldPosition);
    void UpdateTransforms(
        const AppRuntimeState& runtimeState,
        Matrix4x4* wvpData,
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projMatrix,
        uint32_t windowWidth,
        uint32_t windowHeight);
    void SyncRuntimeState(AppRuntimeState& runtimeState, float deltaTime);
    SkinnedModelInstance* GetActiveSkinnedModel();
    const SkinnedModelInstance* GetActiveSkinnedModel() const;
    const AppManagedModelResource* FindManagedModel(uint32_t modelIndex) const;
    const std::vector<AppManagedModelResource>& ManagedModelLibrary() const { return vfxModelLibrary; }
    const std::vector<AppModelObjectInstance>& ModelObjectInstances() const { return vfxModelObjects; }

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
    std::vector<AppManagedTextureResource> vfxTextureLibrary;
    std::vector<AppManagedModelResource> vfxModelLibrary;
    std::vector<AppModelObjectInstance> vfxModelObjects;

    // Sphere
    SphereMeshData sphere{};

    // Skybox
    SkyboxMeshData skybox{};

    // VFX primitives
    RingMeshData ring{};
    CylinderMeshData cylinder{};
    SpearMeshData spear{};
    OrbitRibbonMeshData orbitRibbon{};

    // Model
    ModelData modelData{};
    GpuMeshResource modelMesh{};

    ModelData animatedCubeData{};
    AnimationClip animatedCubeAnimation{};
    GpuMeshResource animatedCubeMesh{};
    Microsoft::WRL::ComPtr<ID3D12Resource> animatedCubeTransformResource;
    TransformationMatrix* animatedCubeTransformData = nullptr;

    static constexpr uint32_t kSkinnedModelCount = 3;
    std::array<SkinnedModelInstance, kSkinnedModelCount> skinnedModels{};
    uint32_t activeSkinnedModelIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> skeletonDebugVertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> skeletonDebugTransformResource;
    D3D12_VERTEX_BUFFER_VIEW skeletonDebugVBV{};
    SkeletonDebugLineVertex* mappedSkeletonDebugLines = nullptr;
    TransformationMatrix* skeletonDebugTransformData = nullptr;
    UINT skeletonDebugVertexCapacity = 0;
    UINT skeletonDebugVertexCount = 0;

private:
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> initialUploadResources_;
};
