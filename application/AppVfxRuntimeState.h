#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "utils/math/MathUtils.h"

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
    enum class ShowcaseEffect : uint32_t {
        ElectricOrbStrike = 0,
        IceProjectile = 1,
        BlackHole = 2,
        Count = 3,
    };

    struct ShowcaseTuning {
        float param1 = 1.0f;
        float param2 = 1.0f;
        float param3 = 1.0f;
        float param4 = 1.0f;
    };

    bool enableParticles = false;
    bool enableTrails = true;
    bool enableBeams = true;
    bool enableDistortions = true;
    bool enableRings = true;
    bool enableCylinders = true;
    bool enableElectricOrbStrike = true;
    bool enableSkinnedSurfaceVfx = false;
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

    bool autoPlayVfxDemo = false;
    float autoPlayVfxInterval = 0.6f;
    float autoPlayVfxRadius = 2.5f;
    float autoPlayVfxTimer = 0.0f;
    float autoPlayVfxAngle = 0.0f;
    bool showcaseMode = false;
    bool showcaseAutoRotate = false;
    bool showcaseHudVisible = true;
    bool showcaseTuningVisible = true;
    float showcaseAutoTimer = 0.0f;
    ShowcaseEffect showcaseEffect = ShowcaseEffect::ElectricOrbStrike;
    std::array<ShowcaseTuning, static_cast<size_t>(ShowcaseEffect::Count)> showcaseTuning{};
    bool iceProjectileClickToFire = true;
    bool electricOrbStrikeActive = false;
    bool electricOrbStrikeLoop = false;
    float electricOrbStrikeTimer = 0.0f;
    float electricOrbStrikeDuration = 4.25f;

    bool holdHitPlaneBurst = false;
    bool holdHitRing = false;
    bool holdHitCylinder = false;
    bool holdHitCylinderCombo = false;
    uint32_t holdHitPlaneBurstInstanceId = 0;
    uint32_t holdHitRingInstanceId = 0;
    uint32_t holdHitCylinderInstanceId = 0;
    uint32_t holdHitCylinderComboInstanceId = 0;

    bool iceProjectilePreviewActive = false;
    bool iceProjectileImpactSpawned = false;
    uint32_t iceProjectileInstanceId = 0;
    float iceProjectileTimer = 0.0f;
    Vector3 iceProjectileStart = {0.0f, -1.55f, -3.05f};
    Vector3 iceProjectileTarget = {2.5f, 0.7f, 0.42f};

    struct IceProjectileShotState {
        bool active = false;
        bool impactSpawned = false;
        uint32_t instanceId = 0;
        float timer = 0.0f;
        bool hasExplicitRotationZ = false;
        float rotationZ = 0.0f;
        Vector3 start = {0.0f, -1.55f, -3.05f};
        Vector3 target = {2.5f, 0.7f, 0.42f};
    };
    static constexpr uint32_t kMaxIceProjectileShots = 16;
    std::array<IceProjectileShotState, kMaxIceProjectileShots> iceProjectileShots{};
};
