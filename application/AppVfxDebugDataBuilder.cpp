#include "AppVfxDebugDataBuilder.h"

#include "vfx/VfxResourceResolver.h"
#include "vfx/VfxRenderContext.h"

namespace {
VfxRenderContext BuildDebugRenderContext(
    const AppVfxDebugDataBuilderContext& context,
    const vfx::VfxTypedResourceSet& resources) {
    return VfxRenderContext{
        context.appPipelines,
        context.renderResources,
        context.scene,
        context.gpuParticleSystem,
        nullptr,
        context.frameState,
        context.srvDescriptorHeap,
        context.vfxTextureHandle,
        context.depthTextureHandle,
        0.0f,
        &resources
    };
}

VfxRenderContext BuildTrailDebugRenderContext(
    const AppVfxDebugDataBuilderContext& context,
    const vfx::VfxTypedResourceSet& resources) {
    return VfxRenderContext{
        context.appPipelines,
        nullptr,
        nullptr,
        context.gpuParticleSystem,
        nullptr,
        context.frameState,
        context.srvDescriptorHeap,
        context.vfxTextureHandle,
        context.depthTextureHandle,
        0.0f,
        &resources
    };
}

const std::vector<ge3::graphics::RenderPassDebugInfo>& RenderPassDebugInfoOrEmpty(
    const AppVfxDebugDataBuilderContext& context) {
    static const std::vector<ge3::graphics::RenderPassDebugInfo> kEmptyPasses;
    return context.renderPassDebugInfo != nullptr ? *context.renderPassDebugInfo : kEmptyPasses;
}

void FillParticleReadbackSnapshots(
    const AppVfxDebugDataBuilderContext& context,
    const AppGpuParticleSystem::ParticleSimulationTelemetry*& simulationTelemetry,
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry*& readbackTelemetry) {
    simulationTelemetry = nullptr;
    readbackTelemetry = nullptr;
    if (context.gpuParticleSystem == nullptr) {
        return;
    }

    simulationTelemetry = &context.gpuParticleSystem->ParticleSimulationTelemetrySnapshot();
    readbackTelemetry = &context.gpuParticleSystem->ParticleDedicatedReadbackTelemetrySnapshot();
}

void FillDistortionReadbackSnapshot(
    const AppVfxDebugDataBuilderContext& context,
    const AppGpuParticleSystem::DistortionDedicatedReadbackTelemetry*& readbackTelemetry,
    uint32_t& expectedDrawCount) {
    readbackTelemetry = nullptr;
    expectedDrawCount = 0;
    if (context.gpuParticleSystem == nullptr) {
        return;
    }

    readbackTelemetry = &context.gpuParticleSystem->DistortionDedicatedReadbackTelemetrySnapshot();
    expectedDrawCount = context.gpuParticleSystem->MaxParticles();
}

void FillBeamReadbackSnapshot(
    const AppVfxDebugDataBuilderContext& context,
    const AppGpuParticleSystem::BeamDedicatedReadbackTelemetry*& readbackTelemetry,
    uint32_t& expectedDrawCount) {
    readbackTelemetry = nullptr;
    expectedDrawCount = 0;
    if (context.gpuParticleSystem == nullptr) {
        return;
    }

    readbackTelemetry = &context.gpuParticleSystem->BeamDedicatedReadbackTelemetrySnapshot();
    expectedDrawCount = context.gpuParticleSystem->MaxParticles();
}

bool HasBeamDedicatedResources(
    const AppVfxDebugDataBuilderContext& context,
    const vfx::BeamVfxResourceSet& resources) {
    if (context.gpuParticleSystem == nullptr) {
        return false;
    }

    return context.gpuParticleSystem->SrvHandleForResource(resources.renderer.renderBuffer).ptr != 0 &&
        context.gpuParticleSystem->UavHandleForResource(resources.renderer.renderBuffer).ptr != 0 &&
        context.gpuParticleSystem->IndirectArgsForResource(resources.renderer.indirectArgs) != nullptr;
}

void FillTrailMeshStreamTelemetry(
    const AppVfxDebugDataBuilderContext& context,
    const vfx::TrailMeshStreamPlan& plan,
    const AppGpuParticleSystem::TrailMeshStreamTelemetry*& telemetry,
    bool& telemetryValid,
    TrailMeshStreamTelemetrySummary& summary) {
    telemetry = nullptr;
    telemetryValid = false;
    summary = TrailMeshStreamTelemetrySummary{};
    if (context.gpuParticleSystem == nullptr) {
        summary.firstFailureReason = "gpu particle system missing";
        return;
    }

    telemetry = &context.gpuParticleSystem->TrailMeshStreamTelemetrySnapshot();
    telemetryValid = telemetry->valid;
    summary = BuildTrailMeshStreamTelemetrySummary(*telemetry, plan);
}
}

