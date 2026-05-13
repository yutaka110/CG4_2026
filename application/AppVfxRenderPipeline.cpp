#include "AppVfxRenderPipeline.h"

#include "AppFrameGraphBuilder.h"
#include "AppGpuParticleSystem.h"
#include "AppPipelines.h"
#include "AppVfxRuntimeState.h"
#include "AppSceneResources.h"
#include "AppVfxRenderTargets.h"
#include "graphics/RenderGraph.h"
#include "vfx/AppVfxRendererSet.h"
#include "vfx/BeamRenderer.h"
#include "vfx/DistortionRenderer.h"
#include "vfx/ParticleRenderer.h"
#include "vfx/TrailRenderer.h"
#include "vfx/VfxResourceResolver.h"
#include "vfx/VfxResources.h"

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
    ctx.gpuParticleSystem->DeclareGraphBuffers(*ctx.renderGraph);

    ctx.vfxRenderers->particle->RegisterPasses(ctx, vfxResources);
    ctx.vfxRenderers->trail->RegisterPasses(ctx, vfxResources);

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
    if (vfxResources.beam.simulation.usesCompute) {
        ctx.vfxRenderers->beam->RegisterDedicatedPasses(ctx, vfxResources);
    } else {
        ctx.vfxRenderers->beam->RegisterPasses(ctx, vfxResources);
    }
    ctx.vfxRenderers->distortion->RegisterPasses(ctx, vfxResources);
}
