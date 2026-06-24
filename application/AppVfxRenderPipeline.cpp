#include "AppVfxRenderPipeline.h"

#include "AppFrameGraphBuilder.h"
#include "AppGpuParticleSystem.h"
#include "AppPipelines.h"
#include "AppRuntimeState.h"
#include "AppVfxRuntimeState.h"
#include "AppSceneResources.h"
#include "AppVfxRenderTargets.h"
#include "terrain/TerrainChunkManager.h"
#include "graphics/RenderGraph.h"
#include "vfx/AppVfxRendererSet.h"
#include "vfx/BeamRenderer.h"
#include "vfx/CylinderRenderer.h"
#include "vfx/DistortionRenderer.h"
#include "vfx/ElectricOrbStrikeRenderer.h"
#include "vfx/OrbitRibbonRenderer.h"
#include "vfx/ParticleRenderer.h"
#include "vfx/RingRenderer.h"
#include "vfx/SpearRenderer.h"
#include "vfx/TrailRenderer.h"
#include "vfx/VfxResourceResolver.h"
#include "vfx/VfxResources.h"

namespace {
bool HasTerrainDustEmitters(const AppFrameGraphBuildContext& ctx) {
    if (ctx.runtimeState == nullptr ||
        ctx.terrainChunkManager == nullptr ||
        !ctx.runtimeState->terrain.enabled ||
        ctx.runtimeState->terrain.settings.dustZoneDensity <= 0.0f) {
        return false;
    }

    for (const TerrainChunkDebugInfo& chunk : ctx.terrainChunkManager->Chunks()) {
        if (!chunk.vfxZones.empty()) {
            return true;
        }
    }
    return false;
}

bool HasParticleWork(const AppFrameGraphBuildContext& ctx) {
    return ctx.runtimeState != nullptr &&
        ctx.effectRuntime != nullptr &&
        ctx.runtimeState->vfx.enableParticles &&
        (!ctx.effectRuntime->particleQueue.empty() || HasTerrainDustEmitters(ctx));
}

bool HasAnyVfxWork(
    const AppFrameGraphBuildContext& ctx,
    const AppVfxRuntimeState& runtimeState) {
    if (HasParticleWork(ctx)) {
        return true;
    }
    if (ctx.effectRuntime == nullptr) {
        return false;
    }
    return (runtimeState.enableTrails && !ctx.effectRuntime->trailQueue.empty()) ||
        (runtimeState.enableBeams && !ctx.effectRuntime->beamQueue.empty()) ||
        (runtimeState.enableRings && !ctx.effectRuntime->ringQueue.empty()) ||
        (runtimeState.enableCylinders && !ctx.effectRuntime->cylinderQueue.empty()) ||
        (runtimeState.enableElectricOrbStrike && runtimeState.electricOrbStrikeActive) ||
        (runtimeState.enableDistortions && !ctx.effectRuntime->distortionQueue.empty());
}
} // namespace

void AppVfxRenderPipeline::RegisterPasses(
    const AppFrameGraphBuildContext& ctx,
    const vfx::VfxTypedResourceSet& vfxResources) const {
    const float transparentBlack[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ctx.renderGraph->DeclarePersistentRenderTarget(
        "VfxAccumulation",
        1.0f,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        transparentBlack,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    const AppVfxRuntimeState& runtimeState = ctx.runtimeState->vfx;
    const bool hasAnyVfxWork = HasAnyVfxWork(ctx, runtimeState);
    const bool hasParticleWork = HasParticleWork(ctx);
    if (!hasAnyVfxWork) {
        return;
    }

    ctx.gpuParticleSystem->DeclareGraphBuffers(*ctx.renderGraph);
    ctx.renderGraph->AddPass({
            "VFX.BeginAccumulation",
            ge3::graphics::RenderPassLayer::Vfx,
            {
                {"SceneDepth", ge3::graphics::RenderResourceAccessType::ReadDepth},
                {"VfxAccumulation", ge3::graphics::RenderResourceAccessType::WriteRtv},
            },
            "SceneDepth",
            [ctx](ge3::graphics::RenderPassContext& passContext) {
                ctx.vfxRenderTargets->BeginVfx(passContext.commandList, ctx.dsv);
            }});
    if (hasParticleWork) {
        ctx.vfxRenderers->particle->RegisterPasses(ctx, vfxResources);
    }
    if (runtimeState.enableTrails) {
        ctx.vfxRenderers->trail->RegisterPasses(ctx, vfxResources);
    }
    if (runtimeState.enableBeams) {
        if (vfxResources.beam.simulation.usesCompute) {
            ctx.vfxRenderers->beam->RegisterDedicatedPasses(ctx, vfxResources);
        } else {
            ctx.vfxRenderers->beam->RegisterPasses(ctx, vfxResources);
        }
    }
    if (runtimeState.enableRings) {
        ctx.vfxRenderers->ring->RegisterPasses(ctx, vfxResources);
    }
    if (runtimeState.enableCylinders) {
        ctx.vfxRenderers->spear->RegisterPasses(ctx, vfxResources);
        ctx.vfxRenderers->orbitRibbon->RegisterPasses(ctx, vfxResources);
        ctx.vfxRenderers->cylinder->RegisterPasses(ctx, vfxResources);
    }
    if (runtimeState.enableElectricOrbStrike) {
        ctx.vfxRenderers->electricOrbStrike->RegisterPasses(ctx, vfxResources);
    }
    if (runtimeState.enableDistortions) {
        ctx.vfxRenderers->distortion->RegisterPasses(ctx, vfxResources);
    }
}
