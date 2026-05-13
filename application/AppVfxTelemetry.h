#pragma once

#include "AppGpuParticleSystem.h"
#include "AppVfxRuntimeState.h"
#include "graphics/RenderGraph.h"
#include "vfx/BeamRenderer.h"
#include "vfx/DistortionRenderer.h"
#include "vfx/ParticleRenderer.h"
#include "vfx/TrailRenderer.h"
#include "vfx/VfxResources.h"

#include <cstdint>
#include <string>
#include <vector>

struct ParticleRenderGraphIntentTelemetry {
    bool simulationPassFound = false;
    bool simulationPassExecuted = false;
    bool simulationStateRead = false;
    bool simulationStateWrite = false;
    bool simulationRenderBufferWrite = false;
    bool drawPassFound = false;
    bool drawPassExecuted = false;
    bool drawRenderBufferRead = false;
    bool drawIndirectArgsRead = false;
    bool ready = false;
    std::string fallbackReason = "unknown";
};

struct DistortionRenderGraphIntentTelemetry {
    bool simulationPassFound = false;
    bool simulationPassExecuted = false;
    bool simulationStateRead = false;
    bool simulationStateWrite = false;
    bool simulationRenderBufferWrite = false;
    bool drawPassFound = false;
    bool drawPassExecuted = false;
    bool drawRenderBufferRead = false;
    bool drawIndirectArgsRead = false;
    bool ready = false;
    std::string fallbackReason = "unknown";
};

struct BeamRenderGraphIntentTelemetry {
    bool simulationPassFound = false;
    bool simulationPassExecuted = false;
    bool simulationStateRead = false;
    bool simulationStateWrite = false;
    bool simulationRenderBufferWrite = false;
    bool drawPassFound = false;
    bool drawPassExecuted = false;
    bool drawRenderBufferRead = false;
    bool drawIndirectArgsRead = false;
    bool ready = false;
    std::string fallbackReason = "unknown";
};

struct ParticleDedicatedReadbackRowValidation {
    bool hasState = false;
    bool hasRender = false;
    bool finiteState = false;
    bool finiteRender = false;
    bool lifetimeOk = false;
    bool ageOk = false;
    bool alphaOk = false;
    float positionDelta = 0.0f;
    bool positionMatches = false;
    bool rowOk = false;
};

struct ParticleDedicatedReadbackValidationSummary {
    bool telemetryValid = false;
    bool drawArgsReadbackValid = false;
    bool drawArgsValidationOk = false;
    uint32_t okRows = 0;
    uint32_t ngRows = 0;
    uint32_t sampleCount = 0;
    uint32_t expectedDrawCount = 0;
    uint32_t actualDrawCount = 0;
    uint32_t expectedIndexCount = 0;
    uint32_t actualIndexCount = 0;
    uint32_t firstFailureSample = UINT32_MAX;
    float maxPositionDelta = 0.0f;
    std::string firstFailureReason = "telemetry waiting";
    std::string drawArgsFailureReason = "draw args waiting";

    bool AllRowsOk() const {
        return telemetryValid && sampleCount > 0 && ngRows == 0;
    }
};

struct DistortionDedicatedReadbackValidationSummary {
    bool telemetryValid = false;
    bool drawArgsReadbackValid = false;
    bool renderBufferValidationOk = false;
    bool drawArgsValidationOk = false;
    uint32_t renderSamples = 0;
    uint32_t expectedDrawCount = 0;
    uint32_t actualDrawCount = 0;
    uint32_t expectedIndexCount = 0;
    uint32_t actualIndexCount = 0;
    std::string readbackFailureReason = "readback waiting";
    std::string drawArgsFailureReason = "draw args waiting";
};

struct BeamDedicatedReadbackValidationSummary {
    bool telemetryValid = false;
    bool drawArgsReadbackValid = false;
    bool renderBufferValidationOk = false;
    bool drawArgsValidationOk = false;
    uint32_t renderSamples = 0;
    uint32_t expectedDrawCount = 0;
    uint32_t actualDrawCount = 0;
    uint32_t expectedIndexCount = 0;
    uint32_t actualIndexCount = 0;
    std::string readbackFailureReason = "readback waiting";
    std::string drawArgsFailureReason = "draw args waiting";
};

