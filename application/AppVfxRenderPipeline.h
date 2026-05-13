#pragma once

struct AppFrameGraphBuildContext;
namespace vfx {
struct VfxTypedResourceSet;
}

class AppVfxRenderPipeline {
public:
    void RegisterPasses(
        const AppFrameGraphBuildContext& context,
        const vfx::VfxTypedResourceSet& resources) const;
};
