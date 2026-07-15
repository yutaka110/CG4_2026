#pragma once

#include "../EditorAssetRegistry.h"
#include "../material/EditorProductionMaterialPipeline.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace editor {

enum class EditorProductionTextureUsage : uint8_t {
    Albedo,
    Normal,
};

struct EditorProductionTexturePolicy {
    uint64_t gpuBudgetBytes = 128ull * 1024ull * 1024ull;
    uint64_t maxTextureBytes = 64ull * 1024ull * 1024ull;
    uint32_t maxDimension = 16384;
    uint32_t inactiveFrameRetention = 120;
    uint16_t minimumResidentMipCount = 1;
};

struct EditorProductionTextureBinding {
    std::string entityGuid;
    uint32_t materialSlot = 0;
    std::string albedoTextureGuid;
    std::string normalTextureGuid;
    D3D12_GPU_DESCRIPTOR_HANDLE albedoHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE normalHandle{};
    uint16_t albedoFirstResidentMip = 0;
    uint16_t albedoResidentMipCount = 0;
    uint16_t normalFirstResidentMip = 0;
    uint16_t normalResidentMipCount = 0;
    bool albedoFallback = true;
    bool normalFallback = true;
};

struct EditorProductionTexturePipelineStats {
    uint32_t requestedBindings = 0;
    uint32_t requestedTextures = 0;
    uint32_t residentTextures = 0;
    uint32_t residentDescriptors = 0;
    uint32_t fullMipTextures = 0;
    uint32_t partialMipTextures = 0;
    uint32_t fallbackTextures = 0;
    uint32_t pendingGpuRetirements = 0;
    uint64_t residentGpuBytes = 0;
    uint64_t pendingGpuBytes = 0;
    uint64_t uploadedGpuBytes = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    uint64_t evictions = 0;
    uint64_t hotReloads = 0;
    uint64_t gpuBudgetBytes = 0;
};

// E-8 owns a non-overlapping range in the shared shader-visible SRV heap.
// Durable Texture GUIDs are resolved into budgeted, fence-safe GPU residency;
// the authoring Scene and Material documents never retain D3D12 objects.
class EditorProductionTexturePipeline {
public:
    bool Initialize(
        ID3D12Device* device,
        ID3D12DescriptorHeap* shaderVisibleSrvHeap,
        uint32_t descriptorSize,
        uint32_t firstDescriptorIndex,
        uint32_t descriptorCapacity,
        EditorProductionTexturePolicy policy = {},
        std::string* errorMessage = nullptr);
    void Shutdown();

    bool Sync(
        const EditorProductionMaterialPipeline& materials,
        const EditorAssetRegistry& registry,
        ID3D12GraphicsCommandList* uploadCommandList,
        uint64_t completedFenceValue,
        uint64_t scheduledFenceValue,
        std::string* errorMessage = nullptr);

    const EditorProductionTextureBinding* Resolve(
        std::string_view entityGuid,
        uint32_t materialSlot) const;
    const EditorProductionTexturePipelineStats& Stats() const noexcept { return stats_; }
    const std::vector<std::string>& Diagnostics() const noexcept { return diagnostics_; }
    const EditorProductionTexturePolicy& Policy() const noexcept { return policy_; }

    static uint16_t ChooseFirstResidentMip(
        const std::vector<uint64_t>& mipByteSizes,
        uint64_t targetBytes,
        uint16_t minimumResidentMipCount = 1) noexcept;

private:
    struct ResidentTexture {
        std::string guid;
        std::filesystem::path sourcePath;
        EditorProductionTextureUsage usage = EditorProductionTextureUsage::Albedo;
        uint64_t sourceTimestamp = 0;
        uint64_t allocationBytes = 0;
        uint64_t lastUsedFrame = 0;
        uint32_t descriptorIndex = UINT32_MAX;
        uint32_t width = 0;
        uint32_t height = 0;
        uint16_t sourceMipCount = 0;
        uint16_t firstResidentMip = 0;
        uint16_t residentMipCount = 0;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
    };
    struct PendingResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint64_t retireFenceValue = 0;
        uint64_t allocationBytes = 0;
        uint32_t descriptorIndex = UINT32_MAX;
    };
    struct TextureRequest {
        std::string key;
        std::string guid;
        EditorProductionTextureUsage usage = EditorProductionTextureUsage::Albedo;
        const EditorAssetRecord* record = nullptr;
    };

    static std::string MakeKey(std::string_view guid, EditorProductionTextureUsage usage);
    bool EnsureResident(
        const TextureRequest& request,
        uint64_t targetBytes,
        ID3D12GraphicsCommandList* uploadCommandList,
        uint64_t scheduledFenceValue,
        std::string* errorMessage);
    bool EvictOneInactive(
        const std::unordered_map<std::string, bool>& activeKeys,
        uint64_t scheduledFenceValue);
    void Retire(ResidentTexture&& texture, uint64_t scheduledFenceValue);
    void CollectRetired(uint64_t completedFenceValue);
    uint32_t AcquireDescriptor();
    void ReleaseDescriptor(uint32_t descriptorIndex);
    uint64_t ResidentBytes() const noexcept;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
    uint32_t descriptorSize_ = 0;
    uint32_t firstDescriptorIndex_ = 0;
    uint32_t descriptorCapacity_ = 0;
    EditorProductionTexturePolicy policy_{};
    uint64_t frameIndex_ = 0;
    std::vector<uint32_t> freeDescriptors_;
    std::unordered_map<std::string, bool> activeKeys_;
    std::unordered_map<std::string, ResidentTexture> resident_;
    std::vector<PendingResource> pending_;
    std::vector<EditorProductionTextureBinding> bindings_;
    std::vector<std::string> diagnostics_;
    EditorProductionTexturePipelineStats stats_{};
};

} // namespace editor
