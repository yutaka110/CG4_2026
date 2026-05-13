#include "vfx/VfxResourceResolver.h"

#include "../../AppVfxRuntimeState.h"

namespace {
bool EffectiveTrailMeshStreamEnabled(const AppVfxRuntimeState& runtimeState) {
    return runtimeState.enableTrailMeshStream &&
        !runtimeState.trailMeshStreamFallbackActive;
}

bool EffectiveDistortionDedicatedEnabled(const AppVfxRuntimeState& runtimeState) {
    return runtimeState.enableDistortionDedicatedResources &&
        !runtimeState.distortionDedicatedResourceFallbackActive;
}

bool EffectiveBeamDedicatedEnabled(const AppVfxRuntimeState& runtimeState) {
    return !runtimeState.beamDedicatedResourceFallbackActive;
}
} // namespace

namespace vfx {
VfxTypedResourceSet VfxResourceResolver::SelectPassResources(
    const AppVfxRuntimeState& runtimeState) const {
    VfxTypedResourceSet resources = MakeLegacySharedIndirectVfxResources();

    resources.trail.renderer.meshStream.usesDedicatedMeshStream =
        EffectiveTrailMeshStreamEnabled(runtimeState);

    if (runtimeState.enableParticleDedicatedResourceProbe &&
        !runtimeState.particleDedicatedResourceFallbackActive) {
        ApplyParticleDedicatedStorageProbe(resources);
    }

    if (EffectiveDistortionDedicatedEnabled(runtimeState)) {
        ApplyDistortionDedicatedResources(resources);
    }

    if (EffectiveBeamDedicatedEnabled(runtimeState)) {
        ApplyBeamDedicatedResources(resources);
    }

    return resources;
}

VfxTypedResourceSet VfxResourceResolver::SelectBeamDedicatedResources() const {
    VfxTypedResourceSet resources = MakeLegacySharedIndirectVfxResources();
    ApplyBeamDedicatedResources(resources);
    return resources;
}
} // namespace vfx
