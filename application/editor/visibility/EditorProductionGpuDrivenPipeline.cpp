#include "EditorProductionGpuDrivenPipeline.h"

#include "core/ShaderCompiler.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace editor {
namespace {

using Microsoft::WRL::ComPtr;

void SetError(std::string* output, std::string value) {
    if (output != nullptr) *output = std::move(value);
}

uint64_t Align256(uint64_t value) { return (value + 255ull) & ~255ull; }

bool CreateBuffer(ID3D12Device* device, uint64_t bytes, D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state,
    ComPtr<ID3D12Resource>& output, void** mapped = nullptr) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = heapType;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = (std::max)(256ull, Align256(bytes));
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            state, nullptr, IID_PPV_ARGS(output.ReleaseAndGetAddressOf())))) return false;
    if (mapped != nullptr) {
        *mapped = nullptr;
        if (FAILED(output->Map(0, nullptr, mapped)) || *mapped == nullptr) return false;
        std::memset(*mapped, 0, static_cast<size_t>(desc.Width));
    }
    return true;
}

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

} // namespace

struct EditorProductionGpuDrivenPipeline::GpuInstance {
    float bounds[4]{};
    uint32_t transformAddress[2]{};
    uint32_t batchIndex = 0;
    uint32_t reserved = 0;
};

struct EditorProductionGpuDrivenPipeline::GpuBatch {
    uint32_t commandOffset = 0;
    uint32_t commandCapacity = 0;
    uint32_t indexCount = 0;
    uint32_t reserved = 0;
};

struct alignas(256) EditorProductionGpuDrivenPipeline::GpuConstants {
    Matrix4x4 viewProjection = MakeIdentity4x4();
    uint32_t instanceCount = 0;
    uint32_t batchCount = 0;
    uint32_t enableOcclusion = 0;
    float occlusionDepthBias = 0.003f;
};

struct EditorProductionGpuDrivenPipeline::FrameResources {
    ComPtr<ID3D12Resource> instances;
    ComPtr<ID3D12Resource> batches;
    ComPtr<ID3D12Resource> constants;
    ComPtr<ID3D12Resource> commands;
    ComPtr<ID3D12Resource> counts;
    ComPtr<ID3D12Resource> readback;
    GpuInstance* mappedInstances = nullptr;
    GpuBatch* mappedBatches = nullptr;
    GpuConstants* mappedConstants = nullptr;
    D3D12_RESOURCE_STATES commandState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES countState = D3D12_RESOURCE_STATE_COMMON;
    uint64_t fenceValue = 0;
    bool readbackPending = false;
    std::vector<uint32_t> readbackCapacities;
    uint32_t expectedFirstIndexCount = 0;
};

EditorProductionGpuDrivenPipeline::EditorProductionGpuDrivenPipeline() = default;
EditorProductionGpuDrivenPipeline::~EditorProductionGpuDrivenPipeline() { Shutdown(); }

std::vector<EditorProductionGpuDrivenBatchRange>
EditorProductionGpuDrivenPipeline::BuildBatchRanges(
    const std::vector<uint32_t>& batchIndices, uint32_t batchCount) {
    std::vector<EditorProductionGpuDrivenBatchRange> result(batchCount);
    for (uint32_t index : batchIndices) {
        if (index < result.size()) ++result[index].commandCapacity;
    }
    uint32_t offset = 0;
    for (auto& range : result) {
        range.commandOffset = offset;
        offset += range.commandCapacity;
    }
    return result;
}

