#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

namespace editor {

struct EditorAssetPreviewRenderTargetAllocation {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuHandle{};
    uint32_t rtvIndex = UINT32_MAX;
    uint32_t width = 0;
    uint32_t height = 0;
    bool reused = false;
    bool resized = false;
    bool initialStateShaderResource = false;
};

class EditorAssetPreviewRenderTargetPool {
public:
    bool Initialize(ID3D12Device* device, uint32_t capacity);
    void Clear();

    bool Allocate(
        uint32_t width,
        uint32_t height,
        EditorAssetPreviewRenderTargetAllocation& outAllocation,
        std::string& outError);
    void Release(uint32_t rtvIndex);

    uint32_t Capacity() const { return capacity_; }
    uint32_t AllocatedCount() const { return allocatedCount_; }
    uint64_t ReuseCount() const { return reuseCount_; }
    uint64_t ResizeCount() const { return resizeCount_; }

private:
    struct Slot {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint32_t width = 0;
        uint32_t height = 0;
        bool inUse = false;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTarget(
        uint32_t width,
        uint32_t height,
        std::string& outError) const;
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(uint32_t index) const;

    ID3D12Device* device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    uint32_t descriptorSize_ = 0;
    uint32_t capacity_ = 0;
    uint32_t allocatedCount_ = 0;
    uint64_t reuseCount_ = 0;
    uint64_t resizeCount_ = 0;
    std::vector<Slot> slots_;
};

} // namespace editor
