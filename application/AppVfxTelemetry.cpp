#include "AppVfxTelemetry.h"

#include "../../externals/imgui/imgui.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
const ge3::graphics::RenderPassDebugInfo* FindRenderPassDebugInfo(
    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes,
    const char* passName) {
    if (passName == nullptr || passName[0] == '\0') {
        return nullptr;
    }

    for (const ge3::graphics::RenderPassDebugInfo& pass : passes) {
        if (pass.name == passName) {
            return &pass;
        }
    }
    return nullptr;
}

bool PassHasAccess(
    const ge3::graphics::RenderPassDebugInfo* pass,
    const char* resource,
    ge3::graphics::RenderResourceAccessType type) {
    if (pass == nullptr || resource == nullptr || resource[0] == '\0') {
        return false;
    }

    for (const ge3::graphics::RenderPassResourceAccess& access : pass->accesses) {
        if (access.resource == resource && access.type == type) {
            return true;
        }
    }
    return false;
}

bool IsFiniteFloat(float value) {
    return std::isfinite(value);
}

bool IsFiniteVector3(const Vector3& value) {
    return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z);
}

bool IsFiniteVector4(const Vector4& value) {
    return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z) && IsFiniteFloat(value.w);
}

float ParticlePositionDelta(
    const AppGpuParticleSystem::ParticleStateSample& state,
    const AppGpuParticleSystem::ParticleRenderSample& render) {
    const float dx = state.position.x - render.World.m[3][0];
    const float dy = state.position.y - render.World.m[3][1];
    const float dz = state.position.z - render.World.m[3][2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
}

ParticleRenderGraphIntentTelemetry BuildParticleRenderGraphIntentTelemetry(
    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes,
    const vfx::ParticleVfxResourceSet& resources) {
    ParticleRenderGraphIntentTelemetry telemetry{};
    const ge3::graphics::RenderPassDebugInfo* simulationPass =
        FindRenderPassDebugInfo(passes, resources.routing.simulationPass);
    const ge3::graphics::RenderPassDebugInfo* drawPass =
        FindRenderPassDebugInfo(passes, resources.routing.drawPass);

    telemetry.simulationPassFound = simulationPass != nullptr;
    telemetry.simulationPassExecuted = simulationPass != nullptr && simulationPass->executed;
    telemetry.simulationStateRead = PassHasAccess(
        simulationPass,
        resources.simulation.stateBuffer,
        ge3::graphics::RenderResourceAccessType::ReadUav);
    telemetry.simulationStateWrite = PassHasAccess(
        simulationPass,
        resources.simulation.stateBuffer,
        ge3::graphics::RenderResourceAccessType::WriteUav);
    telemetry.simulationRenderBufferWrite = PassHasAccess(
        simulationPass,
        resources.simulation.renderBuffer,
        ge3::graphics::RenderResourceAccessType::WriteUav);

    telemetry.drawPassFound = drawPass != nullptr;
    telemetry.drawPassExecuted = drawPass != nullptr && drawPass->executed;
    telemetry.drawRenderBufferRead = PassHasAccess(
        drawPass,
        resources.renderer.renderBuffer,
        ge3::graphics::RenderResourceAccessType::ReadSrv);
    telemetry.drawIndirectArgsRead = PassHasAccess(
        drawPass,
        resources.renderer.indirectArgs,
        ge3::graphics::RenderResourceAccessType::ReadIndirect);

    telemetry.ready =
        telemetry.simulationPassFound &&
        telemetry.simulationPassExecuted &&
        telemetry.simulationStateRead &&
        telemetry.simulationStateWrite &&
        telemetry.simulationRenderBufferWrite &&
        telemetry.drawPassFound &&
        telemetry.drawPassExecuted &&
        telemetry.drawRenderBufferRead &&
        telemetry.drawIndirectArgsRead;

    telemetry.fallbackReason = "ready";
    if (!telemetry.simulationPassFound) {
        telemetry.fallbackReason = "missing particle simulation pass";
    } else if (!telemetry.simulationPassExecuted) {
        telemetry.fallbackReason = "particle simulation pass culled";
    } else if (!telemetry.simulationStateRead || !telemetry.simulationStateWrite) {
        telemetry.fallbackReason = "particle state access mismatch";
    } else if (!telemetry.simulationRenderBufferWrite) {
        telemetry.fallbackReason = "particle render buffer write missing";
    } else if (!telemetry.drawPassFound) {
        telemetry.fallbackReason = "missing particle draw pass";
    } else if (!telemetry.drawPassExecuted) {
        telemetry.fallbackReason = "particle draw pass culled";
    } else if (!telemetry.drawRenderBufferRead) {
        telemetry.fallbackReason = "particle render buffer read missing";
    } else if (!telemetry.drawIndirectArgsRead) {
        telemetry.fallbackReason = "particle indirect args read missing";
    }

    return telemetry;
}

DistortionRenderGraphIntentTelemetry BuildDistortionRenderGraphIntentTelemetry(
    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes,
    const vfx::DistortionVfxResourceSet& resources) {
    DistortionRenderGraphIntentTelemetry telemetry{};
    const ge3::graphics::RenderPassDebugInfo* simulationPass =
        FindRenderPassDebugInfo(passes, resources.routing.simulationPass);
    const ge3::graphics::RenderPassDebugInfo* drawPass =
        FindRenderPassDebugInfo(passes, resources.routing.drawPass);

    telemetry.simulationPassFound = simulationPass != nullptr;
    telemetry.simulationPassExecuted = simulationPass != nullptr && simulationPass->executed;
    telemetry.simulationStateRead = PassHasAccess(
        simulationPass,
        resources.simulation.stateBuffer,
        ge3::graphics::RenderResourceAccessType::ReadUav);
    telemetry.simulationStateWrite = PassHasAccess(
        simulationPass,
        resources.simulation.stateBuffer,
        ge3::graphics::RenderResourceAccessType::WriteUav);
    telemetry.simulationRenderBufferWrite = PassHasAccess(
        simulationPass,
        resources.simulation.renderBuffer,
        ge3::graphics::RenderResourceAccessType::WriteUav);

    telemetry.drawPassFound = drawPass != nullptr;
    telemetry.drawPassExecuted = drawPass != nullptr && drawPass->executed;
    telemetry.drawRenderBufferRead = PassHasAccess(
        drawPass,
        resources.renderer.renderBuffer,
        ge3::graphics::RenderResourceAccessType::ReadSrv);
    telemetry.drawIndirectArgsRead = PassHasAccess(
        drawPass,
        resources.renderer.indirectArgs,
        ge3::graphics::RenderResourceAccessType::ReadIndirect);

    telemetry.ready =
        telemetry.simulationPassFound &&
        telemetry.simulationPassExecuted &&
        telemetry.simulationStateRead &&
        telemetry.simulationStateWrite &&
        telemetry.simulationRenderBufferWrite &&
        telemetry.drawPassFound &&
        telemetry.drawPassExecuted &&
        telemetry.drawRenderBufferRead &&
        telemetry.drawIndirectArgsRead;

    telemetry.fallbackReason = "ready";
    if (!telemetry.simulationPassFound) {
        telemetry.fallbackReason = "missing distortion simulation pass";
    } else if (!telemetry.simulationPassExecuted) {
        telemetry.fallbackReason = "distortion simulation pass culled";
    } else if (!telemetry.simulationStateRead || !telemetry.simulationStateWrite) {
        telemetry.fallbackReason = "distortion state access mismatch";
    } else if (!telemetry.simulationRenderBufferWrite) {
        telemetry.fallbackReason = "distortion render buffer write missing";
    } else if (!telemetry.drawPassFound) {
        telemetry.fallbackReason = "missing distortion draw pass";
    } else if (!telemetry.drawPassExecuted) {
        telemetry.fallbackReason = "distortion draw pass culled";
    } else if (!telemetry.drawRenderBufferRead) {
        telemetry.fallbackReason = "distortion render buffer read missing";
    } else if (!telemetry.drawIndirectArgsRead) {
        telemetry.fallbackReason = "distortion indirect args read missing";
    }

    return telemetry;
}

BeamRenderGraphIntentTelemetry BuildBeamRenderGraphIntentTelemetry(
    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes,
    const vfx::BeamVfxResourceSet& resources) {
    BeamRenderGraphIntentTelemetry telemetry{};
    const ge3::graphics::RenderPassDebugInfo* simulationPass =
        FindRenderPassDebugInfo(passes, resources.routing.simulationPass);
    const ge3::graphics::RenderPassDebugInfo* drawPass =
        FindRenderPassDebugInfo(passes, resources.routing.drawPass);

    telemetry.simulationPassFound = simulationPass != nullptr;
    telemetry.simulationPassExecuted = simulationPass != nullptr && simulationPass->executed;
    telemetry.simulationStateRead = PassHasAccess(
        simulationPass,
        resources.simulation.stateBuffer,
        ge3::graphics::RenderResourceAccessType::ReadUav);
    telemetry.simulationStateWrite = PassHasAccess(
        simulationPass,
        resources.simulation.stateBuffer,
        ge3::graphics::RenderResourceAccessType::WriteUav);
    telemetry.simulationRenderBufferWrite = PassHasAccess(
        simulationPass,
        resources.simulation.renderBuffer,
        ge3::graphics::RenderResourceAccessType::WriteUav);

    telemetry.drawPassFound = drawPass != nullptr;
    telemetry.drawPassExecuted = drawPass != nullptr && drawPass->executed;
    telemetry.drawRenderBufferRead = PassHasAccess(
        drawPass,
        resources.renderer.renderBuffer,
        ge3::graphics::RenderResourceAccessType::ReadSrv);
    telemetry.drawIndirectArgsRead = PassHasAccess(
        drawPass,
        resources.renderer.indirectArgs,
        ge3::graphics::RenderResourceAccessType::ReadIndirect);

    telemetry.ready =
        telemetry.simulationPassFound &&
        telemetry.simulationPassExecuted &&
        telemetry.simulationStateRead &&
        telemetry.simulationStateWrite &&
        telemetry.simulationRenderBufferWrite &&
        telemetry.drawPassFound &&
        telemetry.drawPassExecuted &&
        telemetry.drawRenderBufferRead &&
        telemetry.drawIndirectArgsRead;

    telemetry.fallbackReason = "ready";
    if (!telemetry.simulationPassFound) {
        telemetry.fallbackReason = "missing beam simulation pass";
    } else if (!telemetry.simulationPassExecuted) {
        telemetry.fallbackReason = "beam simulation pass culled";
    } else if (!telemetry.simulationStateRead || !telemetry.simulationStateWrite) {
        telemetry.fallbackReason = "beam state access mismatch";
    } else if (!telemetry.simulationRenderBufferWrite) {
        telemetry.fallbackReason = "beam render buffer write missing";
    } else if (!telemetry.drawPassFound) {
        telemetry.fallbackReason = "missing beam draw pass";
    } else if (!telemetry.drawPassExecuted) {
        telemetry.fallbackReason = "beam draw pass culled";
    } else if (!telemetry.drawRenderBufferRead) {
        telemetry.fallbackReason = "beam render buffer read missing";
    } else if (!telemetry.drawIndirectArgsRead) {
        telemetry.fallbackReason = "beam indirect args read missing";
    }

    return telemetry;
}

TrailMeshStreamRowValidation ValidateTrailMeshStreamTelemetryRow(
    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry,
    const vfx::TrailMeshStreamPlan& trailPlan,
    uint32_t segmentIndex) {
    TrailMeshStreamRowValidation check{};
    if (segmentIndex >= telemetry.sampledSegmentCount) {
        return check;
    }

    const auto& segment = telemetry.segments[segmentIndex];
    check.headLeftVertexIndex = segmentIndex * 2;
    check.headRightVertexIndex = check.headLeftVertexIndex + 1;
    check.tailLeftVertexIndex = check.headLeftVertexIndex + 2;
    check.tailRightVertexIndex = check.headLeftVertexIndex + 3;
    check.hasHeadCp = segment.startControlPoint < telemetry.sampledControlPointCount;
    check.hasTailCp = segment.endControlPoint < telemetry.sampledControlPointCount;
    check.hasHeadLeftVertex = check.headLeftVertexIndex < telemetry.sampledVertexCount;
    check.hasHeadRightVertex = check.headRightVertexIndex < telemetry.sampledVertexCount;
    check.hasTailLeftVertex = check.tailLeftVertexIndex < telemetry.sampledVertexCount;
    check.hasTailRightVertex = check.tailRightVertexIndex < telemetry.sampledVertexCount;
    check.indexBase = segmentIndex * 6u;
    check.hasIndexSix = check.indexBase + 5u < telemetry.sampledIndexCount;
    check.hasHeadChain = check.hasHeadCp && check.hasHeadLeftVertex && check.hasHeadRightVertex;
    check.hasTailChain = check.hasTailCp && check.hasTailLeftVertex && check.hasTailRightVertex;
    check.expectedHeadT = segment.normalizedHead;
    check.expectedTailT = segment.normalizedTail;
    check.expectedHeadAlpha = check.hasHeadCp
        ? telemetry.controlPoints[segment.startControlPoint].positionAge.w
        : 0.0f;
    check.expectedTailAlpha = check.hasTailCp
        ? telemetry.controlPoints[segment.endControlPoint].positionAge.w
        : 0.0f;
    check.expectedHeadWidth = check.hasHeadCp
        ? telemetry.controlPoints[segment.startControlPoint].colorWidth.w
        : 0.0f;
    check.expectedTailWidth = check.hasTailCp
        ? telemetry.controlPoints[segment.endControlPoint].colorWidth.w
        : 0.0f;
    check.headLeftTDelta = check.hasHeadLeftVertex
        ? std::abs(telemetry.vertices[check.headLeftVertexIndex].params.x - check.expectedHeadT)
        : 0.0f;
    check.headRightTDelta = check.hasHeadRightVertex
        ? std::abs(telemetry.vertices[check.headRightVertexIndex].params.x - check.expectedHeadT)
        : 0.0f;
    check.tailLeftTDelta = check.hasTailLeftVertex
        ? std::abs(telemetry.vertices[check.tailLeftVertexIndex].params.x - check.expectedTailT)
        : 0.0f;
    check.tailRightTDelta = check.hasTailRightVertex
        ? std::abs(telemetry.vertices[check.tailRightVertexIndex].params.x - check.expectedTailT)
        : 0.0f;
    check.headLeftAlphaDelta = check.hasHeadLeftVertex
        ? std::abs(telemetry.vertices[check.headLeftVertexIndex].color.w - check.expectedHeadAlpha)
        : 0.0f;
    check.headRightAlphaDelta = check.hasHeadRightVertex
        ? std::abs(telemetry.vertices[check.headRightVertexIndex].color.w - check.expectedHeadAlpha)
        : 0.0f;
    check.tailLeftAlphaDelta = check.hasTailLeftVertex
        ? std::abs(telemetry.vertices[check.tailLeftVertexIndex].color.w - check.expectedTailAlpha)
        : 0.0f;
    check.tailRightAlphaDelta = check.hasTailRightVertex
        ? std::abs(telemetry.vertices[check.tailRightVertexIndex].color.w - check.expectedTailAlpha)
        : 0.0f;
    check.headLeftWidthDelta = check.hasHeadLeftVertex
        ? std::abs(telemetry.vertices[check.headLeftVertexIndex].params.z - check.expectedHeadWidth)
        : 0.0f;
    check.headRightWidthDelta = check.hasHeadRightVertex
        ? std::abs(telemetry.vertices[check.headRightVertexIndex].params.z - check.expectedHeadWidth)
        : 0.0f;
    check.tailLeftWidthDelta = check.hasTailLeftVertex
        ? std::abs(telemetry.vertices[check.tailLeftVertexIndex].params.z - check.expectedTailWidth)
        : 0.0f;
    check.tailRightWidthDelta = check.hasTailRightVertex
        ? std::abs(telemetry.vertices[check.tailRightVertexIndex].params.z - check.expectedTailWidth)
        : 0.0f;
    check.cpRangeOk =
        segment.startControlPoint == segmentIndex &&
        segment.endControlPoint == segmentIndex + 1;
    check.tOk = check.hasHeadLeftVertex && check.hasHeadRightVertex && check.hasTailLeftVertex && check.hasTailRightVertex &&
        check.headLeftTDelta < 0.001f &&
        check.headRightTDelta < 0.001f &&
        check.tailLeftTDelta < 0.001f &&
        check.tailRightTDelta < 0.001f;
    check.sideUvOk = check.hasHeadLeftVertex && check.hasHeadRightVertex && check.hasTailLeftVertex && check.hasTailRightVertex &&
        std::abs(telemetry.vertices[check.headLeftVertexIndex].positionUv.w - 0.0f) < 0.001f &&
        std::abs(telemetry.vertices[check.headRightVertexIndex].positionUv.w - 1.0f) < 0.001f &&
        std::abs(telemetry.vertices[check.tailLeftVertexIndex].positionUv.w - 0.0f) < 0.001f &&
        std::abs(telemetry.vertices[check.tailRightVertexIndex].positionUv.w - 1.0f) < 0.001f;
    check.alphaOk = check.hasHeadChain && check.hasTailChain &&
        check.headLeftAlphaDelta < 0.001f &&
        check.headRightAlphaDelta < 0.001f &&
        check.tailLeftAlphaDelta < 0.001f &&
        check.tailRightAlphaDelta < 0.001f;
    check.widthOk = check.hasHeadChain && check.hasTailChain &&
        check.headLeftWidthDelta < 0.001f &&
        check.headRightWidthDelta < 0.001f &&
        check.tailLeftWidthDelta < 0.001f &&
        check.tailRightWidthDelta < 0.001f;
    check.argsOk =
        telemetry.drawArgs.IndexCountPerInstance == trailPlan.segmentCountEstimate * 6u &&
        telemetry.drawArgs.InstanceCount == (trailPlan.segmentCountEstimate > 0 ? 1u : 0u);
    check.indicesOk = check.hasIndexSix &&
        telemetry.indices[check.indexBase + 0u] == check.headLeftVertexIndex &&
        telemetry.indices[check.indexBase + 1u] == check.headRightVertexIndex &&
        telemetry.indices[check.indexBase + 2u] == check.tailLeftVertexIndex &&
        telemetry.indices[check.indexBase + 3u] == check.headRightVertexIndex &&
        telemetry.indices[check.indexBase + 4u] == check.tailRightVertexIndex &&
        telemetry.indices[check.indexBase + 5u] == check.tailLeftVertexIndex;
    check.rowOk =
        check.cpRangeOk &&
        check.tOk &&
        check.sideUvOk &&
        check.alphaOk &&
        check.widthOk &&
        check.indicesOk &&
        check.argsOk;
    return check;
}

void SetFirstTrailTelemetryFailure(
    TrailMeshStreamTelemetrySummary& summary,
    uint32_t segmentIndex,
    const char* reason) {
    if (summary.firstFailureReason != "none") {
        return;
    }

    summary.firstFailureSegment = segmentIndex;
    std::ostringstream stream;
    stream << "segment " << segmentIndex << ": " << reason;
    summary.firstFailureReason = stream.str();
}

void SetFirstTrailTelemetryFailure(
    TrailMeshStreamTelemetrySummary& summary,
    uint32_t segmentIndex,
    const std::string& reason) {
    if (summary.firstFailureReason != "none") {
        return;
    }

    summary.firstFailureSegment = segmentIndex;
    std::ostringstream stream;
    stream << "segment " << segmentIndex << ": " << reason;
    summary.firstFailureReason = stream.str();
}

std::string BuildTrailIndexMismatchReason(
    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry,
    const TrailMeshStreamRowValidation& check) {
    if (!check.hasIndexSix) {
        std::ostringstream stream;
        stream << "index buffer short at base " << check.indexBase
               << " sampled=" << telemetry.sampledIndexCount;
        return stream.str();
    }

    const uint32_t expectedIndices[6] = {
        check.headLeftVertexIndex,
        check.headRightVertexIndex,
        check.tailLeftVertexIndex,
        check.headRightVertexIndex,
        check.tailRightVertexIndex,
        check.tailLeftVertexIndex,
    };
    for (uint32_t slot = 0; slot < 6; ++slot) {
        const uint32_t actual = telemetry.indices[check.indexBase + slot];
        const uint32_t expected = expectedIndices[slot];
        if (actual != expected) {
            std::ostringstream stream;
            stream << "index mismatch slot " << slot
                   << " actual=" << actual
                   << " expected=" << expected;
            return stream.str();
        }
    }

    return "index buffer mismatch";
}

std::string BuildTrailDrawArgsMismatchReason(
    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry,
    const vfx::TrailMeshStreamPlan& trailPlan) {
    const uint32_t expectedIndexCount = trailPlan.segmentCountEstimate * 6u;
    const uint32_t expectedInstanceCount = trailPlan.segmentCountEstimate > 0 ? 1u : 0u;

    std::ostringstream stream;
    stream << "draw args mismatch"
           << " indexCount actual=" << telemetry.drawArgs.IndexCountPerInstance
           << " expected=" << expectedIndexCount
           << " instanceCount actual=" << telemetry.drawArgs.InstanceCount
           << " expected=" << expectedInstanceCount;
    return stream.str();
}

std::string BuildTrailControlPointRangeMismatchReason(
    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry,
    const TrailMeshStreamRowValidation& check,
    uint32_t segmentIndex) {
    if (segmentIndex >= telemetry.sampledSegmentCount) {
        return "control point range missing segment";
    }

    const auto& segment = telemetry.segments[segmentIndex];
    std::ostringstream stream;
    stream << "control point range mismatch"
           << " start actual=" << segment.startControlPoint
           << " expected=" << segmentIndex
           << " end actual=" << segment.endControlPoint
           << " expected=" << (segmentIndex + 1u)
           << " sampledCp=" << telemetry.sampledControlPointCount
           << " headInRange=" << (check.hasHeadCp ? "yes" : "no")
           << " tailInRange=" << (check.hasTailCp ? "yes" : "no");
    return stream.str();
}

void DrawTrailMeshStreamHealthLine(
    const char* label,
    const vfx::TrailMeshStreamDrawStatus& drawStatus,
    const TrailMeshStreamTelemetrySummary& telemetrySummary,
    bool telemetryValid) {
    const std::string firstFailureSegmentLabel =
        telemetrySummary.firstFailureSegment != UINT32_MAX
            ? std::to_string(telemetrySummary.firstFailureSegment)
            : "none";
    const bool ready = drawStatus.ready;
    const bool rowsOk = telemetrySummary.AllRowsOk();
    const bool healthy = ready && telemetryValid && rowsOk;
    ImGui::TextColored(
        healthy ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "%s=%s ready=%s telemetry=%s rows=%s drawPath=%s fallback=%s firstNG=%s firstFailure=%s",
        label,
        healthy ? "healthy" : "attention",
        ready ? "ok" : "ng",
        telemetryValid ? "valid" : "waiting",
        rowsOk ? "ok" : "ng",
        drawStatus.ready ? "dedicated mesh stream" : "indirect sprite fallback",
        drawStatus.fallbackReason,
        firstFailureSegmentLabel.c_str(),
        telemetrySummary.firstFailureReason.c_str());
}

bool IsTrailMeshStreamHealthy(
    const vfx::TrailMeshStreamDrawStatus& drawStatus,
    const TrailMeshStreamTelemetrySummary& telemetrySummary,
    bool telemetryValid) {
    return drawStatus.ready && telemetryValid && telemetrySummary.AllRowsOk();
}

const char* TrailMeshStreamDefaultOnHealthLabel(
    bool requested,
    bool warmup,
    bool effectiveDedicated,
    bool readyToEnable,
    bool activeStable,
    bool toggleHealthy,
    bool probeHealthy) {
    if (!requested) {
        return "disabled";
    }
    if (warmup) {
        return "warmup";
    }
    if (effectiveDedicated && readyToEnable && activeStable) {
        return "stable";
    }
    if (effectiveDedicated && toggleHealthy && probeHealthy) {
        return "ramping";
    }
    return "attention";
}

bool IsTrailMeshStreamOperationalOk(const char* defaultOnHealth) {
    return defaultOnHealth != nullptr && std::string(defaultOnHealth) == "stable";
}

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
    bool resetFile) {
    std::ofstream stream(
        "trail_mesh_stream_startup_telemetry.log",
        resetFile ? std::ios::trunc : std::ios::app);
    if (!stream) {
        return;
    }

    if (resetFile) {
        stream << "frame warmup safetyFallback effectiveDedicated activeStableFrames probeStableFrames defaultOnHealth operationalOk toggleHealth probeHealth toggleFirstFailure probeFirstFailure\n";
    }

    stream << frameIndex
           << " warmup=" << (warmup ? "yes" : "no")
           << " safetyFallback=" << (runtimeState.trailMeshStreamFallbackActive ? "active" : "off")
           << " effectiveDedicated=" << (effectiveDedicated ? "yes" : "no")
           << " activeStableFrames=" << activeStableFrames
           << " probeStableFrames=" << probeStableFrames
           << " defaultOnHealth=" << (defaultOnHealth != nullptr ? defaultOnHealth : "unknown")
           << " operationalOk=" << (operationalOk ? "yes" : "no")
           << " toggleHealth=" << (warmup ? "warmup" : (toggleHealthy ? "healthy" : "attention"))
           << " probeHealth=" << (warmup ? "warmup" : (probeHealthy ? "healthy" : "attention"))
           << " toggleFirstFailure=\"" << toggleSummary.firstFailureReason << "\""
           << " probeFirstFailure=\"" << probeSummary.firstFailureReason << "\"\n";
}

