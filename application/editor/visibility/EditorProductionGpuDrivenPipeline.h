#pragma once

#include "../material/EditorProductionMaterialPipeline.h"
#include "../scene/EditorProductionScenePipeline.h"
#include "../shader/EditorProductionShaderPipeline.h"
#include "../texture/EditorProductionTexturePipeline.h"

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace editor {

struct EditorProductionIndirectCommandLayout {
    D3D12_GPU_VIRTUAL_ADDRESS transformAddress = 0;
    D3D12_DRAW_INDEXED_ARGUMENTS draw{};
    uint32_t stridePadding = 0;
};
static_assert(offsetof(EditorProductionIndirectCommandLayout, draw) == 8);
static_assert(sizeof(EditorProductionIndirectCommandLayout) == 32);

struct EditorProductionGpuDrivenPolicy {
    uint32_t maximumInstances = 4096;
    uint32_t maximumBatches = 512;
    float occlusionDepthBias = 0.003f;
    bool enableOcclusion = true;
};

struct EditorProductionGpuDrivenBatchRange {
    uint32_t commandOffset = 0;
    uint32_t commandCapacity = 0;
};

struct EditorProductionGpuDrivenBatch {
    EditorProductionSceneRenderPacket representative{};
    EditorProductionGpuDrivenBatchRange range{};
    ID3D12PipelineState* pipelineState = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS materialAddress = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE albedoHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE normalHandle{};
};

struct EditorProductionGpuDrivenStats {
    uint32_t submittedInstances = 0;
    uint32_t residentInstances = 0;
    uint32_t batches = 0;
    uint32_t gpuVisibleInstances = 0;
    uint32_t cpuFallbackPackets = 0;
    uint32_t rejectedByInstanceBudget = 0;
    uint32_t rejectedByBatchBudget = 0;
    uint32_t dispatches = 0;
    uint32_t readbacks = 0;
    uint32_t ringStalls = 0;
    bool occlusionEnabled = false;
    bool commandLayoutValidated = false;
    bool ready = false;
};

// E-11 frame-transient visibility bridge. Durable Scene/Asset documents never
// own instance buffers, command signatures, count buffers, or readback state.
class EditorProductionGpuDrivenPipeline {
public:
    EditorProductionGpuDrivenPipeline();
    ~EditorProductionGpuDrivenPipeline();
    bool Initialize(
        ID3D12Device* device,
        ID3D12DescriptorHeap* sharedSrvHeap,
        uint32_t descriptorSize,
        uint32_t fallbackHiZDescriptorIndex,
        ID3D12RootSignature* mainRootSignature,
        EditorProductionGpuDrivenPolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(
        const std::vector<EditorProductionSceneRenderPacket>& candidates,
        const EditorProductionMaterialPipeline& materials,
        const EditorProductionTexturePipeline& textures,
        const EditorProductionShaderPipeline& shaders,
        const Matrix4x4& viewProjection,
        uint64_t completedFenceValue,
        uint64_t scheduledFenceValue,
        std::string* errorMessage = nullptr);

    bool DispatchVisibility(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_DESCRIPTOR_HANDLE hiZHandle,
        bool hiZAvailable);
    void ExecuteBatch(ID3D12GraphicsCommandList* commandList, uint32_t batchIndex) const;
    void RecordReadback(ID3D12GraphicsCommandList* commandList);

    const std::vector<EditorProductionGpuDrivenBatch>& Batches() const noexcept { return batches_; }
    const std::vector<EditorProductionSceneRenderPacket>& CpuFallbackPackets() const noexcept {
        return cpuFallbackPackets_;
    }
    const EditorProductionGpuDrivenStats& Stats() const noexcept { return stats_; }
    const EditorProductionGpuDrivenPolicy& Policy() const noexcept { return policy_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }
    bool Ready() const noexcept { return stats_.ready && activeFrame_ >= 0; }

    static std::vector<EditorProductionGpuDrivenBatchRange> BuildBatchRanges(
        const std::vector<uint32_t>& batchIndices,
        uint32_t batchCount);

private:
    struct GpuInstance;
    struct GpuBatch;
    struct GpuConstants;
    struct FrameResources;

    bool CreatePipeline(std::string* errorMessage);
    bool CreateFrameResources(FrameResources& frame, std::string* errorMessage);
    void CollectReadbacks(uint64_t completedFenceValue);

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> fallbackHiZ_;
    D3D12_GPU_DESCRIPTOR_HANDLE fallbackHiZHandle_{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> resetPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> cullPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;
    Microsoft::WRL::ComPtr<IDxcBlob> resetShader_;
    Microsoft::WRL::ComPtr<IDxcBlob> cullShader_;
    std::array<std::unique_ptr<FrameResources>, 3> frames_{};
    int activeFrame_ = -1;
    uint64_t scheduledFenceValue_ = 0;
    EditorProductionGpuDrivenPolicy policy_{};
    std::vector<EditorProductionGpuDrivenBatch> batches_;
    std::vector<EditorProductionSceneRenderPacket> cpuFallbackPackets_;
    EditorProductionGpuDrivenStats stats_{};
    std::vector<std::string> diagnostics_;
};

} // namespace editor
