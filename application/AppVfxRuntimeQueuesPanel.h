#pragma once

#include "AppVfxDebugDataBuilder.h"
#include "AppVfxRuntimeState.h"
#include "AppVfxTelemetry.h"

#include <cstdint>

struct AppRuntimeState;
class EffectRuntime;

struct VfxRuntimeQueuesPanelInput {
    AppRuntimeState* runtimeState = nullptr;
    EffectRuntime* effectRuntime = nullptr;
    const AppVfxDebugDataBuilderContext* debugDataContext = nullptr;
    uint32_t* particleDedicatedProbeTelemetryFrames = nullptr;
    uint32_t trailMeshStreamHealthFrames = 0;
    uint32_t trailMeshStreamProbeHealthyFrames = 0;
    uint32_t trailMeshStreamActiveHealthyFrames = 0;
};

struct RuntimeQueuesSummaryPanelInput {
    uint32_t activeInstanceCount = 0;
    uint32_t activeComponentCount = 0;
    uint32_t particleQueueSize = 0;
    uint32_t trailQueueSize = 0;
    uint32_t beamQueueSize = 0;
    uint32_t distortionQueueSize = 0;
};

struct ParticleRendererInspectorStatusPanelInput {
    const vfx::ParticleRendererOperationalStatus* status = nullptr;
    const ParticleRenderGraphIntentTelemetry* graphIntent = nullptr;
};

struct ParticleRendererInspectorDetailsPanelInput {
    const ParticleRendererInspectorDebugData* debugData = nullptr;
    uint32_t particleQueueSize = 0;
    bool particleQueueEmpty = true;
};

struct BeamRendererInspectorPanelInput {
    const BeamRendererInspectorDebugData* debugData = nullptr;
    uint32_t beamQueueSize = 0;
    bool beamQueueEmpty = true;
};

struct TrailMeshStreamRuntimeStatusPanelInput {
    const TrailMeshStreamRuntimeDebugData* debugData = nullptr;
    const TrailMeshStreamRuntimeTelemetryUpdateResult* telemetry = nullptr;
    const AppVfxRuntimeState* runtimeState = nullptr;
    uint32_t probeStableFrames = 0;
    uint32_t activeStableFrames = 0;
};

struct TrailMeshStreamInspectorPanelInput {
    const TrailMeshStreamInspectorDebugData* debugData = nullptr;
    const TrailMeshStreamInspectorTelemetryResult* telemetry = nullptr;
    const TrailRenderInput* trailInput = nullptr;
    uint32_t activeStableFrames = 0;
    uint32_t probeStableFrames = 0;
};

void DrawRuntimeQueuesSummaryPanel(
    const RuntimeQueuesSummaryPanelInput& input);

void DrawParticleRendererInspectorStatusPanel(
    const ParticleRendererInspectorStatusPanelInput& input);

void DrawParticleRendererInspectorDetailsPanel(
    const ParticleRendererInspectorDetailsPanelInput& input);

void DrawBeamRendererInspectorPanel(
    const BeamRendererInspectorPanelInput& input);

void DrawTrailMeshStreamRuntimeStatusPanel(
    const TrailMeshStreamRuntimeStatusPanelInput& input);

void DrawTrailMeshStreamInspectorPanel(
    const TrailMeshStreamInspectorPanelInput& input);

void DrawVfxRuntimeQueuesPanel(
    const VfxRuntimeQueuesPanelInput& input);
