#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

#include "core/DescriptorHeap.h"
#include "RailPath.h"
#include "TerrainGenerationSettings.h"
#include "utils/math/Vector.h"
#include "utils/math/MathUtils.h"

namespace ge3::debug {
class DebugDrawSystem;
}

struct TerrainSpawnCandidate {
    Vector3 position{};
    uint32_t kind = 0;
};

struct TerrainVfxZone {
    Vector3 position{};
    float radius = 1.0f;
    float intensity = 1.0f;
    uint32_t kind = 0;
    uint32_t key = 0;
};

struct TerrainRockScatterDebug {
    Vector3 position{};
    Vector3 normal{0.0f, 1.0f, 0.0f};
    float radius = 1.0f;
    uint32_t kind = 0;
};

struct TerrainDebrisInstance {
    Vector3 position{};
    Vector3 tangent{1.0f, 0.0f, 0.0f};
    Vector3 right{0.0f, 0.0f, 1.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
    float radiusA = 0.1f;
    float radiusB = 0.1f;
    float radiusN = 0.1f;
    float shadowRadiusA = 0.2f;
    float shadowRadiusB = 0.1f;
    float contactAo = 0.5f;
    float variation = 0.5f;
    float uvOffset = 0.0f;
    uint32_t seed = 0;
    uint32_t groupKey = 0;
};

struct TerrainDebrisInstanceGpu {
    Vector4 positionLod{};
    Vector4 tangentRadiusA{};
    Vector4 rightRadiusB{};
    Vector4 upRadiusN{};
    Vector4 attributes{};
};

struct TerrainChunkDebugInfo {
    float startDistance = 0.0f;
    float endDistance = 0.0f;
    uint32_t seed = 0;
    uint32_t lodTier = 0;
    std::vector<TerrainSpawnCandidate> spawnCandidates;
    std::vector<TerrainRockScatterDebug> rockScatter;
    std::vector<TerrainDebrisInstance> debrisInstances;
    std::vector<TerrainVfxZone> vfxZones;
};

struct TerrainRenderChunk {
    static constexpr uint32_t kDebrisLodBucketCount = 3;

    float startDistance = 0.0f;
    float endDistance = 0.0f;
    uint32_t seed = 0;
    uint32_t lodTier = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
    TransformationMatrix* mappedTransform = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    D3D12_INDEX_BUFFER_VIEW ibv{};
    Microsoft::WRL::ComPtr<ID3D12Resource> debrisVertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> debrisIndexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> debrisInstanceResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> debrisVisibleInstanceResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> debrisIndirectArgsResource;
    ge3::core::DescriptorHandle debrisInstanceSrv{};
    ge3::core::DescriptorHandle debrisVisibleInstanceUav{};
    ge3::core::DescriptorHandle debrisIndirectArgsUav{};
    D3D12_VERTEX_BUFFER_VIEW debrisVbv{};
    D3D12_VERTEX_BUFFER_VIEW debrisInstanceVbv{};
    D3D12_VERTEX_BUFFER_VIEW debrisVisibleInstanceVbv{};
    D3D12_INDEX_BUFFER_VIEW debrisIbv{};
    Vector3 debrisBoundsCenter{};
    float debrisBoundsRadius = 0.0f;
    D3D12_RESOURCE_STATES debrisVisibleInstanceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    D3D12_RESOURCE_STATES debrisIndirectArgsState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t debrisVertexCount = 0;
    uint32_t debrisIndexCount = 0;
    uint32_t debrisInstanceCount = 0;
    uint32_t debrisInstanceCapacityPerBucket = 0;
};

struct TerrainDebrisCullingStats {
    uint32_t renderChunkCount = 0;
    uint32_t debrisInstanceCount = 0;
    uint32_t eligibleChunkCount = 0;
    uint32_t hiZBuildCount = 0;
    uint32_t hiZMipDispatchCount = 0;
    uint32_t debrisCullDispatchCount = 0;
};

class TerrainChunkManager {
public:
    void Update(
        ID3D12Device* device,
        ge3::core::DescriptorHeap* srvHeap,
        const RailPath& railPath,
        const TerrainGenerationSettings& settings,
        float focusDistance,
        const Matrix4x4& viewProjection);

    void DispatchDebrisCulling(
        ID3D12GraphicsCommandList* commandList,
        ID3D12DescriptorHeap* srvDescriptorHeap,
        ID3D12Resource* sceneDepthResource,
        D3D12_GPU_DESCRIPTOR_HANDLE sceneDepthSrv,
        ID3D12RootSignature* hiZRootSignature,
        ID3D12PipelineState* hiZPipelineState,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* pipelineState,
        const Vector3& cameraPosition,
        const Matrix4x4& viewProjection,
        float maxVisibleDistance,
        int occlusionMip,
        float occlusionStrength,
        float occlusionDepthBias);

    void DrawDebrisIndirect(ID3D12GraphicsCommandList* commandList) const;
    void ResetDebrisCullingStats();
    bool ShouldDispatchDebrisCulling(uint32_t frameInterval);

    void AppendDebugDraw(
        ge3::debug::DebugDrawSystem& debugDraw,
        const RailPath& railPath,
        const TerrainAuthoringState& authoring) const;

    const std::vector<TerrainChunkDebugInfo>& Chunks() const { return chunks_; }
    const std::vector<TerrainRenderChunk>& RenderChunks() const { return renderChunks_; }
    const TerrainDebrisCullingStats& LastDebrisCullingStats() const { return lastDebrisCullingStats_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetHiZDebugSrv(uint32_t level) const;

private:
    void RebuildRenderChunks(
        ID3D12Device* device,
        ge3::core::DescriptorHeap* srvHeap,
        const RailPath& railPath,
        const TerrainGenerationSettings& settings);
    void UpdateChunkTransforms(const Matrix4x4& viewProjection);
    bool HasMatchingRenderChunks() const;
    bool EnsureHiZResources(ID3D12Device* device, ge3::core::DescriptorHeap* srvHeap);
    bool BuildHiZPyramid(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* sceneDepthResource,
        D3D12_GPU_DESCRIPTOR_HANDLE sceneDepthSrv,
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* pipelineState);

    std::vector<TerrainChunkDebugInfo> chunks_;
    std::vector<TerrainRenderChunk> renderChunks_;
    uint32_t renderSettingsHash_ = 0;
    uint32_t chunkCacheSettingsHash_ = 0;
    int32_t cachedFirstChunkIndex_ = -1;
    int32_t cachedLastChunkIndex_ = -1;
    int32_t cachedFocusBucket_ = -1;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> debrisDrawCommandSignature_;

    static constexpr uint32_t kHiZMipCount = 5;
    static constexpr uint32_t kHiZBaseWidth = 256;
    static constexpr uint32_t kHiZBaseHeight = 144;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kHiZMipCount> hiZResources_{};
    std::array<ge3::core::DescriptorHandle, kHiZMipCount> hiZSrvs_{};
    std::array<ge3::core::DescriptorHandle, kHiZMipCount> hiZUavs_{};
    std::array<D3D12_RESOURCE_STATES, kHiZMipCount> hiZStates_{};
    bool hiZResourcesReady_ = false;
    TerrainDebrisCullingStats lastDebrisCullingStats_{};
    uint32_t debrisCullingFrameCounter_ = 0;
};
