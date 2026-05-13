#include "TechniqueRegistry.h"

#include <algorithm>
#include <utility>

namespace {
EffectTechniqueDescriptor MakeTechniqueDescriptor(
    std::string id,
    std::string displayName,
    std::string category,
    std::string description,
    EffectTechnique technique,
    EffectComponentType componentType,
    std::string rendererId,
    std::string simulationId,
    EffectLayer layer,
    uint32_t renderQueueOffset) {
    EffectTechniqueDescriptor descriptor{};
    descriptor.id = std::move(id);
    descriptor.displayName = std::move(displayName);
    descriptor.category = std::move(category);
    descriptor.description = std::move(description);
    descriptor.technique = technique;
    descriptor.componentType = componentType;
    descriptor.rendererId = std::move(rendererId);
    descriptor.simulationId = std::move(simulationId);
    descriptor.layer = layer;
    descriptor.renderQueueOffset = renderQueueOffset;
    return descriptor;
}

TechniqueRegistry BuildDefaultTechniqueRegistry() {
    TechniqueRegistry registry{};
    registry.Register(
        MakeTechniqueDescriptor(
            "ParticleAdditive",
            "Particle Additive",
            "Particle",
            "GPU-simulated additive sprites for sparks, smoke, and general billboard particles.",
            EffectTechnique::ParticleAdditive,
            EffectComponentType::Particle,
            "ParticleRenderer",
            "CpuSpawnGpuSim",
            EffectLayer::AdditiveFx,
            0));
    registry.Register(
        MakeTechniqueDescriptor(
            "AuthoringSpark",
            "Authoring Spark",
            "Authoring Samples",
            "Sample particle technique that proves new .effect technique IDs can route through the registry without adding a new legacy enum.",
            EffectTechnique::ParticleAdditive,
            EffectComponentType::Particle,
            "ParticleRenderer",
            "CpuSpawnGpuSim",
            EffectLayer::AdditiveFx,
            40));
    registry.Register(
        MakeTechniqueDescriptor(
            "AuthoringRegistryOnlySpark",
            "Authoring Registry-Only Spark",
            "Authoring Samples",
            "Sample technique routed through registry-only renderer/simulation IDs while sharing compatibility mirrors with ParticleAdditive.",
            EffectTechnique::ParticleAdditive,
            EffectComponentType::Particle,
            "AuthoringParticleRenderer",
            "AuthoringCpuSpawnGpuSim",
            EffectLayer::AdditiveFx,
            60));
    registry.Register(
        MakeTechniqueDescriptor(
            "TrailRibbon",
            "Trail Ribbon",
            "Trail",
            "CPU-timeline ribbon trails with mesh stream rendering for streaks and motion accents.",
            EffectTechnique::TrailRibbon,
            EffectComponentType::Trail,
            "TrailRenderer",
            "CpuTimeline",
            EffectLayer::TrailFx,
            100));
    registry.Register(
        MakeTechniqueDescriptor(
            "BeamLightning",
            "Beam Lightning",
            "Beam",
            "CPU-timeline beam routing for lightning-like connected energy effects.",
            EffectTechnique::BeamLightning,
            EffectComponentType::Beam,
            "BeamRenderer",
            "CpuTimeline",
            EffectLayer::AdditiveFx,
            200));
    EffectTechniqueDescriptor distortion = MakeTechniqueDescriptor(
            "DistortionSprite",
            "Distortion Sprite",
            "Distortion",
            "Screen-space distortion sprites routed through the distortion layer and blend mode.",
            EffectTechnique::DistortionSprite,
            EffectComponentType::Distortion,
            "DistortionRenderer",
            "None",
            EffectLayer::DistortionFx,
            300);
    distortion.overrideBlend = true;
    distortion.blend = ge3::graphics::BlendMode::Distortion;
    registry.Register(std::move(distortion));
    return registry;
}
} // namespace

const TechniqueRegistry& TechniqueRegistry::Default() {
    static const TechniqueRegistry registry = BuildDefaultTechniqueRegistry();
    return registry;
}

void TechniqueRegistry::Register(EffectTechniqueDescriptor descriptor) {
    RendererRegistry::Default().ResolveTechniqueRenderer(descriptor);
    SimulationRegistry::Default().ResolveTechniqueSimulation(descriptor);

    const auto existing = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&descriptor](const EffectTechniqueDescriptor& registered) {
            return registered.id == descriptor.id;
        });
    if (existing != descriptors_.end()) {
        *existing = std::move(descriptor);
        return;
    }
    descriptors_.push_back(std::move(descriptor));
}

const EffectTechniqueDescriptor* TechniqueRegistry::Find(std::string_view id) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [id](const EffectTechniqueDescriptor& descriptor) {
            return descriptor.id == id;
        });
    return found != descriptors_.end() ? &*found : nullptr;
}

const EffectTechniqueDescriptor* TechniqueRegistry::FindByLegacyTechnique(
    EffectTechnique technique) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [technique](const EffectTechniqueDescriptor& descriptor) {
            return descriptor.technique == technique;
        });
    return found != descriptors_.end() ? &*found : nullptr;
}

const EffectTechniqueDescriptor* TechniqueRegistry::FindDefaultForComponentType(
    EffectComponentType type) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [type](const EffectTechniqueDescriptor& descriptor) {
            return descriptor.componentType == type;
        });
    return found != descriptors_.end() ? &*found : nullptr;
}

const std::vector<EffectTechniqueDescriptor>& TechniqueRegistry::Descriptors() const {
    return descriptors_;
}

void TechniqueRegistry::ApplyDescriptor(
    EffectComponentCommon& common,
    const EffectTechniqueDescriptor& descriptor) const {
    if (common.techniqueId.empty()) {
        common.techniqueId = descriptor.id;
    }
    common.techniqueDisplayName = descriptor.displayName;
    common.techniqueCategory = descriptor.category;
    common.techniqueDescription = descriptor.description;
    common.techniqueMetadataSource = EffectTechniqueMetadataSource::Registry;
    common.techniqueDisplayNameSource = EffectTechniqueMetadataSource::Registry;
    common.techniqueCategorySource = EffectTechniqueMetadataSource::Registry;
    common.techniqueDescriptionSource = EffectTechniqueMetadataSource::Registry;
    common.type = descriptor.componentType;
    common.rendererId = descriptor.rendererId;
    common.simulationId = descriptor.simulationId;
    common.layer = descriptor.layer;
    if (descriptor.overrideBlend) {
        common.passState.blend = descriptor.blend;
    }
    UpdateCompatibilityMirrors(common, descriptor);
}

void TechniqueRegistry::UpdateCompatibilityMirrors(
    EffectComponentCommon& common,
    const EffectTechniqueDescriptor& descriptor) const {
    common.technique = descriptor.technique;
    common.rendererType = descriptor.rendererType;
    common.simulationType = descriptor.simulationType;
}

void TechniqueRegistry::ApplyRouting(EffectComponentCommon& common) const {
    const EffectTechniqueDescriptor* descriptor = nullptr;
    if (!common.techniqueId.empty()) {
        descriptor = Find(common.techniqueId);
    }
    if (descriptor == nullptr) {
        // Legacy fallback for assets created before techniqueId was persisted.
        descriptor = FindByLegacyTechnique(common.technique);
    }
    if (descriptor == nullptr) {
        descriptor = Find(std::string_view{"ParticleAdditive"});
    }
    if (descriptor == nullptr) {
        descriptor = FindByLegacyTechnique(EffectTechnique::ParticleAdditive);
    }
    if (descriptor == nullptr) {
        return;
    }
    ApplyDescriptor(common, *descriptor);
}