TrailMeshStreamRuntimeTelemetryUpdateResult UpdateTrailMeshStreamRuntimeTelemetry(
    const TrailMeshStreamRuntimeTelemetryUpdateInput& input) {
    TrailMeshStreamRuntimeTelemetryUpdateResult result{};
    if (input.runtimeState == nullptr ||
        input.activeDrawStatus == nullptr ||
        input.activeSummary == nullptr ||
        input.probeDrawStatus == nullptr ||
        input.probeSummary == nullptr ||
        input.healthFrames == nullptr ||
        input.probeHealthyFrames == nullptr ||
        input.activeHealthyFrames == nullptr ||
        input.startupTelemetryFrames == nullptr) {
        return result;
    }

    AppVfxRuntimeState& runtimeState = *input.runtimeState;
    result.warmup = *input.healthFrames < result.warmupFrames;
    const bool effectiveTrailMeshStreamEnabled =
        runtimeState.enableTrailMeshStream && !runtimeState.trailMeshStreamFallbackActive;
    result.activeHealthyRaw =
        effectiveTrailMeshStreamEnabled &&
        IsTrailMeshStreamHealthy(*input.activeDrawStatus, *input.activeSummary, input.telemetryValid);
    result.activeHealthy = !result.warmup && result.activeHealthyRaw;
    if (result.activeHealthy) {
        ++(*input.activeHealthyFrames);
    } else if (!result.warmup) {
        *input.activeHealthyFrames = 0;
    }

    result.probeHealthyRaw =
        IsTrailMeshStreamHealthy(*input.probeDrawStatus, *input.probeSummary, input.telemetryValid);
    result.probeHealthy = !result.warmup && result.probeHealthyRaw;
    const bool requestedDedicatedPath = runtimeState.enableTrailMeshStream;
    if (!requestedDedicatedPath || !runtimeState.enableTrailMeshStreamAutoFallback) {
        runtimeState.trailMeshStreamFallbackActive = false;
    } else if (result.warmup) {
        runtimeState.trailMeshStreamFallbackActive = false;
    } else if (runtimeState.trailMeshStreamFallbackActive) {
        runtimeState.trailMeshStreamFallbackActive = !result.probeHealthy;
    } else {
        runtimeState.trailMeshStreamFallbackActive = !result.activeHealthy;
    }

    if (result.probeHealthy) {
        ++(*input.probeHealthyFrames);
    } else {
        *input.probeHealthyFrames = 0;
    }

    result.readyToEnable = *input.probeHealthyFrames >= result.readyToEnableFrames;
    result.activeStable = *input.activeHealthyFrames >= result.activeStableRequiredFrames;
    result.defaultOnCandidate = result.readyToEnable && result.activeStable;
    result.effectiveDedicatedAfterSafety =
        runtimeState.enableTrailMeshStream && !runtimeState.trailMeshStreamFallbackActive;
    result.defaultOnHealth = TrailMeshStreamDefaultOnHealthLabel(
        requestedDedicatedPath,
        result.warmup,
        result.effectiveDedicatedAfterSafety,
        result.readyToEnable,
        result.activeStable,
        result.activeHealthyRaw,
        result.probeHealthyRaw);
    result.operationalOk = IsTrailMeshStreamOperationalOk(result.defaultOnHealth.c_str());

    if (runtimeState.enableTrailMeshStreamStartupTelemetry &&
        *input.startupTelemetryFrames < result.startupTelemetryFrameLimit) {
        WriteTrailMeshStreamStartupTelemetry(
            *input.startupTelemetryFrames,
            runtimeState,
            result.effectiveDedicatedAfterSafety,
            *input.activeHealthyFrames,
            *input.probeHealthyFrames,
            result.activeHealthyRaw,
            result.probeHealthyRaw,
            result.warmup,
            result.defaultOnHealth.c_str(),
            result.operationalOk,
            *input.activeSummary,
            *input.probeSummary,
            *input.startupTelemetryFrames == 0);
        ++(*input.startupTelemetryFrames);
    }
    ++(*input.healthFrames);

    return result;
}

