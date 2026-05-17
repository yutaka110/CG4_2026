#include "AppSceneRenderPipeline.h"

#include <Windows.h>

#include "AppFrameGraphBuilder.h"
#include "AppFrameRenderer.h"
#include "AppPipelines.h"
#include "AppRuntimeState.h"
#include "AppSceneResources.h"
#include "graphics/RenderGraph.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

constexpr uint32_t kSkinningThreadGroupSize = 256;

enum SkinningComputeRootParameter : uint32_t {
    kSkinningRootInputVertices = 0,
    kSkinningRootInfluences = 1,
    kSkinningRootPalette = 2,
    kSkinningRootOutputVertices = 3,
    kSkinningRootInfo = 4,
};

class SkinningGpuTimingProbe {
public:
    void Begin(
        ID3D12GraphicsCommandList* commandList,
        AppRuntimeState* runtimeState,
        const char* path,
        uint32_t vertexCount,
        uint32_t indexCount,
        uint32_t groupCount) {
        if (commandList == nullptr || path == nullptr) {
            return;
        }

        RefreshLogPath();
        if (!EnsureResources(commandList)) {
            return;
        }

        WritePreviousResult();

        pending_ = true;
        currentRuntimeState_ = runtimeState;
        currentPath_ = path;
        currentVertexCount_ = vertexCount;
        currentIndexCount_ = indexCount;
        currentGroupCount_ = groupCount;
        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    }

    void End(ID3D12GraphicsCommandList* commandList) {
        if (!pending_ || commandList == nullptr || queryHeap_ == nullptr || readback_ == nullptr) {
            return;
        }

        commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
        commandList->ResolveQueryData(
            queryHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            2,
            readback_.Get(),
            0);
    }

    void Cancel() {
        pending_ = false;
        currentRuntimeState_ = nullptr;
    }

private:
    void RefreshLogPath() {
        char path[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableA("GE3_SKINNING_TIMING_LOG", path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) {
            logEnabled_ = false;
            logPath_.clear();
            headerWritten_ = false;
            return;
        }

        if (logPath_ != path) {
            logPath_ = path;
            headerWritten_ = false;
        }
        logEnabled_ = true;
    }

    bool EnsureResources(ID3D12GraphicsCommandList* commandList) {
        if (queryHeap_ != nullptr && readback_ != nullptr) {
            return true;
        }

        ComPtr<ID3D12Device> device;
        if (FAILED(commandList->GetDevice(IID_PPV_ARGS(&device)))) {
            return false;
        }

        D3D12_QUERY_HEAP_DESC queryHeapDesc{};
        queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryHeapDesc.Count = 2;
        if (FAILED(device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap_)))) {
            return false;
        }

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = sizeof(uint64_t) * 2;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        return SUCCEEDED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&readback_)));
    }

    void WritePreviousResult() {
        if (!pending_ || readback_ == nullptr) {
            return;
        }

        uint64_t* timestamps = nullptr;
        D3D12_RANGE readRange{0, sizeof(uint64_t) * 2};
        if (FAILED(readback_->Map(0, &readRange, reinterpret_cast<void**>(&timestamps))) ||
            timestamps == nullptr) {
            return;
        }
        const uint64_t begin = timestamps[0];
        const uint64_t end = timestamps[1];
        const uint64_t ticks = end >= begin ? end - begin : 0;
        D3D12_RANGE writeRange{0, 0};
        readback_->Unmap(0, &writeRange);

        UpdateRuntimeStats(ticks);

        if (!logEnabled_) {
            pending_ = false;
            currentRuntimeState_ = nullptr;
            return;
        }

        std::ofstream log(logPath_, std::ios::out | std::ios::app);
        if (!log) {
            pending_ = false;
            currentRuntimeState_ = nullptr;
            return;
        }

        if (!headerWritten_) {
            log << "path,vertexCount,indexCount,groupCount,gpuTicks\n";
            headerWritten_ = true;
        }

        log << currentPath_ << ","
            << currentVertexCount_ << ","
            << currentIndexCount_ << ","
            << currentGroupCount_ << ","
            << ticks << "\n";
        pending_ = false;
        currentRuntimeState_ = nullptr;
    }

    RuntimeSkinningTimingPathStats* SelectStatsPath() {
        if (currentRuntimeState_ == nullptr) {
            return nullptr;
        }
        if (currentPath_ == "vertex_shader_total") {
            return &currentRuntimeState_->skinningTiming.vertexShaderTotal;
        }
        if (currentPath_ == "compute_total") {
            return &currentRuntimeState_->skinningTiming.computeTotal;
        }
        if (currentPath_ == "compute_surface_only") {
            return &currentRuntimeState_->skinningTiming.computeSurfaceOnly;
        }
        return nullptr;
    }

    void UpdateRuntimeStats(uint64_t ticks) {
        RuntimeSkinningTimingPathStats* stats = SelectStatsPath();
        if (stats == nullptr) {
            return;
        }

        stats->valid = true;
        stats->lastTicks = ticks;
        if (stats->sampleCount == 0) {
            stats->minTicks = ticks;
            stats->maxTicks = ticks;
            stats->averageTicks = static_cast<double>(ticks);
        } else {
            stats->minTicks = (std::min)(stats->minTicks, ticks);
            stats->maxTicks = (std::max)(stats->maxTicks, ticks);
            stats->averageTicks +=
                (static_cast<double>(ticks) - stats->averageTicks) /
                static_cast<double>(stats->sampleCount + 1);
        }
        ++stats->sampleCount;
    }

    ComPtr<ID3D12QueryHeap> queryHeap_;
    ComPtr<ID3D12Resource> readback_;
    AppRuntimeState* currentRuntimeState_ = nullptr;
    std::string logPath_;
    std::string currentPath_;
    uint32_t currentVertexCount_ = 0;
    uint32_t currentIndexCount_ = 0;
    uint32_t currentGroupCount_ = 0;
    bool pending_ = false;
    bool headerWritten_ = false;
    bool logEnabled_ = false;
};

