#pragma once

#include "AppVfxRuntimeState.h"
#include "AppVfxTelemetry.h"
#include "vfx/BeamRenderer.h"
#include "vfx/DistortionRenderer.h"
#include "vfx/TrailRenderer.h"
#include "vfx/VfxRenderInputs.h"
#include "vfx/VfxResources.h"

#include <cstdint>

struct ParticleDedicatedRuntimeTelemetryPanelInput {
    const VfxComponentInputCommon* primaryInput = nullptr;
    const ParticleDedicatedOperationalHealthSummary* health = nullptr;
    bool readyToEnable = false;
    bool activeStable = false;
    bool warmup = false;
    uint32_t warmupFrames = 0;
    uint32_t probeStableFrames = 0;
    uint32_t readyToEnableFrames = 0;
    uint32_t activeStableFrames = 0;
    uint32_t activeStableRequiredFrames = 0;
    const AppVfxRuntimeState* runtimeState = nullptr;
};

struct DistortionDedicatedRuntimeTelemetryPanelInput {
    const VfxComponentInputCommon* primaryInput = nullptr;
    const DistortionDedicatedOperationalHealthSummary* health = nullptr;
    const DistortionRenderGraphIntentTelemetry* graphIntent = nullptr;
    const vfx::DistortionRendererOperationalStatus* status = nullptr;
    bool stableEnough = false;
    bool activeStable = false;
    bool warmup = false;
    uint32_t warmupFrames = 0;
    uint32_t stableFrames = 0;
    uint32_t stableRequiredFrames = 0;
    uint32_t activeStableFrames = 0;
    uint32_t activeStableRequiredFrames = 0;
    uint32_t telemetryFrames = 0;
    uint32_t telemetryFrameLimit = 0;
    const AppVfxRuntimeState* runtimeState = nullptr;
};

struct ParticleRenderGraphIntentPanelInput {
    const vfx::ParticleRendererOperationalStatus* status = nullptr;
    const ParticleRenderGraphIntentTelemetry* graphIntent = nullptr;
    const vfx::ParticleVfxResourceSet* resources = nullptr;
};

struct ParticleDedicatedResourceProbePanelInput {
    const VfxComponentInputCommon* primaryInput = nullptr;
    const ParticleDedicatedOperationalHealthSummary* health = nullptr;
    const vfx::ParticleRendererOperationalStatus* status = nullptr;
    const ParticleRenderGraphIntentTelemetry* graphIntent = nullptr;
    const vfx::ParticleVfxResourceSet* resources = nullptr;
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry = nullptr;
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
    const ParticleDedicatedReadbackValidationSummary* readbackSummary = nullptr;
    const AppVfxRuntimeState* runtimeState = nullptr;
};

struct TrailMeshStreamTelemetryPanelInput {
    const vfx::TrailMeshStreamPlan* plan = nullptr;
    const vfx::TrailMeshStreamDrawStatus* drawStatus = nullptr;
    const AppGpuParticleSystem::TrailMeshStreamTelemetry* telemetry = nullptr;
    bool telemetryValid = false;
    bool operationalOk = false;
    const char* defaultOnHealth = nullptr;
    uint32_t activeStableFrames = 0;
    uint32_t activeStableRequiredFrames = 0;
    uint32_t probeStableFrames = 0;
    uint32_t probeStableRequiredFrames = 0;
    uint32_t okRows = 0;
    uint32_t ngRows = 0;
    uint32_t firstFailureSegment = UINT32_MAX;
    const char* firstFailureReason = nullptr;
};

struct BeamDedicatedRuntimeTelemetryPanelInput {
    const VfxComponentInputCommon* primaryInput = nullptr;
    const BeamDedicatedOperationalHealthSummary* health = nullptr;
    const BeamRenderGraphIntentTelemetry* graphIntent = nullptr;
    const vfx::BeamRendererOperationalStatus* status = nullptr;
    bool stableEnough = false;
    bool activeStable = false;
    bool warmup = false;
    uint32_t warmupFrames = 0;
    uint32_t stableFrames = 0;
    uint32_t stableRequiredFrames = 0;
    uint32_t activeStableFrames = 0;
    uint32_t activeStableRequiredFrames = 0;
    uint32_t telemetryFrames = 0;
    uint32_t telemetryFrameLimit = 0;
    const AppVfxRuntimeState* runtimeState = nullptr;
};

void DrawParticleDedicatedRuntimeTelemetryPanel(
    const ParticleDedicatedRuntimeTelemetryPanelInput& input);

void DrawDistortionDedicatedRuntimeTelemetryPanel(
    const DistortionDedicatedRuntimeTelemetryPanelInput& input);

void DrawBeamDedicatedRuntimeTelemetryPanel(
    const BeamDedicatedRuntimeTelemetryPanelInput& input);

void DrawParticleRenderGraphIntentPanel(
    const ParticleRenderGraphIntentPanelInput& input);

void DrawParticleDedicatedResourceProbePanel(
    const ParticleDedicatedResourceProbePanelInput& input);

void DrawTrailMeshStreamTelemetryPanel(
    const TrailMeshStreamTelemetryPanelInput& input);

void DrawTrailMeshStreamVisualDebugPanel(
    const vfx::TrailMeshStreamDebugPreview& preview);