TrailMeshStreamInspectorTelemetryResult BuildTrailMeshStreamInspectorTelemetry(
    const TrailMeshStreamInspectorTelemetryInput& input) {
    TrailMeshStreamInspectorTelemetryResult result{};
    if (input.runtimeState == nullptr ||
        input.activeDrawStatus == nullptr ||
        input.probeDrawStatus == nullptr ||
        input.summary == nullptr) {
        return result;
    }

    constexpr uint32_t kWarmupFrames = 10;
    const AppVfxRuntimeState& runtimeState = *input.runtimeState;
    const bool warmup = input.healthFrames < kWarmupFrames;
    const bool effectiveDedicated =
        runtimeState.enableTrailMeshStream && !runtimeState.trailMeshStreamFallbackActive;
    const bool activeHealthy =
        effectiveDedicated &&
        IsTrailMeshStreamHealthy(*input.activeDrawStatus, *input.summary, input.telemetryValid);
    const bool probeHealthy =
        IsTrailMeshStreamHealthy(*input.probeDrawStatus, *input.summary, input.telemetryValid);
    const bool readyToEnable =
        input.probeStableFrames >= result.readyToEnableFrames;
    const bool activeStable =
        input.activeStableFrames >= result.activeStableRequiredFrames;

    result.defaultOnHealth = TrailMeshStreamDefaultOnHealthLabel(
        runtimeState.enableTrailMeshStream,
        warmup,
        effectiveDedicated,
        readyToEnable,
        activeStable,
        activeHealthy,
        probeHealthy);
    result.operationalOk = IsTrailMeshStreamOperationalOk(result.defaultOnHealth.c_str());
    return result;
}

