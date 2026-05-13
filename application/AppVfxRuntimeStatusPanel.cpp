#include "AppVfxRuntimeStatusPanel.h"

#include "AppRuntimeState.h"
#include "AppVfxRuntimeQueuesPanel.h"
#include "AppVfxTelemetry.h"
#include "AppVfxTelemetryPanel.h"
#include "EffectRuntime.h"

#include "../../externals/imgui/imgui.h"

namespace {
bool HasRequiredCounters(const AppVfxRuntimeStatusPanelInput& input) {
    return input.trailMeshStreamProbeHealthyFrames != nullptr &&
        input.trailMeshStreamActiveHealthyFrames != nullptr &&
        input.trailMeshStreamHealthFrames != nullptr &&
        input.trailMeshStreamStartupTelemetryFrames != nullptr &&
        input.particleDedicatedProbeTelemetryFrames != nullptr &&
        input.particleDedicatedHealthFrames != nullptr &&
        input.particleDedicatedProbeStableFrames != nullptr &&
        input.particleDedicatedActiveStableFrames != nullptr &&
        input.distortionDedicatedHealthFrames != nullptr &&
        input.distortionDedicatedTelemetryFrames != nullptr &&
        input.distortionDedicatedStableFrames != nullptr &&
        input.distortionDedicatedActiveStableFrames != nullptr &&
        input.beamDedicatedTelemetryFrames != nullptr &&
        input.beamDedicatedHealthFrames != nullptr &&
        input.beamDedicatedStableFrames != nullptr &&
        input.beamDedicatedActiveStableFrames != nullptr;
}

void DrawParticleRuntimeStatusSection(
    AppRuntimeState& runtimeState,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const EffectRuntimeFrame& runtimeFrame,
    const AppVfxRuntimeStatusPanelInput& input) {
    ImGui::Checkbox("Show Particles", &runtimeState.vfx.enableParticles);
    if (ImGui::Checkbox("Particle Dedicated Resource Probe", &runtimeState.vfx.enableParticleDedicatedResourceProbe)) {
        *input.particleDedicatedProbeTelemetryFrames = 0;
        *input.particleDedicatedHealthFrames = 0;
        *input.particleDedicatedProbeStableFrames = 0;
        *input.particleDedicatedActiveStableFrames = 0;
        runtimeState.vfx.particleDedicatedProbeStableFrames = 0;
        runtimeState.vfx.particleDedicatedActiveStableFrames = 0;
        runtimeState.vfx.particleDedicatedDefaultONCandidate = false;
        runtimeState.vfx.particleDedicatedDefaultOnHealth =
            runtimeState.vfx.enableParticleDedicatedResourceProbe ? "warmup" : "disabled";
        runtimeState.vfx.particleDedicatedResourceFallbackActive = false;
    }
    if (ImGui::Checkbox(
            "Particle Dedicated Probe Telemetry Log",
            &runtimeState.vfx.enableParticleDedicatedProbeTelemetry)) {
        *input.particleDedicatedProbeTelemetryFrames = 0;
    }
    ImGui::Checkbox(
        "Particle Dedicated Auto Fallback",
        &runtimeState.vfx.enableParticleDedicatedAutoFallback);
    ImGui::Text(
        "particleDedicatedProbeTelemetry=%s frames=%u/300",
        runtimeState.vfx.enableParticleDedicatedProbeTelemetry ? "on" : "off",
        *input.particleDedicatedProbeTelemetryFrames);

    {
        const ParticleRenderFallback particleFallback = runtimeFrame.PrimaryParticleFallback();
        const ParticleRenderInput particleInput = runtimeFrame.ParticleInput(particleFallback);
        ParticleDedicatedRuntimeDebugData particleDedicatedDebugData{};
        BuildParticleDedicatedRuntimeDebugData(debugDataContext, particleDedicatedDebugData);
        const ParticleDedicatedRuntimeTelemetryUpdateResult particleRuntimeTelemetry =
            UpdateParticleDedicatedRuntimeTelemetry(
                ParticleDedicatedRuntimeTelemetryUpdateInput{
                    &runtimeState.vfx,
                    &particleDedicatedDebugData.status,
                    &particleDedicatedDebugData.graphIntent,
                    particleDedicatedDebugData.simulationTelemetry,
                    particleDedicatedDebugData.readbackTelemetry,
                    input.particleDedicatedHealthFrames,
                    input.particleDedicatedProbeStableFrames,
                    input.particleDedicatedActiveStableFrames});
        DrawParticleDedicatedRuntimeTelemetryPanel(
            ParticleDedicatedRuntimeTelemetryPanelInput{
                &particleInput.primary,
                &runtimeState.vfx.particleDedicatedHealth,
                particleRuntimeTelemetry.readyToEnable,
                particleRuntimeTelemetry.activeStable,
                particleRuntimeTelemetry.warmup,
                particleRuntimeTelemetry.warmupFrames,
                *input.particleDedicatedProbeStableFrames,
                particleRuntimeTelemetry.readyToEnableFrames,
                *input.particleDedicatedActiveStableFrames,
                particleRuntimeTelemetry.activeStableRequiredFrames,
                &runtimeState.vfx});
    }
}

void DrawDistortionRuntimeStatusSection(
    AppRuntimeState& runtimeState,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const EffectRuntimeFrame& runtimeFrame,
    const AppVfxRuntimeStatusPanelInput& input) {
    if (ImGui::Checkbox("Distortion Dedicated Resources", &runtimeState.vfx.enableDistortionDedicatedResources)) {
        *input.distortionDedicatedHealthFrames = 0;
        *input.distortionDedicatedTelemetryFrames = 0;
        *input.distortionDedicatedStableFrames = 0;
        *input.distortionDedicatedActiveStableFrames = 0;
        runtimeState.vfx.distortionDedicatedResourceFallbackActive = false;
        runtimeState.vfx.distortionDedicatedOperationalStable = false;
        runtimeState.vfx.distortionDedicatedDefaultOnHealth =
            runtimeState.vfx.enableDistortionDedicatedResources ? "warmup" : "disabled";
    }
    ImGui::Checkbox(
        "Distortion Dedicated Auto Fallback",
        &runtimeState.vfx.enableDistortionDedicatedAutoFallback);
    if (ImGui::Checkbox(
            "Distortion Dedicated Telemetry Log",
            &runtimeState.vfx.enableDistortionDedicatedTelemetry)) {
        *input.distortionDedicatedTelemetryFrames = 0;
    }
    {
        const DistortionRenderInput distortionInput = runtimeFrame.DistortionInput();
        DistortionDedicatedRuntimeDebugData distortionDedicatedDebugData{};
        BuildDistortionDedicatedRuntimeDebugData(debugDataContext, distortionDedicatedDebugData);
        const DistortionDedicatedRuntimeTelemetryUpdateResult distortionRuntimeTelemetry =
            UpdateDistortionDedicatedRuntimeTelemetry(
                DistortionDedicatedRuntimeTelemetryUpdateInput{
                    &runtimeState.vfx,
                    &distortionDedicatedDebugData.status,
                    &distortionDedicatedDebugData.graphIntent,
                    distortionDedicatedDebugData.readbackTelemetry,
                    distortionDedicatedDebugData.expectedDrawCount,
                    input.distortionDedicatedHealthFrames,
                    input.distortionDedicatedStableFrames,
                    input.distortionDedicatedActiveStableFrames,
                    input.distortionDedicatedTelemetryFrames});

        DrawDistortionDedicatedRuntimeTelemetryPanel(
            DistortionDedicatedRuntimeTelemetryPanelInput{
                &distortionInput.primary,
                &runtimeState.vfx.distortionDedicatedHealth,
                &distortionDedicatedDebugData.graphIntent,
                &distortionDedicatedDebugData.status,
                distortionRuntimeTelemetry.stableEnough,
                distortionRuntimeTelemetry.activeStable,
                distortionRuntimeTelemetry.warmup,
                distortionRuntimeTelemetry.warmupFrames,
                *input.distortionDedicatedStableFrames,
                distortionRuntimeTelemetry.stableRequiredFrames,
                *input.distortionDedicatedActiveStableFrames,
                distortionRuntimeTelemetry.activeStableRequiredFrames,
                *input.distortionDedicatedTelemetryFrames,
                distortionRuntimeTelemetry.telemetryFrameLimit,
                &runtimeState.vfx});
    }
}

void DrawBeamRuntimeStatusSection(
    AppRuntimeState& runtimeState,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const EffectRuntimeFrame& runtimeFrame,
    const AppVfxRuntimeStatusPanelInput& input) {
    const BeamRenderInput beamInput = runtimeFrame.BeamInput();
    BeamDedicatedRuntimeDebugData beamDebugData{};
    BuildBeamDedicatedRuntimeDebugData(debugDataContext, beamDebugData);
    const BeamDedicatedRuntimeTelemetryUpdateResult beamRuntimeTelemetry =
        UpdateBeamDedicatedRuntimeTelemetry(
            BeamDedicatedRuntimeTelemetryUpdateInput{
                &runtimeState.vfx,
                &beamDebugData.status,
                &beamDebugData.graphIntent,
                beamDebugData.readbackTelemetry,
                beamDebugData.expectedDrawCount,
                input.beamDedicatedHealthFrames,
                input.beamDedicatedStableFrames,
                input.beamDedicatedActiveStableFrames,
                input.beamDedicatedTelemetryFrames});
    const BeamDedicatedOperationalHealthSummary& health = runtimeState.vfx.beamDedicatedHealth;
    ImGui::Checkbox(
        "Beam Dedicated Auto Fallback",
        &runtimeState.vfx.enableBeamDedicatedAutoFallback);
    DrawBeamDedicatedRuntimeTelemetryPanel(
        BeamDedicatedRuntimeTelemetryPanelInput{
            &beamInput.primary,
            &health,
            &beamDebugData.graphIntent,
            &beamDebugData.status,
            beamRuntimeTelemetry.stableEnough,
            beamRuntimeTelemetry.activeStable,
            beamRuntimeTelemetry.warmup,
            beamRuntimeTelemetry.warmupFrames,
            *input.beamDedicatedStableFrames,
            beamRuntimeTelemetry.stableRequiredFrames,
            *input.beamDedicatedActiveStableFrames,
            beamRuntimeTelemetry.activeStableRequiredFrames,
            *input.beamDedicatedTelemetryFrames,
            beamRuntimeTelemetry.telemetryFrameLimit,
            &runtimeState.vfx});
    if (ImGui::Checkbox("Beam Dedicated Telemetry Log", &runtimeState.vfx.enableBeamDedicatedTelemetry)) {
        *input.beamDedicatedTelemetryFrames = 0;
    }
}

void DrawTrailRuntimeStatusSection(
    AppRuntimeState& runtimeState,
    const EffectRuntimeFrame& runtimeFrame,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const AppVfxRuntimeStatusPanelInput& input) {
    {
        const TrailRenderInput toggleTrailInput = runtimeFrame.TrailInput();
        const bool effectiveTrailMeshStreamEnabled =
            runtimeState.vfx.enableTrailMeshStream && !runtimeState.vfx.trailMeshStreamFallbackActive;
        TrailMeshStreamRuntimeDebugData trailRuntimeDebugData{};
        BuildTrailMeshStreamRuntimeDebugData(
            debugDataContext,
            toggleTrailInput,
            effectiveTrailMeshStreamEnabled,
            trailRuntimeDebugData);
        const TrailMeshStreamRuntimeTelemetryUpdateResult trailRuntimeTelemetry =
            UpdateTrailMeshStreamRuntimeTelemetry(
                TrailMeshStreamRuntimeTelemetryUpdateInput{
                    &runtimeState.vfx,
                    &trailRuntimeDebugData.activeDrawStatus,
                    &trailRuntimeDebugData.activeSummary,
                    &trailRuntimeDebugData.probeDrawStatus,
                    &trailRuntimeDebugData.probeSummary,
                    trailRuntimeDebugData.telemetryValid,
                    input.trailMeshStreamHealthFrames,
                    input.trailMeshStreamProbeHealthyFrames,
                    input.trailMeshStreamActiveHealthyFrames,
                    input.trailMeshStreamStartupTelemetryFrames});
        DrawTrailMeshStreamRuntimeStatusPanel(
            TrailMeshStreamRuntimeStatusPanelInput{
                &trailRuntimeDebugData,
                &trailRuntimeTelemetry,
                &runtimeState.vfx,
                *input.trailMeshStreamProbeHealthyFrames,
                *input.trailMeshStreamActiveHealthyFrames});
    }
}
void UpdateParticleRuntimeStatusTelemetry(
    AppRuntimeState& runtimeState,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const AppVfxRuntimeStatusPanelInput& input) {
    ParticleDedicatedRuntimeDebugData particleDedicatedDebugData{};
    BuildParticleDedicatedRuntimeDebugData(debugDataContext, particleDedicatedDebugData);
    UpdateParticleDedicatedRuntimeTelemetry(
        ParticleDedicatedRuntimeTelemetryUpdateInput{
            &runtimeState.vfx,
            &particleDedicatedDebugData.status,
            &particleDedicatedDebugData.graphIntent,
            particleDedicatedDebugData.simulationTelemetry,
            particleDedicatedDebugData.readbackTelemetry,
            input.particleDedicatedHealthFrames,
            input.particleDedicatedProbeStableFrames,
            input.particleDedicatedActiveStableFrames});

    ParticleRendererInspectorDebugData particleInspectorData{};
    BuildParticleRendererInspectorDebugData(
        debugDataContext,
        runtimeState.vfx.enableParticleDedicatedResourceProbe,
        particleInspectorData);
    if (particleInspectorData.probeEnabled) {
        BuildParticleDedicatedResourceProbeTelemetry(
            ParticleDedicatedResourceProbeTelemetryInput{
                &runtimeState.vfx,
                &particleInspectorData.probeStatus,
                &particleInspectorData.probeGraphIntent,
                particleInspectorData.simulationTelemetry,
                particleInspectorData.readbackTelemetry,
                input.particleDedicatedProbeTelemetryFrames});
    }
}

void UpdateDistortionRuntimeStatusTelemetry(
    AppRuntimeState& runtimeState,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const AppVfxRuntimeStatusPanelInput& input) {
    DistortionDedicatedRuntimeDebugData distortionDedicatedDebugData{};
    BuildDistortionDedicatedRuntimeDebugData(debugDataContext, distortionDedicatedDebugData);
    UpdateDistortionDedicatedRuntimeTelemetry(
        DistortionDedicatedRuntimeTelemetryUpdateInput{
            &runtimeState.vfx,
            &distortionDedicatedDebugData.status,
            &distortionDedicatedDebugData.graphIntent,
            distortionDedicatedDebugData.readbackTelemetry,
            distortionDedicatedDebugData.expectedDrawCount,
            input.distortionDedicatedHealthFrames,
            input.distortionDedicatedStableFrames,
            input.distortionDedicatedActiveStableFrames,
            input.distortionDedicatedTelemetryFrames});
}

void UpdateBeamRuntimeStatusTelemetry(
    AppRuntimeState& runtimeState,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const AppVfxRuntimeStatusPanelInput& input) {
    BeamDedicatedRuntimeDebugData beamDebugData{};
    BuildBeamDedicatedRuntimeDebugData(debugDataContext, beamDebugData);
    UpdateBeamDedicatedRuntimeTelemetry(
        BeamDedicatedRuntimeTelemetryUpdateInput{
            &runtimeState.vfx,
            &beamDebugData.status,
            &beamDebugData.graphIntent,
            beamDebugData.readbackTelemetry,
            beamDebugData.expectedDrawCount,
            input.beamDedicatedHealthFrames,
            input.beamDedicatedStableFrames,
            input.beamDedicatedActiveStableFrames,
            input.beamDedicatedTelemetryFrames});
}

void UpdateTrailRuntimeStatusTelemetry(
    AppRuntimeState& runtimeState,
    const EffectRuntimeFrame& runtimeFrame,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const AppVfxRuntimeStatusPanelInput& input) {
    const TrailRenderInput toggleTrailInput = runtimeFrame.TrailInput();
    const bool effectiveTrailMeshStreamEnabled =
        runtimeState.vfx.enableTrailMeshStream && !runtimeState.vfx.trailMeshStreamFallbackActive;
    TrailMeshStreamRuntimeDebugData trailRuntimeDebugData{};
    BuildTrailMeshStreamRuntimeDebugData(
        debugDataContext,
        toggleTrailInput,
        effectiveTrailMeshStreamEnabled,
        trailRuntimeDebugData);
    UpdateTrailMeshStreamRuntimeTelemetry(
        TrailMeshStreamRuntimeTelemetryUpdateInput{
            &runtimeState.vfx,
            &trailRuntimeDebugData.activeDrawStatus,
            &trailRuntimeDebugData.activeSummary,
            &trailRuntimeDebugData.probeDrawStatus,
            &trailRuntimeDebugData.probeSummary,
            trailRuntimeDebugData.telemetryValid,
            input.trailMeshStreamHealthFrames,
            input.trailMeshStreamProbeHealthyFrames,
            input.trailMeshStreamActiveHealthyFrames,
            input.trailMeshStreamStartupTelemetryFrames});
}
} // namespace

