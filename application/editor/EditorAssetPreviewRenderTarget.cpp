#include "EditorAssetPreviewRenderTarget.h"

#include <algorithm>

namespace editor {

bool EditorAssetPreviewRenderTargetPool::Initialize(ID3D12Device* device, uint32_t capacity) {
    if (device == nullptr || capacity == 0) {
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = capacity;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)))) {
        return false;
    }

    device_ = device;
    rtvHeap_ = heap;
    descriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    capacity_ = capacity;
    allocatedCount_ = 0;
    reuseCount_ = 0;
    resizeCount_ = 0;
    slots_.clear();
    slots_.resize(capacity_);
    return true;
}

void EditorAssetPreviewRenderTargetPool::Clear() {
    for (Slot& slot : slots_) {
        slot = {};
    }
    allocatedCount_ = 0;
    reuseCount_ = 0;
    resizeCount_ = 0;
}

bool EditorAssetPreviewRenderTargetPool::Allocate(
    uint32_t width,
    uint32_t height,
    EditorAssetPreviewRenderTargetAllocation& outAllocation,
    std::string& outError) {
    if (device_ == nullptr || rtvHeap_ == nullptr || width == 0 || height == 0) {
        outError = "Preview render target pool is not initialized.";
        return false;
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(slots_.size()); ++i) {
        Slot& slot = slots_[i];
        if (slot.inUse) {
            continue;
        }
        if (slot.resource != nullptr && slot.width == width && slot.height == height) {
            slot.inUse = true;
            ++allocatedCount_;
            ++reuseCount_;
            outAllocation.resource = slot.resource;
            outAllocation.rtvCpuHandle = CpuHandle(i);
            outAllocation.rtvIndex = i;
            outAllocation.width = width;
            outAllocation.height = height;
            outAllocation.reused = true;
            outAllocation.initialStateShaderResource = true;
            return true;
        }
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(slots_.size()); ++i) {
        Slot& slot = slots_[i];
        if (slot.inUse) {
            continue;
        }
        const bool resized = slot.resource != nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> target = CreateTarget(width, height, outError);
        if (target == nullptr) {
            return false;
        }

        device_->CreateRenderTargetView(target.Get(), nullptr, CpuHandle(i));
        slot.resource = target;
        slot.width = width;
        slot.height = height;
        slot.inUse = true;
        ++allocatedCount_;
        if (resized) {
            ++resizeCount_;
        }
        outAllocation.resource = target;
        outAllocation.rtvCpuHandle = CpuHandle(i);
        outAllocation.rtvIndex = i;
        outAllocation.width = width;
        outAllocation.height = height;
        outAllocation.reused = false;
        outAllocation.resized = resized;
        outAllocation.initialStateShaderResource = false;
        return true;
    }

    outError = "Preview render target pool is exhausted.";
    return false;
}

void EditorAssetPreviewRenderTargetPool::Release(uint32_t rtvIndex) {
    if (rtvIndex >= slots_.size()) {
        return;
    }
    Slot& slot = slots_[rtvIndex];
    if (!slot.inUse) {
        return;
    }
    slot.inUse = false;
    if (allocatedCount_ > 0) {
        --allocatedCount_;
    }
}

Microsoft::WRL::ComPtr<ID3D12Resource> EditorAssetPreviewRenderTargetPool::CreateTarget(
    uint32_t width,
    uint32_t height,
    std::string& outError) const {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.08f;
    clearValue.Color[1] = 0.09f;
    clearValue.Color[2] = 0.12f;
    clearValue.Color[3] = 1.0f;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> target;
    const HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&target));
    if (FAILED(hr)) {
        outError = "Preview render target resource allocation failed.";
        return nullptr;
    }
    return target;
}

D3D12_CPU_DESCRIPTOR_HANDLE EditorAssetPreviewRenderTargetPool::CpuHandle(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize_) * index;
    return handle;
}

} // namespace editor