TrailMeshStreamTelemetrySummary BuildTrailMeshStreamTelemetrySummary(
    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry,
    const vfx::TrailMeshStreamPlan& trailPlan) {
    TrailMeshStreamTelemetrySummary summary{};
    if (!telemetry.valid) {
        summary.firstFailureReason = "telemetry waiting";
        return summary;
    }

    for (uint32_t i = 0; i < telemetry.sampledSegmentCount; ++i) {
        const TrailMeshStreamRowValidation check =
            ValidateTrailMeshStreamTelemetryRow(telemetry, trailPlan, i);
        if (check.rowOk) {
            ++summary.okRows;
            continue;
        }

        ++summary.ngRows;
        if (!check.cpRangeOk) {
            SetFirstTrailTelemetryFailure(
                summary,
                i,
                BuildTrailControlPointRangeMismatchReason(telemetry, check, i));
        } else if (!check.tOk) {
            SetFirstTrailTelemetryFailure(summary, i, "vertex t mismatch");
        } else if (!check.sideUvOk) {
            SetFirstTrailTelemetryFailure(summary, i, "side uv mismatch");
        } else if (!check.alphaOk) {
            SetFirstTrailTelemetryFailure(summary, i, "alpha mismatch");
        } else if (!check.widthOk) {
            SetFirstTrailTelemetryFailure(summary, i, "width mismatch");
        } else if (!check.indicesOk) {
            SetFirstTrailTelemetryFailure(
                summary,
                i,
                BuildTrailIndexMismatchReason(telemetry, check));
        } else if (!check.argsOk) {
            SetFirstTrailTelemetryFailure(
                summary,
                i,
                BuildTrailDrawArgsMismatchReason(telemetry, trailPlan));
        }
    }

    if (summary.okRows == 0 && summary.ngRows == 0) {
        summary.firstFailureReason = "no sampled segments";
    }
    return summary;
}

void SetFirstParticleReadbackFailure(
    ParticleDedicatedReadbackValidationSummary& summary,
    uint32_t sampleIndex,
    const char* reason) {
    if (summary.firstFailureSample != UINT32_MAX) {
        return;
    }
    summary.firstFailureSample = sampleIndex;
    std::ostringstream stream;
    stream << "sample " << sampleIndex << ": " << reason;
    summary.firstFailureReason = stream.str();
}

ParticleDedicatedReadbackValidationSummary BuildParticleDedicatedReadbackValidationSummary(
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry* telemetry,
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry) {
    ParticleDedicatedReadbackValidationSummary summary{};
    if (telemetry == nullptr || !telemetry->valid) {
        return summary;
    }

    summary.telemetryValid = true;
    summary.expectedIndexCount = 6;
    summary.expectedDrawCount =
        simulationTelemetry != nullptr && simulationTelemetry->valid
            ? simulationTelemetry->maxParticles
            : 0u;
    summary.drawArgsReadbackValid = telemetry->drawArgsValid;
    summary.actualIndexCount = telemetry->drawArgs.IndexCountPerInstance;
    summary.actualDrawCount = telemetry->drawArgs.InstanceCount;
    summary.drawArgsFailureReason = "none";
    summary.drawArgsValidationOk =
        summary.drawArgsReadbackValid &&
        summary.expectedDrawCount > 0 &&
        telemetry->drawArgs.IndexCountPerInstance == summary.expectedIndexCount &&
        telemetry->drawArgs.InstanceCount == summary.expectedDrawCount &&
        telemetry->drawArgs.StartIndexLocation == 0 &&
        telemetry->drawArgs.BaseVertexLocation == 0 &&
        telemetry->drawArgs.StartInstanceLocation == 0;
    if (!summary.drawArgsReadbackValid) {
        summary.drawArgsFailureReason = "draw args readback waiting";
    } else if (summary.expectedDrawCount == 0) {
        summary.drawArgsFailureReason = "expected draw count waiting";
    } else if (telemetry->drawArgs.IndexCountPerInstance != summary.expectedIndexCount) {
        summary.drawArgsFailureReason = "draw args index count mismatch";
    } else if (telemetry->drawArgs.InstanceCount != summary.expectedDrawCount) {
        summary.drawArgsFailureReason = "draw args instance count mismatch";
    } else if (telemetry->drawArgs.StartIndexLocation != 0 ||
        telemetry->drawArgs.BaseVertexLocation != 0 ||
        telemetry->drawArgs.StartInstanceLocation != 0) {
        summary.drawArgsFailureReason = "draw args offsets mismatch";
    }

    summary.sampleCount = (std::min)(telemetry->sampledRenderCount, telemetry->sampledStateCount);
    summary.firstFailureReason = "none";
    if (summary.sampleCount == 0) {
        summary.firstFailureReason = "no readback samples";
        return summary;
    }

    for (uint32_t i = 0; i < summary.sampleCount; ++i) {
        const ParticleDedicatedReadbackRowValidation row = ValidateParticleDedicatedReadbackRow(*telemetry, i);
        summary.maxPositionDelta = (std::max)(summary.maxPositionDelta, row.positionDelta);
        if (row.rowOk) {
            ++summary.okRows;
            continue;
        }

        ++summary.ngRows;
        if (!row.hasState || !row.hasRender) {
            SetFirstParticleReadbackFailure(summary, i, "missing paired sample");
        } else if (!row.finiteState) {
            SetFirstParticleReadbackFailure(summary, i, "state contains NaN/Inf");
        } else if (!row.finiteRender) {
            SetFirstParticleReadbackFailure(summary, i, "render contains NaN/Inf");
        } else if (!row.lifetimeOk) {
            SetFirstParticleReadbackFailure(summary, i, "lifetime invalid");
        } else if (!row.ageOk) {
            SetFirstParticleReadbackFailure(summary, i, "age outside lifetime");
        } else if (!row.alphaOk) {
            SetFirstParticleReadbackFailure(summary, i, "render alpha out of range");
        } else if (!row.positionMatches) {
            SetFirstParticleReadbackFailure(summary, i, "state/render position mismatch");
        }
    }

    return summary;
}

DistortionDedicatedReadbackValidationSummary BuildDistortionDedicatedReadbackValidationSummary(
    const AppGpuParticleSystem::DistortionDedicatedReadbackTelemetry* telemetry,
    uint32_t expectedDrawCount) {
    DistortionDedicatedReadbackValidationSummary summary{};
    summary.expectedIndexCount = 6;
    summary.expectedDrawCount = expectedDrawCount;
    if (telemetry == nullptr || !telemetry->valid) {
        return summary;
    }

    summary.telemetryValid = true;
    summary.renderSamples = telemetry->sampledRenderCount;
    summary.drawArgsReadbackValid = telemetry->drawArgsValid;
    summary.actualIndexCount = telemetry->drawArgs.IndexCountPerInstance;
    summary.actualDrawCount = telemetry->drawArgs.InstanceCount;
    summary.readbackFailureReason = "none";
    summary.drawArgsFailureReason = "none";

    summary.renderBufferValidationOk = summary.renderSamples > 0;
    for (uint32_t i = 0; i < telemetry->sampledRenderCount; ++i) {
        const auto& render = telemetry->renderSamples[i];
        const Vector3 renderPosition{render.World.m[3][0], render.World.m[3][1], render.World.m[3][2]};
        if (!IsFiniteVector3(renderPosition) || !IsFiniteVector4(render.color)) {
            summary.renderBufferValidationOk = false;
            std::ostringstream stream;
            stream << "render sample " << i << " contains NaN/Inf";
            summary.readbackFailureReason = stream.str();
            break;
        }
    }
    if (summary.renderSamples == 0) {
        summary.readbackFailureReason = "no render buffer samples";
    }

    summary.drawArgsValidationOk =
        summary.drawArgsReadbackValid &&
        summary.expectedDrawCount > 0 &&
        telemetry->drawArgs.IndexCountPerInstance == summary.expectedIndexCount &&
        telemetry->drawArgs.InstanceCount == summary.expectedDrawCount &&
        telemetry->drawArgs.StartIndexLocation == 0 &&
        telemetry->drawArgs.BaseVertexLocation == 0 &&
        telemetry->drawArgs.StartInstanceLocation == 0;
    if (!summary.drawArgsReadbackValid) {
        summary.drawArgsFailureReason = "draw args readback waiting";
    } else if (summary.expectedDrawCount == 0) {
        summary.drawArgsFailureReason = "expected draw count waiting";
    } else if (telemetry->drawArgs.IndexCountPerInstance != summary.expectedIndexCount) {
        summary.drawArgsFailureReason = "draw args index count mismatch";
    } else if (telemetry->drawArgs.InstanceCount != summary.expectedDrawCount) {
        summary.drawArgsFailureReason = "draw args instance count mismatch";
    } else if (telemetry->drawArgs.StartIndexLocation != 0 ||
        telemetry->drawArgs.BaseVertexLocation != 0 ||
        telemetry->drawArgs.StartInstanceLocation != 0) {
        summary.drawArgsFailureReason = "draw args offsets mismatch";
    }
    return summary;
}

BeamDedicatedReadbackValidationSummary BuildBeamDedicatedReadbackValidationSummary(
    const AppGpuParticleSystem::BeamDedicatedReadbackTelemetry* telemetry,
    uint32_t expectedDrawCount) {
    BeamDedicatedReadbackValidationSummary summary{};
    summary.expectedIndexCount = 6;
    summary.expectedDrawCount = expectedDrawCount;
    if (telemetry == nullptr || !telemetry->valid) {
        return summary;
    }

    summary.telemetryValid = true;
    summary.renderSamples = telemetry->sampledRenderCount;
    summary.drawArgsReadbackValid = telemetry->drawArgsValid;
    summary.actualIndexCount = telemetry->drawArgs.IndexCountPerInstance;
    summary.actualDrawCount = telemetry->drawArgs.InstanceCount;
    summary.readbackFailureReason = "none";
    summary.drawArgsFailureReason = "none";

    summary.renderBufferValidationOk = summary.renderSamples > 0;
    for (uint32_t i = 0; i < telemetry->sampledRenderCount; ++i) {
        const auto& render = telemetry->renderSamples[i];
        const Vector3 renderPosition{render.World.m[3][0], render.World.m[3][1], render.World.m[3][2]};
        if (!IsFiniteVector3(renderPosition) || !IsFiniteVector4(render.color)) {
            summary.renderBufferValidationOk = false;
            std::ostringstream stream;
            stream << "render sample " << i << " contains NaN/Inf";
            summary.readbackFailureReason = stream.str();
            break;
        }
    }
    if (summary.renderSamples == 0) {
        summary.readbackFailureReason = "no render buffer samples";
    }

    summary.drawArgsValidationOk =
        summary.drawArgsReadbackValid &&
        summary.expectedDrawCount > 0 &&
        telemetry->drawArgs.IndexCountPerInstance == summary.expectedIndexCount &&
        telemetry->drawArgs.InstanceCount == summary.expectedDrawCount &&
        telemetry->drawArgs.StartIndexLocation == 0 &&
        telemetry->drawArgs.BaseVertexLocation == 0 &&
        telemetry->drawArgs.StartInstanceLocation == 0;
    if (!summary.drawArgsReadbackValid) {
        summary.drawArgsFailureReason = "draw args readback waiting";
    } else if (summary.expectedDrawCount == 0) {
        summary.drawArgsFailureReason = "expected draw count waiting";
    } else if (telemetry->drawArgs.IndexCountPerInstance != summary.expectedIndexCount) {
        summary.drawArgsFailureReason = "draw args index count mismatch";
    } else if (telemetry->drawArgs.InstanceCount != summary.expectedDrawCount) {
        summary.drawArgsFailureReason = "draw args instance count mismatch";
    } else if (telemetry->drawArgs.StartIndexLocation != 0 ||
        telemetry->drawArgs.BaseVertexLocation != 0 ||
        telemetry->drawArgs.StartInstanceLocation != 0) {
        summary.drawArgsFailureReason = "draw args offsets mismatch";
    }
    return summary;
}

