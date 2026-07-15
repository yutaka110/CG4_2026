#include "core/DescriptorHeap.h"

#include <algorithm>

using namespace ge3::core;

D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeap::ToNative(HeapKind k) {
    switch (k) {
    case HeapKind::RTV:         return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    case HeapKind::DSV:         return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    case HeapKind::CBV_SRV_UAV: return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    }
    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
}

void DescriptorHeap::Initialize(ID3D12Device* device, HeapKind kind, uint32_t capacity, bool shaderVisible) {
    assert(device);
    capacity_ = capacity;
    shaderVisible_ = (kind == HeapKind::CBV_SRV_UAV) ? shaderVisible : false;
    type_ = ToNative(kind);

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = type_;
    desc.NumDescriptors = capacity_;
    desc.Flags = shaderVisible_ ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_));
    assert(SUCCEEDED(hr));

    incrementSize_ = device->GetDescriptorHandleIncrementSize(type_);
    cpuStart_ = heap_->GetCPUDescriptorHandleForHeapStart();
    gpuStart_.ptr = shaderVisible_ ? heap_->GetGPUDescriptorHandleForHeapStart().ptr : 0;

    while (!freeList_.empty()) freeList_.pop();
    reservedRanges_.clear();
    nextAllocate_ = 0;
}

void DescriptorHeap::Finalize() {
    while (!freeList_.empty()) freeList_.pop();
    reservedRanges_.clear();
    heap_.Reset();
    capacity_ = 0;
    incrementSize_ = 0;
    shaderVisible_ = false;
    cpuStart_.ptr = 0;
    gpuStart_.ptr = 0;
    nextAllocate_ = 0;
}

DescriptorHandle DescriptorHeap::Allocate() {
    while (!freeList_.empty() && IsReserved(freeList_.front())) {
        freeList_.pop();
    }
    uint32_t index;
    if (!freeList_.empty()) { index = freeList_.front(); freeList_.pop(); }
    else {
        SkipReserved();
        index = nextAllocate_++;
        assert(index < capacity_ && "DescriptorHeap exhausted");
    }

    DescriptorHandle h;
    h.index = index;
    h.cpu = { cpuStart_.ptr + SIZE_T(index) * SIZE_T(incrementSize_) };
    h.gpu.ptr = shaderVisible_ ? (gpuStart_.ptr + UINT64(index) * UINT64(incrementSize_)) : 0;
    return h;
}

void DescriptorHeap::Reserve(uint32_t count) {
    assert(count <= capacity_);
    if (nextAllocate_ < count) {
        nextAllocate_ = count;
        SkipReserved();
    }
}

void DescriptorHeap::ReserveRange(uint32_t firstIndex, uint32_t count) {
    assert(firstIndex <= capacity_ && count <= capacity_ - firstIndex);
    if (count == 0) return;
    reservedRanges_.push_back({firstIndex, firstIndex + count});
    std::sort(reservedRanges_.begin(), reservedRanges_.end());
    std::vector<std::pair<uint32_t, uint32_t>> merged;
    for (const auto& range : reservedRanges_) {
        if (merged.empty() || merged.back().second < range.first) {
            merged.push_back(range);
        } else {
            merged.back().second = (std::max)(merged.back().second, range.second);
        }
    }
    reservedRanges_ = std::move(merged);
    std::queue<uint32_t> available;
    while (!freeList_.empty()) {
        const uint32_t index = freeList_.front();
        freeList_.pop();
        if (!IsReserved(index)) available.push(index);
    }
    freeList_ = std::move(available);
    SkipReserved();
}

void DescriptorHeap::Free(uint32_t index) {
    assert(index < capacity_);
    assert(!IsReserved(index) && "Cannot free a statically reserved descriptor index");
    freeList_.push(index);
}

bool DescriptorHeap::IsReserved(uint32_t index) const {
    for (const auto& range : reservedRanges_) {
        if (index < range.first) return false;
        if (index < range.second) return true;
    }
    return false;
}

void DescriptorHeap::SkipReserved() {
    for (;;) {
        bool advanced = false;
        for (const auto& range : reservedRanges_) {
            if (nextAllocate_ >= range.first && nextAllocate_ < range.second) {
                nextAllocate_ = range.second;
                advanced = true;
                break;
            }
        }
        if (!advanced) return;
    }
}

DescriptorHandle DescriptorHeap::GetHandle(uint32_t index) const {
    assert(index < capacity_);
    DescriptorHandle h;
    h.index = index;
    h.cpu = { cpuStart_.ptr + SIZE_T(index) * SIZE_T(incrementSize_) };
    h.gpu.ptr = shaderVisible_ ? (gpuStart_.ptr + UINT64(index) * UINT64(incrementSize_)) : 0;
    return h;
}
