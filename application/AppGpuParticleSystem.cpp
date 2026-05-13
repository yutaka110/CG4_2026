#include "AppGpuParticleSystem.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace {
struct ParticleForGpuLayout {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
};

struct TrailHistoryPointLayout {
    Vector4 position;
};

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device* device,
    size_t size,
    D3D12_RESOURCE_FLAGS flags) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = (size + 0xFF) & ~static_cast<size_t>(0xFF);
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    if (FAILED(device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource)))) {
        return nullptr;
    }
    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* device, size_t size) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = (size + 0xFF) & ~static_cast<size_t>(0xFF);
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    if (FAILED(device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&resource)))) {
        return nullptr;
    }
    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device* device, size_t size) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = (size + 0xFF) & ~static_cast<size_t>(0xFF);
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    if (FAILED(device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&resource)))) {
        return nullptr;
    }
    return resource;
}

D3D12_RESOURCE_BARRIER MakeTransition(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

bool EnsureDefaultBuffer(
    ID3D12Device* device,
    const ge3::graphics::TransientBufferDesc& desc,
    Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
    size_t& currentBytes,
    D3D12_RESOURCE_STATES& currentState) {
    if (resource != nullptr && currentBytes == desc.sizeInBytes) {
        return true;
    }

    resource = CreateDefaultBuffer(device, desc.sizeInBytes, desc.flags);
    if (resource == nullptr) {
        currentBytes = 0;
        return false;
    }

    currentBytes = desc.sizeInBytes;
    currentState = desc.initialState;
    return true;
}
} // namespace

size_t AppGpuParticleSystem::ParticleRenderBufferBytes(uint32_t maxParticles) {
    return sizeof(ParticleForGpuLayout) * maxParticles;
}

size_t AppGpuParticleSystem::ParticleStateBytes(uint32_t maxParticles) {
    return sizeof(ParticleStateSample) * maxParticles;
}

size_t AppGpuParticleSystem::TrailControlPointBufferBytes(uint32_t maxSegments) {
    return sizeof(TrailMeshStreamControlPointSample) * (static_cast<size_t>(maxSegments) + 1);
}

size_t AppGpuParticleSystem::TrailSegmentBufferBytes(uint32_t maxSegments) {
    return sizeof(TrailMeshStreamSegmentSample) * maxSegments;
}

size_t AppGpuParticleSystem::TrailVertexBufferBytes(uint32_t maxSegments) {
    return sizeof(TrailMeshStreamVertexSample) * (static_cast<size_t>(maxSegments) + 1) * 2;
}

size_t AppGpuParticleSystem::TrailIndexBufferBytes(uint32_t maxSegments) {
    return sizeof(uint32_t) * static_cast<size_t>(maxSegments) * 6;
}

size_t AppGpuParticleSystem::TrailDrawArgsBufferBytes() {
    return sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
}

size_t AppGpuParticleSystem::TrailHistoryBufferBytes(uint32_t maxControlPoints) {
    return sizeof(TrailHistoryPointLayout) * maxControlPoints;
}