bool IsParticleDedicatedDrawPath(const vfx::ParticleRendererOperationalStatus& probeStatus) {
    return std::string_view(probeStatus.renderBufferResource) == "ParticleDedicatedRenderBuffer" &&
        std::string_view(probeStatus.indirectArgsResource) == "ParticleDedicatedIndirectArgs";
}

ParticleDedicatedOperationalHealthSummary BuildParticleDedicatedOperationalHealthSummary(
    bool probeEnabled,
    const vfx::ParticleRendererOperationalStatus& probeStatus,
    const ParticleRenderGraphIntentTelemetry& graphIntent,
    const AppGpuParticleSystem::ParticleSimulationTelemetry* simulationTelemetry,
    const ParticleDedicatedReadbackValidationSummary& readbackSummary) {
    ParticleDedicatedOperationalHealthSummary summary{};
    summary.probeEnabled = probeEnabled;
    summary.graphReady = graphIntent.ready;
    summary.handlesReady = probeStatus.resourceHandlesReady;
    summary.simulationTelemetryValid = simulationTelemetry != nullptr && simulationTelemetry->valid;
    summary.simulationDedicated =
        summary.simulationTelemetryValid &&
        simulationTelemetry->usedDedicatedResources;
    summary.drawDedicated = IsParticleDedicatedDrawPath(probeStatus);
    summary.readbackValid = readbackSummary.telemetryValid;
    summary.readbackValidationOk = readbackSummary.AllRowsOk();
    summary.drawArgsReadbackValid = readbackSummary.drawArgsReadbackValid;
    summary.drawArgsValidationOk = readbackSummary.drawArgsValidationOk;
    summary.readbackReady = summary.readbackValid && summary.drawArgsReadbackValid;
    summary.validationOk = summary.readbackValidationOk && summary.drawArgsValidationOk;
    summary.okRows = readbackSummary.okRows;
    summary.ngRows = readbackSummary.ngRows;
    summary.sampleCount = readbackSummary.sampleCount;
    summary.expectedDrawCount = readbackSummary.expectedDrawCount;
    summary.actualDrawCount = readbackSummary.actualDrawCount;
    summary.expectedIndexCount = readbackSummary.expectedIndexCount;
    summary.actualIndexCount = readbackSummary.actualIndexCount;
    summary.maxPositionDelta = readbackSummary.maxPositionDelta;
    summary.firstFailureReason = readbackSummary.firstFailureReason;
    summary.drawArgsFailureReason = readbackSummary.drawArgsFailureReason;
    summary.healthy =
        summary.probeEnabled &&
        summary.graphReady &&
        summary.handlesReady &&
        summary.simulationDedicated &&
        summary.drawDedicated &&
        summary.readbackReady &&
        summary.validationOk;

    summary.fallbackReason = "ready";
    if (!summary.probeEnabled) {
        summary.fallbackReason = "probe disabled";
    } else if (!summary.graphReady) {
        summary.fallbackReason = graphIntent.fallbackReason;
    } else if (!summary.handlesReady) {
        summary.fallbackReason = probeStatus.resourceHandleFallbackReason;
    } else if (!summary.simulationTelemetryValid) {
        summary.fallbackReason = "simulation telemetry waiting";
    } else if (!summary.simulationDedicated) {
        summary.fallbackReason = "simulation path not dedicated";
    } else if (!summary.drawDedicated) {
        summary.fallbackReason = "draw path not dedicated";
    } else if (!summary.readbackReady) {
        summary.fallbackReason = "readback telemetry waiting";
    } else if (!summary.readbackValidationOk) {
        summary.fallbackReason = summary.firstFailureReason;
    } else if (!summary.drawArgsValidationOk) {
        summary.fallbackReason = summary.drawArgsFailureReason;
    }
    return summary;
}

DistortionDedicatedOperationalHealthSummary BuildDistortionDedicatedOperationalHealthSummary(
    bool enabled,
    const vfx::DistortionRendererOperationalStatus& status,
    const DistortionRenderGraphIntentTelemetry& graphIntent,
    const DistortionDedicatedReadbackValidationSummary& readbackSummary) {
    DistortionDedicatedOperationalHealthSummary summary{};
    summary.enabled = enabled;
    summary.graphReady = graphIntent.ready;
    summary.handlesReady = status.resourceHandlesReady;
    summary.drawDedicated =
        std::string_view(status.renderBufferResource) == "DistortionDedicatedRenderBuffer" &&
        std::string_view(status.indirectArgsResource) == "DistortionDedicatedIndirectArgs";
    summary.readbackValid = readbackSummary.telemetryValid;
    summary.drawArgsReadbackValid = readbackSummary.drawArgsReadbackValid;
    summary.renderBufferValidationOk = readbackSummary.renderBufferValidationOk;
    summary.drawArgsValidationOk = readbackSummary.drawArgsValidationOk;
    summary.readbackReady = summary.readbackValid && summary.drawArgsReadbackValid;
    summary.validationOk = summary.renderBufferValidationOk && summary.drawArgsValidationOk;
    summary.renderSamples = readbackSummary.renderSamples;
    summary.expectedDrawCount = readbackSummary.expectedDrawCount;
    summary.actualDrawCount = readbackSummary.actualDrawCount;
    summary.expectedIndexCount = readbackSummary.expectedIndexCount;
    summary.actualIndexCount = readbackSummary.actualIndexCount;
    summary.readbackFailureReason = readbackSummary.readbackFailureReason;
    summary.drawArgsFailureReason = readbackSummary.drawArgsFailureReason;
    summary.healthy =
        summary.enabled &&
        summary.graphReady &&
        summary.handlesReady &&
        status.operationalOk &&
        summary.drawDedicated &&
        summary.readbackReady &&
        summary.validationOk;
    summary.fallbackReason = "ready";
    if (!summary.enabled) {
        summary.fallbackReason = "disabled";
    } else if (!summary.graphReady) {
        summary.fallbackReason = graphIntent.fallbackReason;
    } else if (!status.operationalOk) {
        summary.fallbackReason = status.fallbackReason;
    } else if (!summary.drawDedicated) {
        summary.fallbackReason = "distortion dedicated draw path missing";
    } else if (!summary.readbackReady) {
        summary.fallbackReason = "readback telemetry waiting";
    } else if (!summary.renderBufferValidationOk) {
        summary.fallbackReason = summary.readbackFailureReason;
    } else if (!summary.drawArgsValidationOk) {
        summary.fallbackReason = summary.drawArgsFailureReason;
    }
    return summary;
}

BeamDedicatedOperationalHealthSummary BuildBeamDedicatedOperationalHealthSummary(
    const vfx::BeamRendererOperationalStatus& status,
    const BeamRenderGraphIntentTelemetry& graphIntent,
    const BeamDedicatedReadbackValidationSummary& readbackSummary) {
    BeamDedicatedOperationalHealthSummary summary{};
    summary.graphReady = graphIntent.ready;
    summary.handlesReady = status.resourceHandlesReady;
    summary.drawDedicated =
        std::string_view(status.renderBufferResource) == "BeamDedicatedRenderBuffer" &&
        std::string_view(status.indirectArgsResource) == "BeamDedicatedIndirectArgs";
    summary.readbackValid = readbackSummary.telemetryValid;
    summary.drawArgsReadbackValid = readbackSummary.drawArgsReadbackValid;
    summary.renderBufferValidationOk = readbackSummary.renderBufferValidationOk;
    summary.drawArgsValidationOk = readbackSummary.drawArgsValidationOk;
    summary.readbackReady = summary.readbackValid && summary.drawArgsReadbackValid;
    summary.validationOk = summary.renderBufferValidationOk && summary.drawArgsValidationOk;
    summary.renderSamples = readbackSummary.renderSamples;
    summary.expectedDrawCount = readbackSummary.expectedDrawCount;
    summary.actualDrawCount = readbackSummary.actualDrawCount;
    summary.expectedIndexCount = readbackSummary.expectedIndexCount;
    summary.actualIndexCount = readbackSummary.actualIndexCount;
    summary.readbackFailureReason = readbackSummary.readbackFailureReason;
    summary.drawArgsFailureReason = readbackSummary.drawArgsFailureReason;
    summary.healthy =
        summary.graphReady &&
        summary.handlesReady &&
        status.operationalOk &&
        summary.drawDedicated &&
        summary.readbackReady &&
        summary.validationOk;
    summary.fallbackReason = "ready";
    if (!summary.graphReady) {
        summary.fallbackReason = graphIntent.fallbackReason;
    } else if (!status.operationalOk) {
        summary.fallbackReason = status.fallbackReason;
    } else if (!summary.drawDedicated) {
        summary.fallbackReason = "beam dedicated draw path missing";
    } else if (!summary.readbackReady) {
        summary.fallbackReason = "readback telemetry waiting";
    } else if (!summary.renderBufferValidationOk) {
        summary.fallbackReason = summary.readbackFailureReason;
    } else if (!summary.drawArgsValidationOk) {
        summary.fallbackReason = summary.drawArgsFailureReason;
    }
    return summary;
}

