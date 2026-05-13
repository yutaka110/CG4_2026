#pragma once

#include <cstdint>
#include <string>

struct ParticleDedicatedOperationalHealthSummary {
    bool probeEnabled = false;
    bool graphReady = false;
    bool handlesReady = false;
    bool simulationTelemetryValid = false;
    bool simulationDedicated = false;
    bool drawDedicated = false;
    bool readbackValid = false;
    bool readbackValidationOk = false;
    bool drawArgsReadbackValid = false;
    bool drawArgsValidationOk = false;
    bool readbackReady = false;
    bool validationOk = false;
    bool healthy = false;
    uint32_t okRows = 0;
    uint32_t ngRows = 0;
    uint32_t sampleCount = 0;
    uint32_t expectedDrawCount = 0;
    uint32_t actualDrawCount = 0;
    uint32_t expectedIndexCount = 0;
    uint32_t actualIndexCount = 0;
    float maxPositionDelta = 0.0f;
    std::string fallbackReason = "probe disabled";
    std::string firstFailureReason = "telemetry waiting";
    std::string drawArgsFailureReason = "draw args waiting";
};

struct DistortionDedicatedOperationalHealthSummary {
    bool enabled = false;
    bool graphReady = false;
    bool handlesReady = false;
    bool drawDedicated = false;
    bool readbackValid = false;
    bool drawArgsReadbackValid = false;
    bool renderBufferValidationOk = false;
    bool drawArgsValidationOk = false;
    bool readbackReady = false;
    bool validationOk = false;
    bool healthy = false;
    uint32_t renderSamples = 0;
    uint32_t expectedDrawCount = 0;
    uint32_t actualDrawCount = 0;
    uint32_t expectedIndexCount = 0;
    uint32_t actualIndexCount = 0;
    std::string fallbackReason = "disabled";
    std::string readbackFailureReason = "readback waiting";
    std::string drawArgsFailureReason = "draw args waiting";
};

struct BeamDedicatedOperationalHealthSummary {
    bool graphReady = false;
    bool handlesReady = false;
    bool drawDedicated = false;
    bool readbackValid = false;
    bool drawArgsReadbackValid = false;
    bool renderBufferValidationOk = false;
    bool drawArgsValidationOk = false;
    bool readbackReady = false;
    bool validationOk = false;
    bool healthy = false;
    uint32_t renderSamples = 0;
    uint32_t expectedDrawCount = 0;
    uint32_t actualDrawCount = 0;
    uint32_t expectedIndexCount = 0;
    uint32_t actualIndexCount = 0;
    std::string fallbackReason = "beam dedicated telemetry waiting";
    std::string readbackFailureReason = "readback waiting";
    std::string drawArgsFailureReason = "draw args waiting";
};

struct AppVfxRuntimeState {
    bool enableParticles = false;
    bool enableParticleDedicatedResourceProbe = false;
    bool enableParticleDedicatedProbeTelemetry = false;
    bool enableParticleDedicatedAutoFallback = true;
    bool particleDedicatedResourceFallbackActive = false;
    ParticleDedicatedOperationalHealthSummary particleDedicatedHealth{};
    uint32_t particleDedicatedProbeStableFrames = 0;
    uint32_t particleDedicatedActiveStableFrames = 0;
    bool particleDedicatedDefaultONCandidate = false;
    std::string particleDedicatedDefaultOnHealth = "disabled";

    bool enableTrailMeshStream = true;
    bool enableTrailMeshStreamAutoFallback = true;
    bool enableTrailMeshStreamStartupTelemetry = false;
    bool trailMeshStreamFallbackActive = false;

    bool enableDistortionDedicatedResources = true;
    bool enableDistortionDedicatedAutoFallback = true;
    bool enableDistortionDedicatedTelemetry = false;
    bool distortionDedicatedResourceFallbackActive = false;
    DistortionDedicatedOperationalHealthSummary distortionDedicatedHealth{};
    uint32_t distortionDedicatedStableFrames = 0;
    uint32_t distortionDedicatedActiveStableFrames = 0;
    bool distortionDedicatedOperationalStable = false;
    std::string distortionDedicatedDefaultOnHealth = "disabled";
    BeamDedicatedOperationalHealthSummary beamDedicatedHealth{};
    bool enableBeamDedicatedAutoFallback = true;
    bool beamDedicatedResourceFallbackActive = false;
    bool enableBeamDedicatedTelemetry = false;
    uint32_t beamDedicatedStableFrames = 0;
    uint32_t beamDedicatedActiveStableFrames = 0;
    bool beamDedicatedOperationalStable = false;
    bool beamDedicatedDefaultOnReady = false;
    std::string beamDedicatedDefaultOnHealth = "warmup";

    bool autoPlayVfxDemo = true;
    float autoPlayVfxInterval = 0.6f;
    float autoPlayVfxRadius = 2.5f;
    float autoPlayVfxTimer = 0.0f;
    float autoPlayVfxAngle = 0.0f;
};