bool EditorProductionGpuDrivenPipeline::Initialize(
    ID3D12Device* device, ID3D12DescriptorHeap* sharedSrvHeap,
    uint32_t descriptorSize, uint32_t fallbackHiZDescriptorIndex,
    ID3D12RootSignature* mainRootSignature, EditorProductionGpuDrivenPolicy policy,
    std::string* errorMessage) {
    Shutdown();
    if (device == nullptr || sharedSrvHeap == nullptr || descriptorSize == 0 ||
        mainRootSignature == nullptr) {
        SetError(errorMessage, "E-11 requires a D3D12 device, shared SRV heap, and Main root signature.");
        return false;
    }
    policy.maximumInstances = (std::max)(1u, policy.maximumInstances);
    policy.maximumBatches = (std::max)(1u, policy.maximumBatches);
    policy.occlusionDepthBias = (std::max)(0.0f, policy.occlusionDepthBias);
    policy_ = policy;
    device_ = device;
    srvHeap_ = sharedSrvHeap;
    D3D12_HEAP_PROPERTIES fallbackHeap{};
    fallbackHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC fallbackDesc{};
    fallbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    fallbackDesc.Width = 1;
    fallbackDesc.Height = 1;
    fallbackDesc.DepthOrArraySize = 1;
    fallbackDesc.MipLevels = 1;
    fallbackDesc.Format = DXGI_FORMAT_R32_FLOAT;
    fallbackDesc.SampleDesc.Count = 1;
    if (FAILED(device_->CreateCommittedResource(&fallbackHeap, D3D12_HEAP_FLAG_NONE,
            &fallbackDesc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
            IID_PPV_ARGS(fallbackHiZ_.ReleaseAndGetAddressOf())))) {
        SetError(errorMessage, "E-11 failed to allocate its valid fallback Hi-Z descriptor resource.");
        Shutdown();
        return false;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC fallbackSrv{};
    fallbackSrv.Format = DXGI_FORMAT_R32_FLOAT;
    fallbackSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    fallbackSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    fallbackSrv.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE fallbackCpu = sharedSrvHeap->GetCPUDescriptorHandleForHeapStart();
    fallbackCpu.ptr += static_cast<SIZE_T>(fallbackHiZDescriptorIndex) * descriptorSize;
    device_->CreateShaderResourceView(fallbackHiZ_.Get(), &fallbackSrv, fallbackCpu);
    fallbackHiZHandle_ = sharedSrvHeap->GetGPUDescriptorHandleForHeapStart();
    fallbackHiZHandle_.ptr += static_cast<UINT64>(fallbackHiZDescriptorIndex) * descriptorSize;
    if (!CreatePipeline(errorMessage)) {
        Shutdown();
        return false;
    }
    D3D12_INDIRECT_ARGUMENT_DESC args[2]{};
    args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
    args[0].ConstantBufferView.RootParameterIndex = 1;
    args[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    D3D12_COMMAND_SIGNATURE_DESC signature{};
    signature.ByteStride = sizeof(EditorProductionIndirectCommandLayout);
    signature.NumArgumentDescs = 2;
    signature.pArgumentDescs = args;
    if (FAILED(device_->CreateCommandSignature(
            &signature, mainRootSignature, IID_PPV_ARGS(commandSignature_.ReleaseAndGetAddressOf())))) {
        SetError(errorMessage, "E-11 failed to create the CBV + DrawIndexed command signature.");
        Shutdown();
        return false;
    }
    for (auto& frame : frames_) {
        frame = std::make_unique<FrameResources>();
        if (!CreateFrameResources(*frame, errorMessage)) {
            Shutdown();
            return false;
        }
    }
    return true;
}

void EditorProductionGpuDrivenPipeline::Shutdown() {
    for (auto& frame : frames_) {
        if (frame) {
            if (frame->instances && frame->mappedInstances) frame->instances->Unmap(0, nullptr);
            if (frame->batches && frame->mappedBatches) frame->batches->Unmap(0, nullptr);
            if (frame->constants && frame->mappedConstants) frame->constants->Unmap(0, nullptr);
        }
        frame.reset();
    }
    cullShader_.Reset();
    resetShader_.Reset();
    commandSignature_.Reset();
    cullPipelineState_.Reset();
    resetPipelineState_.Reset();
    computeRootSignature_.Reset();
    srvHeap_.Reset();
    fallbackHiZ_.Reset();
    fallbackHiZHandle_ = {};
    device_.Reset();
    batches_.clear();
    cpuFallbackPackets_.clear();
    diagnostics_.clear();
    stats_ = {};
    activeFrame_ = -1;
    scheduledFenceValue_ = 0;
}

bool EditorProductionGpuDrivenPipeline::CreatePipeline(std::string* errorMessage) {
    D3D12_DESCRIPTOR_RANGE hiZRange{};
    hiZRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    hiZRange.NumDescriptors = 1;
    hiZRange.BaseShaderRegister = 2;
    D3D12_ROOT_PARAMETER parameters[6]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[1].Descriptor.ShaderRegister = 0;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[2].Descriptor.ShaderRegister = 1;
    parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    parameters[3].Descriptor.ShaderRegister = 0;
    parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    parameters[4].Descriptor.ShaderRegister = 1;
    parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[5].DescriptorTable.NumDescriptorRanges = 1;
    parameters[5].DescriptorTable.pDescriptorRanges = &hiZRange;
    for (auto& parameter : parameters) parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = static_cast<UINT>(std::size(parameters));
    desc.pParameters = parameters;
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    if (FAILED(D3D12SerializeRootSignature(
            &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)) || !blob ||
        FAILED(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(computeRootSignature_.ReleaseAndGetAddressOf())))) {
        SetError(errorMessage, "E-11 failed to create its compute root signature.");
        return false;
    }
    ge3::core::ShaderCompiler compiler;
    if (!compiler.Initialize()) {
        SetError(errorMessage, "E-11 failed to initialize DXC.");
        return false;
    }
    resetShader_ = compiler.CompileFromFile(
        L"Resources/ProductionVisibility.CS.hlsl", L"ResetCounts", L"cs_6_0");
    cullShader_ = compiler.CompileFromFile(
        L"Resources/ProductionVisibility.CS.hlsl", L"CullAndBuildCommands", L"cs_6_0");
    if (!resetShader_ || !cullShader_) {
        SetError(errorMessage, "E-11 failed to compile ProductionVisibility.CS.hlsl.");
        return false;
    }
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = computeRootSignature_.Get();
    pso.CS = {resetShader_->GetBufferPointer(), resetShader_->GetBufferSize()};
    if (FAILED(device_->CreateComputePipelineState(
            &pso, IID_PPV_ARGS(resetPipelineState_.ReleaseAndGetAddressOf())))) return false;
    pso.CS = {cullShader_->GetBufferPointer(), cullShader_->GetBufferSize()};
    if (FAILED(device_->CreateComputePipelineState(
            &pso, IID_PPV_ARGS(cullPipelineState_.ReleaseAndGetAddressOf())))) return false;
    return true;
}

bool EditorProductionGpuDrivenPipeline::CreateFrameResources(
    FrameResources& frame, std::string* errorMessage) {
    void* mapped = nullptr;
    if (!CreateBuffer(device_.Get(), uint64_t(policy_.maximumInstances) * sizeof(GpuInstance),
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ, frame.instances, &mapped)) goto fail;
    frame.mappedInstances = static_cast<GpuInstance*>(mapped);
    if (!CreateBuffer(device_.Get(), uint64_t(policy_.maximumBatches) * sizeof(GpuBatch),
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ, frame.batches, &mapped)) goto fail;
    frame.mappedBatches = static_cast<GpuBatch*>(mapped);
    if (!CreateBuffer(device_.Get(), sizeof(GpuConstants), D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ,
            frame.constants, &mapped)) goto fail;
    frame.mappedConstants = static_cast<GpuConstants*>(mapped);
    if (!CreateBuffer(device_.Get(), uint64_t(policy_.maximumInstances) * 32ull,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON, frame.commands)) goto fail;
    if (!CreateBuffer(device_.Get(), uint64_t(policy_.maximumBatches) * sizeof(uint32_t),
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON, frame.counts)) goto fail;
    if (!CreateBuffer(device_.Get(), uint64_t(policy_.maximumBatches) * sizeof(uint32_t) +
            sizeof(EditorProductionIndirectCommandLayout),
            D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST, frame.readback)) goto fail;
    return true;
fail:
    SetError(errorMessage, "E-11 failed to allocate triple-buffered visibility resources.");
    return false;
}

void EditorProductionGpuDrivenPipeline::CollectReadbacks(uint64_t completedFenceValue) {
    for (auto& pointer : frames_) {
        FrameResources& frame = *pointer;
        if (!frame.readbackPending || frame.fenceValue > completedFenceValue) continue;
        uint32_t* counts = nullptr;
        const SIZE_T countBytes = SIZE_T(policy_.maximumBatches) * sizeof(uint32_t);
        D3D12_RANGE range{0, countBytes + sizeof(EditorProductionIndirectCommandLayout)};
        if (SUCCEEDED(frame.readback->Map(0, &range, reinterpret_cast<void**>(&counts))) && counts) {
            uint64_t total = 0;
            for (uint32_t i = 0; i < frame.readbackCapacities.size(); ++i)
                total += (std::min)(counts[i], frame.readbackCapacities[i]);
            stats_.gpuVisibleInstances = static_cast<uint32_t>((std::min<uint64_t>)(total, UINT32_MAX));
            const auto* firstCommand = reinterpret_cast<const EditorProductionIndirectCommandLayout*>(
                reinterpret_cast<const uint8_t*>(counts) + countBytes);
            stats_.commandLayoutValidated = !frame.readbackCapacities.empty() && counts[0] != 0 &&
                firstCommand->draw.IndexCountPerInstance == frame.expectedFirstIndexCount &&
                firstCommand->draw.InstanceCount == 1 &&
                firstCommand->stridePadding == 0;
            D3D12_RANGE written{0, 0};
            frame.readback->Unmap(0, &written);
            ++stats_.readbacks;
        }
        frame.readbackPending = false;
    }
}

bool EditorProductionGpuDrivenPipeline::Sync(
    const std::vector<EditorProductionSceneRenderPacket>& candidates,
    const EditorProductionMaterialPipeline& materials,
    const EditorProductionTexturePipeline& textures,
    const EditorProductionShaderPipeline& shaders,
    const Matrix4x4& viewProjection, uint64_t completedFenceValue,
    uint64_t scheduledFenceValue, std::string* errorMessage) {
    CollectReadbacks(completedFenceValue);
    const uint32_t lastVisible = stats_.gpuVisibleInstances;
    const uint32_t readbacks = stats_.readbacks;
    const bool commandLayoutValidated = stats_.commandLayoutValidated;
    stats_ = {};
    stats_.gpuVisibleInstances = lastVisible;
    stats_.readbacks = readbacks;
    stats_.commandLayoutValidated = commandLayoutValidated;
    stats_.submittedInstances = static_cast<uint32_t>(candidates.size());
    batches_.clear();
    cpuFallbackPackets_.clear();
    diagnostics_.clear();
    activeFrame_ = -1;
    scheduledFenceValue_ = scheduledFenceValue;
    if (!device_ || !commandSignature_) {
        SetError(errorMessage, "E-11 is not initialized.");
        return false;
    }
    for (uint32_t i = 0; i < frames_.size(); ++i) {
        if (frames_[i]->fenceValue == 0 || frames_[i]->fenceValue <= completedFenceValue) {
            activeFrame_ = static_cast<int>(i);
            break;
        }
    }
    if (activeFrame_ < 0) {
        ++stats_.ringStalls;
        diagnostics_.push_back("All E-11 frame rings are still owned by the GPU; CPU fallback is active.");
        SetError(errorMessage, diagnostics_.back());
        return false;
    }

    struct Accepted { const EditorProductionSceneRenderPacket* packet; uint32_t batch; };
    std::vector<Accepted> accepted;
    accepted.reserve((std::min<size_t>)(candidates.size(), policy_.maximumInstances));
    for (const auto& packet : candidates) {
        if (packet.indexCount == 0 || packet.transformAddress == 0 ||
            packet.vertexBuffer.BufferLocation == 0 || packet.indexBuffer.BufferLocation == 0) continue;
        D3D12_GPU_VIRTUAL_ADDRESS materialAddress = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE albedo{};
        D3D12_GPU_DESCRIPTOR_HANDLE normal{};
        ID3D12PipelineState* pso = nullptr;
        if (const auto* binding = materials.Resolve(packet.entityGuid, packet.materialSlot);
            binding && !binding->fallback) materialAddress = binding->materialAddress;
        if (const auto* binding = textures.Resolve(packet.entityGuid, packet.materialSlot); binding) {
            if (!binding->albedoFallback) albedo = binding->albedoHandle;
            if (!binding->normalFallback) normal = binding->normalHandle;
        }
        if (const auto* binding = shaders.Resolve(packet.entityGuid, packet.materialSlot); binding)
            pso = binding->pipelineState;
        uint32_t batchIndex = UINT32_MAX;
        for (uint32_t i = 0; i < batches_.size(); ++i) {
            const auto& batch = batches_[i];
            if (batch.representative.assetGuid == packet.assetGuid &&
                batch.representative.lodIndex == packet.lodIndex &&
                batch.representative.materialSlot == packet.materialSlot &&
                batch.representative.vertexBuffer.BufferLocation == packet.vertexBuffer.BufferLocation &&
                batch.representative.indexBuffer.BufferLocation == packet.indexBuffer.BufferLocation &&
                batch.pipelineState == pso && batch.materialAddress == materialAddress &&
                batch.albedoHandle.ptr == albedo.ptr && batch.normalHandle.ptr == normal.ptr) {
                batchIndex = i;
                break;
            }
        }
        if (batchIndex == UINT32_MAX) {
            if (batches_.size() >= policy_.maximumBatches) {
                ++stats_.rejectedByBatchBudget;
                if (packet.cpuVisible) cpuFallbackPackets_.push_back(packet);
                continue;
            }
            batchIndex = static_cast<uint32_t>(batches_.size());
            EditorProductionGpuDrivenBatch batch{};
            batch.representative = packet;
            batch.pipelineState = pso;
            batch.materialAddress = materialAddress;
            batch.albedoHandle = albedo;
            batch.normalHandle = normal;
            batches_.push_back(std::move(batch));
        }
        if (accepted.size() >= policy_.maximumInstances) {
            ++stats_.rejectedByInstanceBudget;
            if (packet.cpuVisible) cpuFallbackPackets_.push_back(packet);
            continue;
        }
        accepted.push_back({&packet, batchIndex});
    }
    std::vector<uint32_t> indices;
    indices.reserve(accepted.size());
    for (const auto& item : accepted) indices.push_back(item.batch);
    const auto ranges = BuildBatchRanges(indices, static_cast<uint32_t>(batches_.size()));
    FrameResources& frame = *frames_[activeFrame_];
    frame.readbackCapacities.clear();
    frame.readbackCapacities.reserve(ranges.size());
    for (uint32_t i = 0; i < batches_.size(); ++i) {
        batches_[i].range = ranges[i];
        frame.mappedBatches[i] = {ranges[i].commandOffset, ranges[i].commandCapacity,
            batches_[i].representative.indexCount, 0};
        frame.readbackCapacities.push_back(ranges[i].commandCapacity);
    }
    frame.expectedFirstIndexCount = batches_.empty() ? 0 : batches_[0].representative.indexCount;
    for (uint32_t i = 0; i < accepted.size(); ++i) {
        const auto& packet = *accepted[i].packet;
        frame.mappedInstances[i] = {{packet.boundsCenter.x, packet.boundsCenter.y,
            packet.boundsCenter.z, packet.boundsRadius},
            {static_cast<uint32_t>(packet.transformAddress),
             static_cast<uint32_t>(packet.transformAddress >> 32)}, accepted[i].batch, 0};
    }
    *frame.mappedConstants = {};
    frame.mappedConstants->viewProjection = viewProjection;
    frame.mappedConstants->instanceCount = static_cast<uint32_t>(accepted.size());
    frame.mappedConstants->batchCount = static_cast<uint32_t>(batches_.size());
    frame.mappedConstants->occlusionDepthBias = policy_.occlusionDepthBias;
    stats_.residentInstances = static_cast<uint32_t>(accepted.size());
    stats_.batches = static_cast<uint32_t>(batches_.size());
    stats_.cpuFallbackPackets = static_cast<uint32_t>(cpuFallbackPackets_.size());
    stats_.ready = !accepted.empty() && !batches_.empty();
    return true;
}

bool EditorProductionGpuDrivenPipeline::DispatchVisibility(
    ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE hiZHandle,
    bool hiZAvailable) {
    if (!Ready() || commandList == nullptr) return false;
    if (hiZHandle.ptr == 0) hiZHandle = fallbackHiZHandle_;
    if (hiZHandle.ptr == 0) return false;
    FrameResources& frame = *frames_[activeFrame_];
    D3D12_RESOURCE_BARRIER transitions[2] = {
        Transition(frame.commands.Get(), frame.commandState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        Transition(frame.counts.Get(), frame.countState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    commandList->ResourceBarrier(2, transitions);
    frame.commandState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    frame.countState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
    commandList->SetDescriptorHeaps(1, heaps);
    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetComputeRootConstantBufferView(0, frame.constants->GetGPUVirtualAddress());
    commandList->SetComputeRootShaderResourceView(1, frame.instances->GetGPUVirtualAddress());
    commandList->SetComputeRootShaderResourceView(2, frame.batches->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(3, frame.commands->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(4, frame.counts->GetGPUVirtualAddress());
    commandList->SetComputeRootDescriptorTable(5, hiZHandle);
    frame.mappedConstants->enableOcclusion = policy_.enableOcclusion && hiZAvailable ? 1u : 0u;
    stats_.occlusionEnabled = frame.mappedConstants->enableOcclusion != 0;
    commandList->SetPipelineState(resetPipelineState_.Get());
    commandList->Dispatch((stats_.batches + 63u) / 64u, 1, 1);
    D3D12_RESOURCE_BARRIER uav[2]{};
    uav[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uav[0].UAV.pResource = frame.commands.Get();
    uav[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uav[1].UAV.pResource = frame.counts.Get();
    commandList->ResourceBarrier(2, uav);
    commandList->SetPipelineState(cullPipelineState_.Get());
    commandList->Dispatch((stats_.residentInstances + 63u) / 64u, 1, 1);
    D3D12_RESOURCE_BARRIER indirect[2] = {
        Transition(frame.commands.Get(), frame.commandState, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
        Transition(frame.counts.Get(), frame.countState, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)};
    commandList->ResourceBarrier(2, indirect);
    frame.commandState = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    frame.countState = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    ++stats_.dispatches;
    return true;
}

void EditorProductionGpuDrivenPipeline::ExecuteBatch(
    ID3D12GraphicsCommandList* commandList, uint32_t batchIndex) const {
    if (!Ready() || commandList == nullptr || batchIndex >= batches_.size()) return;
    const auto& batch = batches_[batchIndex];
    const FrameResources& frame = *frames_[activeFrame_];
    commandList->ExecuteIndirect(commandSignature_.Get(), batch.range.commandCapacity,
        frame.commands.Get(), uint64_t(batch.range.commandOffset) *
            sizeof(EditorProductionIndirectCommandLayout),
        frame.counts.Get(), uint64_t(batchIndex) * sizeof(uint32_t));
}

void EditorProductionGpuDrivenPipeline::RecordReadback(ID3D12GraphicsCommandList* commandList) {
    if (!Ready() || commandList == nullptr) return;
    FrameResources& frame = *frames_[activeFrame_];
    D3D12_RESOURCE_BARRIER toCopy[2] = {
        Transition(frame.counts.Get(), frame.countState, D3D12_RESOURCE_STATE_COPY_SOURCE),
        Transition(frame.commands.Get(), frame.commandState, D3D12_RESOURCE_STATE_COPY_SOURCE)};
    commandList->ResourceBarrier(2, toCopy);
    const uint64_t countBytes = uint64_t(policy_.maximumBatches) * sizeof(uint32_t);
    commandList->CopyBufferRegion(frame.readback.Get(), 0, frame.counts.Get(), 0,
        countBytes);
    commandList->CopyBufferRegion(frame.readback.Get(), countBytes, frame.commands.Get(), 0,
        sizeof(EditorProductionIndirectCommandLayout));
    D3D12_RESOURCE_BARRIER fromCopy[2] = {
        Transition(frame.counts.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT),
        Transition(frame.commands.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)};
    commandList->ResourceBarrier(2, fromCopy);
    frame.countState = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    frame.commandState = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    frame.fenceValue = scheduledFenceValue_;
    frame.readbackPending = true;
}

} // namespace editor