ParticleDedicatedRuntimeTelemetryUpdateResult UpdateParticleDedicatedRuntimeTelemetry(
    const ParticleDedicatedRuntimeTelemetryUpdateInput& input) {
    ParticleDedicatedRuntimeTelemetryUpdateResult result{};
    if (input.runtimeState == nullptr ||
        input.status == nullptr ||
        input.graphIntent == nullptr ||
        input.healthFrames == nullptr ||
        input.probeStableFrames == nullptr ||
        input.activeStableFrames == nullptr) {
        return result;
    }

    AppVfxRuntimeState& runtimeState = *input.runtimeState;
    result.readbackSummary = BuildParticleDedicatedReadbackValidationSummary(
        input.readbackTelemetry,
        input.simulationTelemetry);
    runtimeState.particleDedicatedHealth =
        BuildParticleDedicatedOperationalHealthSummary(
            runtimeState.enableParticleDedicatedResourceProbe,
            *input.status,
            *input.graphIntent,
            input.simulationTelemetry,
            result.readbackSummary);

    result.warmup =
        runtimeState.enableParticleDedicatedResourceProbe &&
        *input.healthFrames < result.warmupFrames;
    if (!runtimeState.enableParticleDedicatedResourceProbe) {
        runtimeState.particleDedicatedResourceFallbackActive = false;
        *input.healthFrames = 0;
        *input.probeStableFrames = 0;
        *input.activeStableFrames = 0;
    } else if (result.warmup) {
        runtimeState.particleDedicatedResourceFallbackActive = false;
    } else if (runtimeState.enableParticleDedicatedAutoFallback) {
        runtimeState.particleDedicatedResourceFallbackActive =
            !runtimeState.particleDedicatedHealth.healthy;
    } else {
        runtimeState.particleDedicatedResourceFallbackActive = false;
    }

    if (!result.warmup && runtimeState.particleDedicatedHealth.healthy) {
        ++(*input.probeStableFrames);
    } else {
        *input.probeStableFrames = 0;
    }

    const bool effectiveActive =
        runtimeState.enableParticleDedicatedResourceProbe &&
        !runtimeState.particleDedicatedResourceFallbackActive;
    const bool activeHealthy =
        !result.warmup &&
        effectiveActive &&
        runtimeState.particleDedicatedHealth.healthy;
    if (activeHealthy) {
        ++(*input.activeStableFrames);
    } else if (!result.warmup) {
        *input.activeStableFrames = 0;
    }

    if (runtimeState.enableParticleDedicatedResourceProbe) {
        ++(*input.healthFrames);
    }

    result.readyToEnable = *input.probeStableFrames >= result.readyToEnableFrames;
    result.activeStable = *input.activeStableFrames >= result.activeStableRequiredFrames;
    runtimeState.particleDedicatedProbeStableFrames = *input.probeStableFrames;
    runtimeState.particleDedicatedActiveStableFrames = *input.activeStableFrames;
    runtimeState.particleDedicatedDefaultONCandidate =
        result.readyToEnable && result.activeStable;
    runtimeState.particleDedicatedDefaultOnHealth = ParticleDedicatedDefaultOnHealthLabel(
        runtimeState.enableParticleDedicatedResourceProbe,
        result.warmup,
        effectiveActive,
        result.readyToEnable,
        result.activeStable,
        runtimeState.particleDedicatedHealth.healthy);

    return result;
}

DistortionDedicatedRuntimeTelemetryUpdateResult UpdateDistortionDedicatedRuntimeTelemetry(
    const DistortionDedicatedRuntimeTelemetryUpdateInput& input) {
    DistortionDedicatedRuntimeTelemetryUpdateResult result{};
    if (input.runtimeState == nullptr ||
        input.status == nullptr ||
        input.graphIntent == nullptr ||
        input.healthFrames == nullptr ||
        input.stableFrames == nullptr ||
        input.activeStableFrames == nullptr ||
        input.telemetryFrames == nullptr) {
        return result;
    }

    AppVfxRuntimeState& runtimeState = *input.runtimeState;
    const bool enabled = runtimeState.enableDistortionDedicatedResources;
    result.readbackSummary = BuildDistortionDedicatedReadbackValidationSummary(
        input.readbackTelemetry,
        input.expectedDrawCount);
    runtimeState.distortionDedicatedHealth =
        BuildDistortionDedicatedOperationalHealthSummary(
            enabled,
            *input.status,
            *input.graphIntent,
            result.readbackSummary);

    result.warmup =
        enabled &&
        *input.healthFrames < result.warmupFrames;
    if (!enabled) {
        runtimeState.distortionDedicatedResourceFallbackActive = false;
        *input.healthFrames = 0;
        *input.stableFrames = 0;
        *input.activeStableFrames = 0;
    } else if (result.warmup) {
        runtimeState.distortionDedicatedResourceFallbackActive = false;
    } else if (runtimeState.enableDistortionDedicatedAutoFallback) {
        runtimeState.distortionDedicatedResourceFallbackActive =
            !runtimeState.distortionDedicatedHealth.healthy;
    } else {
        runtimeState.distortionDedicatedResourceFallbackActive = false;
    }

    if (enabled) {
        ++(*input.healthFrames);
    }

    if (!result.warmup && runtimeState.distortionDedicatedHealth.healthy) {
        ++(*input.stableFrames);
    } else {
        *input.stableFrames = 0;
    }

    const bool effectiveActive =
        enabled &&
        !runtimeState.distortionDedicatedResourceFallbackActive;
    const bool activeHealthy =
        !result.warmup &&
        effectiveActive &&
        runtimeState.distortionDedicatedHealth.healthy;
    if (activeHealthy) {
        ++(*input.activeStableFrames);
    } else if (!result.warmup) {
        *input.activeStableFrames = 0;
    }

    result.stableEnough = *input.stableFrames >= result.stableRequiredFrames;
    result.activeStable = *input.activeStableFrames >= result.activeStableRequiredFrames;
    runtimeState.distortionDedicatedStableFrames = *input.stableFrames;
    runtimeState.distortionDedicatedActiveStableFrames = *input.activeStableFrames;
    runtimeState.distortionDedicatedOperationalStable =
        result.stableEnough && result.activeStable;
    runtimeState.distortionDedicatedDefaultOnHealth = DistortionDedicatedDefaultOnHealthLabel(
        enabled,
        result.warmup,
        effectiveActive,
        result.stableEnough,
        result.activeStable,
        runtimeState.distortionDedicatedHealth.healthy);

    if (runtimeState.enableDistortionDedicatedTelemetry &&
        *input.telemetryFrames < result.telemetryFrameLimit) {
        WriteDistortionDedicatedTelemetry(
            *input.telemetryFrames,
            *input.status,
            *input.graphIntent,
            runtimeState.distortionDedicatedHealth,
            runtimeState.enableDistortionDedicatedAutoFallback,
            runtimeState.distortionDedicatedResourceFallbackActive,
            runtimeState.distortionDedicatedStableFrames,
            runtimeState.distortionDedicatedActiveStableFrames,
            runtimeState.distortionDedicatedOperationalStable,
            runtimeState.distortionDedicatedDefaultOnHealth.c_str(),
            result.warmup,
            *input.telemetryFrames == 0);
        ++(*input.telemetryFrames);
    }

    return result;
}

ParticleDedicatedResourceProbeTelemetryResult BuildParticleDedicatedResourceProbeTelemetry(
    const ParticleDedicatedResourceProbeTelemetryInput& input) {
    ParticleDedicatedResourceProbeTelemetryResult result{};
    result.readbackSummary = BuildParticleDedicatedReadbackValidationSummary(
        input.readbackTelemetry,
        input.simulationTelemetry);

    if (input.runtimeState == nullptr ||
        input.status == nullptr ||
        input.graphIntent == nullptr ||
        input.telemetryFrames == nullptr) {
        return result;
    }

    const AppVfxRuntimeState& runtimeState = *input.runtimeState;
    if (runtimeState.enableParticleDedicatedProbeTelemetry &&
        *input.telemetryFrames < result.telemetryFrameLimit) {
        WriteParticleDedicatedProbeTelemetry(
            *input.telemetryFrames,
            *input.status,
            *input.graphIntent,
            input.simulationTelemetry,
            input.readbackTelemetry,
            runtimeState.particleDedicatedHealth,
            runtimeState.enableParticleDedicatedAutoFallback,
            runtimeState.particleDedicatedResourceFallbackActive,
            runtimeState.particleDedicatedProbeStableFrames,
            runtimeState.particleDedicatedActiveStableFrames,
            runtimeState.particleDedicatedDefaultONCandidate,
            runtimeState.particleDedicatedDefaultOnHealth.c_str(),
            *input.telemetryFrames == 0);
        ++(*input.telemetryFrames);
    }

    return result;
}

BeamDedicatedRuntimeTelemetryUpdateResult UpdateBeamDedicatedRuntimeTelemetry(
    const BeamDedicatedRuntimeTelemetryUpdateInput& input) {
    BeamDedicatedRuntimeTelemetryUpdateResult result{};
    if (input.runtimeState == nullptr ||
        input.status == nullptr ||
        input.graphIntent == nullptr ||
        input.healthFrames == nullptr ||
        input.stableFrames == nullptr ||
        input.activeStableFrames == nullptr ||
        input.telemetryFrames == nullptr) {
        return result;
    }

    AppVfxRuntimeState& runtimeState = *input.runtimeState;
    result.readbackSummary = BuildBeamDedicatedReadbackValidationSummary(
        input.readbackTelemetry,
        input.expectedDrawCount);
    runtimeState.beamDedicatedHealth =
        BuildBeamDedicatedOperationalHealthSummary(
            *input.status,
            *input.graphIntent,
            result.readbackSummary);

    result.warmup = *input.healthFrames < result.warmupFrames;
    if (result.warmup) {
        runtimeState.beamDedicatedResourceFallbackActive = false;
    } else if (runtimeState.enableBeamDedicatedAutoFallback) {
        runtimeState.beamDedicatedResourceFallbackActive =
            !runtimeState.beamDedicatedHealth.healthy;
    } else {
        runtimeState.beamDedicatedResourceFallbackActive = false;
    }
    ++(*input.healthFrames);
    result.effectiveDedicatedAfterSafety =
        !runtimeState.beamDedicatedResourceFallbackActive;

    if (!result.warmup && runtimeState.beamDedicatedHealth.healthy) {
        ++(*input.stableFrames);
        ++(*input.activeStableFrames);
    } else if (!result.warmup) {
        *input.stableFrames = 0;
        *input.activeStableFrames = 0;
    }

    result.stableEnough = *input.stableFrames >= result.stableRequiredFrames;
    result.activeStable = *input.activeStableFrames >= result.activeStableRequiredFrames;
    runtimeState.beamDedicatedStableFrames = *input.stableFrames;
    runtimeState.beamDedicatedActiveStableFrames = *input.activeStableFrames;
    runtimeState.beamDedicatedOperationalStable =
        result.stableEnough && result.activeStable;
    runtimeState.beamDedicatedDefaultOnReady =
        result.stableEnough && result.activeStable;
    runtimeState.beamDedicatedDefaultOnHealth = BeamDedicatedDefaultOnHealthLabel(
        result.warmup,
        result.stableEnough,
        result.activeStable,
        runtimeState.beamDedicatedHealth.healthy);

    if (runtimeState.enableBeamDedicatedTelemetry &&
        *input.telemetryFrames < result.telemetryFrameLimit) {
        WriteBeamDedicatedTelemetry(
            *input.telemetryFrames,
            *input.status,
            *input.graphIntent,
            runtimeState.beamDedicatedHealth,
            result.warmup,
            runtimeState.enableBeamDedicatedAutoFallback,
            runtimeState.beamDedicatedResourceFallbackActive,
            runtimeState.beamDedicatedStableFrames,
            runtimeState.beamDedicatedActiveStableFrames,
            runtimeState.beamDedicatedOperationalStable,
            runtimeState.beamDedicatedDefaultOnReady,
            runtimeState.beamDedicatedDefaultOnHealth.c_str(),
            *input.telemetryFrames == 0);
        ++(*input.telemetryFrames);
    }

    return result;
}