void UpdateVfxRuntimeStatusTelemetry(
    const AppVfxRuntimeStatusPanelInput& input) {
    if (input.runtimeState == nullptr || input.effectRuntime == nullptr ||
        input.debugDataContext == nullptr || !HasRequiredCounters(input)) {
        return;
    }

    AppRuntimeState& runtimeState = *input.runtimeState;
    EffectRuntime& effectRuntime = *input.effectRuntime;
    const AppVfxDebugDataBuilderContext& debugDataContext = *input.debugDataContext;
    const EffectRuntimeFrame runtimeFrame = effectRuntime.BuildFrame();
    UpdateParticleRuntimeStatusTelemetry(runtimeState, debugDataContext, input);
    UpdateDistortionRuntimeStatusTelemetry(runtimeState, debugDataContext, input);
    UpdateBeamRuntimeStatusTelemetry(runtimeState, debugDataContext, input);
    UpdateTrailRuntimeStatusTelemetry(runtimeState, runtimeFrame, debugDataContext, input);
}

void DrawVfxRuntimeStatusPanel(
    const AppVfxRuntimeStatusPanelInput& input) {
    if (input.runtimeState == nullptr || input.effectRuntime == nullptr ||
        input.debugDataContext == nullptr || !HasRequiredCounters(input)) {
        return;
    }

    AppRuntimeState& runtimeState = *input.runtimeState;
    EffectRuntime& effectRuntime = *input.effectRuntime;
    const AppVfxDebugDataBuilderContext& debugDataContext = *input.debugDataContext;
    const EffectRuntimeFrame runtimeFrame = effectRuntime.BuildFrame();
    DrawParticleRuntimeStatusSection(runtimeState, debugDataContext, runtimeFrame, input);
    DrawDistortionRuntimeStatusSection(runtimeState, debugDataContext, runtimeFrame, input);
    DrawBeamRuntimeStatusSection(runtimeState, debugDataContext, runtimeFrame, input);
    DrawTrailRuntimeStatusSection(runtimeState, runtimeFrame, debugDataContext, input);
}
