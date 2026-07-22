#include "VfxComponentDraw.h"

#include "../../AppFrameState.h"
#include "../../AppGpuParticleSystem.h"
#include "../../AppLogFile.h"
#include "../../AppPipelines.h"
#include "../../AppRenderResources.h"
#include "../../AppSceneResources.h"
#include "vfx/VfxResources.h"

#include <fstream>
#include <string_view>

namespace vfx {
namespace {
bool IsParticleDrawDiagnosticsEnabled() noexcept {
    static const bool enabled = []() {
        char value[2] = {};
        const DWORD length = GetEnvironmentVariableA(
            "GE3_PARTICLE_DRAW_DIAGNOSTICS",
            value,
            static_cast<DWORD>(sizeof(value)));
        return length == 1u && value[0] == '1';
    }();
    return enabled;
}

void WriteParticleDrawDiagnostic(
    const char* stage,
    const char* renderBufferResource,
    const char* indirectArgsResource,
    uint32_t useAliveList) {
    if (!IsParticleDrawDiagnosticsEnabled()) {
        return;
    }
    std::ofstream log = app::OpenRotatingLog("logs/particle_draw_diagnostics.log");
    if (log) {
        log << "stage=" << stage
            << " renderBuffer=" << renderBufferResource
            << " indirectArgs=" << indirectArgsResource
            << " useAliveList=" << useAliveList
            << '\n';
    }
}

} // namespace

ComponentDrawParams ResolveParticleDrawParams(
    const EffectParticleSettings* settings,
    const EffectParticleSettings* fallback,
    const ComponentDrawParams& defaults) {
    const EffectParticleSettings* resolved = settings != nullptr ? settings : fallback;
    if (resolved == nullptr) {
        return defaults;
    }

    return {
        resolved->depthFadeSoftness,
        defaults.distortionDepthAttenuation,
        resolved->edgeSoftness,
        defaults.trailTailFade
    };
}

ComponentDrawParams ResolveTrailDrawParams(
    const EffectTrailSettings* settings,
    const ComponentDrawParams& defaults) {
    if (settings == nullptr) {
        return defaults;
    }

    return {
        settings->depthFadeSoftness,
        defaults.distortionDepthAttenuation,
        defaults.particleEdgeSoftness,
        settings->trailTailFade
    };
}

ComponentDrawParams ResolveDistortionDrawParams(
    const EffectDistortionSettings* settings,
    const ComponentDrawParams& defaults) {
    if (settings == nullptr) {
        return defaults;
    }

    return {
        settings->depthFadeSoftness,
        settings->depthAttenuation,
        defaults.particleEdgeSoftness,
        defaults.trailTailFade
    };
}

void DrawIndirectSpriteComponents(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    ID3D12PipelineState* pipelineState,
    const ComponentDrawParams& drawParams,
    const VfxRendererResourceSet* rendererResources,
    bool useTextureIndexing) {
    const char* renderBufferResource =
        rendererResources != nullptr && rendererResources->renderBuffer[0] != '\0'
            ? rendererResources->renderBuffer
            : "ParticleRenderBuffer";
    const char* indirectArgsResource =
        rendererResources != nullptr && rendererResources->indirectArgs[0] != '\0'
            ? rendererResources->indirectArgs
            : "ParticleIndirectArgs";
    const uint32_t useAliveList =
        std::string_view(renderBufferResource) == "ParticleRenderBuffer" &&
        std::string_view(indirectArgsResource) == "ParticleIndirectArgs"
            ? 1u
            : 0u;

    if (commandList == nullptr ||
        context.srvDescriptorHeap == nullptr ||
        context.appPipelines == nullptr ||
        context.renderResources == nullptr ||
        context.scene == nullptr ||
        context.gpuParticleSystem == nullptr ||
        context.frameState == nullptr ||
        pipelineState == nullptr ||
        (!useTextureIndexing && context.vfxTextureHandle.ptr == 0) ||
        context.depthTextureHandle.ptr == 0 ||
        context.gpuParticleSystem->CommandSignature() == nullptr) {
        WriteParticleDrawDiagnostic(
            "rejected-context",
            renderBufferResource,
            indirectArgsResource,
            useAliveList);
        return;
    }
    if (rendererResources != nullptr && !rendererResources->usesIndirectSprite) {
        WriteParticleDrawDiagnostic(
            "rejected-renderer",
            renderBufferResource,
            indirectArgsResource,
            useAliveList);
        return;
    }

    const D3D12_GPU_DESCRIPTOR_HANDLE renderBufferSrv =
        context.gpuParticleSystem->SrvHandleForResource(renderBufferResource);
    // Dedicated particle buffers are already densely packed, so the vertex
    // shader never reads t1 when useAliveList == 0. Point the otherwise-unused
    // root table at the valid render-buffer SRV instead of coupling this path
    // to the shared GPU-pool AliveList allocation.
    const D3D12_GPU_DESCRIPTOR_HANDLE aliveListSrv = useAliveList != 0
        ? context.gpuParticleSystem->SrvHandleForResource("ParticleAliveList")
        : renderBufferSrv;
    ID3D12Resource* indirectArgs =
        context.gpuParticleSystem->IndirectArgsForResource(indirectArgsResource);
    if (renderBufferSrv.ptr == 0 || aliveListSrv.ptr == 0 || indirectArgs == nullptr) {
        WriteParticleDrawDiagnostic(
            "rejected-resources",
            renderBufferResource,
            indirectArgsResource,
            useAliveList);
        return;
    }
    const D3D12_GPU_VIRTUAL_ADDRESS viewProjectionCbv =
        context.gpuParticleSystem->UpdateTrailViewProjection(
            context.frameState->viewProjectionMatrix);
    if (viewProjectionCbv == 0) {
        WriteParticleDrawDiagnostic(
            "rejected-view-projection",
            renderBufferResource,
            indirectArgsResource,
            useAliveList);
        return;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {context.srvDescriptorHeap};
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(context.appPipelines->GetParticleRootSignature());
    commandList->SetPipelineState(pipelineState);
    commandList->SetGraphicsRootConstantBufferView(0, viewProjectionCbv);
    commandList->SetGraphicsRootDescriptorTable(
        1,
        renderBufferSrv);
    commandList->IASetVertexBuffers(0, 1, &context.renderResources->ParticleVertexBufferView());
    commandList->IASetIndexBuffer(&context.scene->indexBufferViewSprite);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D12_GPU_DESCRIPTOR_HANDLE textureTable = context.vfxTextureHandle;
    if (useTextureIndexing && context.srvDescriptorHeap != nullptr) {
        textureTable = context.srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    }
    commandList->SetGraphicsRootDescriptorTable(2, textureTable);
    commandList->SetGraphicsRootDescriptorTable(3, context.depthTextureHandle);

    const float depthFadeParams[4] = {
        drawParams.depthFadeSoftness,
        drawParams.distortionDepthAttenuation,
        drawParams.particleEdgeSoftness,
        drawParams.trailTailFade
    };
    commandList->SetGraphicsRoot32BitConstants(4, 4, depthFadeParams, 0);
    commandList->SetGraphicsRootDescriptorTable(5, aliveListSrv);
    const uint32_t particleDrawParams[4] = {useAliveList, 0u, 0u, 0u};
    commandList->SetGraphicsRoot32BitConstants(6, 4, particleDrawParams, 0);
    WriteParticleDrawDiagnostic(
        "execute-indirect",
        renderBufferResource,
        indirectArgsResource,
        useAliveList);
    commandList->ExecuteIndirect(
        context.gpuParticleSystem->CommandSignature(),
        1,
        indirectArgs,
        0,
        nullptr,
        0);
}
} // namespace vfx
