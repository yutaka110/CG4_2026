#include "AppVfxRuntimeQueuesPanel.h"

#include "AppRuntimeState.h"
#include "AppVfxTelemetry.h"
#include "AppVfxTelemetryPanel.h"
#include "EffectRuntime.h"

#include "../../externals/imgui/imgui.h"

#include <string>

namespace {
const char* StringOrNone(const std::string& value) {
    return !value.empty() ? value.c_str() : "none";
}

void DrawRuntimeQueueRendererLine(
    const char* label,
    const EffectRuntimeQueueAuthoringStatus& status) {
    ImGui::Text(
        "%s primaryRendererId=%s rendererRegistry=%s rendererDescriptorId=%s rendererCompatMirror=%d techniqueId=%s simulationId=%s simulationRegistry=%s simulationDescriptorId=%s simulationCompatMirror=%d queue=%u",
        label,
        status.hasPrimary ? StringOrNone(status.rendererId) : "none",
        status.hasPrimary && status.rendererResolved ? "resolved" : "unregistered",
        status.hasPrimary ? StringOrNone(status.rendererDescriptorId) : "none",
        status.hasPrimary ? static_cast<int>(status.rendererType) : -1,
        status.hasPrimary ? StringOrNone(status.techniqueId) : "none",
        status.hasPrimary ? StringOrNone(status.simulationId) : "none",
        status.hasPrimary && status.simulationResolved ? "resolved" : "unregistered",
        status.hasPrimary ? StringOrNone(status.simulationDescriptorId) : "none",
        status.hasPrimary ? static_cast<int>(status.simulationType) : -1,
        status.renderQueue);
}

void DrawRuntimeQueueRendererSummary(const EffectRuntimeFrame& runtimeFrame) {
    DrawRuntimeQueueRendererLine("particleQueue", runtimeFrame.authoring.particle);
    DrawRuntimeQueueRendererLine("trailQueue", runtimeFrame.authoring.trail);
    DrawRuntimeQueueRendererLine("beamQueue", runtimeFrame.authoring.beam);
    DrawRuntimeQueueRendererLine("distortionQueue", runtimeFrame.authoring.distortion);
}
} // namespace

void DrawRuntimeQueuesSummaryPanel(
    const RuntimeQueuesSummaryPanelInput& input) {
    ImGui::Text(
        "active effects=%u active components=%u",
        input.activeInstanceCount,
        input.activeComponentCount);
    ImGui::Text(
        "particles=%u trails=%u beams=%u distortion=%u",
        input.particleQueueSize,
        input.trailQueueSize,
        input.beamQueueSize,
        input.distortionQueueSize);
}

void DrawParticleRendererInspectorStatusPanel(
    const ParticleRendererInspectorStatusPanelInput& input) {
    if (input.status == nullptr || input.graphIntent == nullptr) {
        return;
    }

    const vfx::ParticleRendererOperationalStatus& status = *input.status;
    const ParticleRenderGraphIntentTelemetry& graphIntent = *input.graphIntent;
    ImGui::TextColored(
        status.operationalOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "particleOperational=%s resourceHealth=%s intent=%s handles=%s fallbackReason=%s",
        status.operationalOk ? "ok" : "pending",
        status.resourceHealthy ? "healthy" : "attention",
        status.resourceIntentReady ? "ready" : "attention",
        status.resourceHandlesReady ? "ready" : "attention",
        status.fallbackReason);
    ImGui::TextColored(
        graphIntent.ready ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "particleGraphIntent=%s simPass=%s drawPass=%s fallbackReason=%s",
        graphIntent.ready ? "ready" : "attention",
        graphIntent.simulationPassExecuted ? "executed" : (graphIntent.simulationPassFound ? "culled" : "missing"),
        graphIntent.drawPassExecuted ? "executed" : (graphIntent.drawPassFound ? "culled" : "missing"),
        graphIntent.fallbackReason.c_str());
}