void BuildParticleDedicatedRuntimeDebugData(
    const AppVfxDebugDataBuilderContext& context,
    ParticleDedicatedRuntimeDebugData& output) {
    output = ParticleDedicatedRuntimeDebugData{};
    output.resources = vfx::MakeParticleDedicatedStorageProbeVfxResources();
    const VfxRenderContext renderContext = BuildDebugRenderContext(context, output.resources);
    output.status = vfx::EvaluateParticleRendererOperationalStatus(
        renderContext,
        &output.resources.particle);
    output.graphIntent = BuildParticleRenderGraphIntentTelemetry(
        RenderPassDebugInfoOrEmpty(context),
        output.resources.particle);
    FillParticleReadbackSnapshots(
        context,
        output.simulationTelemetry,
        output.readbackTelemetry);
}

void BuildParticleRendererInspectorDebugData(
    const AppVfxDebugDataBuilderContext& context,
    bool probeEnabled,
    ParticleRendererInspectorDebugData& output) {
    output = ParticleRendererInspectorDebugData{};
    output.resources = vfx::MakeLegacySharedIndirectVfxResources();
    const VfxRenderContext renderContext = BuildDebugRenderContext(context, output.resources);
    output.status = vfx::EvaluateParticleRendererOperationalStatus(
        renderContext,
        &output.resources.particle);
    output.graphIntent = BuildParticleRenderGraphIntentTelemetry(
        RenderPassDebugInfoOrEmpty(context),
        output.resources.particle);
    output.probeEnabled = probeEnabled;

    if (probeEnabled) {
        output.probeResources = vfx::MakeParticleDedicatedStorageProbeVfxResources();
        const VfxRenderContext probeContext = BuildDebugRenderContext(context, output.probeResources);
        output.probeStatus = vfx::EvaluateParticleRendererOperationalStatus(
            probeContext,
            &output.probeResources.particle);
        output.probeGraphIntent = BuildParticleRenderGraphIntentTelemetry(
            RenderPassDebugInfoOrEmpty(context),
            output.probeResources.particle);
        FillParticleReadbackSnapshots(
            context,
            output.simulationTelemetry,
            output.readbackTelemetry);
    }
}

void BuildDistortionDedicatedRuntimeDebugData(
    const AppVfxDebugDataBuilderContext& context,
    DistortionDedicatedRuntimeDebugData& output) {
    output = DistortionDedicatedRuntimeDebugData{};
    output.resources = vfx::MakeDistortionDedicatedVfxResources();
    const VfxRenderContext renderContext = BuildDebugRenderContext(context, output.resources);
    output.status = vfx::EvaluateDistortionRendererOperationalStatus(
        renderContext,
        &output.resources.distortion);
    output.graphIntent = BuildDistortionRenderGraphIntentTelemetry(
        RenderPassDebugInfoOrEmpty(context),
        output.resources.distortion);
    FillDistortionReadbackSnapshot(
        context,
        output.readbackTelemetry,
        output.expectedDrawCount);
}

void BuildBeamDedicatedRuntimeDebugData(
    const AppVfxDebugDataBuilderContext& context,
    BeamDedicatedRuntimeDebugData& output) {
    output = BeamDedicatedRuntimeDebugData{};
    const vfx::VfxResourceResolver resolver;
    output.resources = resolver.SelectBeamDedicatedResources();
    const VfxRenderContext renderContext = BuildDebugRenderContext(context, output.resources);
    output.status = vfx::EvaluateBeamRendererOperationalStatus(
        renderContext,
        &output.resources.beam);
    output.graphIntent = BuildBeamRenderGraphIntentTelemetry(
        RenderPassDebugInfoOrEmpty(context),
        output.resources.beam);
    output.dedicatedPathReady = true;
    output.dedicatedResourcesAvailable =
        HasBeamDedicatedResources(context, output.resources.beam);
    FillBeamReadbackSnapshot(
        context,
        output.readbackTelemetry,
        output.expectedDrawCount);
    output.fallbackReason = output.dedicatedResourcesAvailable
        ? "beam dedicated telemetry waiting"
        : "beam dedicated resources not backed";
}