struct TrailMeshStreamRowValidation {
    uint32_t headLeftVertexIndex = 0;
    uint32_t headRightVertexIndex = 0;
    uint32_t tailLeftVertexIndex = 0;
    uint32_t tailRightVertexIndex = 0;
    uint32_t indexBase = 0;
    bool hasHeadCp = false;
    bool hasTailCp = false;
    bool hasHeadLeftVertex = false;
    bool hasHeadRightVertex = false;
    bool hasTailLeftVertex = false;
    bool hasTailRightVertex = false;
    bool hasIndexSix = false;
    bool hasHeadChain = false;
    bool hasTailChain = false;
    float expectedHeadT = 0.0f;
    float expectedTailT = 0.0f;
    float expectedHeadAlpha = 0.0f;
    float expectedTailAlpha = 0.0f;
    float expectedHeadWidth = 0.0f;
    float expectedTailWidth = 0.0f;
    float headLeftTDelta = 0.0f;
    float headRightTDelta = 0.0f;
    float tailLeftTDelta = 0.0f;
    float tailRightTDelta = 0.0f;
    float headLeftAlphaDelta = 0.0f;
    float headRightAlphaDelta = 0.0f;
    float tailLeftAlphaDelta = 0.0f;
    float tailRightAlphaDelta = 0.0f;
    float headLeftWidthDelta = 0.0f;
    float headRightWidthDelta = 0.0f;
    float tailLeftWidthDelta = 0.0f;
    float tailRightWidthDelta = 0.0f;
    bool cpRangeOk = false;
    bool tOk = false;
    bool sideUvOk = false;
    bool alphaOk = false;
    bool widthOk = false;
    bool argsOk = false;
    bool indicesOk = false;
    bool rowOk = false;
};

struct TrailMeshStreamTelemetrySummary {
    uint32_t okRows = 0;
    uint32_t ngRows = 0;
    uint32_t firstFailureSegment = UINT32_MAX;
    std::string firstFailureReason = "none";

    bool AllRowsOk() const {
        return ngRows == 0 && firstFailureReason == "none";
    }
};

struct ParticleDedicatedRuntimeTelemetryUpdateInput {
    AppVfxRuntimeState* runtimeState = nullptr;
    const vfx::ParticleRendererOperationalStatus* status = nullptr;
    const ParticleRenderGraphIntentTelemetry* graphIntent = nullptr;
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry = nullptr;
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
    uint32_t* healthFrames = nullptr;
    uint32_t* probeStableFrames = nullptr;
    uint32_t* activeStableFrames = nullptr;
};

struct ParticleDedicatedRuntimeTelemetryUpdateResult {
    ParticleDedicatedReadbackValidationSummary readbackSummary{};
    bool readyToEnable = false;
    bool activeStable = false;
    bool warmup = false;
    uint32_t warmupFrames = 10;
    uint32_t readyToEnableFrames = 60;
    uint32_t activeStableRequiredFrames = 60;
};

struct DistortionDedicatedRuntimeTelemetryUpdateInput {
    AppVfxRuntimeState* runtimeState = nullptr;
    const vfx::DistortionRendererOperationalStatus* status = nullptr;
    const DistortionRenderGraphIntentTelemetry* graphIntent = nullptr;
    const AppGpuParticleSystem::DistortionDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
    uint32_t expectedDrawCount = 0;
    uint32_t* healthFrames = nullptr;
    uint32_t* stableFrames = nullptr;
    uint32_t* activeStableFrames = nullptr;
    uint32_t* telemetryFrames = nullptr;
};

struct DistortionDedicatedRuntimeTelemetryUpdateResult {
    DistortionDedicatedReadbackValidationSummary readbackSummary{};
    bool stableEnough = false;
    bool activeStable = false;
    bool warmup = false;
    uint32_t warmupFrames = 10;
    uint32_t stableRequiredFrames = 60;
    uint32_t activeStableRequiredFrames = 60;
    uint32_t telemetryFrameLimit = 300;
};