SkinningGpuTimingProbe& GetSkinningGpuTimingProbe() {
    static SkinningGpuTimingProbe probe;
    return probe;
}

void TransitionResource(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    if (commandList == nullptr || resource == nullptr || before == after) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}

void WriteSkinningComputeProbe(uint32_t vertexCount, uint32_t groupCount) {
    char logPath[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableA(
        "GE3_SKINNING_COMPUTE_PROBE_LOG",
        logPath,
        MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return;
    }

    static bool wrote = false;
    if (wrote) {
        return;
    }
    wrote = true;

    std::ofstream log(logPath, std::ios::out | std::ios::trunc);
    if (!log) {
        return;
    }
    log << "SkinningComputePath=Dispatch\n";
    log << "VertexCount=" << vertexCount << "\n";
    log << "ThreadGroupSize=" << kSkinningThreadGroupSize << "\n";
    log << "GroupCount=" << groupCount << "\n";
}

bool IsComputeSkinningDisabled() {
    return GetEnvironmentVariableA("GE3_DISABLE_COMPUTE_SKINNING", nullptr, 0) > 0;
}

bool RequiresSkinnedSurfaceVfx(const AppRuntimeState* runtimeState) {
    return runtimeState != nullptr && runtimeState->vfx.enableSkinnedSurfaceVfx;
}

bool UploadPaletteIfNeeded(
    ID3D12GraphicsCommandList* commandList,
    SkinCluster& skinCluster) {
    if (!skinCluster.paletteDirty) {
        return true;
    }
    if (commandList == nullptr ||
        skinCluster.paletteResource == nullptr ||
        skinCluster.paletteUploadResource == nullptr ||
        skinCluster.mappedPaletteUpload == nullptr ||
        skinCluster.paletteEntries.empty()) {
        return false;
    }

    const size_t paletteBytes =
        sizeof(JointPaletteEntry) * skinCluster.paletteEntries.size();
    std::memcpy(
        skinCluster.mappedPaletteUpload,
        skinCluster.paletteEntries.data(),
        paletteBytes);

    TransitionResource(
        commandList,
        skinCluster.paletteResource.Get(),
        skinCluster.paletteState,
        D3D12_RESOURCE_STATE_COPY_DEST);
    skinCluster.paletteState = D3D12_RESOURCE_STATE_COPY_DEST;

    commandList->CopyBufferRegion(
        skinCluster.paletteResource.Get(),
        0,
        skinCluster.paletteUploadResource.Get(),
        0,
        paletteBytes);

    TransitionResource(
        commandList,
        skinCluster.paletteResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    skinCluster.paletteState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    skinCluster.paletteDirty = false;
    return true;
}

bool DispatchSkinningPass(
    const AppFrameGraphBuildContext& ctx,
    ge3::graphics::RenderPassContext& passContext,
    SkinnedModelInstance& instance) {
    SkinCluster& skinCluster = instance.skinCluster;
    if (!instance.loaded ||
        IsComputeSkinningDisabled() ||
        instance.mesh.vertexCount == 0 ||
        ctx.srvDescriptorHeap == nullptr ||
        ctx.appPipelines->GetSkinningComputeRootSignature() == nullptr ||
        ctx.appPipelines->GetSkinningComputePSO() == nullptr ||
        skinCluster.vertexSrvGpu.ptr == 0 ||
        skinCluster.influenceSrvGpu.ptr == 0 ||
        skinCluster.paletteSrvGpu.ptr == 0 ||
        skinCluster.skinnedVertexUavGpu.ptr == 0 ||
        skinCluster.skinningInfoResource == nullptr ||
        skinCluster.skinnedVertexResource == nullptr) {
        return false;
    }

    if (!UploadPaletteIfNeeded(passContext.commandList, skinCluster)) {
        return false;
    }

    TransitionResource(
        passContext.commandList,
        skinCluster.skinnedVertexResource.Get(),
        skinCluster.skinnedVertexState,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    skinCluster.skinnedVertexState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    ID3D12DescriptorHeap* descriptorHeaps[] = { ctx.srvDescriptorHeap };
    passContext.commandList->SetDescriptorHeaps(1, descriptorHeaps);
    passContext.commandList->SetComputeRootSignature(
        ctx.appPipelines->GetSkinningComputeRootSignature());
    passContext.commandList->SetPipelineState(ctx.appPipelines->GetSkinningComputePSO());
    passContext.commandList->SetComputeRootDescriptorTable(
        kSkinningRootInputVertices,
        skinCluster.vertexSrvGpu);
    passContext.commandList->SetComputeRootDescriptorTable(
        kSkinningRootInfluences,
        skinCluster.influenceSrvGpu);
    passContext.commandList->SetComputeRootDescriptorTable(
        kSkinningRootPalette,
        skinCluster.paletteSrvGpu);
    passContext.commandList->SetComputeRootDescriptorTable(
        kSkinningRootOutputVertices,
        skinCluster.skinnedVertexUavGpu);
    passContext.commandList->SetComputeRootConstantBufferView(
        kSkinningRootInfo,
        skinCluster.skinningInfoResource->GetGPUVirtualAddress());

    const uint32_t groupCount =
        (instance.mesh.vertexCount + kSkinningThreadGroupSize - 1) / kSkinningThreadGroupSize;
    WriteSkinningComputeProbe(instance.mesh.vertexCount, groupCount);
    passContext.commandList->Dispatch(groupCount, 1, 1);

    TransitionResource(
        passContext.commandList,
        skinCluster.skinnedVertexResource.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    skinCluster.skinnedVertexState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    return true;
}

bool DispatchComputeSkinnedSurface(
    const AppFrameGraphBuildContext& ctx,
    ge3::graphics::RenderPassContext& passContext,
    SkinnedModelInstance& instance) {
    if (!instance.loaded || IsComputeSkinningDisabled()) {
        return false;
    }

    const uint32_t groupCount =
        (instance.mesh.vertexCount + kSkinningThreadGroupSize - 1) / kSkinningThreadGroupSize;
    GetSkinningGpuTimingProbe().Begin(
        passContext.commandList,
        ctx.runtimeState,
        "compute_surface_only",
        instance.mesh.vertexCount,
        instance.mesh.indexCount,
        groupCount);

    if (!DispatchSkinningPass(ctx, passContext, instance)) {
        GetSkinningGpuTimingProbe().Cancel();
        return false;
    }

    GetSkinningGpuTimingProbe().End(passContext.commandList);
    return true;
}

bool DrawComputeSkinnedModelInstance(
    const AppFrameGraphBuildContext& ctx,
    ge3::graphics::RenderPassContext& passContext,
    SkinnedModelInstance& instance) {
    if (!instance.loaded ||
        IsComputeSkinningDisabled() ||
        !instance.visible ||
        !instance.transformResource ||
        instance.skinCluster.skinnedVertexBufferView.BufferLocation == 0 ||
        ctx.srvDescriptorHeap == nullptr ||
        ctx.appPipelines->GetSkinningComputeRootSignature() == nullptr ||
        ctx.appPipelines->GetSkinningComputePSO() == nullptr ||
        instance.skinCluster.vertexSrvGpu.ptr == 0 ||
        instance.skinCluster.influenceSrvGpu.ptr == 0 ||
        instance.skinCluster.paletteSrvGpu.ptr == 0 ||
        instance.skinCluster.skinnedVertexUavGpu.ptr == 0 ||
        instance.skinCluster.skinningInfoResource == nullptr ||
        instance.skinCluster.skinnedVertexResource == nullptr) {
        return false;
    }

    const uint32_t groupCount =
        (instance.mesh.vertexCount + kSkinningThreadGroupSize - 1) / kSkinningThreadGroupSize;
    GetSkinningGpuTimingProbe().Begin(
        passContext.commandList,
        ctx.runtimeState,
        "compute_total",
        instance.mesh.vertexCount,
        instance.mesh.indexCount,
        groupCount);

    if (!DispatchSkinningPass(ctx, passContext, instance)) {
        GetSkinningGpuTimingProbe().Cancel();
        return false;
    }

    const bool mainReady = ctx.frameRenderer->PrepareMainPass(
        passContext.commandList,
        ctx.runtimeState->viewport,
        ctx.runtimeState->scissorRect,
        ctx.appPipelines->GetMainRootSignature(),
        ctx.appPipelines->GetMainPSO());
    if (!mainReady) {
        GetSkinningGpuTimingProbe().Cancel();
        return false;
    }

    ctx.frameRenderer->DrawMainModel(
        passContext.commandList,
        instance.skinCluster.skinnedVertexBufferView,
        instance.mesh.ibv,
        ctx.scene->materialResource->GetGPUVirtualAddress(),
        instance.transformResource->GetGPUVirtualAddress(),
        ctx.scene->textureSrvHandleGPU,
        ctx.scene->textureSrvHandleGPU2,
        ctx.scene->textureSrvHandleGPU2,
        ctx.scene->skyboxTextureSrvHandleGPU,
        ctx.scene->directionalLightResource->GetGPUVirtualAddress(),
        ctx.scene->cameraResource->GetGPUVirtualAddress(),
        ctx.scene->pointLightResource->GetGPUVirtualAddress(),
        ctx.scene->spotLightResource->GetGPUVirtualAddress(),
        instance.mesh.indexCount);
    GetSkinningGpuTimingProbe().End(passContext.commandList);
    return true;
}

void DrawSkinnedModelInstance(
    const AppFrameGraphBuildContext& ctx,
    ge3::graphics::RenderPassContext& passContext,
    SkinnedModelInstance& instance) {
    if (!instance.loaded ||
        !instance.visible ||
        !instance.transformResource ||
        instance.skinCluster.paletteSrvGpu.ptr == 0) {
        return;
    }

    GetSkinningGpuTimingProbe().Begin(
        passContext.commandList,
        ctx.runtimeState,
        "vertex_shader_total",
        instance.mesh.vertexCount,
        instance.mesh.indexCount,
        0);

    if (!UploadPaletteIfNeeded(passContext.commandList, instance.skinCluster)) {
        GetSkinningGpuTimingProbe().Cancel();
        return;
    }

    const bool skinnedReady = ctx.frameRenderer->PrepareMainPass(
        passContext.commandList,
        ctx.runtimeState->viewport,
        ctx.runtimeState->scissorRect,
        ctx.appPipelines->GetSkinnedRootSignature(),
        ctx.appPipelines->GetSkinnedPSO());
    if (!skinnedReady) {
        GetSkinningGpuTimingProbe().Cancel();
        return;
    }

    ctx.frameRenderer->DrawSkinnedModel(
        passContext.commandList,
        instance.mesh.vbv,
        instance.skinCluster.influenceBufferView,
        instance.mesh.ibv,
        ctx.scene->materialResource->GetGPUVirtualAddress(),
        instance.transformResource->GetGPUVirtualAddress(),
        ctx.scene->textureSrvHandleGPU,
        ctx.scene->textureSrvHandleGPU2,
        ctx.scene->textureSrvHandleGPU2,
        ctx.scene->skyboxTextureSrvHandleGPU,
        instance.skinCluster.paletteSrvGpu,
        ctx.scene->directionalLightResource->GetGPUVirtualAddress(),
        ctx.scene->cameraResource->GetGPUVirtualAddress(),
        ctx.scene->pointLightResource->GetGPUVirtualAddress(),
        ctx.scene->spotLightResource->GetGPUVirtualAddress(),
        instance.mesh.indexCount);
    GetSkinningGpuTimingProbe().End(passContext.commandList);
}

} // namespace

void AppSceneRenderPipeline::RegisterPasses(const AppFrameGraphBuildContext& ctx) const {
    const float opaqueBlack[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ctx.renderGraph->DeclarePersistentRenderTarget(
        "SceneColor",
        1.0f,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        opaqueBlack,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    ctx.renderGraph->DeclarePersistentDepthTarget(
        "SceneDepth",
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        1.0f,
        0,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    ctx.renderGraph->DeclarePersistentDepthTarget(
        "SceneDepthReadOnly",
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        1.0f,
        0,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);

    ctx.renderGraph->AddPass({
        "Background.Skybox",
        ge3::graphics::RenderPassLayer::Geometry,
        {
            {"SceneColor", ge3::graphics::RenderResourceAccessType::WriteRtv},
        },
        "",
        [ctx](ge3::graphics::RenderPassContext& passContext) {
            const bool skyboxReady = ctx.frameRenderer->PrepareMainPass(
                passContext.commandList,
                ctx.runtimeState->viewport,
                ctx.runtimeState->scissorRect,
                ctx.appPipelines->GetSkyboxRootSignature(),
                ctx.appPipelines->GetSkyboxPSO());

            if (skyboxReady &&
                ctx.scene->skybox.cbvResource &&
                ctx.scene->skyboxTextureSrvHandleGPU.ptr != 0) {
                ctx.frameRenderer->DrawSkybox(
                    passContext.commandList,
                    ctx.srvDescriptorHeap,
                    ctx.scene->skybox.vbv,
                    ctx.scene->skybox.cbvResource->GetGPUVirtualAddress(),
                    ctx.scene->skyboxTextureSrvHandleGPU,
                    ctx.scene->skybox.vertexCount);
            }
        }});

    ctx.renderGraph->AddPass({
        "Geometry.Sprite",
        ge3::graphics::RenderPassLayer::Geometry,
        {
            {"SceneColor", ge3::graphics::RenderResourceAccessType::WriteRtv},
            {"SceneDepth", ge3::graphics::RenderResourceAccessType::WriteDepth},
        },
        "SceneDepth",
        [ctx](ge3::graphics::RenderPassContext& passContext) {
            const bool spritePassReady = ctx.frameRenderer->PrepareMainPass(
                passContext.commandList,
                ctx.runtimeState->viewport,
                ctx.runtimeState->scissorRect,
                ctx.appPipelines->GetSpriteRootSignature(),
                ctx.appPipelines->GetSpritePSO());

            if (spritePassReady &&
                ctx.scene->materialResourceSprite &&
                ctx.scene->transformationMatrixResourceSprite) {
                ctx.frameRenderer->DrawSprite(
                    passContext.commandList,
                    ctx.srvDescriptorHeap,
                    ctx.scene->indexBufferViewSprite,
                    ctx.scene->vertexBufferViewSprite,
                    ctx.scene->materialResourceSprite->GetGPUVirtualAddress(),
                    ctx.scene->transformationMatrixResourceSprite->GetGPUVirtualAddress(),
                    ctx.spriteTextureHandle);
            } else {
                OutputDebugStringA("[AppSceneRenderPipeline] Sprite pass skipped because pipeline or resources are not ready.\n");
            }
        }});

    ctx.renderGraph->AddPass({
        "Geometry.MainModel",
        ge3::graphics::RenderPassLayer::Geometry,
        {
            {"SceneColor", ge3::graphics::RenderResourceAccessType::WriteRtv},
            {"SceneDepth", ge3::graphics::RenderResourceAccessType::WriteDepth},
        },
        "SceneDepth",
        [ctx](ge3::graphics::RenderPassContext& passContext) {
            bool mainPassPrepared = false;
            auto prepareMainPass = [&]() {
                if (mainPassPrepared) {
                    return true;
                }
                mainPassPrepared = ctx.frameRenderer->PrepareMainPass(
                    passContext.commandList,
                    ctx.runtimeState->viewport,
                    ctx.runtimeState->scissorRect,
                    ctx.appPipelines->GetMainRootSignature(),
                    ctx.appPipelines->GetMainPSO());
                return mainPassPrepared;
            };

            if (ctx.runtimeState->useMonsterBall && prepareMainPass()) {
                ctx.frameRenderer->DrawMainModel(
                    passContext.commandList,
                    ctx.scene->modelMesh.vbv,
                    ctx.scene->modelMesh.ibv,
                    ctx.scene->materialResource->GetGPUVirtualAddress(),
                    ctx.scene->sphere.cbvResource->GetGPUVirtualAddress(),
                    ctx.scene->textureSrvHandleGPU2,
                    ctx.scene->textureSrvHandleGPU2,
                    ctx.scene->textureSrvHandleGPU2,
                    ctx.scene->skyboxTextureSrvHandleGPU,
                    ctx.scene->directionalLightResource->GetGPUVirtualAddress(),
                    ctx.scene->cameraResource->GetGPUVirtualAddress(),
                    ctx.scene->pointLightResource->GetGPUVirtualAddress(),
                    ctx.scene->spotLightResource->GetGPUVirtualAddress(),
                    ctx.scene->modelMesh.indexCount);
            }

            if (ctx.runtimeState->showAnimatedCube &&
                ctx.scene->animatedCubeTransformResource &&
                ctx.scene->animatedCubeTextureSrvHandleGPU.ptr != 0 &&
                prepareMainPass()) {
                ctx.frameRenderer->DrawMainModel(
                    passContext.commandList,
                    ctx.scene->animatedCubeMesh.vbv,
                    ctx.scene->animatedCubeMesh.ibv,
                    ctx.scene->materialResource->GetGPUVirtualAddress(),
                    ctx.scene->animatedCubeTransformResource->GetGPUVirtualAddress(),
                    ctx.scene->animatedCubeTextureSrvHandleGPU,
                    ctx.scene->textureSrvHandleGPU2,
                    ctx.scene->textureSrvHandleGPU2,
                    ctx.scene->skyboxTextureSrvHandleGPU,
                    ctx.scene->directionalLightResource->GetGPUVirtualAddress(),
                    ctx.scene->cameraResource->GetGPUVirtualAddress(),
                    ctx.scene->pointLightResource->GetGPUVirtualAddress(),
                    ctx.scene->spotLightResource->GetGPUVirtualAddress(),
                    ctx.scene->animatedCubeMesh.indexCount);
            }

            if (ctx.runtimeState->showSkinnedModel) {
                if (SkinnedModelInstance* activeSkinnedModel =
                        ctx.scene->GetActiveSkinnedModel()) {
                    const bool needsSkinnedSurfaceVfx = RequiresSkinnedSurfaceVfx(ctx.runtimeState);
                    if (needsSkinnedSurfaceVfx &&
                        !DrawComputeSkinnedModelInstance(ctx, passContext, *activeSkinnedModel)) {
                        DrawSkinnedModelInstance(ctx, passContext, *activeSkinnedModel);
                    } else if (!needsSkinnedSurfaceVfx) {
                        DrawSkinnedModelInstance(ctx, passContext, *activeSkinnedModel);
                    }
                }
            } else if (RequiresSkinnedSurfaceVfx(ctx.runtimeState)) {
                if (SkinnedModelInstance* activeSkinnedModel =
                        ctx.scene->GetActiveSkinnedModel()) {
                    DispatchComputeSkinnedSurface(ctx, passContext, *activeSkinnedModel);
                }
            }
        }});

    ctx.renderGraph->AddPass({
        "Debug.Skeleton",
        ge3::graphics::RenderPassLayer::Geometry,
        {
            {"SceneColor", ge3::graphics::RenderResourceAccessType::WriteRtv},
            {"SceneDepth", ge3::graphics::RenderResourceAccessType::ReadDepth},
        },
        "SceneDepth",
        [ctx](ge3::graphics::RenderPassContext& passContext) {
            if (!ctx.runtimeState->showSkeletonDebug ||
                ctx.scene->skeletonDebugVertexCount == 0 ||
                !ctx.scene->skeletonDebugTransformResource) {
                return;
            }

            const bool ready = ctx.frameRenderer->PrepareMainPass(
                passContext.commandList,
                ctx.runtimeState->viewport,
                ctx.runtimeState->scissorRect,
                ctx.appPipelines->GetSkeletonDebugRootSignature(),
                ctx.appPipelines->GetSkeletonDebugPSO());
            if (!ready) {
                return;
            }

            ctx.frameRenderer->DrawSkeletonDebugLines(
                passContext.commandList,
                ctx.scene->skeletonDebugVBV,
                ctx.scene->skeletonDebugTransformResource->GetGPUVirtualAddress(),
                ctx.scene->skeletonDebugVertexCount);
        }});
}
