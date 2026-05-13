#pragma once

#include "AppGpuParticleSystem.h"
#include "AppVfxTelemetry.h"
#include "graphics/RenderGraph.h"
#include "vfx/DistortionRenderer.h"
#include "vfx/ParticleRenderer.h"
#include "vfx/TrailRenderer.h"
#include "vfx/VfxResources.h"

#include <d3d12.h>
#include <vector>

class AppPipelines;
class AppRenderResources;
class AppSceneResources;
struct FrameLoopState;

struct AppVfxDebugDataBuilderContext {
    AppPipelines* appPipelines = nullptr;
    AppRenderResources* renderResources = nullptr;
    AppSceneResources* scene = nullptr;
    AppGpuParticleSystem* gpuParticleSystem = nullptr;
    const FrameLoopState* frameState = nullptr;
    ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE vfxTextureHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthTextureHandle{};
    const std::vector<ge3::graphics::RenderPassDebugInfo>* renderPassDebugInfo = nullptr;
};

struct ParticleDedicatedRuntimeDebugData {
    vfx::VfxTypedResourceSet resources{};
    vfx::ParticleRendererOperationalStatus status{};
    ParticleRenderGraphIntentTelemetry graphIntent{};
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry = nullptr;
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
};

struct ParticleRendererInspectorDebugData {
    vfx::VfxTypedResourceSet resources{};
    vfx::ParticleRendererOperationalStatus status{};
    ParticleRenderGraphIntentTelemetry graphIntent{};
    bool probeEnabled = false;
    vfx::VfxTypedResourceSet probeResources{};
    vfx::ParticleRendererOperationalStatus probeStatus{};
    ParticleRenderGraphIntentTelemetry probeGraphIntent{};
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry = nullptr;
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
};

struct DistortionDedicatedRuntimeDebugData {
    vfx::VfxTypedResourceSet resources{};
    vfx::DistortionRendererOperationalStatus status{};
    DistortionRenderGraphIntentTelemetry graphIntent{};
    const AppGpuParticleSystem::DistortionDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
    uint32_t expectedDrawCount = 0;
};

struct BeamDedicatedRuntimeDebugData {
    vfx::VfxTypedResourceSet resources{};
    vfx::BeamRendererOperationalStatus status{};
    BeamRenderGraphIntentTelemetry graphIntent{};
    const AppGpuParticleSystem::BeamDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
    uint32_t expectedDrawCount = 0;
    bool dedicatedPathReady = false;
    bool dedicatedResourcesAvailable = false;
    const char* fallbackReason = "beam dedicated path not implemented";
};

struct BeamRendererInspectorDebugData {
    vfx::VfxTypedResourceSet resources{};
    vfx::BeamRendererOperationalStatus status{};
    BeamRenderGraphIntentTelemetry graphIntent{};
    const AppGpuParticleSystem::BeamDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
    uint32_t expectedDrawCount = 0;
    bool dedicatedPathReady = false;
    bool dedicatedResourcesAvailable = false;
    const char* fallbackReason = "beam dedicated path not implemented";
};

struct TrailMeshStreamRuntimeDebugData {
    vfx::VfxTypedResourceSet activeResources{};
    vfx::TrailMeshStreamPlan activePlan{};
    vfx::TrailMeshStreamDrawStatus activeDrawStatus{};
    TrailMeshStreamTelemetrySummary activeSummary{};
    vfx::VfxTypedResourceSet probeResources{};
    vfx::TrailMeshStreamPlan probePlan{};
    vfx::TrailMeshStreamDrawStatus probeDrawStatus{};
    TrailMeshStreamTelemetrySummary probeSummary{};
    const AppGpuParticleSystem::TrailMeshStreamTelemetry* telemetry = nullptr;
    bool telemetryValid = false;
};

struct TrailMeshStreamInspectorDebugData {
    vfx::VfxTypedResourceSet resources{};
    vfx::TrailMeshStreamPlan plan{};
    vfx::TrailMeshStreamDrawStatus drawStatus{};
    vfx::TrailMeshStreamDebugPreview debugPreview{};
    TrailMeshStreamTelemetrySummary summary{};
    vfx::VfxTypedResourceSet probeResources{};
    vfx::TrailMeshStreamPlan probePlan{};
    vfx::TrailMeshStreamDrawStatus probeDrawStatus{};
    const AppGpuParticleSystem::TrailMeshStreamTelemetry* telemetry = nullptr;
    bool telemetryValid = false;
};

void BuildParticleDedicatedRuntimeDebugData(
    const AppVfxDebugDataBuilderContext& context,
    ParticleDedicatedRuntimeDebugData& output);

void BuildParticleRendererInspectorDebugData(
    const AppVfxDebugDataBuilderContext& context,
    bool probeEnabled,
    ParticleRendererInspectorDebugData& output);

void BuildDistortionDedicatedRuntimeDebugData(
    const AppVfxDebugDataBuilderContext& context,
    DistortionDedicatedRuntimeDebugData& output);

void BuildBeamDedicatedRuntimeDebugData(
    const AppVfxDebugDataBuilderContext& context,
    BeamDedicatedRuntimeDebugData& output);

void BuildBeamRendererInspectorDebugData(
    const AppVfxDebugDataBuilderContext& context,
    BeamRendererInspectorDebugData& output);

void BuildTrailMeshStreamRuntimeDebugData(
    const AppVfxDebugDataBuilderContext& context,
    const TrailRenderInput& trailInput,
    bool effectiveDedicated,
    TrailMeshStreamRuntimeDebugData& output);

void BuildTrailMeshStreamInspectorDebugData(
    const AppVfxDebugDataBuilderContext& context,
    const TrailRenderInput& trailInput,
    const Vector3& cameraWorldPosition,
    bool effectiveDedicated,
    TrailMeshStreamInspectorDebugData& output);