void DrawParticleRendererInspectorDetailsPanel(
    const ParticleRendererInspectorDetailsPanelInput& input) {
    if (input.debugData == nullptr) {
        return;
    }

    const ParticleRendererInspectorDebugData& debugData = *input.debugData;
    const vfx::ParticleRendererOperationalStatus& status = debugData.status;
    ImGui::Text("intentMode=%s intentReason=%s",
        status.resourceIntentMode,
        status.resourceIntentFallbackReason);
    ImGui::Text("handleReason=%s", status.resourceHandleFallbackReason);
    ImGui::Text("simulation state=%s renderBuffer=%s indirectArgs=%s",
        status.simulationStateResource,
        status.simulationRenderBufferResource,
        status.simulationIndirectArgsResource);
    ImGui::Text("renderer renderBuffer=%s indirectArgs=%s queue=%u activeInput=%s",
        status.renderBufferResource,
        status.indirectArgsResource,
        input.particleQueueSize,
        input.particleQueueEmpty ? "no" : "yes");
    ImGui::Text("usesIndirectSprite=%s simulationCompute=%s",
        debugData.resources.particle.renderer.usesIndirectSprite ? "yes" : "no",
        debugData.resources.particle.simulation.usesCompute ? "yes" : "no");
    DrawParticleRenderGraphIntentPanel(
        ParticleRenderGraphIntentPanelInput{
            &debugData.status,
            &debugData.graphIntent,
            &debugData.resources.particle});
}

void DrawBeamRendererInspectorPanel(
    const BeamRendererInspectorPanelInput& input) {
    if (input.debugData == nullptr) {
        return;
    }

    const BeamRendererInspectorDebugData& debugData = *input.debugData;
    const BeamDedicatedReadbackValidationSummary readbackSummary =
        BuildBeamDedicatedReadbackValidationSummary(
            debugData.readbackTelemetry,
            debugData.expectedDrawCount);
    const BeamDedicatedOperationalHealthSummary health =
        BuildBeamDedicatedOperationalHealthSummary(
            debugData.status,
            debugData.graphIntent,
            readbackSummary);
    ImGui::TextColored(
        health.healthy ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "BeamDedicatedOperational=%s graph=%s handles=%s readback=%s validation=%s fallbackReason=%s",
        health.healthy ? "healthy" : "attention",
        debugData.graphIntent.ready ? "ready" : "attention",
        debugData.status.resourceHandlesReady ? "ready" : "attention",
        health.readbackReady ? "valid" : "waiting",
        health.validationOk ? "ok" : "attention",
        health.fallbackReason.c_str());
    if (!ImGui::TreeNodeEx("Beam Renderer")) {
        return;
    }

    ImGui::Text("queue=%u activeInput=%s", input.beamQueueSize, input.beamQueueEmpty ? "no" : "yes");
    ImGui::Text("graph simulationPass=%s drawPass=%s reason=%s",
        debugData.graphIntent.simulationPassExecuted
            ? "executed"
            : (debugData.graphIntent.simulationPassFound ? "culled" : "missing"),
        debugData.graphIntent.drawPassExecuted
            ? "executed"
            : (debugData.graphIntent.drawPassFound ? "culled" : "missing"),
        debugData.graphIntent.fallbackReason.c_str());
    ImGui::Text("handles renderSrv=%s renderUav=%s indirectArgs=%s reason=%s",
        debugData.status.renderBufferSrvValid ? "ready" : "missing",
        debugData.status.renderBufferUavValid ? "ready" : "missing",
        debugData.status.indirectArgsValid ? "ready" : "missing",
        debugData.status.resourceHandleFallbackReason);
    ImGui::Text("drawPass=%s depthTarget=%s",
        debugData.resources.beam.routing.drawPass,
        debugData.resources.beam.routing.depthTarget);
    ImGui::Text("renderBuffer=%s indirectArgs=%s usesIndirectSprite=%s",
        debugData.resources.beam.renderer.renderBuffer,
        debugData.resources.beam.renderer.indirectArgs,
        debugData.resources.beam.renderer.usesIndirectSprite ? "yes" : "no");
    ImGui::Text("readback renderSamples=%u renderBufferValidation=%s drawArgsReadback=%s drawArgsValidation=%s",
        health.renderSamples,
        health.renderBufferValidationOk ? "ok" : "attention",
        health.drawArgsReadbackValid ? "valid" : "waiting",
        health.drawArgsValidationOk ? "ok" : "attention");
    ImGui::Text("drawArgs expectedDrawCount=%u actualDrawCount=%u expectedIndexCount=%u actualIndexCount=%u",
        health.expectedDrawCount,
        health.actualDrawCount,
        health.expectedIndexCount,
        health.actualIndexCount);
    ImGui::Text("readbackFailure=%s drawArgsFailure=%s",
        health.readbackFailureReason.c_str(),
        health.drawArgsFailureReason.c_str());
    ImGui::TreePop();
}