void BuildBeamRendererInspectorDebugData(
    const AppVfxDebugDataBuilderContext& context,
    BeamRendererInspectorDebugData& output) {
    output = BeamRendererInspectorDebugData{};
    const vfx::VfxResourceResolver resolver;
    output.resources = resolver.SelectBeamDedicatedResources();
    const VfxRenderContext renderContext = BuildDebugRenderContext(context, output.resources);
    output.status = vfx::EvaluateBeamRendererOperationalStatus(
        renderContext,
        &output.resources.beam);
    output.graphIntent = BuildBeamRenderGraphIntentTelemetry(
        RenderPassDebugInfoOrEmpty(context),
        output.resources.beam);
    output.dedicatedPathReady = true;
    output.dedicatedResourcesAvailable =
        HasBeamDedicatedResources(context, output.resources.beam);
    FillBeamReadbackSnapshot(
        context,
        output.readbackTelemetry,
        output.expectedDrawCount);
    output.fallbackReason = output.dedicatedResourcesAvailable
        ? "beam dedicated telemetry waiting"
        : "beam dedicated resources not backed";
}

void BuildTrailMeshStreamRuntimeDebugData(
    const AppVfxDebugDataBuilderContext& context,
    const TrailRenderInput& trailInput,
    bool effectiveDedicated,
    TrailMeshStreamRuntimeDebugData& output) {
    output = TrailMeshStreamRuntimeDebugData{};
    output.activeResources = vfx::MakeLegacySharedIndirectVfxResources();
    output.activeResources.trail.renderer.meshStream.usesDedicatedMeshStream = effectiveDedicated;
    output.activePlan = vfx::BuildTrailMeshStreamPlan(
        trailInput,
        &output.activeResources.trail.renderer.meshStream);
    const VfxRenderContext activeContext = BuildTrailDebugRenderContext(context, output.activeResources);
    output.activeDrawStatus = vfx::EvaluateTrailMeshStreamDrawStatus(output.activePlan, activeContext);
    FillTrailMeshStreamTelemetry(
        context,
        output.activePlan,
        output.telemetry,
        output.telemetryValid,
        output.activeSummary);

    output.probeResources = vfx::MakeLegacySharedIndirectVfxResources();
    output.probeResources.trail.renderer.meshStream.usesDedicatedMeshStream = true;
    output.probePlan = vfx::BuildTrailMeshStreamPlan(
        trailInput,
        &output.probeResources.trail.renderer.meshStream);
    const VfxRenderContext probeContext = BuildTrailDebugRenderContext(context, output.probeResources);
    output.probeDrawStatus = vfx::EvaluateTrailMeshStreamDrawStatus(output.probePlan, probeContext);
    if (output.telemetry != nullptr) {
        output.probeSummary = BuildTrailMeshStreamTelemetrySummary(*output.telemetry, output.probePlan);
    } else {
        output.probeSummary.firstFailureReason = "gpu particle system missing";
    }
}

void BuildTrailMeshStreamInspectorDebugData(
    const AppVfxDebugDataBuilderContext& context,
    const TrailRenderInput& trailInput,
    const Vector3& cameraWorldPosition,
    bool effectiveDedicated,
    TrailMeshStreamInspectorDebugData& output) {
    output = TrailMeshStreamInspectorDebugData{};
    output.resources = vfx::MakeLegacySharedIndirectVfxResources();
    output.resources.trail.renderer.meshStream.usesDedicatedMeshStream = effectiveDedicated;
    output.plan = vfx::BuildTrailMeshStreamPlan(
        trailInput,
        &output.resources.trail.renderer.meshStream);
    const VfxRenderContext trailContext = BuildTrailDebugRenderContext(context, output.resources);
    output.drawStatus = vfx::EvaluateTrailMeshStreamDrawStatus(output.plan, trailContext);
    output.debugPreview = vfx::BuildTrailMeshStreamDebugPreview(trailInput, cameraWorldPosition, 16);
    FillTrailMeshStreamTelemetry(
        context,
        output.plan,
        output.telemetry,
        output.telemetryValid,
        output.summary);

    output.probeResources = vfx::MakeLegacySharedIndirectVfxResources();
    output.probeResources.trail.renderer.meshStream.usesDedicatedMeshStream = true;
    output.probePlan = vfx::BuildTrailMeshStreamPlan(
        trailInput,
        &output.probeResources.trail.renderer.meshStream);
    const VfxRenderContext probeContext = BuildTrailDebugRenderContext(context, output.probeResources);
    output.probeDrawStatus = vfx::EvaluateTrailMeshStreamDrawStatus(output.probePlan, probeContext);
}