bool AppGpuParticleSystem::CreateParticleOutputViews(ID3D12Device* device) {
    if (device == nullptr || particleOutput_ == nullptr || !particleSrv_.IsValid() || !particleUav_.IsValid()) {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = maxParticles_;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGpuLayout);
    device->CreateShaderResourceView(particleOutput_.Get(), &srvDesc, particleSrv_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC outputUav{};
    outputUav.Format = DXGI_FORMAT_UNKNOWN;
    outputUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    outputUav.Buffer.NumElements = maxParticles_;
    outputUav.Buffer.StructureByteStride = sizeof(ParticleForGpuLayout);
    device->CreateUnorderedAccessView(particleOutput_.Get(), nullptr, &outputUav, particleUav_.cpu);
    return true;
}

bool AppGpuParticleSystem::CreateDedicatedParticleOutputViews(ID3D12Device* device) {
    if (device == nullptr ||
        dedicatedParticleOutput_ == nullptr ||
        !dedicatedParticleSrv_.IsValid() ||
        !dedicatedParticleUav_.IsValid()) {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = maxParticles_;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGpuLayout);
    device->CreateShaderResourceView(dedicatedParticleOutput_.Get(), &srvDesc, dedicatedParticleSrv_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC outputUav{};
    outputUav.Format = DXGI_FORMAT_UNKNOWN;
    outputUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    outputUav.Buffer.NumElements = maxParticles_;
    outputUav.Buffer.StructureByteStride = sizeof(ParticleForGpuLayout);
    device->CreateUnorderedAccessView(
        dedicatedParticleOutput_.Get(),
        nullptr,
        &outputUav,
        dedicatedParticleUav_.cpu);
    return true;
}

bool AppGpuParticleSystem::CreateDedicatedDistortionOutputViews(ID3D12Device* device) {
    if (device == nullptr ||
        dedicatedDistortionOutput_ == nullptr ||
        !dedicatedDistortionSrv_.IsValid() ||
        !dedicatedDistortionUav_.IsValid()) {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = maxParticles_;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGpuLayout);
    device->CreateShaderResourceView(dedicatedDistortionOutput_.Get(), &srvDesc, dedicatedDistortionSrv_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC outputUav{};
    outputUav.Format = DXGI_FORMAT_UNKNOWN;
    outputUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    outputUav.Buffer.NumElements = maxParticles_;
    outputUav.Buffer.StructureByteStride = sizeof(ParticleForGpuLayout);
    device->CreateUnorderedAccessView(
        dedicatedDistortionOutput_.Get(),
        nullptr,
        &outputUav,
        dedicatedDistortionUav_.cpu);
    return true;
}

bool AppGpuParticleSystem::CreateDedicatedBeamOutputViews(ID3D12Device* device) {
    if (device == nullptr ||
        dedicatedBeamOutput_ == nullptr ||
        !dedicatedBeamSrv_.IsValid() ||
        !dedicatedBeamUav_.IsValid()) {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = maxParticles_;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGpuLayout);
    device->CreateShaderResourceView(dedicatedBeamOutput_.Get(), &srvDesc, dedicatedBeamSrv_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC outputUav{};
    outputUav.Format = DXGI_FORMAT_UNKNOWN;
    outputUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    outputUav.Buffer.NumElements = maxParticles_;
    outputUav.Buffer.StructureByteStride = sizeof(ParticleForGpuLayout);
    device->CreateUnorderedAccessView(
        dedicatedBeamOutput_.Get(),
        nullptr,
        &outputUav,
        dedicatedBeamUav_.cpu);
    return true;
}

bool AppGpuParticleSystem::CreateTrailMeshStreamViews(ID3D12Device* device) {
    if (device == nullptr ||
        trailControlPointBuffer_ == nullptr ||
        trailSegmentBuffer_ == nullptr ||
        trailVertexBuffer_ == nullptr ||
        trailIndexBuffer_ == nullptr ||
        trailDrawArgs_ == nullptr ||
        !trailControlPointSrv_.IsValid() ||
        !trailControlPointUav_.IsValid() ||
        !trailSegmentSrv_.IsValid() ||
        !trailSegmentUav_.IsValid() ||
        !trailVertexSrv_.IsValid() ||
        !trailVertexUav_.IsValid() ||
        !trailIndexSrv_.IsValid() ||
        !trailIndexUav_.IsValid() ||
        !trailDrawArgsUav_.IsValid()) {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC controlSrv{};
    controlSrv.Format = DXGI_FORMAT_UNKNOWN;
    controlSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    controlSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    controlSrv.Buffer.NumElements = maxParticles_ + 1;
    controlSrv.Buffer.StructureByteStride = sizeof(TrailMeshStreamControlPointSample);
    device->CreateShaderResourceView(trailControlPointBuffer_.Get(), &controlSrv, trailControlPointSrv_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC controlUav{};
    controlUav.Format = DXGI_FORMAT_UNKNOWN;
    controlUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    controlUav.Buffer.NumElements = maxParticles_ + 1;
    controlUav.Buffer.StructureByteStride = sizeof(TrailMeshStreamControlPointSample);
    device->CreateUnorderedAccessView(trailControlPointBuffer_.Get(), nullptr, &controlUav, trailControlPointUav_.cpu);

    D3D12_SHADER_RESOURCE_VIEW_DESC segmentSrv{};
    segmentSrv.Format = DXGI_FORMAT_UNKNOWN;
    segmentSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    segmentSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    segmentSrv.Buffer.NumElements = maxParticles_;
    segmentSrv.Buffer.StructureByteStride = sizeof(TrailMeshStreamSegmentSample);
    device->CreateShaderResourceView(trailSegmentBuffer_.Get(), &segmentSrv, trailSegmentSrv_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC segmentUav{};
    segmentUav.Format = DXGI_FORMAT_UNKNOWN;
    segmentUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    segmentUav.Buffer.NumElements = maxParticles_;
    segmentUav.Buffer.StructureByteStride = sizeof(TrailMeshStreamSegmentSample);
    device->CreateUnorderedAccessView(trailSegmentBuffer_.Get(), nullptr, &segmentUav, trailSegmentUav_.cpu);

    D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrv{};
    vertexSrv.Format = DXGI_FORMAT_UNKNOWN;
    vertexSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    vertexSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    vertexSrv.Buffer.NumElements = (maxParticles_ + 1) * 2;
    vertexSrv.Buffer.StructureByteStride = sizeof(TrailMeshStreamVertexSample);
    device->CreateShaderResourceView(trailVertexBuffer_.Get(), &vertexSrv, trailVertexSrv_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC vertexUav{};
    vertexUav.Format = DXGI_FORMAT_UNKNOWN;
    vertexUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    vertexUav.Buffer.NumElements = (maxParticles_ + 1) * 2;
    vertexUav.Buffer.StructureByteStride = sizeof(TrailMeshStreamVertexSample);
    device->CreateUnorderedAccessView(trailVertexBuffer_.Get(), nullptr, &vertexUav, trailVertexUav_.cpu);

    D3D12_SHADER_RESOURCE_VIEW_DESC indexSrv{};
    indexSrv.Format = DXGI_FORMAT_UNKNOWN;
    indexSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    indexSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    indexSrv.Buffer.NumElements = maxParticles_ * 6;
    indexSrv.Buffer.StructureByteStride = sizeof(uint32_t);
    device->CreateShaderResourceView(trailIndexBuffer_.Get(), &indexSrv, trailIndexSrv_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC indexUav{};
    indexUav.Format = DXGI_FORMAT_UNKNOWN;
    indexUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    indexUav.Buffer.NumElements = maxParticles_ * 6;
    indexUav.Buffer.StructureByteStride = sizeof(uint32_t);
    device->CreateUnorderedAccessView(trailIndexBuffer_.Get(), nullptr, &indexUav, trailIndexUav_.cpu);

    D3D12_UNORDERED_ACCESS_VIEW_DESC drawArgsUav{};
    drawArgsUav.Format = DXGI_FORMAT_UNKNOWN;
    drawArgsUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    drawArgsUav.Buffer.NumElements = 1;
    drawArgsUav.Buffer.StructureByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    device->CreateUnorderedAccessView(trailDrawArgs_.Get(), nullptr, &drawArgsUav, trailDrawArgsUav_.cpu);
    return true;
}

bool AppGpuParticleSystem::CreateTrailHistoryView(ID3D12Device* device) {
    if (device == nullptr || trailHistoryBuffer_ == nullptr || !trailHistorySrv_.IsValid()) {
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC historySrv{};
    historySrv.Format = DXGI_FORMAT_UNKNOWN;
    historySrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    historySrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    historySrv.Buffer.NumElements = maxParticles_ + 1;
    historySrv.Buffer.StructureByteStride = sizeof(TrailHistoryPointLayout);
    device->CreateShaderResourceView(trailHistoryBuffer_.Get(), &historySrv, trailHistorySrv_.cpu);
    return true;
}

bool AppGpuParticleSystem::Initialize(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList,
    ge3::core::DescriptorHeapSet& heaps,
    uint32_t maxParticles) {
    if (initialized_) {
        return true;
    }
    if (device == nullptr || commandList == nullptr || maxParticles == 0) {
        return false;
    }

    maxParticles_ = maxParticles;
    const size_t outputBytes = ParticleRenderBufferBytes(maxParticles_);
    const size_t stateBytes = ParticleStateBytes(maxParticles_);

    particleState_ = CreateDefaultBuffer(
        device,
        stateBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    uploadState_ = CreateUploadBuffer(device, stateBytes);
    indirectArgs_ = CreateDefaultBuffer(device, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), D3D12_RESOURCE_FLAG_NONE);
    uploadIndirectArgs_ = CreateUploadBuffer(device, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
    trailHistoryBuffer_ = CreateUploadBuffer(device, TrailHistoryBufferBytes(maxParticles_ + 1));
    trailViewProjection_ = CreateUploadBuffer(device, sizeof(Matrix4x4));
    trailControlPointReadback_ = CreateReadbackBuffer(
        device,
        sizeof(TrailMeshStreamControlPointSample) * kTrailTelemetryControlPointSampleCount);
    trailSegmentReadback_ = CreateReadbackBuffer(
        device,
        sizeof(TrailMeshStreamSegmentSample) * kTrailTelemetrySegmentSampleCount);
    trailDrawArgsReadback_ = CreateReadbackBuffer(device, TrailDrawArgsBufferBytes());
    trailVertexReadback_ = CreateReadbackBuffer(
        device,
        sizeof(TrailMeshStreamVertexSample) * kTrailTelemetryVertexSampleCount);
    trailIndexReadback_ = CreateReadbackBuffer(
        device,
        sizeof(uint32_t) * kTrailTelemetryIndexSampleCount);
    particleDedicatedRenderReadback_ = CreateReadbackBuffer(
        device,
        sizeof(ParticleRenderSample) * kParticleDedicatedTelemetrySampleCount);
    particleDedicatedStateReadback_ = CreateReadbackBuffer(
        device,
        sizeof(ParticleStateSample) * kParticleDedicatedTelemetrySampleCount);
    particleDedicatedIndirectArgsReadback_ = CreateReadbackBuffer(
        device,
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
    distortionDedicatedRenderReadback_ = CreateReadbackBuffer(
        device,
        sizeof(ParticleRenderSample) * kParticleDedicatedTelemetrySampleCount);
    distortionDedicatedIndirectArgsReadback_ = CreateReadbackBuffer(
        device,
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
    beamDedicatedRenderReadback_ = CreateReadbackBuffer(
        device,
        sizeof(ParticleRenderSample) * kParticleDedicatedTelemetrySampleCount);
    beamDedicatedIndirectArgsReadback_ = CreateReadbackBuffer(
        device,
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
    if (particleState_ == nullptr ||
        uploadState_ == nullptr ||
        indirectArgs_ == nullptr ||
        uploadIndirectArgs_ == nullptr ||
        trailHistoryBuffer_ == nullptr ||
        trailViewProjection_ == nullptr ||
        trailControlPointReadback_ == nullptr ||
        trailSegmentReadback_ == nullptr ||
        trailDrawArgsReadback_ == nullptr ||
        trailVertexReadback_ == nullptr ||
        trailIndexReadback_ == nullptr ||
        particleDedicatedRenderReadback_ == nullptr ||
        particleDedicatedStateReadback_ == nullptr ||
        particleDedicatedIndirectArgsReadback_ == nullptr ||
        distortionDedicatedRenderReadback_ == nullptr ||
        distortionDedicatedIndirectArgsReadback_ == nullptr ||
        beamDedicatedRenderReadback_ == nullptr ||
        beamDedicatedIndirectArgsReadback_ == nullptr) {
        return false;
    }

    if (FAILED(trailHistoryBuffer_->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedTrailHistory_)))) {
        mappedTrailHistory_ = nullptr;
        return false;
    }
    for (uint32_t i = 0; i < maxParticles_ + 1; ++i) {
        mappedTrailHistory_[i] = {0.0f, 0.0f, 0.0f, 1.0f};
    }

    if (FAILED(trailViewProjection_->Map(
            0,
            nullptr,
            reinterpret_cast<void**>(&mappedTrailViewProjection_)))) {
        mappedTrailViewProjection_ = nullptr;
        return false;
    }
    *mappedTrailViewProjection_ = MakeIdentity4x4();

    std::vector<ParticleStateSample> initial(maxParticles_);
    for (uint32_t i = 0; i < maxParticles_; ++i) {
        const float seed = static_cast<float>((i * 1664525u + 1013904223u) & 0xffffu) / 65535.0f;
        initial[i].position = {
            (static_cast<float>(i % 256) / 255.0f - 0.5f) * 8.0f,
            (static_cast<float>((i / 256) % 256) / 255.0f - 0.5f) * 4.0f,
            2.0f + seed * 4.0f};
        initial[i].velocity = {
            seed * 0.6f - 0.3f,
            0.4f + seed * 0.8f,
            seed * 0.4f - 0.2f};
        initial[i].lifetime = 2.0f + seed * 3.0f;
        initial[i].age = seed * initial[i].lifetime;
        initial[i].color = {0.25f + seed * 0.75f, 0.55f, 1.0f, 1.0f};
        initial[i].scale = {0.08f + seed * 0.08f, 0.08f + seed * 0.08f, 1.0f};
        initial[i].seed = seed;
    }

    void* mapped = nullptr;
    if (SUCCEEDED(uploadState_->Map(0, nullptr, &mapped))) {
        std::memcpy(mapped, initial.data(), stateBytes);
        uploadState_->Unmap(0, nullptr);
    }
    D3D12_RESOURCE_BARRIER stateCopy =
        MakeTransition(particleState_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &stateCopy);
    particleStateState_ = D3D12_RESOURCE_STATE_COPY_DEST;

    commandList->CopyBufferRegion(particleState_.Get(), 0, uploadState_.Get(), 0, stateBytes);
    D3D12_RESOURCE_BARRIER stateReady =
        MakeTransition(particleState_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->ResourceBarrier(1, &stateReady);
    particleStateState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    particleOutputState_ = D3D12_RESOURCE_STATE_COMMON;

    D3D12_DRAW_INDEXED_ARGUMENTS args{};
    args.IndexCountPerInstance = 6;
    args.InstanceCount = maxParticles_;
    void* argsMapped = nullptr;
    if (SUCCEEDED(uploadIndirectArgs_->Map(0, nullptr, &argsMapped))) {
        std::memcpy(argsMapped, &args, sizeof(args));
        uploadIndirectArgs_->Unmap(0, nullptr);
    }
    D3D12_RESOURCE_BARRIER argsCopy =
        MakeTransition(indirectArgs_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &argsCopy);
    indirectArgsState_ = D3D12_RESOURCE_STATE_COPY_DEST;
    commandList->CopyBufferRegion(indirectArgs_.Get(), 0, uploadIndirectArgs_.Get(), 0, sizeof(args));
    D3D12_RESOURCE_BARRIER argsReady =
        MakeTransition(indirectArgs_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    commandList->ResourceBarrier(1, &argsReady);
    indirectArgsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

    particleSrv_ = heaps.srv.Allocate();
    particleUav_ = heaps.srv.Allocate();
    dedicatedParticleSrv_ = heaps.srv.Allocate();
    dedicatedParticleUav_ = heaps.srv.Allocate();
    dedicatedDistortionSrv_ = heaps.srv.Allocate();
    dedicatedDistortionUav_ = heaps.srv.Allocate();
    dedicatedBeamSrv_ = heaps.srv.Allocate();
    dedicatedBeamUav_ = heaps.srv.Allocate();
    trailControlPointSrv_ = heaps.srv.Allocate();
    trailControlPointUav_ = heaps.srv.Allocate();
    trailSegmentSrv_ = heaps.srv.Allocate();
    trailSegmentUav_ = heaps.srv.Allocate();
    trailVertexSrv_ = heaps.srv.Allocate();
    trailVertexUav_ = heaps.srv.Allocate();
    trailIndexSrv_ = heaps.srv.Allocate();
    trailIndexUav_ = heaps.srv.Allocate();
    trailDrawArgsUav_ = heaps.srv.Allocate();
    trailHistorySrv_ = heaps.srv.Allocate();
    stateUav_ = heaps.srv.Allocate();
    dedicatedStateUav_ = heaps.srv.Allocate();
    dedicatedDistortionStateUav_ = heaps.srv.Allocate();
    dedicatedBeamStateUav_ = heaps.srv.Allocate();

    D3D12_UNORDERED_ACCESS_VIEW_DESC stateUav{};
    stateUav.Format = DXGI_FORMAT_UNKNOWN;
    stateUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    stateUav.Buffer.NumElements = maxParticles_;
    stateUav.Buffer.StructureByteStride = sizeof(ParticleStateSample);
    device->CreateUnorderedAccessView(particleState_.Get(), nullptr, &stateUav, stateUav_.cpu);
    if (!CreateTrailHistoryView(device)) {
        return false;
    }

    D3D12_INDIRECT_ARGUMENT_DESC argumentDesc{};
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
    signatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    signatureDesc.NumArgumentDescs = 1;
    signatureDesc.pArgumentDescs = &argumentDesc;
    if (FAILED(device->CreateCommandSignature(
            &signatureDesc,
            nullptr,
            IID_PPV_ARGS(&commandSignature_)))) {
        return false;
    }

    initialized_ = true;
    return true;
}

void AppGpuParticleSystem::DeclareGraphBuffers(ge3::graphics::RenderGraph& renderGraph) const {
    if (!initialized_ || maxParticles_ == 0) {
        return;
    }

    renderGraph.DeclarePersistentBuffer(
        "ParticleState",
        ParticleStateBytes(maxParticles_),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderGraph.DeclareTransientBuffer(
        "ParticleRenderBuffer",
        ParticleRenderBufferBytes(maxParticles_),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);
    renderGraph.DeclarePersistentBuffer(
        "ParticleIndirectArgs",
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    renderGraph.DeclarePersistentBuffer(
        "ParticleDedicatedState",
        ParticleStateBytes(maxParticles_),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderGraph.DeclarePersistentBuffer(
        "DistortionDedicatedState",
        ParticleStateBytes(maxParticles_),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderGraph.DeclarePersistentBuffer(
        "BeamDedicatedState",
        ParticleStateBytes(maxParticles_),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderGraph.DeclareTransientBuffer(
        "ParticleDedicatedRenderBuffer",
        ParticleRenderBufferBytes(maxParticles_),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);
    renderGraph.DeclarePersistentBuffer(
        "ParticleDedicatedIndirectArgs",
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    renderGraph.DeclareTransientBuffer(
        "DistortionDedicatedRenderBuffer",
        ParticleRenderBufferBytes(maxParticles_),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);
    renderGraph.DeclarePersistentBuffer(
        "DistortionDedicatedIndirectArgs",
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    renderGraph.DeclareTransientBuffer(
        "BeamDedicatedRenderBuffer",
        ParticleRenderBufferBytes(maxParticles_),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);
    renderGraph.DeclarePersistentBuffer(
        "BeamDedicatedIndirectArgs",
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    const uint32_t trailSegmentCapacity = maxParticles_;
    renderGraph.DeclareTransientBuffer(
        "TrailControlPointBuffer",
        TrailControlPointBufferBytes(trailSegmentCapacity),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);
    renderGraph.DeclareTransientBuffer(
        "TrailSegmentBuffer",
        TrailSegmentBufferBytes(trailSegmentCapacity),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);
    renderGraph.DeclareTransientBuffer(
        "TrailVertexBuffer",
        TrailVertexBufferBytes(trailSegmentCapacity),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);
    renderGraph.DeclareTransientBuffer(
        "TrailIndexBuffer",
        TrailIndexBufferBytes(trailSegmentCapacity),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);
    renderGraph.DeclareTransientBuffer(
        "TrailDrawArgs",
        TrailDrawArgsBufferBytes(),
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
}

bool AppGpuParticleSystem::EnsureGraphBuffers(
    ID3D12Device* device,
    const ge3::graphics::RenderGraph& renderGraph) {
    if (!initialized_ || device == nullptr) {
        return false;
    }

    const std::vector<ge3::graphics::TransientBufferDesc> bufferPlan =
        renderGraph.BuildTransientBufferPlan();
    bool particleOutputReady = particleOutput_ != nullptr;
    bool dedicatedParticleStateReady = dedicatedParticleState_ != nullptr;
    bool dedicatedDistortionStateReady = dedicatedDistortionState_ != nullptr;
    bool dedicatedBeamStateReady = dedicatedBeamState_ != nullptr;
    bool dedicatedParticleOutputReady = dedicatedParticleOutput_ != nullptr;
    bool dedicatedIndirectArgsReady = dedicatedIndirectArgs_ != nullptr;
    bool dedicatedDistortionOutputReady = dedicatedDistortionOutput_ != nullptr;
    bool dedicatedDistortionIndirectArgsReady = dedicatedDistortionIndirectArgs_ != nullptr;
    bool dedicatedBeamOutputReady = dedicatedBeamOutput_ != nullptr;
    bool dedicatedBeamIndirectArgsReady = dedicatedBeamIndirectArgs_ != nullptr;
    bool trailControlPointReady = trailControlPointBuffer_ != nullptr;
    bool trailSegmentReady = trailSegmentBuffer_ != nullptr;
    bool trailVertexReady = trailVertexBuffer_ != nullptr;
    bool trailIndexReady = trailIndexBuffer_ != nullptr;
    bool trailDrawArgsReady = trailDrawArgs_ != nullptr;
    bool trailControlPointRequired = false;
    bool trailSegmentRequired = false;
    bool trailVertexRequired = false;
    bool trailIndexRequired = false;
    bool trailDrawArgsRequired = false;
    bool dedicatedParticleStateRequired = false;
    bool dedicatedDistortionStateRequired = false;
    bool dedicatedBeamStateRequired = false;
    bool dedicatedParticleOutputRequired = false;
    bool dedicatedIndirectArgsRequired = false;
    bool dedicatedDistortionOutputRequired = false;
    bool dedicatedDistortionIndirectArgsRequired = false;
    bool dedicatedBeamOutputRequired = false;
    bool dedicatedBeamIndirectArgsRequired = false;
    bool trailViewsDirty = false;

    for (const ge3::graphics::TransientBufferDesc& buffer : bufferPlan) {
        if (buffer.name == "ParticleRenderBuffer") {
            const bool needsParticleViews =
                particleOutput_ == nullptr || particleOutputBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    particleOutput_,
                    particleOutputBytes_,
                    particleOutputState_)) {
                return false;
            }
            if (needsParticleViews) {
                if (!CreateParticleOutputViews(device)) {
                    return false;
                }
            }
            particleOutputReady = true;
        } else if (buffer.name == "ParticleDedicatedState") {
            dedicatedParticleStateRequired = true;
            const bool needsDedicatedStateView =
                dedicatedParticleState_ == nullptr || dedicatedParticleStateBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedParticleState_,
                    dedicatedParticleStateBytes_,
                    dedicatedParticleStateState_)) {
                return false;
            }
            if (needsDedicatedStateView) {
                dedicatedParticleStateInitialized_ = false;
            }
            if (needsDedicatedStateView) {
                if (!dedicatedStateUav_.IsValid()) {
                    return false;
                }
                D3D12_UNORDERED_ACCESS_VIEW_DESC stateUav{};
                stateUav.Format = DXGI_FORMAT_UNKNOWN;
                stateUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                stateUav.Buffer.NumElements = maxParticles_;
                stateUav.Buffer.StructureByteStride = sizeof(ParticleStateSample);
                device->CreateUnorderedAccessView(
                    dedicatedParticleState_.Get(),
                    nullptr,
                    &stateUav,
                    dedicatedStateUav_.cpu);
            }
            dedicatedParticleStateReady = true;
        } else if (buffer.name == "DistortionDedicatedState") {
            dedicatedDistortionStateRequired = true;
            const bool needsDedicatedDistortionStateView =
                dedicatedDistortionState_ == nullptr || dedicatedDistortionStateBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedDistortionState_,
                    dedicatedDistortionStateBytes_,
                    dedicatedDistortionStateState_)) {
                return false;
            }
            if (needsDedicatedDistortionStateView) {
                dedicatedDistortionStateInitialized_ = false;
                if (!dedicatedDistortionStateUav_.IsValid()) {
                    return false;
                }
                D3D12_UNORDERED_ACCESS_VIEW_DESC stateUav{};
                stateUav.Format = DXGI_FORMAT_UNKNOWN;
                stateUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                stateUav.Buffer.NumElements = maxParticles_;
                stateUav.Buffer.StructureByteStride = sizeof(ParticleStateSample);
                device->CreateUnorderedAccessView(
                    dedicatedDistortionState_.Get(),
                    nullptr,
                    &stateUav,
                    dedicatedDistortionStateUav_.cpu);
            }
            dedicatedDistortionStateReady = true;
        } else if (buffer.name == "BeamDedicatedState") {
            dedicatedBeamStateRequired = true;
            const bool needsDedicatedBeamStateView =
                dedicatedBeamState_ == nullptr || dedicatedBeamStateBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedBeamState_,
                    dedicatedBeamStateBytes_,
                    dedicatedBeamStateState_)) {
                return false;
            }
            if (needsDedicatedBeamStateView) {
                dedicatedBeamStateInitialized_ = false;
                if (!dedicatedBeamStateUav_.IsValid()) {
                    return false;
                }
                D3D12_UNORDERED_ACCESS_VIEW_DESC stateUav{};
                stateUav.Format = DXGI_FORMAT_UNKNOWN;
                stateUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                stateUav.Buffer.NumElements = maxParticles_;
                stateUav.Buffer.StructureByteStride = sizeof(ParticleStateSample);
                device->CreateUnorderedAccessView(
                    dedicatedBeamState_.Get(),
                    nullptr,
                    &stateUav,
                    dedicatedBeamStateUav_.cpu);
            }
            dedicatedBeamStateReady = true;
        } else if (buffer.name == "ParticleDedicatedRenderBuffer") {
            dedicatedParticleOutputRequired = true;
            const bool needsDedicatedParticleViews =
                dedicatedParticleOutput_ == nullptr || dedicatedParticleOutputBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedParticleOutput_,
                    dedicatedParticleOutputBytes_,
                    dedicatedParticleOutputState_)) {
                return false;
            }
            if (needsDedicatedParticleViews) {
                if (!CreateDedicatedParticleOutputViews(device)) {
                    return false;
                }
            }
            dedicatedParticleOutputReady = true;
        } else if (buffer.name == "ParticleDedicatedIndirectArgs") {
            dedicatedIndirectArgsRequired = true;
            const bool needsDedicatedIndirectArgs =
                dedicatedIndirectArgs_ == nullptr || dedicatedIndirectArgsBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedIndirectArgs_,
                    dedicatedIndirectArgsBytes_,
                    dedicatedIndirectArgsState_)) {
                return false;
            }
            if (needsDedicatedIndirectArgs) {
                dedicatedIndirectArgsInitialized_ = false;
            }
            dedicatedIndirectArgsReady = true;
        } else if (buffer.name == "DistortionDedicatedRenderBuffer") {
            dedicatedDistortionOutputRequired = true;
            const bool needsDedicatedDistortionViews =
                dedicatedDistortionOutput_ == nullptr || dedicatedDistortionOutputBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedDistortionOutput_,
                    dedicatedDistortionOutputBytes_,
                    dedicatedDistortionOutputState_)) {
                return false;
            }
            if (needsDedicatedDistortionViews) {
                if (!CreateDedicatedDistortionOutputViews(device)) {
                    return false;
                }
            }
            dedicatedDistortionOutputReady = true;
        } else if (buffer.name == "DistortionDedicatedIndirectArgs") {
            dedicatedDistortionIndirectArgsRequired = true;
            const bool needsDedicatedDistortionIndirectArgs =
                dedicatedDistortionIndirectArgs_ == nullptr ||
                dedicatedDistortionIndirectArgsBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedDistortionIndirectArgs_,
                    dedicatedDistortionIndirectArgsBytes_,
                    dedicatedDistortionIndirectArgsState_)) {
                return false;
            }
            if (needsDedicatedDistortionIndirectArgs) {
                dedicatedDistortionIndirectArgsInitialized_ = false;
            }
            dedicatedDistortionIndirectArgsReady = true;
        } else if (buffer.name == "BeamDedicatedRenderBuffer") {
            dedicatedBeamOutputRequired = true;
            const bool needsDedicatedBeamViews =
                dedicatedBeamOutput_ == nullptr || dedicatedBeamOutputBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedBeamOutput_,
                    dedicatedBeamOutputBytes_,
                    dedicatedBeamOutputState_)) {
                return false;
            }
            if (needsDedicatedBeamViews) {
                if (!CreateDedicatedBeamOutputViews(device)) {
                    return false;
                }
            }
            dedicatedBeamOutputReady = true;
        } else if (buffer.name == "BeamDedicatedIndirectArgs") {
            dedicatedBeamIndirectArgsRequired = true;
            const bool needsDedicatedBeamIndirectArgs =
                dedicatedBeamIndirectArgs_ == nullptr ||
                dedicatedBeamIndirectArgsBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    dedicatedBeamIndirectArgs_,
                    dedicatedBeamIndirectArgsBytes_,
                    dedicatedBeamIndirectArgsState_)) {
                return false;
            }
            if (needsDedicatedBeamIndirectArgs) {
                dedicatedBeamIndirectArgsInitialized_ = false;
            }
            dedicatedBeamIndirectArgsReady = true;
        } else if (buffer.name == "TrailControlPointBuffer") {
            trailControlPointRequired = true;
            const bool needsTrailControlPointViews =
                trailControlPointBuffer_ == nullptr || trailControlPointBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    trailControlPointBuffer_,
                    trailControlPointBytes_,
                    trailControlPointState_)) {
                return false;
            }
            trailViewsDirty = trailViewsDirty || needsTrailControlPointViews;
            trailControlPointReady = true;
        } else if (buffer.name == "TrailSegmentBuffer") {
            trailSegmentRequired = true;
            const bool needsTrailSegmentViews =
                trailSegmentBuffer_ == nullptr || trailSegmentBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    trailSegmentBuffer_,
                    trailSegmentBytes_,
                    trailSegmentState_)) {
                return false;
            }
            trailViewsDirty = trailViewsDirty || needsTrailSegmentViews;
            trailSegmentReady = true;
        } else if (buffer.name == "TrailVertexBuffer") {
            trailVertexRequired = true;
            const bool needsTrailVertexViews =
                trailVertexBuffer_ == nullptr || trailVertexBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    trailVertexBuffer_,
                    trailVertexBytes_,
                    trailVertexState_)) {
                return false;
            }
            trailViewsDirty = trailViewsDirty || needsTrailVertexViews;
            trailVertexReady = true;
        } else if (buffer.name == "TrailIndexBuffer") {
            trailIndexRequired = true;
            const bool needsTrailIndexViews =
                trailIndexBuffer_ == nullptr || trailIndexBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    trailIndexBuffer_,
                    trailIndexBytes_,
                    trailIndexState_)) {
                return false;
            }
            trailViewsDirty = trailViewsDirty || needsTrailIndexViews;
            trailIndexReady = true;
        } else if (buffer.name == "TrailDrawArgs") {
            trailDrawArgsRequired = true;
            const bool needsTrailDrawArgsViews =
                trailDrawArgs_ == nullptr || trailDrawArgsBytes_ != buffer.sizeInBytes;
            if (!EnsureDefaultBuffer(
                    device,
                    buffer,
                    trailDrawArgs_,
                    trailDrawArgsBytes_,
                    trailDrawArgsState_)) {
                return false;
            }
            trailViewsDirty = trailViewsDirty || needsTrailDrawArgsViews;
            trailDrawArgsReady = true;
        }
    }

    if ((trailControlPointRequired || trailSegmentRequired || trailVertexRequired || trailIndexRequired || trailDrawArgsRequired) &&
        trailControlPointReady &&
        trailSegmentReady &&
        trailVertexReady &&
        trailIndexReady &&
        trailDrawArgsReady &&
        trailViewsDirty &&
        !CreateTrailMeshStreamViews(device)) {
        return false;
    }

    return particleOutputReady &&
        (!dedicatedParticleStateRequired || dedicatedParticleStateReady) &&
        (!dedicatedDistortionStateRequired || dedicatedDistortionStateReady) &&
        (!dedicatedBeamStateRequired || dedicatedBeamStateReady) &&
        (!dedicatedParticleOutputRequired || dedicatedParticleOutputReady) &&
        (!dedicatedIndirectArgsRequired || dedicatedIndirectArgsReady) &&
        (!dedicatedDistortionOutputRequired || dedicatedDistortionOutputReady) &&
        (!dedicatedDistortionIndirectArgsRequired || dedicatedDistortionIndirectArgsReady) &&
        (!dedicatedBeamOutputRequired || dedicatedBeamOutputReady) &&
        (!dedicatedBeamIndirectArgsRequired || dedicatedBeamIndirectArgsReady) &&
        (!trailControlPointRequired || trailControlPointReady) &&
        (!trailSegmentRequired || trailSegmentReady) &&
        (!trailVertexRequired || trailVertexReady) &&
        (!trailIndexRequired || trailIndexReady) &&
        (!trailDrawArgsRequired || trailDrawArgsReady);
}