void DrawTrailMeshStreamRuntimeStatusPanel(
    const TrailMeshStreamRuntimeStatusPanelInput& input) {
    if (input.debugData == nullptr || input.telemetry == nullptr || input.runtimeState == nullptr) {
        return;
    }

    const TrailMeshStreamRuntimeDebugData& debugData = *input.debugData;
    const TrailMeshStreamRuntimeTelemetryUpdateResult& telemetry = *input.telemetry;
    DrawTrailMeshStreamHealthLine(
        "trailMeshStreamToggleHealth",
        debugData.activeDrawStatus,
        debugData.activeSummary,
        debugData.telemetryValid);
    DrawTrailMeshStreamHealthLine(
        "trailMeshStreamProbeOnHealth",
        debugData.probeDrawStatus,
        debugData.probeSummary,
        debugData.telemetryValid);
    ImGui::TextColored(
        telemetry.readyToEnable ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "trailMeshStreamReadyToEnable=%s stableFrames=%u/%u",
        telemetry.readyToEnable ? "yes" : "no",
        input.probeStableFrames,
        telemetry.readyToEnableFrames);
    ImGui::TextColored(
        telemetry.activeStable ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "trailMeshStreamActiveStable=%s activeStableFrames=%u/%u",
        telemetry.activeStable ? "yes" : "no",
        input.activeStableFrames,
        telemetry.activeStableRequiredFrames);
    ImGui::TextColored(
        telemetry.defaultOnCandidate ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "trailMeshStreamDefaultONCandidate=%s",
        telemetry.defaultOnCandidate ? "yes" : "no");
    ImGui::TextColored(
        telemetry.operationalOk
            ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f)
            : (telemetry.warmup || telemetry.defaultOnHealth == "ramping"
                ? ImVec4(0.72f, 0.74f, 0.78f, 1.0f)
                : ImVec4(1.0f, 0.55f, 0.35f, 1.0f)),
        "trailMeshStreamDefaultONHealth=%s",
        telemetry.defaultOnHealth.c_str());
    ImGui::TextColored(
        telemetry.operationalOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "trailMeshStreamOperational=%s",
        telemetry.operationalOk ? "ok" : "pending");
    ImGui::TextColored(
        input.runtimeState->trailMeshStreamFallbackActive ? ImVec4(1.0f, 0.55f, 0.35f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "trailMeshStreamSafetyFallback=%s effectiveDedicated=%s warmup=%s/%u",
        input.runtimeState->trailMeshStreamFallbackActive ? "active" : "off",
        telemetry.effectiveDedicatedAfterSafety ? "yes" : "no",
        telemetry.warmup ? "yes" : "no",
        telemetry.warmupFrames);
}

