#pragma once

#include <vector>

#include "graphics/RenderGraph.h"

namespace vfx {
struct VfxSimulationResourceSet {
    const char* stateBuffer = "";
    const char* renderBuffer = "";
    const char* indirectArgs = "";
    bool usesCompute = false;
};

struct VfxRendererResourceSet {
    const char* sceneColor = "SceneColor";
    const char* sceneDepth = "SceneDepth";
    const char* renderBuffer = "";
    const char* indirectArgs = "";
    const char* accumulationTarget = "VfxAccumulation";
    bool usesIndirectSprite = false;
};

struct TrailMeshStreamResourceSet {
    const char* controlPointBuffer = "";
    const char* segmentBuffer = "";
    const char* vertexBuffer = "";
    const char* indexBuffer = "";
    const char* drawArgs = "";
    bool usesDedicatedMeshStream = false;
};

struct TrailRendererResources {
    VfxRendererResourceSet indirectSprite{};
    TrailMeshStreamResourceSet meshStream{};
};

struct VfxPassRoutingResourceSet {
    const char* simulationPass = "";
    const char* drawPass = "";
    const char* depthTarget = "SceneDepthReadOnly";
};

struct ParticleVfxResourceSet {
    VfxSimulationResourceSet simulation{};
    VfxRendererResourceSet renderer{};
    VfxPassRoutingResourceSet routing{};
};

struct TrailVfxResourceSet {
    VfxSimulationResourceSet simulation{};
    TrailRendererResources renderer{};
    VfxPassRoutingResourceSet routing{};
};

struct BeamVfxResourceSet {
    VfxSimulationResourceSet simulation{};
    VfxRendererResourceSet renderer{};
    VfxPassRoutingResourceSet routing{};
};

struct DistortionVfxResourceSet {
    VfxSimulationResourceSet simulation{};
    VfxRendererResourceSet renderer{};
    VfxPassRoutingResourceSet routing{};
};

struct VfxTypedResourceSet {
    ParticleVfxResourceSet particle{};
    TrailVfxResourceSet trail{};
    BeamVfxResourceSet beam{};
    DistortionVfxResourceSet distortion{};
};

inline VfxTypedResourceSet MakeLegacySharedIndirectVfxResources() {
    VfxTypedResourceSet resources{};

    resources.particle.simulation = {
        "ParticleState",
        "ParticleRenderBuffer",
        "ParticleIndirectArgs",
        true
    };
    resources.particle.renderer = {
        "SceneColor",
        "SceneDepth",
        "ParticleRenderBuffer",
        "ParticleIndirectArgs",
        "VfxAccumulation",
        true
    };
    resources.particle.routing = {
        "VFX.ParticleSimulation",
        "VFX.Particles",
        "SceneDepthReadOnly"
    };

    resources.trail.renderer.indirectSprite = {
        "SceneColor",
        "SceneDepth",
        "TrailRenderBuffer",
        "TrailIndirectArgs",
        "VfxAccumulation",
        true
    };
    resources.trail.renderer.meshStream = {
        "TrailControlPointBuffer",
        "TrailSegmentBuffer",
        "TrailVertexBuffer",
        "TrailIndexBuffer",
        "TrailDrawArgs",
        false
    };
    resources.trail.routing = {
        "VFX.TrailSimulation",
        "VFX.Trails",
        "SceneDepthReadOnly"
    };

    resources.beam.renderer = {
        "SceneColor",
        "SceneDepth",
        "",
        "",
        "VfxAccumulation",
        false
    };
    resources.beam.routing = {
        "",
        "VFX.Beam",
        "SceneDepthReadOnly"
    };

    resources.distortion.renderer = {
        "SceneColor",
        "SceneDepth",
        "DistortionRenderBuffer",
        "DistortionIndirectArgs",
        "VfxAccumulation",
        true
    };
    resources.distortion.routing = {
        "",
        "VFX.Distortion",
        "SceneDepthReadOnly"
    };

    return resources;
}

inline VfxTypedResourceSet MakeParticleDedicatedStorageProbeVfxResources() {
    VfxTypedResourceSet resources = MakeLegacySharedIndirectVfxResources();
    resources.particle.simulation = {
        "ParticleDedicatedState",
        "ParticleDedicatedRenderBuffer",
        "ParticleDedicatedIndirectArgs",
        true
    };
    resources.particle.renderer = {
        "SceneColor",
        "SceneDepth",
        "ParticleDedicatedRenderBuffer",
        "ParticleDedicatedIndirectArgs",
        "VfxAccumulation",
        true
    };
    return resources;
}

inline VfxTypedResourceSet MakeDistortionDedicatedVfxResources() {
    VfxTypedResourceSet resources = MakeLegacySharedIndirectVfxResources();
    resources.distortion.simulation = {
        "DistortionDedicatedState",
        "DistortionDedicatedRenderBuffer",
        "DistortionDedicatedIndirectArgs",
        true
    };
    resources.distortion.renderer = {
        "SceneColor",
        "SceneDepth",
        "DistortionDedicatedRenderBuffer",
        "DistortionDedicatedIndirectArgs",
        "VfxAccumulation",
        true
    };
    resources.distortion.routing = {
        "VFX.DistortionDedicatedSimulation",
        "VFX.DistortionDedicated",
        "SceneDepthReadOnly"
    };
    return resources;
}

inline VfxTypedResourceSet MakeBeamDedicatedVfxResources() {
    VfxTypedResourceSet resources = MakeLegacySharedIndirectVfxResources();
    resources.beam.simulation = {
        "BeamDedicatedState",
        "BeamDedicatedRenderBuffer",
        "BeamDedicatedIndirectArgs",
        true
    };
    resources.beam.renderer = {
        "SceneColor",
        "SceneDepth",
        "BeamDedicatedRenderBuffer",
        "BeamDedicatedIndirectArgs",
        "VfxAccumulation",
        true
    };
    resources.beam.routing = {
        "VFX.BeamDedicatedSimulation",
        "VFX.BeamDedicated",
        "SceneDepthReadOnly"
    };
    return resources;
}

inline void ApplyParticleDedicatedStorageProbe(VfxTypedResourceSet& resources) {
    const VfxTypedResourceSet dedicatedResources = MakeParticleDedicatedStorageProbeVfxResources();
    resources.particle = dedicatedResources.particle;
}

inline void ApplyDistortionDedicatedResources(VfxTypedResourceSet& resources) {
    const VfxTypedResourceSet dedicatedResources = MakeDistortionDedicatedVfxResources();
    resources.distortion = dedicatedResources.distortion;
}

inline void ApplyBeamDedicatedResources(VfxTypedResourceSet& resources) {
    const VfxTypedResourceSet dedicatedResources = MakeBeamDedicatedVfxResources();
    resources.beam = dedicatedResources.beam;
}

inline std::vector<ge3::graphics::RenderPassResourceAccess> SimulationAccesses(
    const VfxSimulationResourceSet& resources) {
    using ge3::graphics::RenderPassResourceAccess;
    using ge3::graphics::RenderResourceAccessType;

    std::vector<RenderPassResourceAccess> accesses;
    if (!resources.usesCompute) {
        return accesses;
    }

    accesses.push_back({resources.stateBuffer, RenderResourceAccessType::ReadUav});
    accesses.push_back({resources.stateBuffer, RenderResourceAccessType::WriteUav});
    accesses.push_back({resources.renderBuffer, RenderResourceAccessType::WriteUav});
    return accesses;
}

inline std::vector<ge3::graphics::RenderPassResourceAccess> DrawAccesses(
    const VfxRendererResourceSet& resources) {
    using ge3::graphics::RenderPassResourceAccess;
    using ge3::graphics::RenderResourceAccessType;

    std::vector<RenderPassResourceAccess> accesses = {
        {resources.sceneColor, RenderResourceAccessType::ReadSrv},
        {resources.sceneDepth, RenderResourceAccessType::ReadDepth},
    };

    if (resources.usesIndirectSprite) {
        accesses.push_back({resources.renderBuffer, RenderResourceAccessType::ReadSrv});
        accesses.push_back({resources.indirectArgs, RenderResourceAccessType::ReadIndirect});
    }

    accesses.push_back({resources.accumulationTarget, RenderResourceAccessType::WriteRtv});
    return accesses;
}

inline std::vector<ge3::graphics::RenderPassResourceAccess> DrawAccesses(
    const TrailRendererResources& resources) {
    using ge3::graphics::RenderPassResourceAccess;
    using ge3::graphics::RenderResourceAccessType;

    std::vector<RenderPassResourceAccess> accesses = DrawAccesses(resources.indirectSprite);
    const TrailMeshStreamResourceSet& meshStream = resources.meshStream;
    if (meshStream.controlPointBuffer[0] != '\0') {
        accesses.push_back({meshStream.controlPointBuffer, RenderResourceAccessType::ReadSrv});
    }
    if (meshStream.segmentBuffer[0] != '\0') {
        accesses.push_back({meshStream.segmentBuffer, RenderResourceAccessType::ReadSrv});
    }
    if (meshStream.vertexBuffer[0] != '\0') {
        accesses.push_back({meshStream.vertexBuffer, RenderResourceAccessType::ReadVertexBuffer});
    }
    if (meshStream.indexBuffer[0] != '\0') {
        accesses.push_back({meshStream.indexBuffer, RenderResourceAccessType::ReadIndexBuffer});
    }
    if (meshStream.drawArgs[0] != '\0') {
        accesses.push_back({meshStream.drawArgs, RenderResourceAccessType::ReadIndirect});
    }
    return accesses;
}

inline std::vector<ge3::graphics::RenderPassResourceAccess> TrailMeshStreamSimulationAccesses(
    const TrailMeshStreamResourceSet& resources) {
    using ge3::graphics::RenderPassResourceAccess;
    using ge3::graphics::RenderResourceAccessType;

    std::vector<RenderPassResourceAccess> accesses;
    if (resources.controlPointBuffer[0] != '\0') {
        accesses.push_back({resources.controlPointBuffer, RenderResourceAccessType::WriteUav});
    }
    if (resources.segmentBuffer[0] != '\0') {
        accesses.push_back({resources.segmentBuffer, RenderResourceAccessType::WriteUav});
    }
    return accesses;
}

inline std::vector<ge3::graphics::RenderPassResourceAccess> TrailMeshBuildAccesses(
    const TrailMeshStreamResourceSet& resources) {
    using ge3::graphics::RenderPassResourceAccess;
    using ge3::graphics::RenderResourceAccessType;

    std::vector<RenderPassResourceAccess> accesses;
    if (resources.controlPointBuffer[0] != '\0') {
        accesses.push_back({resources.controlPointBuffer, RenderResourceAccessType::ReadSrv});
    }
    if (resources.segmentBuffer[0] != '\0') {
        accesses.push_back({resources.segmentBuffer, RenderResourceAccessType::ReadSrv});
    }
    if (resources.vertexBuffer[0] != '\0') {
        accesses.push_back({resources.vertexBuffer, RenderResourceAccessType::WriteUav});
    }
    if (resources.indexBuffer[0] != '\0') {
        accesses.push_back({resources.indexBuffer, RenderResourceAccessType::WriteUav});
    }
    if (resources.drawArgs[0] != '\0') {
        accesses.push_back({resources.drawArgs, RenderResourceAccessType::WriteUav});
    }
    return accesses;
}
} // namespace vfx