void AppGpuParticleSystem::InitializeDedicatedParticleResources(ID3D12GraphicsCommandList* commandList) {
    if (!initialized_ || commandList == nullptr) {
        return;
    }

    if (dedicatedParticleState_ != nullptr &&
        uploadState_ != nullptr &&
        !dedicatedParticleStateInitialized_) {
        D3D12_RESOURCE_BARRIER stateCopy =
            MakeTransition(
                dedicatedParticleState_.Get(),
                dedicatedParticleStateState_,
                D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &stateCopy);
        dedicatedParticleStateState_ = D3D12_RESOURCE_STATE_COPY_DEST;
        commandList->CopyBufferRegion(
            dedicatedParticleState_.Get(),
            0,
            uploadState_.Get(),
            0,
            ParticleStateBytes(maxParticles_));
        D3D12_RESOURCE_BARRIER stateReady =
            MakeTransition(
                dedicatedParticleState_.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &stateReady);
        dedicatedParticleStateState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        dedicatedParticleStateInitialized_ = true;
    }

    if (dedicatedDistortionState_ != nullptr &&
        uploadState_ != nullptr &&
        !dedicatedDistortionStateInitialized_) {
        D3D12_RESOURCE_BARRIER stateCopy =
            MakeTransition(
                dedicatedDistortionState_.Get(),
                dedicatedDistortionStateState_,
                D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &stateCopy);
        dedicatedDistortionStateState_ = D3D12_RESOURCE_STATE_COPY_DEST;
        commandList->CopyBufferRegion(
            dedicatedDistortionState_.Get(),
            0,
            uploadState_.Get(),
            0,
            ParticleStateBytes(maxParticles_));
        D3D12_RESOURCE_BARRIER stateReady =
            MakeTransition(
                dedicatedDistortionState_.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &stateReady);
        dedicatedDistortionStateState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        dedicatedDistortionStateInitialized_ = true;
    }

    if (dedicatedBeamState_ != nullptr &&
        uploadState_ != nullptr &&
        !dedicatedBeamStateInitialized_) {
        D3D12_RESOURCE_BARRIER stateCopy =
            MakeTransition(
                dedicatedBeamState_.Get(),
                dedicatedBeamStateState_,
                D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &stateCopy);
        dedicatedBeamStateState_ = D3D12_RESOURCE_STATE_COPY_DEST;
        commandList->CopyBufferRegion(
            dedicatedBeamState_.Get(),
            0,
            uploadState_.Get(),
            0,
            ParticleStateBytes(maxParticles_));
        D3D12_RESOURCE_BARRIER stateReady =
            MakeTransition(
                dedicatedBeamState_.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &stateReady);
        dedicatedBeamStateState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        dedicatedBeamStateInitialized_ = true;
    }

    if (dedicatedIndirectArgs_ != nullptr &&
        uploadIndirectArgs_ != nullptr &&
        !dedicatedIndirectArgsInitialized_) {
        D3D12_RESOURCE_BARRIER argsCopy =
            MakeTransition(
                dedicatedIndirectArgs_.Get(),
                dedicatedIndirectArgsState_,
                D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &argsCopy);
        dedicatedIndirectArgsState_ = D3D12_RESOURCE_STATE_COPY_DEST;
        commandList->CopyBufferRegion(
            dedicatedIndirectArgs_.Get(),
            0,
            uploadIndirectArgs_.Get(),
            0,
            sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
        D3D12_RESOURCE_BARRIER argsReady =
            MakeTransition(
                dedicatedIndirectArgs_.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        commandList->ResourceBarrier(1, &argsReady);
        dedicatedIndirectArgsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        dedicatedIndirectArgsInitialized_ = true;
    }

    if (dedicatedDistortionIndirectArgs_ != nullptr &&
        uploadIndirectArgs_ != nullptr &&
        !dedicatedDistortionIndirectArgsInitialized_) {
        D3D12_RESOURCE_BARRIER argsCopy =
            MakeTransition(
                dedicatedDistortionIndirectArgs_.Get(),
                dedicatedDistortionIndirectArgsState_,
                D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &argsCopy);
        dedicatedDistortionIndirectArgsState_ = D3D12_RESOURCE_STATE_COPY_DEST;
        commandList->CopyBufferRegion(
            dedicatedDistortionIndirectArgs_.Get(),
            0,
            uploadIndirectArgs_.Get(),
            0,
            sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
        D3D12_RESOURCE_BARRIER argsReady =
            MakeTransition(
                dedicatedDistortionIndirectArgs_.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        commandList->ResourceBarrier(1, &argsReady);
        dedicatedDistortionIndirectArgsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        dedicatedDistortionIndirectArgsInitialized_ = true;
    }

    if (dedicatedBeamIndirectArgs_ != nullptr &&
        uploadIndirectArgs_ != nullptr &&
        !dedicatedBeamIndirectArgsInitialized_) {
        D3D12_RESOURCE_BARRIER argsCopy =
            MakeTransition(
                dedicatedBeamIndirectArgs_.Get(),
                dedicatedBeamIndirectArgsState_,
                D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &argsCopy);
        dedicatedBeamIndirectArgsState_ = D3D12_RESOURCE_STATE_COPY_DEST;
        commandList->CopyBufferRegion(
            dedicatedBeamIndirectArgs_.Get(),
            0,
            uploadIndirectArgs_.Get(),
            0,
            sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
        D3D12_RESOURCE_BARRIER argsReady =
            MakeTransition(
                dedicatedBeamIndirectArgs_.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        commandList->ResourceBarrier(1, &argsReady);
        dedicatedBeamIndirectArgsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        dedicatedBeamIndirectArgsInitialized_ = true;
    }
}

void AppGpuParticleSystem::Simulate(
    ID3D12GraphicsCommandList* commandList,
    ID3D12RootSignature* rootSignature,
    ID3D12PipelineState* pipelineState,
    const Matrix4x4& viewProjection,
    float deltaTime,
    float time,
    const Vector4& tint,
    const Vector3& scale,
        float emissive,
        float turbulence,
        float pulseSpeed,
        float spawnRadius,
        float uvScrollSpeed,
        std::string_view renderBufferResource,
        std::string_view stateBufferResource) {
    if (!initialized_ || commandList == nullptr || rootSignature == nullptr || pipelineState == nullptr) {
        return;
    }
    ID3D12Resource* outputResource = nullptr;
    D3D12_RESOURCE_STATES* outputState = nullptr;
    if (renderBufferResource == "ParticleDedicatedRenderBuffer") {
        outputResource = dedicatedParticleOutput_.Get();
        outputState = &dedicatedParticleOutputState_;
    } else if (renderBufferResource == "DistortionDedicatedRenderBuffer") {
        outputResource = dedicatedDistortionOutput_.Get();
        outputState = &dedicatedDistortionOutputState_;
    } else if (renderBufferResource == "BeamDedicatedRenderBuffer") {
        outputResource = dedicatedBeamOutput_.Get();
        outputState = &dedicatedBeamOutputState_;
    } else if (renderBufferResource == "ParticleRenderBuffer") {
        outputResource = particleOutput_.Get();
        outputState = &particleOutputState_;
    }

    ID3D12Resource* stateResource = nullptr;
    D3D12_RESOURCE_STATES* stateState = nullptr;
    if (stateBufferResource == "ParticleDedicatedState") {
        stateResource = dedicatedParticleState_.Get();
        stateState = &dedicatedParticleStateState_;
    } else if (stateBufferResource == "DistortionDedicatedState") {
        stateResource = dedicatedDistortionState_.Get();
        stateState = &dedicatedDistortionStateState_;
    } else if (stateBufferResource == "BeamDedicatedState") {
        stateResource = dedicatedBeamState_.Get();
        stateState = &dedicatedBeamStateState_;
    } else if (stateBufferResource == "ParticleState") {
        stateResource = particleState_.Get();
        stateState = &particleStateState_;
    }

    const D3D12_GPU_DESCRIPTOR_HANDLE outputUav = UavHandleForResource(renderBufferResource);
    const D3D12_GPU_DESCRIPTOR_HANDLE stateUav = UavHandleForResource(stateBufferResource);
    if (outputResource == nullptr ||
        outputState == nullptr ||
        outputUav.ptr == 0 ||
        stateResource == nullptr ||
        stateState == nullptr ||
        stateUav.ptr == 0) {
        return;
    }

    struct Constants {
        Matrix4x4 viewProjection;
        float deltaTime;
        float time;
        uint32_t maxParticles;
        float pad;
        Vector4 tint;
        Vector4 scaleAndParams;
        Vector4 effectParams;
    } constants{};
    constants.viewProjection = viewProjection;
    constants.deltaTime = deltaTime;
    constants.time = time;
    constants.maxParticles = maxParticles_;
    constants.tint = tint;
    constants.scaleAndParams = {scale.x, scale.y, emissive, turbulence};
    constants.effectParams = {pulseSpeed, spawnRadius, uvScrollSpeed, 0.0f};

    if (*outputState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER outputReady =
            MakeTransition(outputResource, *outputState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &outputReady);
        *outputState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if (*stateState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER stateReady =
            MakeTransition(stateResource, *stateState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList->ResourceBarrier(1, &stateReady);
        *stateState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    commandList->SetComputeRootSignature(rootSignature);
    commandList->SetPipelineState(pipelineState);
    commandList->SetComputeRoot32BitConstants(0, 32, &constants, 0);
    commandList->SetComputeRootDescriptorTable(1, outputUav);
    commandList->SetComputeRootDescriptorTable(2, stateUav);
    const uint32_t dispatchGroupCount = (maxParticles_ + 255) / 256;
    particleSimulationTelemetry_.valid = true;
    particleSimulationTelemetry_.usedDedicatedResources =
        renderBufferResource == "ParticleDedicatedRenderBuffer" ||
        stateBufferResource == "ParticleDedicatedState";
    particleSimulationTelemetry_.renderBufferResource =
        particleSimulationTelemetry_.usedDedicatedResources && renderBufferResource == "ParticleDedicatedRenderBuffer"
            ? "ParticleDedicatedRenderBuffer"
            : "ParticleRenderBuffer";
    particleSimulationTelemetry_.stateBufferResource =
        particleSimulationTelemetry_.usedDedicatedResources && stateBufferResource == "ParticleDedicatedState"
            ? "ParticleDedicatedState"
            : "ParticleState";
    particleSimulationTelemetry_.renderBufferUav = outputUav;
    particleSimulationTelemetry_.stateBufferUav = stateUav;
    particleSimulationTelemetry_.dispatchGroupCount = dispatchGroupCount;
    particleSimulationTelemetry_.maxParticles = maxParticles_;
    commandList->Dispatch(dispatchGroupCount, 1, 1);
}

void AppGpuParticleSystem::RegisterGraphResources(ge3::graphics::RenderGraph& renderGraph) const {
    if (!initialized_ || particleOutput_ == nullptr) {
        return;
    }

    renderGraph.RegisterResource(
        "ParticleRenderBuffer",
        particleOutput_.Get(),
        particleOutputState_);
    if (dedicatedParticleOutput_ != nullptr) {
        renderGraph.RegisterResource(
            "ParticleDedicatedRenderBuffer",
            dedicatedParticleOutput_.Get(),
            dedicatedParticleOutputState_);
    }
    if (dedicatedDistortionOutput_ != nullptr) {
        renderGraph.RegisterResource(
            "DistortionDedicatedRenderBuffer",
            dedicatedDistortionOutput_.Get(),
            dedicatedDistortionOutputState_);
    }
    if (dedicatedBeamOutput_ != nullptr) {
        renderGraph.RegisterResource(
            "BeamDedicatedRenderBuffer",
            dedicatedBeamOutput_.Get(),
            dedicatedBeamOutputState_);
    }
    renderGraph.RegisterResource(
        "TrailRenderBuffer",
        particleOutput_.Get(),
        particleOutputState_,
        "ParticleRenderBuffer");
    renderGraph.RegisterResource(
        "TrailControlPointBuffer",
        trailControlPointBuffer_.Get(),
        trailControlPointState_);
    renderGraph.RegisterResource(
        "TrailSegmentBuffer",
        trailSegmentBuffer_.Get(),
        trailSegmentState_);
    renderGraph.RegisterResource(
        "TrailVertexBuffer",
        trailVertexBuffer_.Get(),
        trailVertexState_);
    renderGraph.RegisterResource(
        "TrailIndexBuffer",
        trailIndexBuffer_.Get(),
        trailIndexState_);
    renderGraph.RegisterResource(
        "DistortionRenderBuffer",
        particleOutput_.Get(),
        particleOutputState_,
        "ParticleRenderBuffer");
    renderGraph.RegisterResource(
        "ParticleState",
        particleState_.Get(),
        particleStateState_);
    if (dedicatedParticleState_ != nullptr) {
        renderGraph.RegisterResource(
            "ParticleDedicatedState",
            dedicatedParticleState_.Get(),
            dedicatedParticleStateState_);
    }
    if (dedicatedDistortionState_ != nullptr) {
        renderGraph.RegisterResource(
            "DistortionDedicatedState",
            dedicatedDistortionState_.Get(),
            dedicatedDistortionStateState_);
    }
    if (dedicatedBeamState_ != nullptr) {
        renderGraph.RegisterResource(
            "BeamDedicatedState",
            dedicatedBeamState_.Get(),
            dedicatedBeamStateState_);
    }
    renderGraph.RegisterResource(
        "ParticleIndirectArgs",
        indirectArgs_.Get(),
        indirectArgsState_);
    if (dedicatedIndirectArgs_ != nullptr) {
        renderGraph.RegisterResource(
            "ParticleDedicatedIndirectArgs",
            dedicatedIndirectArgs_.Get(),
            dedicatedIndirectArgsState_);
    }
    if (dedicatedDistortionIndirectArgs_ != nullptr) {
        renderGraph.RegisterResource(
            "DistortionDedicatedIndirectArgs",
            dedicatedDistortionIndirectArgs_.Get(),
            dedicatedDistortionIndirectArgsState_);
    }
    if (dedicatedBeamIndirectArgs_ != nullptr) {
        renderGraph.RegisterResource(
            "BeamDedicatedIndirectArgs",
            dedicatedBeamIndirectArgs_.Get(),
            dedicatedBeamIndirectArgsState_);
    }
    renderGraph.RegisterResource(
        "TrailIndirectArgs",
        indirectArgs_.Get(),
        indirectArgsState_,
        "ParticleIndirectArgs");
    renderGraph.RegisterResource(
        "TrailDrawArgs",
        trailDrawArgs_.Get(),
        trailDrawArgsState_);
    renderGraph.RegisterResource(
        "DistortionIndirectArgs",
        indirectArgs_.Get(),
        indirectArgsState_,
        "ParticleIndirectArgs");
}

D3D12_GPU_DESCRIPTOR_HANDLE AppGpuParticleSystem::SrvHandleForResource(std::string_view name) const {
    if (name == "ParticleRenderBuffer" || name == "TrailRenderBuffer" || name == "DistortionRenderBuffer") {
        return particleSrv_.gpu;
    }
    if (name == "ParticleDedicatedRenderBuffer") {
        return dedicatedParticleSrv_.gpu;
    }
    if (name == "DistortionDedicatedRenderBuffer") {
        return dedicatedDistortionSrv_.gpu;
    }
    if (name == "BeamDedicatedRenderBuffer") {
        return dedicatedBeamSrv_.gpu;
    }
    if (name == "TrailControlPointBuffer") {
        return trailControlPointSrv_.gpu;
    }
    if (name == "TrailSegmentBuffer") {
        return trailSegmentSrv_.gpu;
    }
    if (name == "TrailVertexBuffer") {
        return trailVertexSrv_.gpu;
    }
    if (name == "TrailIndexBuffer") {
        return trailIndexSrv_.gpu;
    }
    return {};
}

D3D12_GPU_DESCRIPTOR_HANDLE AppGpuParticleSystem::UavHandleForResource(std::string_view name) const {
    if (name == "ParticleRenderBuffer" || name == "TrailRenderBuffer" || name == "DistortionRenderBuffer") {
        return particleUav_.gpu;
    }
    if (name == "ParticleDedicatedRenderBuffer") {
        return dedicatedParticleUav_.gpu;
    }
    if (name == "DistortionDedicatedRenderBuffer") {
        return dedicatedDistortionUav_.gpu;
    }
    if (name == "BeamDedicatedRenderBuffer") {
        return dedicatedBeamUav_.gpu;
    }
    if (name == "TrailControlPointBuffer") {
        return trailControlPointUav_.gpu;
    }
    if (name == "TrailSegmentBuffer") {
        return trailSegmentUav_.gpu;
    }
    if (name == "TrailVertexBuffer") {
        return trailVertexUav_.gpu;
    }
    if (name == "TrailIndexBuffer") {
        return trailIndexUav_.gpu;
    }
    if (name == "TrailDrawArgs") {
        return trailDrawArgsUav_.gpu;
    }
    if (name == "ParticleState") {
        return stateUav_.gpu;
    }
    if (name == "ParticleDedicatedState") {
        return dedicatedStateUav_.gpu;
    }
    if (name == "DistortionDedicatedState") {
        return dedicatedDistortionStateUav_.gpu;
    }
    if (name == "BeamDedicatedState") {
        return dedicatedBeamStateUav_.gpu;
    }
    return {};
}

D3D12_GPU_DESCRIPTOR_HANDLE AppGpuParticleSystem::UpdateTrailHistory(const Vector4* points, uint32_t count) {
    if (mappedTrailHistory_ == nullptr || !trailHistorySrv_.IsValid()) {
        return {};
    }

    const uint32_t maxPoints = maxParticles_ + 1;
    const uint32_t copyCount = (std::min)(count, maxPoints);
    for (uint32_t i = 0; i < copyCount; ++i) {
        mappedTrailHistory_[i] = points != nullptr ? points[i] : Vector4{0.0f, 0.0f, 0.0f, 1.0f};
    }
    const Vector4 tail = copyCount > 0 ? mappedTrailHistory_[copyCount - 1] : Vector4{0.0f, 0.0f, 0.0f, 1.0f};
    for (uint32_t i = copyCount; i < maxPoints; ++i) {
        mappedTrailHistory_[i] = tail;
    }
    return trailHistorySrv_.gpu;
}

ID3D12Resource* AppGpuParticleSystem::IndirectArgsForResource(std::string_view name) const {
    if (name == "ParticleIndirectArgs" || name == "TrailIndirectArgs" || name == "DistortionIndirectArgs") {
        return indirectArgs_.Get();
    }
    if (name == "ParticleDedicatedIndirectArgs") {
        return dedicatedIndirectArgs_.Get();
    }
    if (name == "DistortionDedicatedIndirectArgs") {
        return dedicatedDistortionIndirectArgs_.Get();
    }
    if (name == "BeamDedicatedIndirectArgs") {
        return dedicatedBeamIndirectArgs_.Get();
    }
    if (name == "TrailDrawArgs") {
        return trailDrawArgs_.Get();
    }
    return nullptr;
}

bool AppGpuParticleSystem::WriteIndirectArgsForResource(
    ID3D12GraphicsCommandList* commandList,
    std::string_view name,
    const D3D12_DRAW_INDEXED_ARGUMENTS& args) {
    if (!initialized_ || commandList == nullptr || uploadIndirectArgs_ == nullptr) {
        return false;
    }

    ID3D12Resource* argsResource = nullptr;
    D3D12_RESOURCE_STATES* argsState = nullptr;
    if (name == "ParticleIndirectArgs" || name == "TrailIndirectArgs" || name == "DistortionIndirectArgs") {
        argsResource = indirectArgs_.Get();
        argsState = &indirectArgsState_;
    } else if (name == "ParticleDedicatedIndirectArgs") {
        argsResource = dedicatedIndirectArgs_.Get();
        argsState = &dedicatedIndirectArgsState_;
    } else if (name == "DistortionDedicatedIndirectArgs") {
        argsResource = dedicatedDistortionIndirectArgs_.Get();
        argsState = &dedicatedDistortionIndirectArgsState_;
    } else if (name == "BeamDedicatedIndirectArgs") {
        argsResource = dedicatedBeamIndirectArgs_.Get();
        argsState = &dedicatedBeamIndirectArgsState_;
        dedicatedBeamIndirectArgsInitialized_ = true;
    }
    if (argsResource == nullptr || argsState == nullptr) {
        return false;
    }

    void* mapped = nullptr;
    if (FAILED(uploadIndirectArgs_->Map(0, nullptr, &mapped)) || mapped == nullptr) {
        return false;
    }
    std::memcpy(mapped, &args, sizeof(args));
    uploadIndirectArgs_->Unmap(0, nullptr);

    if (*argsState != D3D12_RESOURCE_STATE_COPY_DEST) {
        D3D12_RESOURCE_BARRIER argsCopy =
            MakeTransition(argsResource, *argsState, D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &argsCopy);
        *argsState = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    commandList->CopyBufferRegion(argsResource, 0, uploadIndirectArgs_.Get(), 0, sizeof(args));

    D3D12_RESOURCE_BARRIER argsReady =
        MakeTransition(argsResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    commandList->ResourceBarrier(1, &argsReady);
    *argsState = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    return true;
}

D3D12_VERTEX_BUFFER_VIEW AppGpuParticleSystem::TrailVertexBufferView() const {
    D3D12_VERTEX_BUFFER_VIEW view{};
    if (trailVertexBuffer_ == nullptr || trailVertexBytes_ == 0) {
        return view;
    }
    view.BufferLocation = trailVertexBuffer_->GetGPUVirtualAddress();
    view.SizeInBytes = static_cast<UINT>(trailVertexBytes_);
    view.StrideInBytes = sizeof(TrailMeshStreamVertexSample);
    return view;
}

D3D12_INDEX_BUFFER_VIEW AppGpuParticleSystem::TrailIndexBufferView() const {
    D3D12_INDEX_BUFFER_VIEW view{};
    if (trailIndexBuffer_ == nullptr || trailIndexBytes_ == 0) {
        return view;
    }
    view.BufferLocation = trailIndexBuffer_->GetGPUVirtualAddress();
    view.SizeInBytes = static_cast<UINT>(trailIndexBytes_);
    view.Format = DXGI_FORMAT_R32_UINT;
    return view;
}

bool AppGpuParticleSystem::HasTrailViewProjectionBuffer() const {
    return trailViewProjection_ != nullptr && mappedTrailViewProjection_ != nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS AppGpuParticleSystem::UpdateTrailViewProjection(const Matrix4x4& viewProjection) {
    if (trailViewProjection_ == nullptr || mappedTrailViewProjection_ == nullptr) {
        return 0;
    }

    *mappedTrailViewProjection_ = viewProjection;
    return trailViewProjection_->GetGPUVirtualAddress();
}

void AppGpuParticleSystem::CaptureTrailMeshStreamTelemetry(ID3D12GraphicsCommandList* commandList) {
    trailTelemetry_.copiedThisFrame = false;
    trailTelemetryPending_ = false;
    if (commandList == nullptr ||
        trailControlPointBuffer_ == nullptr ||
        trailSegmentBuffer_ == nullptr ||
        trailDrawArgs_ == nullptr ||
        trailVertexBuffer_ == nullptr ||
        trailIndexBuffer_ == nullptr ||
        trailControlPointReadback_ == nullptr ||
        trailSegmentReadback_ == nullptr ||
        trailDrawArgsReadback_ == nullptr ||
        trailVertexReadback_ == nullptr ||
        trailIndexReadback_ == nullptr ||
        trailControlPointBytes_ == 0 ||
        trailSegmentBytes_ == 0 ||
        trailDrawArgsBytes_ == 0 ||
        trailVertexBytes_ == 0 ||
        trailIndexBytes_ == 0) {
        return;
    }

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    if (trailControlPointState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            trailControlPointBuffer_.Get(),
            trailControlPointState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        trailControlPointState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (trailSegmentState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            trailSegmentBuffer_.Get(),
            trailSegmentState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        trailSegmentState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (trailDrawArgsState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            trailDrawArgs_.Get(),
            trailDrawArgsState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        trailDrawArgsState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (trailVertexState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            trailVertexBuffer_.Get(),
            trailVertexState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        trailVertexState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (trailIndexState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            trailIndexBuffer_.Get(),
            trailIndexState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        trailIndexState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (!barriers.empty()) {
        commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    const size_t controlPointBytes = (std::min)(
        trailControlPointBytes_,
        sizeof(TrailMeshStreamControlPointSample) * kTrailTelemetryControlPointSampleCount);
    const size_t segmentBytes = (std::min)(
        trailSegmentBytes_,
        sizeof(TrailMeshStreamSegmentSample) * kTrailTelemetrySegmentSampleCount);
    const size_t vertexBytes = (std::min)(
        trailVertexBytes_,
        sizeof(TrailMeshStreamVertexSample) * kTrailTelemetryVertexSampleCount);
    const size_t indexBytes = (std::min)(
        trailIndexBytes_,
        sizeof(uint32_t) * kTrailTelemetryIndexSampleCount);
    commandList->CopyBufferRegion(
        trailControlPointReadback_.Get(),
        0,
        trailControlPointBuffer_.Get(),
        0,
        controlPointBytes);
    commandList->CopyBufferRegion(
        trailSegmentReadback_.Get(),
        0,
        trailSegmentBuffer_.Get(),
        0,
        segmentBytes);
    commandList->CopyBufferRegion(
        trailDrawArgsReadback_.Get(),
        0,
        trailDrawArgs_.Get(),
        0,
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
    commandList->CopyBufferRegion(
        trailVertexReadback_.Get(),
        0,
        trailVertexBuffer_.Get(),
        0,
        vertexBytes);
    commandList->CopyBufferRegion(
        trailIndexReadback_.Get(),
        0,
        trailIndexBuffer_.Get(),
        0,
        indexBytes);

    trailTelemetry_.sampledControlPointCount =
        static_cast<uint32_t>(controlPointBytes / sizeof(TrailMeshStreamControlPointSample));
    trailTelemetry_.sampledSegmentCount =
        static_cast<uint32_t>(segmentBytes / sizeof(TrailMeshStreamSegmentSample));
    trailTelemetry_.sampledVertexCount =
        static_cast<uint32_t>(vertexBytes / sizeof(TrailMeshStreamVertexSample));
    trailTelemetry_.sampledIndexCount =
        static_cast<uint32_t>(indexBytes / sizeof(uint32_t));
    trailTelemetryPending_ = true;
    trailTelemetry_.copiedThisFrame = true;
}

void AppGpuParticleSystem::ResolveTrailMeshStreamTelemetry() {
    if (!trailTelemetryPending_ ||
        trailControlPointReadback_ == nullptr ||
        trailSegmentReadback_ == nullptr ||
        trailDrawArgsReadback_ == nullptr ||
        trailVertexReadback_ == nullptr ||
        trailIndexReadback_ == nullptr) {
        trailTelemetry_.copiedThisFrame = false;
        return;
    }

    const size_t controlPointBytes =
        sizeof(TrailMeshStreamControlPointSample) * trailTelemetry_.sampledControlPointCount;
    D3D12_RANGE controlPointReadRange{0, controlPointBytes};
    void* mappedControlPoints = nullptr;
    if (controlPointBytes > 0 &&
        SUCCEEDED(trailControlPointReadback_->Map(0, &controlPointReadRange, &mappedControlPoints))) {
        if (mappedControlPoints != nullptr) {
            std::memcpy(trailTelemetry_.controlPoints, mappedControlPoints, controlPointBytes);
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        trailControlPointReadback_->Unmap(0, &emptyWriteRange);
    }

    const size_t segmentBytes =
        sizeof(TrailMeshStreamSegmentSample) * trailTelemetry_.sampledSegmentCount;
    D3D12_RANGE segmentReadRange{0, segmentBytes};
    void* mappedSegments = nullptr;
    if (segmentBytes > 0 &&
        SUCCEEDED(trailSegmentReadback_->Map(0, &segmentReadRange, &mappedSegments))) {
        if (mappedSegments != nullptr) {
            std::memcpy(trailTelemetry_.segments, mappedSegments, segmentBytes);
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        trailSegmentReadback_->Unmap(0, &emptyWriteRange);
    }

    D3D12_RANGE drawArgsReadRange{0, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)};
    void* mappedDrawArgs = nullptr;
    if (SUCCEEDED(trailDrawArgsReadback_->Map(0, &drawArgsReadRange, &mappedDrawArgs))) {
        if (mappedDrawArgs != nullptr) {
            std::memcpy(
                &trailTelemetry_.drawArgs,
                mappedDrawArgs,
                sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        trailDrawArgsReadback_->Unmap(0, &emptyWriteRange);
    }

    const size_t vertexBytes =
        sizeof(TrailMeshStreamVertexSample) * trailTelemetry_.sampledVertexCount;
    D3D12_RANGE vertexReadRange{0, vertexBytes};
    void* mappedVertices = nullptr;
    if (vertexBytes > 0 &&
        SUCCEEDED(trailVertexReadback_->Map(0, &vertexReadRange, &mappedVertices))) {
        if (mappedVertices != nullptr) {
            std::memcpy(trailTelemetry_.vertices, mappedVertices, vertexBytes);
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        trailVertexReadback_->Unmap(0, &emptyWriteRange);
    }

    const size_t indexBytes = sizeof(uint32_t) * trailTelemetry_.sampledIndexCount;
    D3D12_RANGE indexReadRange{0, indexBytes};
    void* mappedIndices = nullptr;
    if (indexBytes > 0 &&
        SUCCEEDED(trailIndexReadback_->Map(0, &indexReadRange, &mappedIndices))) {
        if (mappedIndices != nullptr) {
            std::memcpy(trailTelemetry_.indices, mappedIndices, indexBytes);
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        trailIndexReadback_->Unmap(0, &emptyWriteRange);
    }

    trailTelemetry_.valid = true;
    trailTelemetryPending_ = false;
}


void AppGpuParticleSystem::CaptureParticleDedicatedReadbackTelemetry(ID3D12GraphicsCommandList* commandList) {
    particleDedicatedReadbackTelemetry_.copiedThisFrame = false;
    particleDedicatedReadbackPending_ = false;
    if (commandList == nullptr ||
        dedicatedParticleOutput_ == nullptr ||
        dedicatedParticleState_ == nullptr ||
        dedicatedIndirectArgs_ == nullptr ||
        particleDedicatedRenderReadback_ == nullptr ||
        particleDedicatedStateReadback_ == nullptr ||
        particleDedicatedIndirectArgsReadback_ == nullptr ||
        dedicatedParticleOutputBytes_ == 0 ||
        dedicatedParticleStateBytes_ == 0 ||
        dedicatedIndirectArgsBytes_ < sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)) {
        return;
    }

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    if (dedicatedParticleOutputState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            dedicatedParticleOutput_.Get(),
            dedicatedParticleOutputState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        dedicatedParticleOutputState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (dedicatedParticleStateState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            dedicatedParticleState_.Get(),
            dedicatedParticleStateState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        dedicatedParticleStateState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (dedicatedIndirectArgsState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            dedicatedIndirectArgs_.Get(),
            dedicatedIndirectArgsState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        dedicatedIndirectArgsState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (!barriers.empty()) {
        commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    const size_t renderBytes = (std::min)(
        dedicatedParticleOutputBytes_,
        sizeof(ParticleRenderSample) * kParticleDedicatedTelemetrySampleCount);
    const size_t stateBytes = (std::min)(
        dedicatedParticleStateBytes_,
        sizeof(ParticleStateSample) * kParticleDedicatedTelemetrySampleCount);
    commandList->CopyBufferRegion(
        particleDedicatedRenderReadback_.Get(),
        0,
        dedicatedParticleOutput_.Get(),
        0,
        renderBytes);
    commandList->CopyBufferRegion(
        particleDedicatedStateReadback_.Get(),
        0,
        dedicatedParticleState_.Get(),
        0,
        stateBytes);
    commandList->CopyBufferRegion(
        particleDedicatedIndirectArgsReadback_.Get(),
        0,
        dedicatedIndirectArgs_.Get(),
        0,
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));

    particleDedicatedReadbackTelemetry_.sampledRenderCount =
        static_cast<uint32_t>(renderBytes / sizeof(ParticleRenderSample));
    particleDedicatedReadbackTelemetry_.sampledStateCount =
        static_cast<uint32_t>(stateBytes / sizeof(ParticleStateSample));
    particleDedicatedReadbackTelemetry_.drawArgsValid = false;
    particleDedicatedReadbackPending_ = true;
    particleDedicatedReadbackTelemetry_.copiedThisFrame = true;
}

void AppGpuParticleSystem::ResolveParticleDedicatedReadbackTelemetry() {
    if (!particleDedicatedReadbackPending_ ||
        particleDedicatedRenderReadback_ == nullptr ||
        particleDedicatedStateReadback_ == nullptr ||
        particleDedicatedIndirectArgsReadback_ == nullptr) {
        particleDedicatedReadbackTelemetry_.copiedThisFrame = false;
        return;
    }

    const size_t renderBytes =
        sizeof(ParticleRenderSample) * particleDedicatedReadbackTelemetry_.sampledRenderCount;
    D3D12_RANGE renderReadRange{0, renderBytes};
    void* mappedRender = nullptr;
    if (renderBytes > 0 &&
        SUCCEEDED(particleDedicatedRenderReadback_->Map(0, &renderReadRange, &mappedRender))) {
        if (mappedRender != nullptr) {
            std::memcpy(particleDedicatedReadbackTelemetry_.renderSamples, mappedRender, renderBytes);
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        particleDedicatedRenderReadback_->Unmap(0, &emptyWriteRange);
    }

    const size_t stateBytes =
        sizeof(ParticleStateSample) * particleDedicatedReadbackTelemetry_.sampledStateCount;
    D3D12_RANGE stateReadRange{0, stateBytes};
    void* mappedState = nullptr;
    if (stateBytes > 0 &&
        SUCCEEDED(particleDedicatedStateReadback_->Map(0, &stateReadRange, &mappedState))) {
        if (mappedState != nullptr) {
            std::memcpy(particleDedicatedReadbackTelemetry_.stateSamples, mappedState, stateBytes);
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        particleDedicatedStateReadback_->Unmap(0, &emptyWriteRange);
    }

    D3D12_RANGE drawArgsReadRange{0, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)};
    void* mappedDrawArgs = nullptr;
    if (SUCCEEDED(particleDedicatedIndirectArgsReadback_->Map(0, &drawArgsReadRange, &mappedDrawArgs))) {
        if (mappedDrawArgs != nullptr) {
            std::memcpy(
                &particleDedicatedReadbackTelemetry_.drawArgs,
                mappedDrawArgs,
                sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
            particleDedicatedReadbackTelemetry_.drawArgsValid = true;
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        particleDedicatedIndirectArgsReadback_->Unmap(0, &emptyWriteRange);
    }

    particleDedicatedReadbackTelemetry_.valid = true;
    particleDedicatedReadbackPending_ = false;
}

void AppGpuParticleSystem::CaptureDistortionDedicatedReadbackTelemetry(ID3D12GraphicsCommandList* commandList) {
    distortionDedicatedReadbackTelemetry_.copiedThisFrame = false;
    distortionDedicatedReadbackPending_ = false;
    if (commandList == nullptr ||
        dedicatedDistortionOutput_ == nullptr ||
        dedicatedDistortionIndirectArgs_ == nullptr ||
        distortionDedicatedRenderReadback_ == nullptr ||
        distortionDedicatedIndirectArgsReadback_ == nullptr ||
        dedicatedDistortionOutputBytes_ == 0 ||
        dedicatedDistortionIndirectArgsBytes_ < sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)) {
        return;
    }

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    if (dedicatedDistortionOutputState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            dedicatedDistortionOutput_.Get(),
            dedicatedDistortionOutputState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        dedicatedDistortionOutputState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (dedicatedDistortionIndirectArgsState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            dedicatedDistortionIndirectArgs_.Get(),
            dedicatedDistortionIndirectArgsState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        dedicatedDistortionIndirectArgsState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (!barriers.empty()) {
        commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    const size_t renderBytes = (std::min)(
        dedicatedDistortionOutputBytes_,
        sizeof(ParticleRenderSample) * kParticleDedicatedTelemetrySampleCount);
    commandList->CopyBufferRegion(
        distortionDedicatedRenderReadback_.Get(),
        0,
        dedicatedDistortionOutput_.Get(),
        0,
        renderBytes);
    commandList->CopyBufferRegion(
        distortionDedicatedIndirectArgsReadback_.Get(),
        0,
        dedicatedDistortionIndirectArgs_.Get(),
        0,
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));

    distortionDedicatedReadbackTelemetry_.sampledRenderCount =
        static_cast<uint32_t>(renderBytes / sizeof(ParticleRenderSample));
    distortionDedicatedReadbackTelemetry_.drawArgsValid = false;
    distortionDedicatedReadbackPending_ = true;
    distortionDedicatedReadbackTelemetry_.copiedThisFrame = true;
}

void AppGpuParticleSystem::ResolveDistortionDedicatedReadbackTelemetry() {
    if (!distortionDedicatedReadbackPending_ ||
        distortionDedicatedRenderReadback_ == nullptr ||
        distortionDedicatedIndirectArgsReadback_ == nullptr) {
        distortionDedicatedReadbackTelemetry_.copiedThisFrame = false;
        return;
    }

    const size_t renderBytes =
        sizeof(ParticleRenderSample) * distortionDedicatedReadbackTelemetry_.sampledRenderCount;
    D3D12_RANGE renderReadRange{0, renderBytes};
    void* mappedRender = nullptr;
    if (renderBytes > 0 &&
        SUCCEEDED(distortionDedicatedRenderReadback_->Map(0, &renderReadRange, &mappedRender))) {
        if (mappedRender != nullptr) {
            std::memcpy(distortionDedicatedReadbackTelemetry_.renderSamples, mappedRender, renderBytes);
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        distortionDedicatedRenderReadback_->Unmap(0, &emptyWriteRange);
    }

    D3D12_RANGE drawArgsReadRange{0, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)};
    void* mappedDrawArgs = nullptr;
    if (SUCCEEDED(distortionDedicatedIndirectArgsReadback_->Map(0, &drawArgsReadRange, &mappedDrawArgs))) {
        if (mappedDrawArgs != nullptr) {
            std::memcpy(
                &distortionDedicatedReadbackTelemetry_.drawArgs,
                mappedDrawArgs,
                sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
            distortionDedicatedReadbackTelemetry_.drawArgsValid = true;
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        distortionDedicatedIndirectArgsReadback_->Unmap(0, &emptyWriteRange);
    }

    distortionDedicatedReadbackTelemetry_.valid = true;
    distortionDedicatedReadbackPending_ = false;
}

void AppGpuParticleSystem::CaptureBeamDedicatedReadbackTelemetry(ID3D12GraphicsCommandList* commandList) {
    beamDedicatedReadbackTelemetry_.copiedThisFrame = false;
    beamDedicatedReadbackPending_ = false;
    if (commandList == nullptr ||
        dedicatedBeamOutput_ == nullptr ||
        dedicatedBeamIndirectArgs_ == nullptr ||
        beamDedicatedRenderReadback_ == nullptr ||
        beamDedicatedIndirectArgsReadback_ == nullptr ||
        dedicatedBeamOutputBytes_ == 0 ||
        dedicatedBeamIndirectArgsBytes_ < sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)) {
        return;
    }

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    if (dedicatedBeamOutputState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            dedicatedBeamOutput_.Get(),
            dedicatedBeamOutputState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        dedicatedBeamOutputState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (dedicatedBeamIndirectArgsState_ != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers.push_back(MakeTransition(
            dedicatedBeamIndirectArgs_.Get(),
            dedicatedBeamIndirectArgsState_,
            D3D12_RESOURCE_STATE_COPY_SOURCE));
        dedicatedBeamIndirectArgsState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    if (!barriers.empty()) {
        commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    const size_t renderBytes = (std::min)(
        dedicatedBeamOutputBytes_,
        sizeof(ParticleRenderSample) * kParticleDedicatedTelemetrySampleCount);
    commandList->CopyBufferRegion(
        beamDedicatedRenderReadback_.Get(),
        0,
        dedicatedBeamOutput_.Get(),
        0,
        renderBytes);
    commandList->CopyBufferRegion(
        beamDedicatedIndirectArgsReadback_.Get(),
        0,
        dedicatedBeamIndirectArgs_.Get(),
        0,
        sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));

    beamDedicatedReadbackTelemetry_.sampledRenderCount =
        static_cast<uint32_t>(renderBytes / sizeof(ParticleRenderSample));
    beamDedicatedReadbackTelemetry_.drawArgsValid = false;
    beamDedicatedReadbackPending_ = true;
    beamDedicatedReadbackTelemetry_.copiedThisFrame = true;
}

void AppGpuParticleSystem::ResolveBeamDedicatedReadbackTelemetry() {
    if (!beamDedicatedReadbackPending_ ||
        beamDedicatedRenderReadback_ == nullptr ||
        beamDedicatedIndirectArgsReadback_ == nullptr) {
        beamDedicatedReadbackTelemetry_.copiedThisFrame = false;
        return;
    }

    const size_t renderBytes =
        sizeof(ParticleRenderSample) * beamDedicatedReadbackTelemetry_.sampledRenderCount;
    D3D12_RANGE renderReadRange{0, renderBytes};
    void* mappedRender = nullptr;
    if (renderBytes > 0 &&
        SUCCEEDED(beamDedicatedRenderReadback_->Map(0, &renderReadRange, &mappedRender))) {
        if (mappedRender != nullptr) {
            std::memcpy(beamDedicatedReadbackTelemetry_.renderSamples, mappedRender, renderBytes);
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        beamDedicatedRenderReadback_->Unmap(0, &emptyWriteRange);
    }

    D3D12_RANGE drawArgsReadRange{0, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS)};
    void* mappedDrawArgs = nullptr;
    if (SUCCEEDED(beamDedicatedIndirectArgsReadback_->Map(0, &drawArgsReadRange, &mappedDrawArgs))) {
        if (mappedDrawArgs != nullptr) {
            std::memcpy(
                &beamDedicatedReadbackTelemetry_.drawArgs,
                mappedDrawArgs,
                sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
            beamDedicatedReadbackTelemetry_.drawArgsValid = true;
        }
        const D3D12_RANGE emptyWriteRange{0, 0};
        beamDedicatedIndirectArgsReadback_->Unmap(0, &emptyWriteRange);
    }

    beamDedicatedReadbackTelemetry_.valid = true;
    beamDedicatedReadbackPending_ = false;
}

void AppGpuParticleSystem::SetResourceState(std::string_view name, D3D12_RESOURCE_STATES state) {
    if (name == "ParticleRenderBuffer") {
        particleOutputState_ = state;
    } else if (name == "ParticleDedicatedRenderBuffer") {
        dedicatedParticleOutputState_ = state;
    } else if (name == "DistortionDedicatedRenderBuffer") {
        dedicatedDistortionOutputState_ = state;
    } else if (name == "BeamDedicatedRenderBuffer") {
        dedicatedBeamOutputState_ = state;
    } else if (name == "TrailControlPointBuffer") {
        trailControlPointState_ = state;
    } else if (name == "TrailSegmentBuffer") {
        trailSegmentState_ = state;
    } else if (name == "TrailVertexBuffer") {
        trailVertexState_ = state;
    } else if (name == "TrailIndexBuffer") {
        trailIndexState_ = state;
    } else if (name == "TrailDrawArgs") {
        trailDrawArgsState_ = state;
    } else if (name == "ParticleState") {
        particleStateState_ = state;
    } else if (name == "ParticleDedicatedState") {
        dedicatedParticleStateState_ = state;
    } else if (name == "DistortionDedicatedState") {
        dedicatedDistortionStateState_ = state;
    } else if (name == "BeamDedicatedState") {
        dedicatedBeamStateState_ = state;
    } else if (name == "ParticleIndirectArgs") {
        indirectArgsState_ = state;
    } else if (name == "ParticleDedicatedIndirectArgs") {
        dedicatedIndirectArgsState_ = state;
    } else if (name == "DistortionDedicatedIndirectArgs") {
        dedicatedDistortionIndirectArgsState_ = state;
    } else if (name == "BeamDedicatedIndirectArgs") {
        dedicatedBeamIndirectArgsState_ = state;
    }
}
