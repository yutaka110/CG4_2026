#include "RendererRegistry.h"

#include <algorithm>
#include <utility>

namespace {
RendererRegistry BuildDefaultRendererRegistry() {
    RendererRegistry registry{};
    registry.Register(
        EffectRendererDescriptor{
            "ParticleRenderer",
            EffectRendererType::ParticleRenderer,
            EffectComponentType::Particle});
    registry.Register(
        EffectRendererDescriptor{
            "AuthoringParticleRenderer",
            EffectRendererType::ParticleRenderer,
            EffectComponentType::Particle});
    registry.Register(
        EffectRendererDescriptor{
            "TrailRenderer",
            EffectRendererType::TrailRenderer,
            EffectComponentType::Trail});
    registry.Register(
        EffectRendererDescriptor{
            "BeamRenderer",
            EffectRendererType::BeamRenderer,
            EffectComponentType::Beam});
    registry.Register(
        EffectRendererDescriptor{
            "DistortionRenderer",
            EffectRendererType::DistortionRenderer,
            EffectComponentType::Distortion});
    return registry;
}
} // namespace

const RendererRegistry& RendererRegistry::Default() {
    static const RendererRegistry registry = BuildDefaultRendererRegistry();
    return registry;
}

void RendererRegistry::Register(EffectRendererDescriptor descriptor) {
    const auto existing = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&descriptor](const EffectRendererDescriptor& registered) {
            return registered.id == descriptor.id;
        });
    if (existing != descriptors_.end()) {
        *existing = std::move(descriptor);
        return;
    }
    descriptors_.push_back(std::move(descriptor));
}

const EffectRendererDescriptor* RendererRegistry::Find(std::string_view id) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [id](const EffectRendererDescriptor& descriptor) {
            return descriptor.id == id;
        });
    return found != descriptors_.end() ? &*found : nullptr;
}

const EffectRendererDescriptor* RendererRegistry::FindByLegacyRendererType(
    EffectRendererType rendererType) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [rendererType](const EffectRendererDescriptor& descriptor) {
            return descriptor.rendererType == rendererType;
        });
    return found != descriptors_.end() ? &*found : nullptr;
}

const EffectRendererDescriptor* RendererRegistry::FindDefaultForComponentType(
    EffectComponentType type) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [type](const EffectRendererDescriptor& descriptor) {
            return descriptor.componentType == type;
        });
    return found != descriptors_.end() ? &*found : nullptr;
}

void RendererRegistry::ResolveTechniqueRenderer(EffectTechniqueDescriptor& descriptor) const {
    if (!descriptor.rendererId.empty()) {
        if (const EffectRendererDescriptor* renderer = Find(descriptor.rendererId)) {
            descriptor.rendererType = renderer->rendererType;
        }
        return;
    }

    if (const EffectRendererDescriptor* renderer = FindByLegacyRendererType(descriptor.rendererType)) {
        descriptor.rendererId = renderer->id;
    }
}

const std::vector<EffectRendererDescriptor>& RendererRegistry::Descriptors() const {
    return descriptors_;
}
