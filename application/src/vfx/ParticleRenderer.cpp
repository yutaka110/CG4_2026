#include "vfx/ParticleRenderer.h"

#include "../../AppFrameGraphBuilder.h"
#include "../../AppGpuParticleSystem.h"
#include "../../AppPipelines.h"
#include "../../AppFrameState.h"
#include "../../AppRenderResources.h"
#include "../../AppRuntimeState.h"
#include "../../AppSceneResources.h"
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

bool IsAnyResourceName(const char* actual, const char* expected, const char* probeExpected) {
    return IsResourceName(actual, expected) || IsResourceName(actual, probeExpected);
}

bool MatchesResourceName(const char* lhs, const char* rhs) {
    return std::string_view(lhs != nullptr ? lhs : "") == std::string_view(rhs != nullptr ? rhs : "");
}
} // namespace

ParticleRendererOperationalStatus EvaluateParticleRendererOperationalStatus(
    const VfxRenderContext& context,
    const ParticleVfxResourceSet* particleResources) {
    const VfxSimulationResourceSet* simulationResources =
        particleResources != nullptr ? &particleResources->simulation : nullptr;
    const VfxRendererResourceSet* rendererResources =
        particleResources != nullptr ? &particleResources->renderer : nullptr;

    ParticleRendererOperationalStatus status{};
    status.simulationStateResource =
        simulationResources != nullptr && simulationResources->stateBuffer[0] != '\0'
            ? simulationResources->stateBuffer
            : "ParticleState";
    status.simulationRenderBufferResource =
        simulationResources != nullptr && simulationResources->renderBuffer[0] != '\0'
            ? simulationResources->renderBuffer
            : "ParticleRenderBuffer";
    status.simulationIndirectArgsResource =
        simulationResources != nullptr && simulationResources->indirectArgs[0] != '\0'
            ? simulationResources->indirectArgs
            : "ParticleIndirectArgs";
    status.renderBufferResource =
        rendererResources != nullptr && rendererResources->renderBuffer[0] != '\0'
            ? rendererResources->renderBuffer
            : "ParticleRenderBuffer";
    status.indirectArgsResource =
        rendererResources != nullptr && rendererResources->indirectArgs[0] != '\0'
            ? rendererResources->indirectArgs
            : "ParticleIndirectArgs";
    const bool particleDedicatedIntent =
        IsAnyResourceName(status.simulationStateResource, "ParticleState", "ParticleDedicatedState") &&
        IsAnyResourceName(status.simulationRenderBufferResource, "ParticleRenderBuffer", "ParticleDedicatedRenderBuffer") &&
        IsAnyResourceName(status.simulationIndirectArgsResource, "ParticleIndirectArgs", "ParticleDedicatedIndirectArgs") &&
        IsAnyResourceName(status.renderBufferResource, "ParticleRenderBuffer", "ParticleDedicatedRenderBuffer") &&
        IsAnyResourceName(status.indirectArgsResource, "ParticleIndirectArgs", "ParticleDedicatedIndirectArgs");
    const bool particleDedicatedProbeIntent =
        IsResourceName(status.simulationStateResource, "ParticleDedicatedState") &&
        IsResourceName(status.simulationRenderBufferResource, "ParticleDedicatedRenderBuffer") &&
        IsResourceName(status.simulationIndirectArgsResource, "ParticleDedicatedIndirectArgs") &&
        IsResourceName(status.renderBufferResource, "ParticleDedicatedRenderBuffer") &&
        IsResourceName(status.indirectArgsResource, "ParticleDedicatedIndirectArgs");
    const bool simulationRendererIntentMatches =
        MatchesResourceName(status.simulationRenderBufferResource, status.renderBufferResource) &&
        MatchesResourceName(status.simulationIndirectArgsResource, status.indirectArgsResource);
    const bool resourceIntentReady =
        particleResources != nullptr &&
        simulationResources != nullptr &&
        rendererResources != nullptr &&
        simulationResources->usesCompute &&
        rendererResources->usesIndirectSprite &&
        particleDedicatedIntent &&
        simulationRendererIntentMatches;

    status.resourceIntentReady = resourceIntentReady;
    status.resourceIntentMode = particleDedicatedProbeIntent
        ? "particle-dedicated-probe"
        : (particleDedicatedIntent ? "particle-dedicated" : "custom-or-shared");
    status.resourceIntentFallbackReason = "ready";
    if (particleResources == nullptr) {
        status.resourceIntentFallbackReason = "missing particle resource set";
    } else if (simulationResources == nullptr || rendererResources == nullptr) {
        status.resourceIntentFallbackReason = "missing particle resource intent";
    } else if (!simulationResources->usesCompute) {
        status.resourceIntentFallbackReason = "simulation compute disabled";
    } else if (!rendererResources->usesIndirectSprite) {
        status.resourceIntentFallbackReason = "indirect sprite disabled";
    } else if (!particleDedicatedIntent) {
        status.resourceIntentFallbackReason = "not particle dedicated intent";
    } else if (!simulationRendererIntentMatches) {
        status.resourceIntentFallbackReason = "simulation renderer intent mismatch";
    }

    if (rendererResources != nullptr && !rendererResources->usesIndirectSprite) {
        status.fallbackReason = "indirect sprite disabled";
        return status;
    }
    if (context.appPipelines == nullptr) {
        status.fallbackReason = "missing pipelines";
        return status;
    }
    if (context.renderResources == nullptr) {
        status.fallbackReason = "missing render resources";
        return status;
    }
    if (context.scene == nullptr) {
        status.fallbackReason = "missing scene resources";
        return status;
    }
    if (context.gpuParticleSystem == nullptr || !context.gpuParticleSystem->IsInitialized()) {
        status.fallbackReason = "missing gpu particle system";
        return status;
    }
    if (context.frameState == nullptr) {
        status.fallbackReason = "missing frame state";
        return status;
    }
    if (context.srvDescriptorHeap == nullptr) {
        status.fallbackReason = "missing srv descriptor heap";
        return status;
    }
    if (context.vfxTextureHandle.ptr == 0) {
        status.fallbackReason = "missing vfx texture";
        return status;
    }
    if (context.appPipelines->GetGpuParticleComputeRootSignature() == nullptr ||
        context.appPipelines->GetGpuParticleComputePSO() == nullptr) {
        status.fallbackReason = "missing particle simulation pipeline";
        return status;
    }
    if (context.appPipelines->GetParticleRootSignature() == nullptr ||
        context.appPipelines->GetParticleAlphaPSO() == nullptr) {
        status.fallbackReason = "missing particle draw pipeline";
        return status;
    }
    if (context.gpuParticleSystem->CommandSignature() == nullptr) {
        status.fallbackReason = "missing particle command signature";
        return status;
    }

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
    status.resourceHandleFallbackReason = "ready";
    if (!status.simulationStateUavValid) {
        status.resourceHandleFallbackReason = "missing particle state uav";
    } else if (!status.simulationRenderBufferUavValid) {
        status.resourceHandleFallbackReason = "missing particle render buffer uav";
    } else if (!status.renderBufferSrvValid) {
        status.resourceHandleFallbackReason = "missing particle render buffer srv";
    } else if (!status.indirectArgsValid) {
        status.resourceHandleFallbackReason = "missing particle indirect args";
    }

    if (!status.renderBufferSrvValid) {
        status.fallbackReason = "missing particle render buffer srv";
        return status;
    }
    if (!status.indirectArgsValid) {
        status.fallbackReason = "missing particle indirect args";
        return status;
    }
    if (context.renderResources->ParticleVertexBufferView().BufferLocation == 0) {
        status.fallbackReason = "missing particle vertex buffer";
        return status;
    }
    if (context.scene->indexBufferViewSprite.BufferLocation == 0) {
        status.fallbackReason = "missing sprite index buffer";
        return status;
    }

    status.resourceHealthy = true;
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

void ParticleRenderer::RegisterPasses(
    const AppFrameGraphBuildContext& ctx,
    const vfx::VfxTypedResourceSet& resources) const {
    const vfx::VfxTypedResourceSet vfxResources = resources;
    ctx.renderGraph->AddPass({
        vfxResources.particle.routing.simulationPass,
        ge3::graphics::RenderPassLayer::Vfx,
        vfx::SimulationAccesses(vfxResources.particle.simulation),
        "",
        [this, ctx, vfxResources](ge3::graphics::RenderPassContext& passContext) {
            Simulate(
                passContext.commandList,
                vfx::BuildPassRenderContext(ctx, vfxResources),
                ctx.effectRuntime->ParticleInput(ctx.primaryParticleFx));
        }});

    ctx.renderGraph->AddPass({
        vfxResources.particle.routing.drawPass,
        ge3::graphics::RenderPassLayer::Vfx,
        vfx::DrawAccesses(vfxResources.particle.renderer),
        vfxResources.particle.routing.depthTarget,
        [this, ctx, vfxResources](ge3::graphics::RenderPassContext& passContext) {
            const ParticleRenderQueue& queue = ctx.effectRuntime->particleQueue;
            if (!ctx.runtimeState->vfx.enableParticles && queue.empty()) {
                return;
            }
            Draw(
                passContext.commandList,
                vfx::BuildPassRenderContext(ctx, vfxResources),
                ctx.effectRuntime->ParticleInput(ctx.primaryParticleFx));
        }});
}

void ParticleRenderer::Simulate(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const ParticleRenderInput& input) const {
    if (commandList == nullptr ||
        context.srvDescriptorHeap == nullptr ||
        context.appPipelines == nullptr ||
        context.gpuParticleSystem == nullptr ||
        context.frameState == nullptr) {
        return;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { context.srvDescriptorHeap };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    Vector4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 scale = {1.0f, 1.0f, 1.0f};
    float emissive = 1.0f;
    float turbulence = 0.0f;
    float pulseSpeed = 5.0f;
    float spawnRadius = 4.0f;
    float uvScrollSpeed = 0.0f;

    if (input.primary.instance != nullptr &&
        input.primary.componentCommon != nullptr &&
        input.settings != nullptr) {
        const EffectInstance& instance = *input.primary.instance;
        const EffectComponentCommon& component = *input.primary.componentCommon;
        const EffectParticleSettings& settings = *input.settings;
        tint = {
            instance.color.x * component.color.x,
            instance.color.y * component.color.y,
            instance.color.z * component.color.z,
            instance.color.w * component.color.w,
        };
        scale = instance.transform.scale;
        emissive = settings.emissive;
        turbulence = settings.noiseStrength + settings.distortionStrength;
        pulseSpeed = settings.pulseSpeed;
        spawnRadius = settings.spawnRadius;
        uvScrollSpeed = settings.uvScrollSpeed;
    } else if (input.fallbackCommon != nullptr && input.fallbackSettings != nullptr) {
        tint = input.fallbackCommon->color;
        scale = input.fallbackCommon->size;
        emissive = input.fallbackSettings->emissive;
        turbulence = input.fallbackSettings->noiseStrength + input.fallbackSettings->distortionStrength;
        pulseSpeed = input.fallbackSettings->pulseSpeed;
        spawnRadius = input.fallbackSettings->spawnRadius;
        uvScrollSpeed = input.fallbackSettings->uvScrollSpeed;
    }

    const vfx::VfxSimulationResourceSet* simulationResources =
        context.typedResources != nullptr ? &context.typedResources->particle.simulation : nullptr;
    const char* renderBufferResource =
        simulationResources != nullptr && simulationResources->renderBuffer[0] != '\0'
            ? simulationResources->renderBuffer
            : "ParticleRenderBuffer";
    const char* stateBufferResource =
        simulationResources != nullptr && simulationResources->stateBuffer[0] != '\0'
            ? simulationResources->stateBuffer
            : "ParticleState";
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

void ParticleRenderer::Draw(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const ParticleRenderInput& input) const {
    const vfx::ComponentDrawParams drawParams =
        vfx::ResolveParticleDrawParams(
            input.settings,
            input.fallbackSettings,
            {0.02f, 1.0f, 0.5f, 1.35f});
    const vfx::VfxRendererResourceSet* rendererResources =
        context.typedResources != nullptr ? &context.typedResources->particle.renderer : nullptr;
    vfx::DrawIndirectSpriteComponents(
        commandList,
        context,
        context.appPipelines != nullptr ? context.appPipelines->GetParticleAlphaPSO() : nullptr,
        drawParams,
        rendererResources);
}
