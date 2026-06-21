#pragma once

#include <d3d12.h>

#include "../../EffectSystem.h"
#include "vfx/VfxRenderContext.h"
#include "vfx/VfxResources.h"

struct AppFrameGraphBuildContext;

class OrbitRibbonRenderer {
public:
    void RegisterPasses(
        const AppFrameGraphBuildContext& ctx,
        const vfx::VfxTypedResourceSet& resources) const;

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const CylinderRenderQueue& queue) const;
};