void DrawTrailMeshStreamInspectorPanel(
    const TrailMeshStreamInspectorPanelInput& input) {
    if (input.debugData == nullptr || input.telemetry == nullptr) {
        return;
    }

    const TrailMeshStreamInspectorDebugData& debugData = *input.debugData;
    const TrailMeshStreamInspectorTelemetryResult& telemetry = *input.telemetry;
    DrawTrailMeshStreamHealthLine(
        "trailMeshStream",
        debugData.drawStatus,
        debugData.summary,
        debugData.telemetryValid);
    if (!ImGui::TreeNodeEx("Trail Mesh Stream", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    DrawTrailMeshStreamTelemetryPanel(
        TrailMeshStreamTelemetryPanelInput{
            &debugData.plan,
            &debugData.drawStatus,
            debugData.telemetry,
            debugData.telemetryValid,
            telemetry.operationalOk,
            telemetry.defaultOnHealth.c_str(),
            input.activeStableFrames,
            telemetry.activeStableRequiredFrames,
            input.probeStableFrames,
            telemetry.readyToEnableFrames,
            debugData.summary.okRows,
            debugData.summary.ngRows,
            debugData.summary.firstFailureSegment,
            debugData.summary.firstFailureReason.c_str()});
    if (input.trailInput != nullptr && input.trailInput->settings != nullptr) {
        ImGui::Text("miterLimit=%.2f", input.trailInput->settings->miterLimit);
    }
    ImGui::Text("history=%u sampleDistance=%.3f smoothing=%.2f previewPoints=%u",
        debugData.debugPreview.historyCount,
        debugData.debugPreview.sampleDistance,
        debugData.debugPreview.smoothing,
        static_cast<unsigned int>(debugData.debugPreview.points.size()));
    DrawTrailMeshStreamVisualDebugPanel(debugData.debugPreview);
    if (debugData.plan.resources != nullptr) {
        ImGui::Text("control=%s", debugData.plan.resources->controlPointBuffer);
        ImGui::Text("segment=%s", debugData.plan.resources->segmentBuffer);
        ImGui::Text("vertex=%s", debugData.plan.resources->vertexBuffer);
        ImGui::Text("index=%s", debugData.plan.resources->indexBuffer);
        ImGui::Text("drawArgs=%s", debugData.plan.resources->drawArgs);
    }
    ImGui::TreePop();
}

namespace {
void DrawParticleRuntimeQueueInspectorSection(
    AppRuntimeState& runtimeState,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const EffectRuntimeFrame& runtimeFrame,
    const VfxRuntimeQueuesPanelInput& input) {
    const ParticleRenderFallback particleFallback = runtimeFrame.PrimaryParticleFallback();
    const ParticleRenderInput particleInput = runtimeFrame.ParticleInput(particleFallback);
    ParticleRendererInspectorDebugData particleInspectorData{};
    BuildParticleRendererInspectorDebugData(
        debugDataContext,
        runtimeState.vfx.enableParticleDedicatedResourceProbe,
        particleInspectorData);
    DrawParticleRendererInspectorStatusPanel(
        ParticleRendererInspectorStatusPanelInput{
            &particleInspectorData.status,
            &particleInspectorData.graphIntent});
    if (ImGui::TreeNodeEx("Particle Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawParticleRendererInspectorDetailsPanel(
            ParticleRendererInspectorDetailsPanelInput{
                &particleInspectorData,
                static_cast<uint32_t>(runtimeFrame.particleQueue.size()),
                runtimeFrame.particleQueue.empty()});
        if (particleInspectorData.probeEnabled) {
            const ParticleDedicatedOperationalHealthSummary& probeOperationalHealth =
                runtimeState.vfx.particleDedicatedHealth;
            const ParticleDedicatedResourceProbeTelemetryResult probeTelemetry =
                BuildParticleDedicatedResourceProbeTelemetry(
                    ParticleDedicatedResourceProbeTelemetryInput{
                        &runtimeState.vfx,
                        &particleInspectorData.probeStatus,
                        &particleInspectorData.probeGraphIntent,
                        particleInspectorData.simulationTelemetry,
                        particleInspectorData.readbackTelemetry,
                        input.particleDedicatedProbeTelemetryFrames});

            DrawParticleDedicatedResourceProbePanel(
                ParticleDedicatedResourceProbePanelInput{
                    &particleInput.primary,
                    &probeOperationalHealth,
                    &particleInspectorData.probeStatus,
                    &particleInspectorData.probeGraphIntent,
                    &particleInspectorData.probeResources.particle,
                    particleInspectorData.simulationTelemetry,
                    particleInspectorData.readbackTelemetry,
                    &probeTelemetry.readbackSummary,
                    &runtimeState.vfx});
        }
        ImGui::TreePop();
    }
}

void DrawTrailRuntimeQueueInspectorSection(
    AppRuntimeState& runtimeState,
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const EffectRuntimeFrame& runtimeFrame,
    const VfxRuntimeQueuesPanelInput& input) {
    const TrailRenderInput trailInput = runtimeFrame.TrailInput();
    TrailMeshStreamInspectorDebugData trailInspectorData{};
    BuildTrailMeshStreamInspectorDebugData(
        debugDataContext,
        trailInput,
        runtimeState.cameraWorldPosition,
        runtimeState.vfx.enableTrailMeshStream && !runtimeState.vfx.trailMeshStreamFallbackActive,
        trailInspectorData);
    const TrailMeshStreamInspectorTelemetryResult inspectorTelemetry =
        BuildTrailMeshStreamInspectorTelemetry(
            TrailMeshStreamInspectorTelemetryInput{
                &runtimeState.vfx,
                &trailInspectorData.drawStatus,
                &trailInspectorData.probeDrawStatus,
                &trailInspectorData.summary,
                trailInspectorData.telemetryValid,
                input.trailMeshStreamHealthFrames,
                input.trailMeshStreamProbeHealthyFrames,
                input.trailMeshStreamActiveHealthyFrames});
    DrawTrailMeshStreamInspectorPanel(
        TrailMeshStreamInspectorPanelInput{
            &trailInspectorData,
            &inspectorTelemetry,
            &trailInput,
            input.trailMeshStreamActiveHealthyFrames,
            input.trailMeshStreamProbeHealthyFrames});
}

void DrawBeamRuntimeQueueInspectorSection(
    const AppVfxDebugDataBuilderContext& debugDataContext,
    const EffectRuntimeFrame& runtimeFrame) {
    BeamRendererInspectorDebugData beamInspectorData{};
    BuildBeamRendererInspectorDebugData(debugDataContext, beamInspectorData);
    DrawBeamRendererInspectorPanel(
        BeamRendererInspectorPanelInput{
            &beamInspectorData,
            static_cast<uint32_t>(runtimeFrame.beamQueue.size()),
            runtimeFrame.beamQueue.empty()});
}
} // namespace

void DrawVfxRuntimeQueuesPanel(
    const VfxRuntimeQueuesPanelInput& input) {
    if (input.runtimeState == nullptr || input.effectRuntime == nullptr ||
        input.debugDataContext == nullptr || input.particleDedicatedProbeTelemetryFrames == nullptr) {
        return;
    }

    AppRuntimeState& runtimeState = *input.runtimeState;
    EffectRuntime& effectRuntime = *input.effectRuntime;
    const EffectRuntimeFrame runtimeFrame = effectRuntime.BuildFrame();
    DrawRuntimeQueuesSummaryPanel(
        RuntimeQueuesSummaryPanelInput{
            runtimeFrame.activeInstanceCount,
            runtimeFrame.activeComponentCount,
            static_cast<uint32_t>(runtimeFrame.particleQueue.size()),
            static_cast<uint32_t>(runtimeFrame.trailQueue.size()),
            static_cast<uint32_t>(runtimeFrame.beamQueue.size()),
            static_cast<uint32_t>(runtimeFrame.distortionQueue.size())});
    DrawRuntimeQueueRendererSummary(runtimeFrame);

    const AppVfxDebugDataBuilderContext& debugDataContext = *input.debugDataContext;
    DrawParticleRuntimeQueueInspectorSection(runtimeState, debugDataContext, runtimeFrame, input);
    DrawTrailRuntimeQueueInspectorSection(runtimeState, debugDataContext, runtimeFrame, input);
    DrawBeamRuntimeQueueInspectorSection(debugDataContext, runtimeFrame);
}
