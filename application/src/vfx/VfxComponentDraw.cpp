#include "VfxComponentDraw.h"

#include "../../AppGpuParticleSystem.h"
#include "../../AppPipelines.h"
#include "../../AppRenderResources.h"
#include "../../AppSceneResources.h"
#include "vfx/VfxResources.h"

#include <string_view>

namespace vfx {
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

    if (commandList == nullptr ||
        context.appPipelines == nullptr ||
        context.renderResources == nullptr ||
        context.scene == nullptr ||
        context.gpuParticleSystem == nullptr ||
        pipelineState == nullptr ||
        context.vfxTextureHandle.ptr == 0 ||
        context.gpuParticleSystem->CommandSignature() == nullptr) {
        return;
    }
    if (rendererResources != nullptr && !rendererResources->usesIndirectSprite) {
        return;
    }

    const D3D12_GPU_DESCRIPTOR_HANDLE renderBufferSrv =
        context.gpuParticleSystem->SrvHandleForResource(renderBufferResource);
    const D3D12_GPU_DESCRIPTOR_HANDLE aliveListSrv =
        context.gpuParticleSystem->SrvHandleForResource("ParticleAliveList");
    ID3D12Resource* indirectArgs =
        context.gpuParticleSystem->IndirectArgsForResource(indirectArgsResource);
    if (renderBufferSrv.ptr == 0 || aliveListSrv.ptr == 0 || indirectArgs == nullptr) {
        return;
    }
    const uint32_t useAliveList =
        std::string_view(renderBufferResource) == "ParticleRenderBuffer" &&
        std::string_view(indirectArgsResource) == "ParticleIndirectArgs"
            ? 1u
            : 0u;

    commandList->SetGraphicsRootSignature(context.appPipelines->GetParticleRootSignature());
    commandList->SetPipelineState(pipelineState);
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
    if (context.depthTextureHandle.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(3, context.depthTextureHandle);
    }

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
    commandList->ExecuteIndirect(
        context.gpuParticleSystem->CommandSignature(),
        1,
        indirectArgs,
        0,
        nullptr,
        0);
}
} // namespace vfx
