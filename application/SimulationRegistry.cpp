#include "SimulationRegistry.h"

#include <algorithm>
#include <utility>

namespace {
SimulationRegistry BuildDefaultSimulationRegistry() {
    SimulationRegistry registry{};
    registry.Register(
        EffectSimulationDescriptor{
            "CpuSpawnGpuSim",
            EffectSimulationType::CpuSpawnGpuSim});
    registry.Register(
        EffectSimulationDescriptor{
            "AuthoringCpuSpawnGpuSim",
            EffectSimulationType::CpuSpawnGpuSim});
    registry.Register(
        EffectSimulationDescriptor{
            "CpuTimeline",
            EffectSimulationType::CpuTimeline});
    registry.Register(
        EffectSimulationDescriptor{
            "GpuSimulation",
            EffectSimulationType::GpuSimulation});
    registry.Register(
        EffectSimulationDescriptor{
            "None",
            EffectSimulationType::None});
    return registry;
}
} // namespace

const SimulationRegistry& SimulationRegistry::Default() {
    static const SimulationRegistry registry = BuildDefaultSimulationRegistry();
    return registry;
}

void SimulationRegistry::Register(EffectSimulationDescriptor descriptor) {
    const auto existing = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&descriptor](const EffectSimulationDescriptor& registered) {
            return registered.id == descriptor.id;
        });
    if (existing != descriptors_.end()) {
        *existing = std::move(descriptor);
        return;
    }
    descriptors_.push_back(std::move(descriptor));
}

const EffectSimulationDescriptor* SimulationRegistry::Find(std::string_view id) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [id](const EffectSimulationDescriptor& descriptor) {
            return descriptor.id == id;
        });
    return found != descriptors_.end() ? &*found : nullptr;
}

const EffectSimulationDescriptor* SimulationRegistry::FindByLegacySimulationType(
    EffectSimulationType simulationType) const {
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [simulationType](const EffectSimulationDescriptor& descriptor) {
            return descriptor.simulationType == simulationType;
        });
    return found != descriptors_.end() ? &*found : nullptr;
}

void SimulationRegistry::ResolveTechniqueSimulation(
    EffectTechniqueDescriptor& descriptor) const {
    if (!descriptor.simulationId.empty()) {
        if (const EffectSimulationDescriptor* simulation = Find(descriptor.simulationId)) {
            descriptor.simulationType = simulation->simulationType;
        }
        return;
    }

    if (const EffectSimulationDescriptor* simulation =
            FindByLegacySimulationType(descriptor.simulationType)) {
        descriptor.simulationId = simulation->id;
    }
}

const std::vector<EffectSimulationDescriptor>& SimulationRegistry::Descriptors() const {
    return descriptors_;
}