struct BeamDedicatedRuntimeTelemetryUpdateInput {
    AppVfxRuntimeState* runtimeState = nullptr;
    const vfx::BeamRendererOperationalStatus* status = nullptr;
    const BeamRenderGraphIntentTelemetry* graphIntent = nullptr;
    const AppGpuParticleSystem::BeamDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
    uint32_t expectedDrawCount = 0;
    uint32_t* healthFrames = nullptr;
    uint32_t* stableFrames = nullptr;
    uint32_t* activeStableFrames = nullptr;
    uint32_t* telemetryFrames = nullptr;
};

struct BeamDedicatedRuntimeTelemetryUpdateResult {
    BeamDedicatedReadbackValidationSummary readbackSummary{};
    bool stableEnough = false;
    bool activeStable = false;
    bool warmup = false;
    bool effectiveDedicatedAfterSafety = false;
    uint32_t warmupFrames = 10;
    uint32_t stableRequiredFrames = 60;
    uint32_t activeStableRequiredFrames = 60;
    uint32_t telemetryFrameLimit = 300;
};

struct ParticleDedicatedResourceProbeTelemetryInput {
    const AppVfxRuntimeState* runtimeState = nullptr;
    const vfx::ParticleRendererOperationalStatus* status = nullptr;
    const ParticleRenderGraphIntentTelemetry* graphIntent = nullptr;
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry = nullptr;
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry* readbackTelemetry = nullptr;
    uint32_t* telemetryFrames = nullptr;
};

struct ParticleDedicatedResourceProbeTelemetryResult {
    ParticleDedicatedReadbackValidationSummary readbackSummary{};
    uint32_t telemetryFrameLimit = 300;
};

struct TrailMeshStreamRuntimeTelemetryUpdateInput {
    AppVfxRuntimeState* runtimeState = nullptr;
    const vfx::TrailMeshStreamDrawStatus* activeDrawStatus = nullptr;
    const TrailMeshStreamTelemetrySummary* activeSummary = nullptr;
    const vfx::TrailMeshStreamDrawStatus* probeDrawStatus = nullptr;
    const TrailMeshStreamTelemetrySummary* probeSummary = nullptr;
    bool telemetryValid = false;
    uint32_t* healthFrames = nullptr;
    uint32_t* probeHealthyFrames = nullptr;
    uint32_t* activeHealthyFrames = nullptr;
    uint32_t* startupTelemetryFrames = nullptr;
};

struct TrailMeshStreamRuntimeTelemetryUpdateResult {
    bool warmup = false;
    bool activeHealthyRaw = false;
    bool activeHealthy = false;
    bool probeHealthyRaw = false;
    bool probeHealthy = false;
    bool readyToEnable = false;
    bool activeStable = false;
    bool defaultOnCandidate = false;
    bool effectiveDedicatedAfterSafety = false;
    bool operationalOk = false;
    std::string defaultOnHealth = "unknown";
    uint32_t warmupFrames = 10;
    uint32_t readyToEnableFrames = 60;
    uint32_t activeStableRequiredFrames = 60;
    uint32_t startupTelemetryFrameLimit = 300;
};

struct TrailMeshStreamInspectorTelemetryInput {
    const AppVfxRuntimeState* runtimeState = nullptr;
    const vfx::TrailMeshStreamDrawStatus* activeDrawStatus = nullptr;
    const vfx::TrailMeshStreamDrawStatus* probeDrawStatus = nullptr;
    const TrailMeshStreamTelemetrySummary* summary = nullptr;
    bool telemetryValid = false;
    uint32_t healthFrames = 0;
    uint32_t probeStableFrames = 0;
    uint32_t activeStableFrames = 0;
};

struct TrailMeshStreamInspectorTelemetryResult {
    bool operationalOk = false;
    std::string defaultOnHealth = "unknown";
    uint32_t readyToEnableFrames = 60;
    uint32_t activeStableRequiredFrames = 60;
};

