#include "EffectAuthoringRegistry.h"

EffectAuthoringRegistry::EffectAuthoringRegistry()
    : techniqueRegistry_(&TechniqueRegistry::Default()),
      rendererRegistry_(&RendererRegistry::Default()),
      simulationRegistry_(&SimulationRegistry::Default()) {
}

EffectAuthoringRegistry::EffectAuthoringRegistry(
    const TechniqueRegistry& techniqueRegistry,
    const RendererRegistry& rendererRegistry,
    const SimulationRegistry& simulationRegistry)
    : techniqueRegistry_(&techniqueRegistry),
      rendererRegistry_(&rendererRegistry),
      simulationRegistry_(&simulationRegistry) {
}

EffectAuthoringRegistry EffectAuthoringRegistry::Default() {
    return EffectAuthoringRegistry{
        TechniqueRegistry::Default(),
        RendererRegistry::Default(),
        SimulationRegistry::Default()};
}

const TechniqueRegistry& EffectAuthoringRegistry::Techniques() const {
    return techniqueRegistry_ != nullptr
        ? *techniqueRegistry_
        : TechniqueRegistry::Default();
}

const RendererRegistry& EffectAuthoringRegistry::Renderers() const {
    return rendererRegistry_ != nullptr
        ? *rendererRegistry_
        : RendererRegistry::Default();
}

const SimulationRegistry& EffectAuthoringRegistry::Simulations() const {
    return simulationRegistry_ != nullptr
        ? *simulationRegistry_
        : SimulationRegistry::Default();
}
