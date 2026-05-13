#pragma once

#include "AppVfxDebugDataBuilder.h"

#include <cstdint>

struct AppRuntimeState;
class EffectRuntime;

struct AppVfxRuntimeStatusPanelInput {
    AppRuntimeState* runtimeState = nullptr;
    EffectRuntime* effectRuntime = nullptr;
    const AppVfxDebugDataBuilderContext* debugDataContext = nullptr;
    uint32_t* trailMeshStreamProbeHealthyFrames = nullptr;
    uint32_t* trailMeshStreamActiveHealthyFrames = nullptr;
    uint32_t* trailMeshStreamHealthFrames = nullptr;
    uint32_t* trailMeshStreamStartupTelemetryFrames = nullptr;
    uint32_t* particleDedicatedProbeTelemetryFrames = nullptr;
    uint32_t* particleDedicatedHealthFrames = nullptr;
    uint32_t* particleDedicatedProbeStableFrames = nullptr;
    uint32_t* particleDedicatedActiveStableFrames = nullptr;
    uint32_t* distortionDedicatedHealthFrames = nullptr;
    uint32_t* distortionDedicatedTelemetryFrames = nullptr;
    uint32_t* distortionDedicatedStableFrames = nullptr;
    uint32_t* distortionDedicatedActiveStableFrames = nullptr;
    uint32_t* beamDedicatedTelemetryFrames = nullptr;
    uint32_t* beamDedicatedHealthFrames = nullptr;
    uint32_t* beamDedicatedStableFrames = nullptr;
    uint32_t* beamDedicatedActiveStableFrames = nullptr;
};

void UpdateVfxRuntimeStatusTelemetry(
    const AppVfxRuntimeStatusPanelInput& input);

void DrawVfxRuntimeStatusPanel(
    const AppVfxRuntimeStatusPanelInput& input);
