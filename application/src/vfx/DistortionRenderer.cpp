#include "vfx/DistortionRenderer.h"

#include "../../AppFrameGraphBuilder.h"
#include "../../AppGpuParticleSystem.h"
#include "../../AppPipelines.h"
#include "VfxComponentDraw.h"
#include "graphics/RenderGraph.h"
#include "vfx/VfxPassRegistration.h"
#include "vfx/VfxResources.h"

#include <string_view>

namespace vfx {
namespace {
bool IsResourceName(const char* actual, const char* expected) {
    return std::string_view(actual != nullptr ? actual : "") == expected;
}
} // namespace

DistortionRendererOperationalStatus EvaluateDistortionRendererOperationalStatus(
    const VfxRenderContext& context,
    const DistortionVfxResourceSet* distortionResources) {
    const VfxRendererResourceSet* rendererResources =
        distortionResources != nullptr ? &distortionResources->renderer : nullptr;
    const VfxSimulationResourceSet* simulationResources =
        distortionResources != nullptr ? &distortionResources->simulation : nullptr;

    DistortionRendererOperationalStatus status{};
    status.simulationStateResource =
        simulationResources != nullptr && simulationResources->stateBuffer[0] != '\0'
            ? simulationResources->stateBuffer
            : "";
    status.simulationRenderBufferResource =
        simulationResources != nullptr && simulationResources->renderBuffer[0] != '\0'
            ? simulationResources->renderBuffer
            : "";
    status.simulationIndirectArgsResource =
        simulationResources != nullptr && simulationResources->indirectArgs[0] != '\0'
            ? simulationResources->indirectArgs
            : "";
    status.renderBufferResource =
        rendererResources != nullptr && rendererResources->renderBuffer[0] != '\0'
            ? rendererResources->renderBuffer
            : "DistortionRenderBuffer";
    status.indirectArgsResource =
        rendererResources != nullptr && rendererResources->indirectArgs[0] != '\0'
            ? rendererResources->indirectArgs
            : "DistortionIndirectArgs";

    status.resourceIntentReady =
        simulationResources != nullptr &&
        rendererResources != nullptr &&
        simulationResources->usesCompute &&
        rendererResources->usesIndirectSprite &&
        IsResourceName(status.simulationStateResource, "DistortionDedicatedState") &&
        IsResourceName(status.simulationRenderBufferResource, "DistortionDedicatedRenderBuffer") &&
        IsResourceName(status.simulationIndirectArgsResource, "DistortionDedicatedIndirectArgs") &&
        IsResourceName(status.renderBufferResource, "DistortionDedicatedRenderBuffer") &&
        IsResourceName(status.indirectArgsResource, "DistortionDedicatedIndirectArgs");
    status.resourceIntentFallbackReason = "ready";
    if (distortionResources == nullptr || simulationResources == nullptr || rendererResources == nullptr) {
        status.resourceIntentFallbackReason = "missing distortion resource set";
    } else if (!simulationResources->usesCompute) {
        status.resourceIntentFallbackReason = "simulation compute disabled";
    } else if (!rendererResources->usesIndirectSprite) {
        status.resourceIntentFallbackReason = "indirect sprite disabled";
    } else if (!IsResourceName(status.simulationStateResource, "DistortionDedicatedState")) {
        status.resourceIntentFallbackReason = "not distortion dedicated state";
    } else if (!IsResourceName(status.simulationRenderBufferResource, "DistortionDedicatedRenderBuffer")) {
        status.resourceIntentFallbackReason = "not distortion dedicated simulation render buffer";
    } else if (!IsResourceName(status.simulationIndirectArgsResource, "DistortionDedicatedIndirectArgs")) {
        status.resourceIntentFallbackReason = "not distortion dedicated simulation indirect args";
    } else if (!IsResourceName(status.renderBufferResource, "DistortionDedicatedRenderBuffer")) {
        status.resourceIntentFallbackReason = "not distortion dedicated render buffer";
    } else if (!IsResourceName(status.indirectArgsResource, "DistortionDedicatedIndirectArgs")) {
        status.resourceIntentFallbackReason = "not distortion dedicated indirect args";
    }

    status.resourceHandleFallbackReason = "ready";
    if (context.gpuParticleSystem == nullptr || !context.gpuParticleSystem->IsInitialized()) {
        status.resourceHandleFallbackReason = "missing gpu particle system";
    } else {
        status.simulationStateUavValid =
            context.gpuParticleSystem->UavHandleForResource(status.simulationStateResource).ptr != 0;
        status.simulationRenderBufferUavValid =
            context.gpuParticleSystem->UavHandleForResource(status.simulationRenderBufferResource).ptr != 0;
        status.renderBufferSrvValid =
            context.gpuParticleSystem->SrvHandleForResource(status.renderBufferResource).ptr != 0;
        status.indirectArgsValid =
            context.gpuParticleSystem->IndirectArgsForResource(status.indirectArgsResource) != nullptr;
        status.resourceHandlesReady =
            status.simulationStateUavValid &&
            status.simulationRenderBufferUavValid &&
            status.renderBufferSrvValid &&
            status.indirectArgsValid;
        if (!status.simulationStateUavValid) {
            status.resourceHandleFallbackReason = "missing distortion state uav";
        } else if (!status.simulationRenderBufferUavValid) {
            status.resourceHandleFallbackReason = "missing distortion render buffer uav";
        } else if (!status.renderBufferSrvValid) {
            status.resourceHandleFallbackReason = "missing distortion render buffer srv";
        } else if (!status.indirectArgsValid) {
            status.resourceHandleFallbackReason = "missing distortion indirect args";
        }
    }

    status.operationalOk = status.resourceIntentReady && status.resourceHandlesReady;
    if (status.operationalOk) {
        status.fallbackReason = "ready";
    } else if (!status.resourceIntentReady) {
        status.fallbackReason = status.resourceIntentFallbackReason;
    } else {
        status.fallbackReason = status.resourceHandleFallbackReason;
    }
    return status;
}
} // namespace vfx

