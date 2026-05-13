#pragma once

#include "TechniqueRegistry.h"
#include "RendererRegistry.h"
#include "SimulationRegistry.h"

class EffectAuthoringRegistry {
public:
    EffectAuthoringRegistry();
    EffectAuthoringRegistry(
        const TechniqueRegistry& techniqueRegistry,
        const RendererRegistry& rendererRegistry,
        const SimulationRegistry& simulationRegistry);

    static EffectAuthoringRegistry Default();

    const TechniqueRegistry& Techniques() const;
    const RendererRegistry& Renderers() const;
    const SimulationRegistry& Simulations() const;

private:
    const TechniqueRegistry* techniqueRegistry_ = nullptr;
    const RendererRegistry* rendererRegistry_ = nullptr;
    const SimulationRegistry* simulationRegistry_ = nullptr;
};
