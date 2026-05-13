#pragma once

#include "EffectAssetLoader.h"

namespace EffectAuthoringUi {

inline const char* ComponentTypeLabel(EffectComponentType type) {
    switch (type) {
    case EffectComponentType::Particle:
        return "Particle";
    case EffectComponentType::Trail:
        return "Trail";
    case EffectComponentType::Beam:
        return "Beam";
    case EffectComponentType::Distortion:
        return "Distortion";
    }
    return "Unknown";
}

inline const char* EffectTypeLabel(EffectComponentType type) {
    return ComponentTypeLabel(type);
}

inline const char* TechniqueLabel(EffectTechnique technique) {
    switch (technique) {
    case EffectTechnique::ParticleAdditive:
        return "ParticleAdditive";
    case EffectTechnique::TrailRibbon:
        return "TrailRibbon";
    case EffectTechnique::BeamLightning:
        return "BeamLightning";
    case EffectTechnique::DistortionSprite:
        return "DistortionSprite";
    }
    return "Unknown";
}

inline const char* EffectTechniqueLabel(EffectTechnique technique) {
    return TechniqueLabel(technique);
}

inline const char* RendererTypeLabel(EffectRendererType type) {
    switch (type) {
    case EffectRendererType::ParticleRenderer:
        return "ParticleRenderer";
    case EffectRendererType::TrailRenderer:
        return "TrailRenderer";
    case EffectRendererType::BeamRenderer:
        return "BeamRenderer";
    case EffectRendererType::DistortionRenderer:
        return "DistortionRenderer";
    }
    return "Unknown";
}

inline const char* EffectRendererTypeLabel(EffectRendererType type) {
    return RendererTypeLabel(type);
}

inline const char* SimulationTypeLabel(EffectSimulationType type) {
    switch (type) {
    case EffectSimulationType::CpuSpawnGpuSim:
        return "CpuSpawnGpuSim";
    case EffectSimulationType::CpuTimeline:
        return "CpuTimeline";
    case EffectSimulationType::GpuSimulation:
        return "GpuSimulation";
    case EffectSimulationType::None:
        return "None";
    }
    return "Unknown";
}

inline const char* EffectSimulationTypeLabel(EffectSimulationType type) {
    return SimulationTypeLabel(type);
}

inline const char* MetadataSourceLabel(EffectTechniqueMetadataSource source) {
    switch (source) {
    case EffectTechniqueMetadataSource::Component:
        return "componentOverride";
    case EffectTechniqueMetadataSource::Asset:
        return "assetOverride";
    case EffectTechniqueMetadataSource::Registry:
        return "registry";
    }
    return "registry";
}

inline const char* TechniqueMetadataSourceLabel(EffectTechniqueMetadataSource source) {
    return MetadataSourceLabel(source);
}

inline const char* AssetMetadataSourceLabel(bool overridden) {
    return overridden ? "assetOverride" : "registry/default";
}

inline const char* RegistryStateLabel(bool hasId, bool resolved) {
    if (!hasId) {
        return "empty";
    }
    return resolved ? "resolved" : "unregistered";
}

inline const char* RegistryState(bool hasId, bool resolved) {
    return RegistryStateLabel(hasId, resolved);
}

inline bool IsRegistryDiagnostic(const EffectAssetDiagnostic& diagnostic) {
    return diagnostic.source == "techniqueRegistry" ||
        diagnostic.source == "rendererRegistry" ||
        diagnostic.source == "simulationRegistry";
}

inline bool IsMetadataOverrideDiagnostic(const EffectAssetDiagnostic& diagnostic) {
    return diagnostic.code == "metadataOverride";
}

} // namespace EffectAuthoringUi