const char* ParticleDedicatedDefaultOnHealthLabel(
    bool requested,
    bool warmup,
    bool effectiveDedicated,
    bool readyToEnable,
    bool activeStable,
    bool healthOk) {
    if (!requested) {
        return "disabled";
    }
    if (warmup) {
        return "warmup";
    }
    if (effectiveDedicated && readyToEnable && activeStable) {
        return "stable";
    }
    if (effectiveDedicated && healthOk) {
        return "ramping";
    }
    return "attention";
}

const char* DistortionDedicatedDefaultOnHealthLabel(
    bool requested,
    bool warmup,
    bool effectiveDedicated,
    bool readyToEnable,
    bool activeStable,
    bool healthOk) {
    if (!requested) {
        return "disabled";
    }
    if (warmup) {
        return "warmup";
    }
    if (effectiveDedicated && readyToEnable && activeStable) {
        return "stable";
    }
    if (effectiveDedicated && healthOk) {
        return "ramping";
    }
    return "attention";
}

const char* BeamDedicatedDefaultOnHealthLabel(
    bool warmup,
    bool readyToEnable,
    bool activeStable,
    bool healthOk) {
    if (warmup) {
        return "warmup";
    }
    if (readyToEnable && activeStable) {
        return "stable";
    }
    if (healthOk) {
        return "ramping";
    }
    return "attention";
}

bool IsParticleDedicatedOperationalOk(const char* defaultOnHealth) {
    return defaultOnHealth != nullptr && std::string(defaultOnHealth) == "stable";
}

const char* ParticleSimulationPathLabel(const ParticleDedicatedOperationalHealthSummary& health) {
    if (!health.simulationTelemetryValid) {
        return "waiting";
    }
    return health.simulationDedicated ? "dedicated" : "legacy";
}

const char* ParticleRenderPathLabel(const ParticleDedicatedOperationalHealthSummary& health) {
    return health.drawDedicated ? "dedicated" : "legacy_or_custom";
}

const char* ParticleReadbackLabel(const ParticleDedicatedOperationalHealthSummary& health) {
    return health.readbackReady ? "valid" : "waiting";
}

const char* ParticleValidationLabel(const ParticleDedicatedOperationalHealthSummary& health) {
    return health.validationOk ? "ok" : "attention";
}

const char* DistortionReadbackLabel(const DistortionDedicatedOperationalHealthSummary& health) {
    return health.readbackReady ? "valid" : "waiting";
}

const char* DistortionValidationLabel(const DistortionDedicatedOperationalHealthSummary& health) {
    return health.validationOk ? "ok" : "attention";
}

ParticleDedicatedReadbackRowValidation ValidateParticleDedicatedReadbackRow(
    const AppGpuParticleSystem::ParticleDedicatedReadbackTelemetry& telemetry,
    uint32_t sampleIndex) {
    ParticleDedicatedReadbackRowValidation row{};
    row.hasState = sampleIndex < telemetry.sampledStateCount;
    row.hasRender = sampleIndex < telemetry.sampledRenderCount;
    if (!row.hasState || !row.hasRender) {
        return row;
    }

    const auto& state = telemetry.stateSamples[sampleIndex];
    const auto& render = telemetry.renderSamples[sampleIndex];
    const Vector3 renderPosition{render.World.m[3][0], render.World.m[3][1], render.World.m[3][2]};
    row.finiteState =
        IsFiniteVector3(state.position) &&
        IsFiniteFloat(state.age) &&
        IsFiniteVector3(state.velocity) &&
        IsFiniteFloat(state.lifetime) &&
        IsFiniteVector4(state.color) &&
        IsFiniteVector3(state.scale) &&
        IsFiniteFloat(state.seed);
    row.finiteRender =
        IsFiniteVector3(renderPosition) &&
        IsFiniteVector4(render.color);
    row.lifetimeOk = row.finiteState && state.lifetime > 0.0f;
    row.ageOk = row.finiteState && state.age >= -0.001f && state.age <= state.lifetime + 0.25f;
    row.alphaOk = row.finiteRender && render.color.w >= -0.001f && render.color.w <= 1.25f;
    row.positionDelta = ParticlePositionDelta(state, render);
    row.positionMatches = IsFiniteFloat(row.positionDelta) && row.positionDelta <= 0.01f;
    row.rowOk =
        row.hasState &&
        row.hasRender &&
        row.finiteState &&
        row.finiteRender &&
        row.lifetimeOk &&
        row.ageOk &&
        row.alphaOk &&
        row.positionMatches;
    return row;
}

