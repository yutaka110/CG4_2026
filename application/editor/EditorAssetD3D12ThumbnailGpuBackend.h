#pragma once

#include "EditorAssetGpuThumbnailRenderer.h"
#include "EditorAssetPreviewRenderTarget.h"
#include "EditorAssetThumbnailCache.h"
#include "EditorThumbnailUploadRetirementQueue.h"
#include "core/ShaderCompiler.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor {

class EditorAssetD3D12ThumbnailGpuBackend final : public EditorAssetGpuThumbnailBackend {
public:
    bool Initialize(
        ID3D12Device* device,
        ID3D12DescriptorHeap* srvHeap,
        uint32_t descriptorSize,
        uint32_t firstDescriptorIndex,
        uint32_t descriptorCapacity);
    void SetUploadCommandList(ID3D12GraphicsCommandList* commandList);
    void SetFrameFenceValues(uint64_t completedFenceValue, uint64_t scheduledFenceValue);
    void ConfigureCache(EditorAssetThumbnailCachePolicy policy);
    uint32_t RetireCompletedUploads(uint64_t completedFenceValue);

    bool AllocateThumbnail(
        const EditorAssetGpuThumbnailAllocationRequest& request,
        EditorAssetGpuThumbnailAllocation& outAllocation,
        std::string& outError) override;
    void ReleaseThumbnail(std::string_view key, uint64_t resourceId) override;
    EditorAssetGpuThumbnailBackendTelemetry Telemetry() const override;

    uint32_t Capacity() const { return descriptorCapacity_; }
    uint32_t AllocatedCount() const { return allocatedCount_; }
    uint32_t FirstDescriptorIndex() const { return firstDescriptorIndex_; }

private:
    struct PreviewMaterialTextureTable {
        uint32_t baseDescriptorIndex = UINT32_MAX;
        uint32_t descriptorCount = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textureResources;
        std::vector<bool> boundSlots;
        uint32_t boundCount = 0;
        uint32_t fallbackCount = 0;
    };

    uint32_t AllocateDescriptorIndex();
    uint32_t AllocateDescriptorRange(uint32_t count);
    void FreeDescriptorIndex(uint32_t descriptorIndex);
    void FreeDescriptorRange(uint32_t firstDescriptorIndex, uint32_t count);
    void RetainUploadResource(Microsoft::WRL::ComPtr<ID3D12Resource> resource, uint64_t byteSize);
    uint32_t ActivePreviewMaterialDescriptorCount() const;
    bool EnsurePreviewMeshPipeline(std::string& outError);
    bool TryDrawRendererBackedMeshPreview(
        const EditorAssetGpuThumbnailAllocationRequest& request,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv,
        D3D12_GPU_DESCRIPTOR_HANDLE fallbackSrvHandle,
        uint32_t width,
        uint32_t height,
        bool& outProceduralFallback,
        std::string& outDetail,
        std::string& outError);
    bool TryCreatePreviewMaterialTextureTable(
        const std::string& key,
        const std::vector<std::string>& texturePaths,
        PreviewMaterialTextureTable& outTable,
        std::string& outError);
    bool TryPublishPreviewSceneRenderTarget(
        const EditorAssetGpuThumbnailAllocationRequest& request,
        uint32_t descriptorIndex,
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outResource,
        uint32_t& outWidth,
        uint32_t& outHeight,
        std::string& outDetail,
        std::string& outError);
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(uint32_t descriptorIndex) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(uint32_t descriptorIndex) const;

    ID3D12Device* device_ = nullptr;
    ID3D12DescriptorHeap* srvHeap_ = nullptr;
    ID3D12GraphicsCommandList* uploadCommandList_ = nullptr;
    uint32_t descriptorSize_ = 0;
    uint32_t firstDescriptorIndex_ = 0;
    uint32_t descriptorCapacity_ = 0;
    uint32_t nextLocalIndex_ = 0;
    uint32_t allocatedCount_ = 0;
    std::vector<uint32_t> freeDescriptorIndices_;
    std::unordered_map<std::string, uint32_t> keyToDescriptorIndex_;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> keyToTextureResource_;
    std::unordered_map<std::string, uint32_t> keyToPreviewRtvIndex_;
    std::unordered_map<std::string, uint32_t> keyToPreviewMaterialDescriptorIndex_;
    std::unordered_map<std::string, uint32_t> keyToPreviewMaterialDescriptorCount_;
    std::unordered_map<std::string, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>> keyToPreviewMaterialTextureResources_;
    EditorAssetPreviewRenderTargetPool previewRenderTargets_;
    EditorThumbnailUploadRetirementQueue uploadRetirementQueue_;
    EditorAssetThumbnailCacheStore cacheStore_;
    EditorAssetGpuThumbnailBackendTelemetry telemetry_{};
    ge3::core::ShaderCompiler previewShaderCompiler_{};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> previewMeshRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> previewMeshPipelineState_;
    uint64_t completedFenceValue_ = 0;
    uint64_t scheduledFenceValue_ = 0;
};

} // namespace editor
