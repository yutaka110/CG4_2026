#pragma once

#include <vector>

#include "vfx/VfxRenderInputs.h"
#include "vfx/VfxRenderContext.h"

struct AppFrameGraphBuildContext;
namespace vfx {
struct DistortionVfxResourceSet;
struct VfxTypedResourceSet;

struct DistortionRendererOperationalStatus {
    bool resourceIntentReady = false;
    bool resourceHandlesReady = false;
    bool operationalOk = false;
    const char* fallbackReason = "unknown";
    const char* resourceIntentFallbackReason = "unknown";
    const char* resourceHandleFallbackReason = "unknown";
    const char* simulationStateResource = "";
    const char* simulationRenderBufferResource = "";
    const char* simulationIndirectArgsResource = "";
    const char* renderBufferResource = "";
    const char* indirectArgsResource = "";
    bool simulationStateUavValid = false;
    bool simulationRenderBufferUavValid = false;
    bool renderBufferSrvValid = false;
    bool indirectArgsValid = false;
};

DistortionRendererOperationalStatus EvaluateDistortionRendererOperationalStatus(
    const VfxRenderContext& context,
    const DistortionVfxResourceSet* distortionResources);
}

class DistortionRenderer {
public:
    void RegisterPasses(
        const AppFrameGraphBuildContext& context,
        const vfx::VfxTypedResourceSet& resources) const;

    void Simulate(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const DistortionRenderInput& input) const;

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const VfxRenderContext& context,
        const DistortionRenderInput& input) const;
};
