#pragma once

#include "../../AppFrameGraphBuilder.h"
#include "vfx/AppVfxRendererSet.h"
#include "vfx/VfxRenderContext.h"
#include "vfx/VfxResources.h"

namespace vfx {
inline VfxRenderContext BuildPassRenderContext(
    const AppFrameGraphBuildContext& ctx,
    const VfxTypedResourceSet& resources) {
    return {
        ctx.appPipelines,
        ctx.renderResources,
        ctx.scene,
        ctx.gpuParticleSystem,
        ctx.vfxRenderers->beam,
        ctx.frameState,
        ctx.srvDescriptorHeap,
        ctx.vfxTextureHandle,
        ctx.depthTextureHandle,
        ctx.beamTime,
        &resources
    };
}
} // namespace vfx