ParticleRenderGraphIntentTelemetry BuildParticleRenderGraphIntentTelemetry(
    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes,
    const vfx::ParticleVfxResourceSet& resources);

DistortionRenderGraphIntentTelemetry BuildDistortionRenderGraphIntentTelemetry(
    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes,
    const vfx::DistortionVfxResourceSet& resources);

BeamRenderGraphIntentTelemetry BuildBeamRenderGraphIntentTelemetry(
    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes,
    const vfx::BeamVfxResourceSet& resources);

const char* ParticleDedicatedDefaultOnHealthLabel(
    bool requested,
    bool warmup,
    bool effectiveDedicated,
    bool readyToEnable,
    bool activeStable,
    bool healthOk);

const char* DistortionDedicatedDefaultOnHealthLabel(
    bool requested,
    bool warmup,
    bool effectiveDedicated,
    bool readyToEnable,
    bool activeStable,
    bool healthOk);

const char* BeamDedicatedDefaultOnHealthLabel(
    bool warmup,
    bool readyToEnable,
    bool activeStable,
    bool healthOk);

bool IsParticleDedicatedOperationalOk(const char* defaultOnHealth);

const char* ParticleSimulationPathLabel(const ParticleDedicatedOperationalHealthSummary& health);
const char* ParticleRenderPathLabel(const ParticleDedicatedOperationalHealthSummary& health);
const char* ParticleReadbackLabel(const ParticleDedicatedOperationalHealthSummary& health);
const char* ParticleValidationLabel(const ParticleDedicatedOperationalHealthSummary& health);
const char* DistortionReadbackLabel(const DistortionDedicatedOperationalHealthSummary& health);
const char* DistortionValidationLabel(const DistortionDedicatedOperationalHealthSummary& health);

ParticleDedicatedReadbackRowValidation ValidateParticleDedicatedReadbackRow(
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry& telemetry,
    uint32_t sampleIndex);

TrailMeshStreamRowValidation ValidateTrailMeshStreamTelemetryRow(
    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry,
    const vfx::TrailMeshStreamPlan& trailPlan,
    uint32_t segmentIndex);

TrailMeshStreamTelemetrySummary BuildTrailMeshStreamTelemetrySummary(
    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry,
    const vfx::TrailMeshStreamPlan& trailPlan);

ParticleDedicatedReadbackValidationSummary BuildParticleDedicatedReadbackValidationSummary(
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry* telemetry,
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry);

DistortionDedicatedReadbackValidationSummary BuildDistortionDedicatedReadbackValidationSummary(
    const AppGpuParticleSystem::DistortionDedicatedReadbackTelemetry* telemetry,
    uint32_t expectedDrawCount);

BeamDedicatedReadbackValidationSummary BuildBeamDedicatedReadbackValidationSummary(
    const AppGpuParticleSystem::BeamDedicatedReadbackTelemetry* telemetry,
    uint32_t expectedDrawCount);

ParticleDedicatedOperationalHealthSummary BuildParticleDedicatedOperationalHealthSummary(
    bool probeEnabled,
    const vfx::ParticleRendererOperationalStatus& probeStatus,
    const ParticleRenderGraphIntentTelemetry& graphIntent,
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry,
    const ParticleDedicatedReadbackValidationSummary& readbackSummary);

DistortionDedicatedOperationalHealthSummary BuildDistortionDedicatedOperationalHealthSummary(
    bool enabled,
    const vfx::DistortionRendererOperationalStatus& status,
    const DistortionRenderGraphIntentTelemetry& graphIntent,
    const DistortionDedicatedReadbackValidationSummary& readbackSummary);

BeamDedicatedOperationalHealthSummary BuildBeamDedicatedOperationalHealthSummary(
    const vfx::BeamRendererOperationalStatus& status,
    const BeamRenderGraphIntentTelemetry& graphIntent,
    const BeamDedicatedReadbackValidationSummary& readbackSummary);

ParticleDedicatedRuntimeTelemetryUpdateResult UpdateParticleDedicatedRuntimeTelemetry(
    const ParticleDedicatedRuntimeTelemetryUpdateInput& input);