void DistortionRenderer::RegisterPasses(
    const AppFrameGraphBuildContext& ctx,
    const vfx::VfxTypedResourceSet& resources) const {
    const vfx::VfxTypedResourceSet vfxResources = resources;
    if (vfxResources.distortion.routing.simulationPass[0] != '\0') {
        ctx.renderGraph->AddPass({
            vfxResources.distortion.routing.simulationPass,
            ge3::graphics::RenderPassLayer::Vfx,
            vfx::SimulationAccesses(vfxResources.distortion.simulation),
            "",
            [this, ctx, vfxResources](ge3::graphics::RenderPassContext& passContext) {
                const DistortionRenderQueue& queue = ctx.effectRuntime->distortionQueue;
                if (queue.empty()) {
                    return;
                }
                Simulate(
                    passContext.commandList,
                    vfx::BuildPassRenderContext(ctx, vfxResources),
                    ctx.effectRuntime->DistortionInput());
            }});
    }

    ctx.renderGraph->AddPass({
        vfxResources.distortion.routing.drawPass,
        ge3::graphics::RenderPassLayer::Vfx,
        vfx::DrawAccesses(vfxResources.distortion.renderer),
        vfxResources.distortion.routing.depthTarget,
        [this, ctx, vfxResources](ge3::graphics::RenderPassContext& passContext) {
            const DistortionRenderQueue& queue = ctx.effectRuntime->distortionQueue;
            if (queue.empty()) {
                return;
            }
            Draw(
                passContext.commandList,
                vfx::BuildPassRenderContext(ctx, vfxResources),
                ctx.effectRuntime->DistortionInput());
        }});
}

void DistortionRenderer::Simulate(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const DistortionRenderInput& input) const {
    if (commandList == nullptr ||
        context.srvDescriptorHeap == nullptr ||
        context.appPipelines == nullptr ||
        context.gpuParticleSystem == nullptr ||
        context.frameState == nullptr) {
        return;
    }

    const vfx::VfxSimulationResourceSet* simulationResources =
        context.typedResources != nullptr ? &context.typedResources->distortion.simulation : nullptr;
    if (simulationResources == nullptr || !simulationResources->usesCompute) {
        return;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { context.srvDescriptorHeap };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    Vector4 tint = {0.55f, 0.85f, 1.0f, 1.0f};
    Vector3 scale = {1.0f, 1.0f, 1.0f};
    float emissive = 1.0f;
    float turbulence = 0.0f;
    float pulseSpeed = 4.0f;
    float spawnRadius = 3.0f;
    float uvScrollSpeed = 0.0f;

    if (input.primary.instance != nullptr &&
        input.primary.componentCommon != nullptr &&
        input.settings != nullptr) {
        const EffectInstance& instance = *input.primary.instance;
        const EffectComponentCommon& component = *input.primary.componentCommon;
        const EffectDistortionSettings& settings = *input.settings;
        tint = {
            instance.color.x * component.color.x,
            instance.color.y * component.color.y,
            instance.color.z * component.color.z,
            instance.color.w * component.color.w,
        };
        scale = instance.transform.scale;
        emissive = 1.0f + settings.strength;
        turbulence = settings.noiseStrength + settings.strength;
        pulseSpeed = 4.0f + settings.strength * 2.0f;
        spawnRadius = 3.0f + settings.strength;
        uvScrollSpeed = settings.uvScrollSpeed;
    }

    const char* renderBufferResource =
        simulationResources->renderBuffer[0] != '\0'
            ? simulationResources->renderBuffer
            : "DistortionDedicatedRenderBuffer";
    const char* stateBufferResource =
        simulationResources->stateBuffer[0] != '\0'
            ? simulationResources->stateBuffer
            : "DistortionDedicatedState";
    context.gpuParticleSystem->Simulate(
        commandList,
        context.appPipelines->GetGpuParticleComputeRootSignature(),
        context.appPipelines->GetGpuParticleComputePSO(),
        context.frameState->viewProjectionMatrix,
        0.016f,
        context.beamTime,
        tint,
        scale,
        emissive,
        turbulence,
        pulseSpeed,
        spawnRadius,
        uvScrollSpeed,
        renderBufferResource,
        stateBufferResource);
}

void DistortionRenderer::Draw(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const DistortionRenderInput& input) const {
    const vfx::ComponentDrawParams drawParams =
        vfx::ResolveDistortionDrawParams(
            input.settings,
            {0.03f, 1.0f, 0.5f, 1.35f});
    const vfx::VfxRendererResourceSet* rendererResources =
        context.typedResources != nullptr ? &context.typedResources->distortion.renderer : nullptr;
    vfx::DrawIndirectSpriteComponents(
        commandList,
        context,
        context.appPipelines != nullptr ? context.appPipelines->GetDistortionSpritePSO() : nullptr,
        drawParams,
        rendererResources);
}