void DrawParticleDedicatedOperationalHealthLine(
    const char* label,
    const ParticleDedicatedOperationalHealthSummary& health) {
    ImGui::TextColored(
        health.healthy ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "%s=%s simulationPath=%s renderPath=%s graph=%s handles=%s readback=%s validation=%s fallbackReason=%s",
        label,
        health.healthy ? "healthy" : "attention",
        ParticleSimulationPathLabel(health),
        ParticleRenderPathLabel(health),
        health.graphReady ? "ready" : "attention",
        health.handlesReady ? "ready" : "attention",
        ParticleReadbackLabel(health),
        ParticleValidationLabel(health),
        health.fallbackReason.c_str());
}

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
    bool resetFile) {
    std::ofstream stream(
        "particle_dedicated_probe_telemetry.log",
        resetFile ? std::ios::trunc : std::ios::app);
    if (!stream) {
        return;
    }

    if (resetFile) {
        stream << "frame ParticleDedicatedOperational simulationPath renderPath graph handles readback validation fallbackReason fallbackActive fallbackTarget autoFallback probeStableFrames activeStableFrames defaultONCandidate defaultOnHealth simulationTelemetry renderBuffer renderBufferUav state stateUav renderBufferResource indirectArgsResource groups maxParticles sampleReadback sampleValidation drawArgsReadback drawArgsValidation expectedDrawCount actualDrawCount expectedIndexCount actualIndexCount drawArgsFirstFailure okRows ngRows maxPosDelta firstFailure renderSamples stateSamples state0Pos state0Age render0World render0Color statusReason graphReason\n";
    }

    const bool telemetryValid = simulationTelemetry != nullptr && simulationTelemetry->valid;
    const bool readbackValid = readbackTelemetry != nullptr && readbackTelemetry->valid;
    const AppGpuParticleSystem::ParticleStateSample* state0 =
        readbackValid && readbackTelemetry->sampledStateCount > 0 ? &readbackTelemetry->stateSamples[0] : nullptr;
    const AppGpuParticleSystem::ParticleRenderSample* render0 =
        readbackValid && readbackTelemetry->sampledRenderCount > 0 ? &readbackTelemetry->renderSamples[0] : nullptr;
    stream << frameIndex
           << " ParticleDedicatedOperational=" << (operationalHealth.healthy ? "healthy" : "attention")
           << " simulationPath=" << ParticleSimulationPathLabel(operationalHealth)
           << " renderPath=" << ParticleRenderPathLabel(operationalHealth)
           << " graph=" << (graphIntent.ready ? "ready" : "attention")
           << " handles=" << (probeStatus.resourceHandlesReady ? "ready" : "attention")
           << " readback=" << ParticleReadbackLabel(operationalHealth)
           << " validation=" << ParticleValidationLabel(operationalHealth)
           << " fallbackReason=\"" << operationalHealth.fallbackReason << "\""
           << " fallbackActive=" << (fallbackActive ? "yes" : "no")
           << " fallbackTarget=" << (fallbackActive ? "legacy" : "dedicated")
           << " autoFallback=" << (autoFallbackEnabled ? "on" : "off")
           << " probeStableFrames=" << probeStableFrames
           << " activeStableFrames=" << activeStableFrames
           << " defaultONCandidate=" << (defaultOnCandidate ? "yes" : "no")
           << " defaultOnHealth=" << (defaultOnHealth != nullptr ? defaultOnHealth : "unknown")
           << " simulationTelemetry=" << (telemetryValid ? "valid" : "waiting")
           << " renderBuffer=\"" << (telemetryValid ? simulationTelemetry->renderBufferResource : "")
           << "\" renderBufferUav=0x" << std::hex
           << (telemetryValid ? static_cast<unsigned long long>(simulationTelemetry->renderBufferUav.ptr) : 0ull)
           << std::dec
           << " state=\"" << (telemetryValid ? simulationTelemetry->stateBufferResource : "")
           << "\" stateUav=0x" << std::hex
           << (telemetryValid ? static_cast<unsigned long long>(simulationTelemetry->stateBufferUav.ptr) : 0ull)
           << std::dec
           << " renderBufferResource=\"" << probeStatus.renderBufferResource << "\""
           << " indirectArgsResource=\"" << probeStatus.indirectArgsResource << "\""
           << " groups=" << (telemetryValid ? simulationTelemetry->dispatchGroupCount : 0u)
           << " maxParticles=" << (telemetryValid ? simulationTelemetry->maxParticles : 0u)
           << " sampleReadback=" << (operationalHealth.readbackValid ? "valid" : "waiting")
           << " sampleValidation=" << (operationalHealth.readbackValidationOk ? "ok" : "attention")
           << " drawArgsReadback=" << (operationalHealth.drawArgsReadbackValid ? "valid" : "waiting")
           << " drawArgsValidation=" << (operationalHealth.drawArgsValidationOk ? "ok" : "attention")
           << " expectedDrawCount=" << operationalHealth.expectedDrawCount
           << " actualDrawCount=" << operationalHealth.actualDrawCount
           << " expectedIndexCount=" << operationalHealth.expectedIndexCount
           << " actualIndexCount=" << operationalHealth.actualIndexCount
           << " drawArgsFirstFailure=\"" << operationalHealth.drawArgsFailureReason << "\""
           << " okRows=" << operationalHealth.okRows
           << " ngRows=" << operationalHealth.ngRows
           << " maxPosDelta=" << operationalHealth.maxPositionDelta
           << " firstFailure=\"" << operationalHealth.firstFailureReason << "\""
           << " renderSamples=" << (readbackValid ? readbackTelemetry->sampledRenderCount : 0u)
           << " stateSamples=" << (readbackValid ? readbackTelemetry->sampledStateCount : 0u)
           << " state0Pos=\""
           << (state0 != nullptr ? state0->position.x : 0.0f) << ","
           << (state0 != nullptr ? state0->position.y : 0.0f) << ","
           << (state0 != nullptr ? state0->position.z : 0.0f) << "\""
           << " state0Age=" << (state0 != nullptr ? state0->age : 0.0f)
           << " render0World=\""
           << (render0 != nullptr ? render0->World.m[3][0] : 0.0f) << ","
           << (render0 != nullptr ? render0->World.m[3][1] : 0.0f) << ","
           << (render0 != nullptr ? render0->World.m[3][2] : 0.0f) << "\""
           << " render0Color=\""
           << (render0 != nullptr ? render0->color.x : 0.0f) << ","
           << (render0 != nullptr ? render0->color.y : 0.0f) << ","
           << (render0 != nullptr ? render0->color.z : 0.0f) << ","
           << (render0 != nullptr ? render0->color.w : 0.0f) << "\""
           << " statusReason=\"" << probeStatus.fallbackReason << "\""
           << " graphReason=\"" << graphIntent.fallbackReason << "\"\n";
}

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
    bool resetFile) {
    std::ofstream stream(
        "distortion_dedicated_telemetry.log",
        resetFile ? std::ios::trunc : std::ios::app);
    if (!stream) {
        return;
    }

    if (resetFile) {
        stream << "frame DistortionDedicatedOperational graph handles readback validation simulationPass simulationStateRead simulationStateWrite simulationRenderBufferWrite drawPath fallbackReason fallbackActive fallbackTarget autoFallback warmup stableFrames activeStableFrames operationalStable defaultOnHealth drawPass renderBufferRead indirectArgsRead simulationStateResource simulationRenderBufferResource renderBufferResource indirectArgsResource simulationStateUav simulationRenderBufferUav renderBufferSrv indirectArgs renderSamples renderBufferValidation drawArgsReadback drawArgsValidation expectedDrawCount actualDrawCount expectedIndexCount actualIndexCount readbackFailure drawArgsFailure statusReason graphReason handleReason\n";
    }

    stream << frameIndex
           << " DistortionDedicatedOperational=" << (operationalHealth.healthy ? "healthy" : "attention")
           << " graph=" << (graphIntent.ready ? "ready" : "attention")
           << " handles=" << (status.resourceHandlesReady ? "ready" : "attention")
           << " readback=" << DistortionReadbackLabel(operationalHealth)
           << " validation=" << DistortionValidationLabel(operationalHealth)
           << " simulationPass=" << (graphIntent.simulationPassFound ? (graphIntent.simulationPassExecuted ? "executed" : "culled") : "missing")
           << " simulationStateRead=" << (graphIntent.simulationStateRead ? "yes" : "no")
           << " simulationStateWrite=" << (graphIntent.simulationStateWrite ? "yes" : "no")
           << " simulationRenderBufferWrite=" << (graphIntent.simulationRenderBufferWrite ? "yes" : "no")
           << " drawPath=" << (operationalHealth.drawDedicated ? "dedicated" : "legacy_or_custom")
           << " fallbackReason=\"" << operationalHealth.fallbackReason << "\""
           << " fallbackActive=" << (fallbackActive ? "yes" : "no")
           << " fallbackTarget=" << (fallbackActive ? "legacy" : "dedicated")
           << " autoFallback=" << (autoFallbackEnabled ? "on" : "off")
           << " warmup=" << (warmup ? "yes" : "no")
           << " stableFrames=" << stableFrames
           << " activeStableFrames=" << activeStableFrames
           << " operationalStable=" << (operationalStable ? "yes" : "no")
           << " defaultOnHealth=" << (defaultOnHealth != nullptr ? defaultOnHealth : "unknown")
           << " drawPass=" << (graphIntent.drawPassFound ? "found" : "missing")
           << " renderBufferRead=" << (graphIntent.drawRenderBufferRead ? "yes" : "no")
           << " indirectArgsRead=" << (graphIntent.drawIndirectArgsRead ? "yes" : "no")
           << " simulationStateResource=\"" << status.simulationStateResource << "\""
           << " simulationRenderBufferResource=\"" << status.simulationRenderBufferResource << "\""
           << " renderBufferResource=\"" << status.renderBufferResource << "\""
           << " indirectArgsResource=\"" << status.indirectArgsResource << "\""
           << " simulationStateUav=" << (status.simulationStateUavValid ? "valid" : "missing")
           << " simulationRenderBufferUav=" << (status.simulationRenderBufferUavValid ? "valid" : "missing")
           << " renderBufferSrv=" << (status.renderBufferSrvValid ? "valid" : "missing")
           << " indirectArgs=" << (status.indirectArgsValid ? "valid" : "missing")
           << " renderSamples=" << operationalHealth.renderSamples
           << " renderBufferValidation=" << (operationalHealth.renderBufferValidationOk ? "ok" : "attention")
           << " drawArgsReadback=" << (operationalHealth.drawArgsReadbackValid ? "valid" : "waiting")
           << " drawArgsValidation=" << (operationalHealth.drawArgsValidationOk ? "ok" : "attention")
           << " expectedDrawCount=" << operationalHealth.expectedDrawCount
           << " actualDrawCount=" << operationalHealth.actualDrawCount
           << " expectedIndexCount=" << operationalHealth.expectedIndexCount
           << " actualIndexCount=" << operationalHealth.actualIndexCount
           << " readbackFailure=\"" << operationalHealth.readbackFailureReason << "\""
           << " drawArgsFailure=\"" << operationalHealth.drawArgsFailureReason << "\""
           << " statusReason=\"" << status.fallbackReason << "\""
           << " graphReason=\"" << graphIntent.fallbackReason << "\""
           << " handleReason=\"" << status.resourceHandleFallbackReason << "\"\n";
}

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
    bool resetFile) {
    std::ofstream stream(
        "beam_dedicated_telemetry.log",
        resetFile ? std::ios::trunc : std::ios::app);
    if (!stream) {
        return;
    }

    if (resetFile) {
        stream << "frame BeamDedicatedOperational graph handles readback validation warmup fallbackActive fallbackTarget autoFallback stableFrames activeStableFrames operationalStable defaultOnReady defaultOnHealth simulationPass simulationStateRead simulationStateWrite simulationRenderBufferWrite drawPath fallbackReason drawPass renderBufferRead indirectArgsRead simulationStateResource simulationRenderBufferResource renderBufferResource indirectArgsResource simulationStateUav simulationRenderBufferUav renderBufferSrv renderBufferUav indirectArgs renderSamples renderBufferValidation drawArgsReadback drawArgsValidation expectedDrawCount actualDrawCount expectedIndexCount actualIndexCount readbackFailure drawArgsFailure statusReason graphReason handleReason\n";
    }

    stream << frameIndex
           << " BeamDedicatedOperational=" << (operationalHealth.healthy ? "healthy" : "attention")
           << " graph=" << (graphIntent.ready ? "ready" : "attention")
           << " handles=" << (status.resourceHandlesReady ? "ready" : "attention")
           << " readback=" << (operationalHealth.readbackReady ? "valid" : "waiting")
           << " validation=" << (operationalHealth.validationOk ? "ok" : "attention")
           << " warmup=" << (warmup ? "yes" : "no")
           << " fallbackActive=" << (fallbackActive ? "yes" : "no")
           << " fallbackTarget=" << (fallbackActive ? "legacy" : "dedicated")
           << " autoFallback=" << (autoFallbackEnabled ? "on" : "off")
           << " stableFrames=" << stableFrames
           << " activeStableFrames=" << activeStableFrames
           << " operationalStable=" << (operationalStable ? "yes" : "no")
           << " defaultOnReady=" << (defaultOnReady ? "yes" : "no")
           << " defaultOnHealth=" << (defaultOnHealth != nullptr ? defaultOnHealth : "unknown")
           << " simulationPass=" << (graphIntent.simulationPassFound ? (graphIntent.simulationPassExecuted ? "executed" : "culled") : "missing")
           << " simulationStateRead=" << (graphIntent.simulationStateRead ? "yes" : "no")
           << " simulationStateWrite=" << (graphIntent.simulationStateWrite ? "yes" : "no")
           << " simulationRenderBufferWrite=" << (graphIntent.simulationRenderBufferWrite ? "yes" : "no")
           << " drawPath=" << (operationalHealth.drawDedicated ? "dedicated" : "legacy_or_custom")
           << " fallbackReason=\"" << operationalHealth.fallbackReason << "\""
           << " drawPass=" << (graphIntent.drawPassFound ? (graphIntent.drawPassExecuted ? "executed" : "culled") : "missing")
           << " renderBufferRead=" << (graphIntent.drawRenderBufferRead ? "yes" : "no")
           << " indirectArgsRead=" << (graphIntent.drawIndirectArgsRead ? "yes" : "no")
           << " simulationStateResource=\"" << status.simulationStateResource << "\""
           << " simulationRenderBufferResource=\"" << status.simulationRenderBufferResource << "\""
           << " renderBufferResource=\"" << status.renderBufferResource << "\""
           << " indirectArgsResource=\"" << status.indirectArgsResource << "\""
           << " simulationStateUav=" << (status.simulationStateUavValid ? "valid" : "missing")
           << " simulationRenderBufferUav=" << (status.simulationRenderBufferUavValid ? "valid" : "missing")
           << " renderBufferSrv=" << (status.renderBufferSrvValid ? "valid" : "missing")
           << " renderBufferUav=" << (status.renderBufferUavValid ? "valid" : "missing")
           << " indirectArgs=" << (status.indirectArgsValid ? "valid" : "missing")
           << " renderSamples=" << operationalHealth.renderSamples
           << " renderBufferValidation=" << (operationalHealth.renderBufferValidationOk ? "ok" : "attention")
           << " drawArgsReadback=" << (operationalHealth.drawArgsReadbackValid ? "valid" : "waiting")
           << " drawArgsValidation=" << (operationalHealth.drawArgsValidationOk ? "ok" : "attention")
           << " expectedDrawCount=" << operationalHealth.expectedDrawCount
           << " actualDrawCount=" << operationalHealth.actualDrawCount
           << " expectedIndexCount=" << operationalHealth.expectedIndexCount
           << " actualIndexCount=" << operationalHealth.actualIndexCount
           << " readbackFailure=\"" << operationalHealth.readbackFailureReason << "\""
           << " drawArgsFailure=\"" << operationalHealth.drawArgsFailureReason << "\""
           << " statusReason=\"" << status.fallbackReason << "\""
           << " graphReason=\"" << graphIntent.fallbackReason << "\""
           << " handleReason=\"" << status.resourceHandleFallbackReason << "\"\n";
}
