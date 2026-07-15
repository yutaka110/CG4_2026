#pragma once

#include "../scene/EditorProductionScenePipeline.h"
#include "../scene/EditorScene.h"

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "utils/math/MathUtils.h"

namespace editor {

inline constexpr uint32_t kEditorProductionMaximumShadowMaps = 8;

enum class EditorProductionLightType : uint32_t {
    Directional = 0,
    Point = 1,
    Spot = 2,
};

struct alignas(16) EditorProductionGpuLight {
    Vector4 colorIntensity{};
    Vector4 positionRange{};
    Vector4 directionType{};
    Vector4 attenuationShadow{};
};

struct EditorProductionClusterRange {
    uint32_t offset = 0;
    uint32_t count = 0;
};

struct EditorProductionShadowAllocation {
    std::string entityGuid;
    EditorProductionLightType type = EditorProductionLightType::Directional;
    uint32_t lightIndex = 0;
    uint32_t atlasSlice = 0;
    Matrix4x4 lightViewProjection = MakeIdentity4x4();
};

struct alignas(16) EditorProductionLightingConstants {
    uint32_t tileCountX = 1;
    uint32_t tileCountY = 1;
    uint32_t sliceCount = 1;
    uint32_t lightCount = 0;
    uint32_t maxLightsPerCluster = 1;
    uint32_t shadowCount = 0;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    Vector4 viewportAndInverse{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 clusterParameters{64.0f, 0.0f, 0.0f, 0.0f};
    Vector4 cameraPosition{};
    std::array<Matrix4x4, kEditorProductionMaximumShadowMaps> shadowViewProjection{};
    std::array<Vector4, kEditorProductionMaximumShadowMaps> shadowParameters{};
};

struct EditorProductionLightingPolicy {
    uint32_t maximumVisibleLights = 256;
    uint32_t tileSizePixels = 64;
    uint32_t depthSliceCount = 24;
    uint32_t maximumTileCountX = 32;
    uint32_t maximumTileCountY = 18;
    uint32_t maximumLightsPerCluster = 64;
    uint32_t maximumShadowMaps = kEditorProductionMaximumShadowMaps;
    uint32_t shadowMapSize = 1024;
    uint64_t shadowBudgetBytes = 64ull * 1024ull * 1024ull;
};

struct EditorProductionLightingStats {
    uint32_t submittedLights = 0;
    uint32_t visibleLights = 0;
    uint32_t rejectedByLightBudget = 0;
    uint32_t clusterCount = 0;
    uint32_t clusterIndexCount = 0;
    uint32_t clusterOverflowCount = 0;
    uint32_t shadowRequests = 0;
    uint32_t residentShadowMaps = 0;
    uint32_t rejectedByShadowBudget = 0;
    uint32_t renderedShadowDraws = 0;
    uint64_t lightBufferBytes = 0;
    uint64_t clusterBufferBytes = 0;
    uint64_t shadowAtlasBytes = 0;
};

// E-10 Scene-View-owned lighting bridge. Authoring light components remain in
// the Scene document; bounded GPU light/cluster buffers and the shadow atlas
// are transient renderer data and are never serialized back into the document.
class EditorProductionLightingPipeline {
public:
    explicit EditorProductionLightingPipeline(EditorProductionLightingPolicy policy = {})
        : policy_(std::move(policy)) {}
    bool Initialize(
        ID3D12Device* device,
        ID3D12DescriptorHeap* sharedSrvHeap,
        uint32_t descriptorSize,
        uint32_t shadowSrvDescriptorIndex,
        ID3D12RootSignature* mainRootSignature,
        EditorProductionLightingPolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(
        const EditorScene& scene,
        const Vector3& cameraWorldPosition,
        const Matrix4x4& view,
        const Matrix4x4& projection,
        const Matrix4x4& viewProjection,
        uint32_t viewportWidth,
        uint32_t viewportHeight,
        float nearPlane,
        float farPlane,
        std::string* errorMessage = nullptr,
        const std::unordered_set<std::string>* sourceResidentEntities = nullptr);

    void RenderShadowMaps(
        ID3D12GraphicsCommandList* commandList,
        const std::vector<EditorProductionSceneRenderPacket>& packets);

    D3D12_GPU_VIRTUAL_ADDRESS LightBufferAddress() const noexcept;
    D3D12_GPU_VIRTUAL_ADDRESS ClusterRangeBufferAddress() const noexcept;
    D3D12_GPU_VIRTUAL_ADDRESS ClusterIndexBufferAddress() const noexcept;
    D3D12_GPU_VIRTUAL_ADDRESS ConstantsAddress() const noexcept;
    D3D12_GPU_DESCRIPTOR_HANDLE ShadowAtlasHandle() const noexcept { return shadowAtlasGpu_; }

    const std::vector<EditorProductionGpuLight>& Lights() const noexcept { return lights_; }
    const std::vector<EditorProductionClusterRange>& ClusterRanges() const noexcept {
        return clusterRanges_;
    }
    const std::vector<uint32_t>& ClusterLightIndices() const noexcept { return clusterIndices_; }
    const std::vector<EditorProductionShadowAllocation>& ShadowAllocations() const noexcept {
        return shadowAllocations_;
    }
    const EditorProductionLightingConstants& Constants() const noexcept { return constants_; }
    const EditorProductionLightingStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }
    const EditorProductionLightingPolicy& Policy() const noexcept { return policy_; }

    static uint32_t DepthSlice(float depth, float nearPlane, float farPlane, uint32_t sliceCount) noexcept;

private:
    bool CreateGpuResources(std::string* errorMessage);
    bool CreateShadowResources(std::string* errorMessage);
    bool CreateShadowPipeline(std::string* errorMessage);
    void UploadCpuData();

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sharedSrvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;
    Microsoft::WRL::ComPtr<IDxcBlob> shadowVertexShader_;
    Microsoft::WRL::ComPtr<ID3D12Resource> lightBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> clusterRangeBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> clusterIndexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantsBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> shadowDrawBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> shadowAtlas_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shadowDsvHeap_;
    uint8_t* mappedLights_ = nullptr;
    uint8_t* mappedClusterRanges_ = nullptr;
    uint8_t* mappedClusterIndices_ = nullptr;
    EditorProductionLightingConstants* mappedConstants_ = nullptr;
    uint8_t* mappedShadowDraws_ = nullptr;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> shadowDsvs_;
    D3D12_GPU_DESCRIPTOR_HANDLE shadowAtlasGpu_{};
    D3D12_RESOURCE_STATES shadowAtlasState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    EditorProductionLightingPolicy policy_{};
    EditorProductionLightingConstants constants_{};
    std::vector<EditorProductionGpuLight> lights_;
    std::vector<EditorProductionClusterRange> clusterRanges_;
    std::vector<uint32_t> clusterIndices_;
    std::vector<EditorProductionShadowAllocation> shadowAllocations_;
    EditorProductionLightingStats stats_{};
    std::vector<std::string> diagnostics_;
};

} // namespace editor