DistortionDedicatedRuntimeTelemetryUpdateResult UpdateDistortionDedicatedRuntimeTelemetry(
    const DistortionDedicatedRuntimeTelemetryUpdateInput& input);

BeamDedicatedRuntimeTelemetryUpdateResult UpdateBeamDedicatedRuntimeTelemetry(
    const BeamDedicatedRuntimeTelemetryUpdateInput& input);

ParticleDedicatedResourceProbeTelemetryResult BuildParticleDedicatedResourceProbeTelemetry(
    const ParticleDedicatedResourceProbeTelemetryInput& input);

TrailMeshStreamRuntimeTelemetryUpdateResult UpdateTrailMeshStreamRuntimeTelemetry(
    const TrailMeshStreamRuntimeTelemetryUpdateInput& input);

TrailMeshStreamInspectorTelemetryResult BuildTrailMeshStreamInspectorTelemetry(
    const TrailMeshStreamInspectorTelemetryInput& input);

void DrawTrailMeshStreamHealthLine(
    const char* label,
    const vfx::TrailMeshStreamDrawStatus& drawStatus,
    const TrailMeshStreamTelemetrySummary& telemetrySummary,
    bool telemetryValid);

bool IsTrailMeshStreamHealthy(
    const vfx::TrailMeshStreamDrawStatus& drawStatus,
    const TrailMeshStreamTelemetrySummary& telemetrySummary,
    bool telemetryValid);

const char* TrailMeshStreamDefaultOnHealthLabel(
    bool requested,
    bool warmup,
    bool effectiveDedicated,
    bool readyToEnable,
    bool activeStable,
    bool toggleHealthy,
    bool probeHealthy);

bool IsTrailMeshStreamOperationalOk(const char* defaultOnHealth);

void WriteTrailMeshStreamStartupTelemetry(
    uint32_t frameIndex,
    const AppVfxRuntimeState& runtimeState,
    bool effectiveDedicated,
    uint32_t activeStableFrames,
    uint32_t probeStableFrames,
    bool toggleHealthy,
    bool probeHealthy,
    bool warmup,
    const char* defaultOnHealth,
    bool operationalOk,
    const TrailMeshStreamTelemetrySummary& toggleSummary,
    const TrailMeshStreamTelemetrySummary& probeSummary,
    bool resetFile);

void DrawParticleDedicatedOperationalHealthLine(
    const char* label,
    const ParticleDedicatedOperationalHealthSummary& health);

void WriteParticleDedicatedProbeTelemetry(
    uint32_t frameIndex,
    const vfx::ParticleRendererOperationalStatus& probeStatus,
    const ParticleRenderGraphIntentTelemetry& graphIntent,
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry,
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry* readbackTelemetry,
    const ParticleDedicatedOperationalHealthSummary& operationalHealth,
    bool autoFallbackEnabled,
    bool fallbackActive,
    uint32_t probeStableFrames,
    uint32_t activeStableFrames,
    bool defaultOnCandidate,
    const char* defaultOnHealth,
    bool resetFile);

void WriteDistortionDedicatedTelemetry(
    uint32_t frameIndex,
    const vfx::DistortionRendererOperationalStatus& status,
    const DistortionRenderGraphIntentTelemetry& graphIntent,
    const DistortionDedicatedOperationalHealthSummary& operationalHealth,
    bool autoFallbackEnabled,
    bool fallbackActive,
    uint32_t stableFrames,
    uint32_t activeStableFrames,
    bool operationalStable,
    const char* defaultOnHealth,
    bool warmup,
    bool resetFile);

void WriteBeamDedicatedTelemetry(
    uint32_t frameIndex,
    const vfx::BeamRendererOperationalStatus& status,
    const BeamRenderGraphIntentTelemetry& graphIntent,
    const BeamDedicatedOperationalHealthSummary& operationalHealth,
    bool warmup,
    bool autoFallbackEnabled,
    bool fallbackActive,
    uint32_t stableFrames,
    uint32_t activeStableFrames,
    bool operationalStable,
    bool defaultOnReady,
    const char* defaultOnHealth,
    bool resetFile);
