#include "AppVfxTelemetryPanel.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
struct TrailMeshStreamPanelRowValidation {
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

const char* DescriptorIdOrNone(const EffectRendererDescriptor* descriptor) {
    return descriptor != nullptr && !descriptor->id.empty()
        ? descriptor->id.c_str()
        : "none";
}

const char* DescriptorIdOrNone(const EffectSimulationDescriptor* descriptor) {
    return descriptor != nullptr && !descriptor->id.empty()
        ? descriptor->id.c_str()
        : "none";
}

void DrawInputDescriptorLine(const VfxComponentInputCommon* primaryInput) {
    const EffectRendererDescriptor* rendererDescriptor =
        primaryInput != nullptr ? primaryInput->rendererDescriptor : nullptr;
    const EffectSimulationDescriptor* simulationDescriptor =
        primaryInput != nullptr ? primaryInput->simulationDescriptor : nullptr;
    ImGui::Text(
        "inputDescriptor renderer=%s simulation=%s",
        DescriptorIdOrNone(rendererDescriptor),
        DescriptorIdOrNone(simulationDescriptor));
}

TrailMeshStreamPanelRowValidation ValidateTrailMeshStreamPanelRow(
    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry,
    const vfx::TrailMeshStreamPlan& trailPlan,
    uint32_t segmentIndex) {
    TrailMeshStreamPanelRowValidation check{};
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

    const float expectedHeadT = segment.normalizedHead;
    const float expectedTailT = segment.normalizedTail;
    const float expectedHeadAlpha = check.hasHeadCp
        ? telemetry.controlPoints[segment.startControlPoint].positionAge.w
        : 0.0f;
    const float expectedTailAlpha = check.hasTailCp
        ? telemetry.controlPoints[segment.endControlPoint].positionAge.w
        : 0.0f;
    const float expectedHeadWidth = check.hasHeadCp
        ? telemetry.controlPoints[segment.startControlPoint].colorWidth.w
        : 0.0f;
    const float expectedTailWidth = check.hasTailCp
        ? telemetry.controlPoints[segment.endControlPoint].colorWidth.w
        : 0.0f;
    check.headLeftTDelta = check.hasHeadLeftVertex
        ? std::abs(telemetry.vertices[check.headLeftVertexIndex].params.x - expectedHeadT)
        : 0.0f;
    check.headRightTDelta = check.hasHeadRightVertex
        ? std::abs(telemetry.vertices[check.headRightVertexIndex].params.x - expectedHeadT)
        : 0.0f;
    check.tailLeftTDelta = check.hasTailLeftVertex
        ? std::abs(telemetry.vertices[check.tailLeftVertexIndex].params.x - expectedTailT)
        : 0.0f;
    check.tailRightTDelta = check.hasTailRightVertex
        ? std::abs(telemetry.vertices[check.tailRightVertexIndex].params.x - expectedTailT)
        : 0.0f;
    check.headLeftAlphaDelta = check.hasHeadLeftVertex
        ? std::abs(telemetry.vertices[check.headLeftVertexIndex].color.w - expectedHeadAlpha)
        : 0.0f;
    check.headRightAlphaDelta = check.hasHeadRightVertex
        ? std::abs(telemetry.vertices[check.headRightVertexIndex].color.w - expectedHeadAlpha)
        : 0.0f;
    check.tailLeftAlphaDelta = check.hasTailLeftVertex
        ? std::abs(telemetry.vertices[check.tailLeftVertexIndex].color.w - expectedTailAlpha)
        : 0.0f;
    check.tailRightAlphaDelta = check.hasTailRightVertex
        ? std::abs(telemetry.vertices[check.tailRightVertexIndex].color.w - expectedTailAlpha)
        : 0.0f;
    check.headLeftWidthDelta = check.hasHeadLeftVertex
        ? std::abs(telemetry.vertices[check.headLeftVertexIndex].params.z - expectedHeadWidth)
        : 0.0f;
    check.headRightWidthDelta = check.hasHeadRightVertex
        ? std::abs(telemetry.vertices[check.headRightVertexIndex].params.z - expectedHeadWidth)
        : 0.0f;
    check.tailLeftWidthDelta = check.hasTailLeftVertex
        ? std::abs(telemetry.vertices[check.tailLeftVertexIndex].params.z - expectedTailWidth)
        : 0.0f;
    check.tailRightWidthDelta = check.hasTailRightVertex
        ? std::abs(telemetry.vertices[check.tailRightVertexIndex].params.z - expectedTailWidth)
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

ImVec2 TrailDebugToCanvas(
    const Vector3& point,
    const ImVec2& origin,
    const ImVec2& size,
    const Vector3& minBounds,
    const Vector3& maxBounds) {
    const float width = (std::max)(0.001f, maxBounds.x - minBounds.x);
    const float height = (std::max)(0.001f, maxBounds.y - minBounds.y);
    const float scale = (std::min)((size.x - 16.0f) / width, (size.y - 16.0f) / height);
    const float centerX = (minBounds.x + maxBounds.x) * 0.5f;
    const float centerY = (minBounds.y + maxBounds.y) * 0.5f;
    return {
        origin.x + size.x * 0.5f + (point.x - centerX) * scale,
        origin.y + size.y * 0.5f - (point.y - centerY) * scale,
    };
}

void ExpandTrailDebugBounds(Vector3& minBounds, Vector3& maxBounds, const Vector3& point) {
    minBounds.x = (std::min)(minBounds.x, point.x);
    minBounds.y = (std::min)(minBounds.y, point.y);
    minBounds.z = (std::min)(minBounds.z, point.z);
    maxBounds.x = (std::max)(maxBounds.x, point.x);
    maxBounds.y = (std::max)(maxBounds.y, point.y);
    maxBounds.z = (std::max)(maxBounds.z, point.z);
}

void DrawTrailMeshStreamDebugCanvas(const vfx::TrailMeshStreamDebugPreview& preview) {
    const ImVec2 canvasSize(0.0f, 220.0f);
    ImGui::BeginChild("TrailMeshStreamVisualDebug", canvasSize, true, ImGuiWindowFlags_NoScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(18, 20, 24, 255));
    drawList->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(72, 78, 88, 255));
    ImGui::InvisibleButton("TrailMeshStreamVisualDebugCanvas", size);

    if (preview.points.empty()) {
        drawList->AddText(ImVec2(origin.x + 12.0f, origin.y + 12.0f), IM_COL32(170, 176, 188, 255), "No active Trail mesh stream preview");
        ImGui::EndChild();
        return;
    }

    Vector3 minBounds = preview.points.front().position;
    Vector3 maxBounds = preview.points.front().position;
    for (const auto& point : preview.points) {
        ExpandTrailDebugBounds(minBounds, maxBounds, point.position);
        ExpandTrailDebugBounds(minBounds, maxBounds, {
            point.position.x + point.miterOffset.x * point.width,
            point.position.y + point.miterOffset.y * point.width,
            point.position.z + point.miterOffset.z * point.width,
        });
        ExpandTrailDebugBounds(minBounds, maxBounds, {
            point.position.x - point.miterOffset.x * point.width,
            point.position.y - point.miterOffset.y * point.width,
            point.position.z - point.miterOffset.z * point.width,
        });
    }

    for (size_t i = 1; i < preview.points.size(); ++i) {
        const ImVec2 a = TrailDebugToCanvas(preview.points[i - 1].position, origin, size, minBounds, maxBounds);
        const ImVec2 b = TrailDebugToCanvas(preview.points[i].position, origin, size, minBounds, maxBounds);
        drawList->AddLine(a, b, IM_COL32(235, 235, 235, 210), 2.0f);
    }

    for (size_t i = 1; i < preview.points.size(); ++i) {
        const auto& prev = preview.points[i - 1];
        const auto& current = preview.points[i];
        const Vector3 prevLeft{
            prev.position.x - prev.miterOffset.x * prev.width,
            prev.position.y - prev.miterOffset.y * prev.width,
            prev.position.z - prev.miterOffset.z * prev.width,
        };
        const Vector3 prevRight{
            prev.position.x + prev.miterOffset.x * prev.width,
            prev.position.y + prev.miterOffset.y * prev.width,
            prev.position.z + prev.miterOffset.z * prev.width,
        };
        const Vector3 currentLeft{
            current.position.x - current.miterOffset.x * current.width,
            current.position.y - current.miterOffset.y * current.width,
            current.position.z - current.miterOffset.z * current.width,
        };
        const Vector3 currentRight{
            current.position.x + current.miterOffset.x * current.width,
            current.position.y + current.miterOffset.y * current.width,
            current.position.z + current.miterOffset.z * current.width,
        };
        drawList->AddLine(
            TrailDebugToCanvas(prevLeft, origin, size, minBounds, maxBounds),
            TrailDebugToCanvas(currentLeft, origin, size, minBounds, maxBounds),
            IM_COL32(255, 180, 80, 190),
            1.5f);
        drawList->AddLine(
            TrailDebugToCanvas(prevRight, origin, size, minBounds, maxBounds),
            TrailDebugToCanvas(currentRight, origin, size, minBounds, maxBounds),
            IM_COL32(255, 180, 80, 190),
            1.5f);
    }

    for (const auto& point : preview.points) {
        const ImVec2 center = TrailDebugToCanvas(point.position, origin, size, minBounds, maxBounds);
        const Vector3 sideTip{
            point.position.x + point.side.x * point.width,
            point.position.y + point.side.y * point.width,
            point.position.z + point.side.z * point.width,
        };
        const Vector3 miterTip{
            point.position.x + point.miterOffset.x * point.width,
            point.position.y + point.miterOffset.y * point.width,
            point.position.z + point.miterOffset.z * point.width,
        };
        drawList->AddLine(center, TrailDebugToCanvas(sideTip, origin, size, minBounds, maxBounds), IM_COL32(70, 190, 255, 220), 1.0f);
        drawList->AddLine(center, TrailDebugToCanvas(miterTip, origin, size, minBounds, maxBounds), IM_COL32(255, 235, 90, 230), 1.5f);
        drawList->AddCircleFilled(center, 3.0f, IM_COL32(120, 255, 160, 255));
    }

    drawList->AddText(ImVec2(origin.x + 10.0f, origin.y + 8.0f), IM_COL32(235, 235, 235, 235), "white=center  orange=ribbon  cyan=side  yellow=miter");
    ImGui::EndChild();
}
}

void DrawParticleDedicatedRuntimeTelemetryPanel(
    const ParticleDedicatedRuntimeTelemetryPanelInput& input) {
    if (input.health == nullptr || input.runtimeState == nullptr) {
        return;
    }

    const AppVfxRuntimeState& runtimeState = *input.runtimeState;
    const ParticleDedicatedOperationalHealthSummary& health = *input.health;

    DrawParticleDedicatedOperationalHealthLine("ParticleDedicatedOperational", health);
    DrawInputDescriptorLine(input.primaryInput);
    ImGui::TextColored(
        input.readyToEnable ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "particleDedicatedReadyToEnable=%s probeStableFrames=%u/%u",
        input.readyToEnable ? "yes" : "no",
        input.probeStableFrames,
        input.readyToEnableFrames);
    ImGui::TextColored(
        input.activeStable ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "particleDedicatedActiveStable=%s activeStableFrames=%u/%u",
        input.activeStable ? "yes" : "no",
        input.activeStableFrames,
        input.activeStableRequiredFrames);
    ImGui::TextColored(
        runtimeState.particleDedicatedDefaultONCandidate ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "particleDedicatedDefaultONCandidate=%s defaultOnHealth=%s",
        runtimeState.particleDedicatedDefaultONCandidate ? "yes" : "no",
        runtimeState.particleDedicatedDefaultOnHealth.c_str());
    ImGui::TextColored(
        runtimeState.particleDedicatedResourceFallbackActive
            ? ImVec4(1.0f, 0.55f, 0.35f, 1.0f)
            : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "particleDedicatedFallback=%s target=%s auto=%s warmup=%s/%u probeStableFrames=%u",
        runtimeState.particleDedicatedResourceFallbackActive ? "active" : "off",
        runtimeState.particleDedicatedResourceFallbackActive ? "legacy" : "dedicated",
        runtimeState.enableParticleDedicatedAutoFallback ? "on" : "off",
        input.warmup ? "yes" : "no",
        input.warmupFrames,
        input.probeStableFrames);
}

void DrawParticleRenderGraphIntentPanel(
    const ParticleRenderGraphIntentPanelInput& input) {
    if (input.status == nullptr || input.graphIntent == nullptr || input.resources == nullptr) {
        return;
    }

    const vfx::ParticleRendererOperationalStatus& status = *input.status;
    const ParticleRenderGraphIntentTelemetry& graphIntent = *input.graphIntent;
    const vfx::ParticleVfxResourceSet& resources = *input.resources;

    if (!ImGui::TreeNodeEx("RenderGraph Intent", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Text("graphReason=%s", graphIntent.fallbackReason.c_str());
    if (ImGui::BeginTable("ParticleRenderGraphIntent", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableSetupColumn("Pass");
        ImGui::TableSetupColumn("Executed", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Resource");
        ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableHeadersRow();

        const auto drawGraphRow = [](
            const char* stage,
            const char* pass,
            bool executed,
            const char* resource,
            const char* access,
            bool graphOk,
            bool handleOk) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(stage);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(pass != nullptr && pass[0] != '\0' ? pass : "-");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(executed ? "yes" : "no");
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(resource != nullptr && resource[0] != '\0' ? resource : "-");
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(access);
            ImGui::TableSetColumnIndex(5);
            ImGui::TextColored(
                graphOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                "%s",
                graphOk ? "OK" : "NG");
            ImGui::TableSetColumnIndex(6);
            ImGui::TextColored(
                handleOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                "%s",
                handleOk ? "OK" : "NG");
        };

        drawGraphRow(
            "Simulation",
            resources.routing.simulationPass,
            graphIntent.simulationPassExecuted,
            resources.simulation.stateBuffer,
            "ReadUAV",
            graphIntent.simulationStateRead,
            status.simulationStateUavValid);
        drawGraphRow(
            "Simulation",
            resources.routing.simulationPass,
            graphIntent.simulationPassExecuted,
            resources.simulation.stateBuffer,
            "WriteUAV",
            graphIntent.simulationStateWrite,
            status.simulationStateUavValid);
        drawGraphRow(
            "Simulation",
            resources.routing.simulationPass,
            graphIntent.simulationPassExecuted,
            resources.simulation.renderBuffer,
            "WriteUAV",
            graphIntent.simulationRenderBufferWrite,
            status.simulationRenderBufferUavValid);
        drawGraphRow(
            "Renderer",
            resources.routing.drawPass,
            graphIntent.drawPassExecuted,
            resources.renderer.renderBuffer,
            "ReadSRV",
            graphIntent.drawRenderBufferRead,
            status.renderBufferSrvValid);
        drawGraphRow(
            "Renderer",
            resources.routing.drawPass,
            graphIntent.drawPassExecuted,
            resources.renderer.indirectArgs,
            "ReadIndirect",
            graphIntent.drawIndirectArgsRead,
            status.indirectArgsValid);
        ImGui::EndTable();
    }
    ImGui::TreePop();
}

void DrawTrailMeshStreamTelemetryPanel(
    const TrailMeshStreamTelemetryPanelInput& input) {
    if (input.plan == nullptr || input.drawStatus == nullptr) {
        return;
    }

    const vfx::TrailMeshStreamPlan& trailPlan = *input.plan;
    const vfx::TrailMeshStreamDrawStatus& drawStatus = *input.drawStatus;
    const EffectComponentCommon* primaryCommon =
        trailPlan.input != nullptr ? trailPlan.input->primary.componentCommon : nullptr;
    const VfxComponentInputCommon* primaryInput =
        trailPlan.input != nullptr ? &trailPlan.input->primary : nullptr;
    const char* rendererId =
        primaryCommon != nullptr && !primaryCommon->rendererId.empty()
            ? primaryCommon->rendererId.c_str()
            : "none";
    const char* simulationId =
        primaryCommon != nullptr && !primaryCommon->simulationId.empty()
            ? primaryCommon->simulationId.c_str()
            : "none";
    const bool rowsOk = input.ngRows == 0 && input.firstFailureSegment == UINT32_MAX;
    const std::string firstFailureSegmentLabel =
        input.firstFailureSegment != UINT32_MAX
            ? std::to_string(input.firstFailureSegment)
            : "none";

    ImGui::TextColored(
        input.operationalOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "defaultOnHealth=%s operationalOk=%s activeStableFrames=%u/%u probeStableFrames=%u/%u",
        input.defaultOnHealth != nullptr ? input.defaultOnHealth : "unknown",
        input.operationalOk ? "yes" : "no",
        input.activeStableFrames,
        input.activeStableRequiredFrames,
        input.probeStableFrames,
        input.probeStableRequiredFrames);
    ImGui::Text("dedicated=%s primary=%s rendererId=%s simulationId=%s queue=%u age=%.2f",
        trailPlan.enabled ? "on" : "off",
        trailPlan.hasPrimaryItem ? "yes" : "no",
        rendererId,
        simulationId,
        trailPlan.renderQueue,
        trailPlan.normalizedAge);
    DrawInputDescriptorLine(primaryInput);
    ImGui::TextColored(
        drawStatus.ready ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "drawPath=%s fallbackReason=%s",
        drawStatus.ready ? "dedicated mesh stream" : "indirect sprite fallback",
        drawStatus.fallbackReason);
    ImGui::Text("segments=%u controlPoints=%u vertices=%u indices=%u",
        trailPlan.segmentCountEstimate,
        trailPlan.controlPointCountEstimate,
        trailPlan.vertexCountEstimate,
        trailPlan.indexCountEstimate);

    if (input.telemetry == nullptr) {
        return;
    }

    const AppGpuParticleSystem::TrailMeshStreamTelemetry& telemetry = *input.telemetry;
    ImGui::Text("gpuTelemetry=%s copied=%s cp=%u seg=%u vertices=%u",
        telemetry.valid ? "valid" : "waiting",
        telemetry.copiedThisFrame ? "yes" : "no",
        telemetry.sampledControlPointCount,
        telemetry.sampledSegmentCount,
        telemetry.sampledVertexCount);
    ImGui::Text("drawArgs indexCount=%u instanceCount=%u startIndex=%u baseVertex=%d startInstance=%u",
        telemetry.drawArgs.IndexCountPerInstance,
        telemetry.drawArgs.InstanceCount,
        telemetry.drawArgs.StartIndexLocation,
        telemetry.drawArgs.BaseVertexLocation,
        telemetry.drawArgs.StartInstanceLocation);
    ImGui::TextColored(
        rowsOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "summary=%s OK rows=%u NG rows=%u firstNG=%s firstFailure=%s",
        rowsOk ? "OK" : "NG",
        input.okRows,
        input.ngRows,
        firstFailureSegmentLabel.c_str(),
        input.firstFailureReason != nullptr ? input.firstFailureReason : "none");

    if (telemetry.valid &&
        ImGui::BeginTable(
            "TrailMeshStreamGpuInspector",
            13,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX,
            ImVec2(0.0f, 190.0f))) {
        ImGui::TableSetupColumn("segment", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("cp range", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("head/tail", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("cp head pos/width");
        ImGui::TableSetupColumn("cp tail pos/width");
        ImGui::TableSetupColumn("head v left pos/uv");
        ImGui::TableSetupColumn("head v right pos/uv");
        ImGui::TableSetupColumn("tail v left pos/uv");
        ImGui::TableSetupColumn("tail v right pos/uv");
        ImGui::TableSetupColumn("indices", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("alpha h/t", ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableSetupColumn("diffs", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("checks", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableHeadersRow();
        for (uint32_t i = 0; i < telemetry.sampledSegmentCount; ++i) {
            const auto& segment = telemetry.segments[i];
            const TrailMeshStreamPanelRowValidation check =
                ValidateTrailMeshStreamPanelRow(telemetry, trailPlan, i);

            ImGui::TableNextRow();
            if (i == input.firstFailureSegment) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(120, 68, 24, 165));
            }
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u -> %u", segment.startControlPoint, segment.endControlPoint);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f -> %.2f", segment.normalizedHead, segment.normalizedTail);
            ImGui::TableSetColumnIndex(3);
            if (check.hasHeadCp) {
                const auto& cp = telemetry.controlPoints[segment.startControlPoint];
                ImGui::Text("%.2f %.2f %.2f / %.3f",
                    cp.positionAge.x,
                    cp.positionAge.y,
                    cp.positionAge.z,
                    cp.colorWidth.w);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(4);
            if (check.hasTailCp) {
                const auto& cp = telemetry.controlPoints[segment.endControlPoint];
                ImGui::Text("%.2f %.2f %.2f / %.3f",
                    cp.positionAge.x,
                    cp.positionAge.y,
                    cp.positionAge.z,
                    cp.colorWidth.w);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(5);
            if (check.hasHeadLeftVertex) {
                const auto& vertex = telemetry.vertices[check.headLeftVertexIndex];
                ImGui::Text("%.2f %.2f %.2f / %.2f",
                    vertex.positionUv.x,
                    vertex.positionUv.y,
                    vertex.positionUv.z,
                    vertex.positionUv.w);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(6);
            if (check.hasHeadRightVertex) {
                const auto& vertex = telemetry.vertices[check.headRightVertexIndex];
                ImGui::Text("%.2f %.2f %.2f / %.2f",
                    vertex.positionUv.x,
                    vertex.positionUv.y,
                    vertex.positionUv.z,
                    vertex.positionUv.w);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(7);
            if (check.hasTailLeftVertex) {
                const auto& vertex = telemetry.vertices[check.tailLeftVertexIndex];
                ImGui::Text("%.2f %.2f %.2f / %.2f",
                    vertex.positionUv.x,
                    vertex.positionUv.y,
                    vertex.positionUv.z,
                    vertex.positionUv.w);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(8);
            if (check.hasTailRightVertex) {
                const auto& vertex = telemetry.vertices[check.tailRightVertexIndex];
                ImGui::Text("%.2f %.2f %.2f / %.2f",
                    vertex.positionUv.x,
                    vertex.positionUv.y,
                    vertex.positionUv.z,
                    vertex.positionUv.w);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(9);
            if (check.hasIndexSix) {
                ImGui::Text("%u %u %u / %u %u %u",
                    telemetry.indices[check.indexBase + 0u],
                    telemetry.indices[check.indexBase + 1u],
                    telemetry.indices[check.indexBase + 2u],
                    telemetry.indices[check.indexBase + 3u],
                    telemetry.indices[check.indexBase + 4u],
                    telemetry.indices[check.indexBase + 5u]);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(10);
            if (check.hasHeadChain && check.hasTailChain) {
                ImGui::Text("%.2f -> %.2f",
                    telemetry.vertices[check.headLeftVertexIndex].color.w,
                    telemetry.vertices[check.tailLeftVertexIndex].color.w);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(11);
            if (check.hasHeadChain && check.hasTailChain) {
                const float maxTDelta = (std::max)(
                    (std::max)(check.headLeftTDelta, check.headRightTDelta),
                    (std::max)(check.tailLeftTDelta, check.tailRightTDelta));
                const float maxAlphaDelta = (std::max)(
                    (std::max)(check.headLeftAlphaDelta, check.headRightAlphaDelta),
                    (std::max)(check.tailLeftAlphaDelta, check.tailRightAlphaDelta));
                const float maxWidthDelta = (std::max)(
                    (std::max)(check.headLeftWidthDelta, check.headRightWidthDelta),
                    (std::max)(check.tailLeftWidthDelta, check.tailRightWidthDelta));
                ImGui::Text("t %.3f a %.3f w %.3f", maxTDelta, maxAlphaDelta, maxWidthDelta);
            } else {
                ImGui::TextUnformatted("--");
            }
            ImGui::TableSetColumnIndex(12);
            ImGui::TextColored(
                check.rowOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                "%s cp:%s t:%s uv:%s a:%s w:%s idx:%s args:%s",
                check.rowOk ? "OK" : "NG",
                check.cpRangeOk ? "ok" : "ng",
                check.tOk ? "ok" : "ng",
                check.sideUvOk ? "ok" : "ng",
                check.alphaOk ? "ok" : "ng",
                check.widthOk ? "ok" : "ng",
                check.indicesOk ? "ok" : "ng",
                check.argsOk ? "ok" : "ng");
        }
        ImGui::EndTable();
    }

    if (telemetry.valid &&
        ImGui::BeginTable("TrailMeshStreamGpuControlPoints", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("pos");
        ImGui::TableSetupColumn("age", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("color");
        ImGui::TableSetupColumn("width", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableHeadersRow();
        for (uint32_t i = 0; i < telemetry.sampledControlPointCount; ++i) {
            const auto& controlPoint = telemetry.controlPoints[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f %.2f %.2f",
                controlPoint.positionAge.x,
                controlPoint.positionAge.y,
                controlPoint.positionAge.z);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", controlPoint.positionAge.w);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f %.2f %.2f",
                controlPoint.colorWidth.x,
                controlPoint.colorWidth.y,
                controlPoint.colorWidth.z);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.3f", controlPoint.colorWidth.w);
        }
        ImGui::EndTable();
    }

    if (telemetry.valid &&
        ImGui::BeginTable("TrailMeshStreamGpuSegments", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("startCp", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("endCp", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("head", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn("tail", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableHeadersRow();
        for (uint32_t i = 0; i < telemetry.sampledSegmentCount; ++i) {
            const auto& segment = telemetry.segments[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", segment.startControlPoint);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", segment.endControlPoint);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", segment.normalizedHead);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", segment.normalizedTail);
        }
        ImGui::EndTable();
    }

    if (telemetry.valid &&
        ImGui::BeginTable("TrailMeshStreamGpuVertices", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("pos/uv");
        ImGui::TableSetupColumn("color");
        ImGui::TableSetupColumn("params");
        ImGui::TableSetupColumn("alpha", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableHeadersRow();
        for (uint32_t i = 0; i < telemetry.sampledVertexCount; ++i) {
            const auto& vertex = telemetry.vertices[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f %.2f %.2f / %.2f",
                vertex.positionUv.x,
                vertex.positionUv.y,
                vertex.positionUv.z,
                vertex.positionUv.w);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f %.2f %.2f",
                vertex.color.x,
                vertex.color.y,
                vertex.color.z);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f %.2f %.2f %.2f",
                vertex.params.x,
                vertex.params.y,
                vertex.params.z,
                vertex.params.w);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", vertex.color.w);
        }
        ImGui::EndTable();
    }
}

void DrawTrailMeshStreamVisualDebugPanel(
    const vfx::TrailMeshStreamDebugPreview& preview) {
    if (!ImGui::TreeNodeEx("Visual Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    DrawTrailMeshStreamDebugCanvas(preview);
    if (ImGui::BeginTable(
            "TrailMeshStreamDebugPoints",
            8,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 180.0f))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 32.0f);
        ImGui::TableSetupColumn("t", ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn("pos");
        ImGui::TableSetupColumn("tan");
        ImGui::TableSetupColumn("side");
        ImGui::TableSetupColumn("miter");
        ImGui::TableSetupColumn("width", ImGuiTableColumnFlags_WidthFixed, 54.0f);
        ImGui::TableSetupColumn("alpha", ImGuiTableColumnFlags_WidthFixed, 54.0f);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < preview.points.size(); ++i) {
            const auto& point = preview.points[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", static_cast<unsigned int>(i));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", point.normalized);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f %.2f %.2f", point.position.x, point.position.y, point.position.z);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f %.2f %.2f", point.tangent.x, point.tangent.y, point.tangent.z);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f %.2f %.2f", point.side.x, point.side.y, point.side.z);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.2f %.2f %.2f", point.miterOffset.x, point.miterOffset.y, point.miterOffset.z);
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.3f", point.width);
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.2f", point.alpha);
        }
        ImGui::EndTable();
    }
    ImGui::TreePop();
}

void DrawParticleDedicatedResourceProbePanel(
    const ParticleDedicatedResourceProbePanelInput& input) {
    if (input.health == nullptr || input.status == nullptr || input.graphIntent == nullptr ||
        input.resources == nullptr || input.readbackSummary == nullptr || input.runtimeState == nullptr) {
        return;
    }

    const ParticleDedicatedOperationalHealthSummary& health = *input.health;
    const vfx::ParticleRendererOperationalStatus& status = *input.status;
    const ParticleRenderGraphIntentTelemetry& graphIntent = *input.graphIntent;
    const vfx::ParticleVfxResourceSet& resources = *input.resources;
    const ParticleDedicatedReadbackValidationSummary& readbackSummary = *input.readbackSummary;
    const AppVfxRuntimeState& runtimeState = *input.runtimeState;

    if (!ImGui::TreeNodeEx("Dedicated Resource Probe", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    DrawParticleDedicatedOperationalHealthLine("ParticleDedicatedOperational", health);
    DrawInputDescriptorLine(input.primaryInput);
    ImGui::TextColored(
        health.healthy ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "particleDedicatedProbe=%s simulationPath=%s renderPath=%s graph=%s handles=%s readback=%s validation=%s fallbackReason=%s fallbackTarget=%s",
        health.healthy ? "healthy" : "attention",
        ParticleSimulationPathLabel(health),
        ParticleRenderPathLabel(health),
        graphIntent.ready ? "ready" : "attention",
        status.resourceHandlesReady ? "ready" : "attention",
        ParticleReadbackLabel(health),
        ParticleValidationLabel(health),
        health.fallbackReason.c_str(),
        runtimeState.particleDedicatedResourceFallbackActive ? "legacy" : "dedicated");
    ImGui::Text("probeMode=%s handleReason=%s",
        status.resourceIntentMode,
        status.resourceHandleFallbackReason);
    ImGui::TextColored(
        IsParticleDedicatedOperationalOk(runtimeState.particleDedicatedDefaultOnHealth.c_str())
            ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f)
            : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "defaultOnHealth=%s defaultONCandidate=%s probeStableFrames=%u activeStableFrames=%u",
        runtimeState.particleDedicatedDefaultOnHealth.c_str(),
        runtimeState.particleDedicatedDefaultONCandidate ? "yes" : "no",
        runtimeState.particleDedicatedProbeStableFrames,
        runtimeState.particleDedicatedActiveStableFrames);
    ImGui::Text("probe resources: state=%s renderBuffer=%s indirectArgs=%s",
        resources.simulation.stateBuffer,
        resources.simulation.renderBuffer,
        resources.simulation.indirectArgs);
    ImGui::TextColored(
        health.simulationDedicated && health.drawDedicated
            ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f)
            : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "simulationPath=%s renderPath=%s simulationTelemetry=%s groups=%u maxParticles=%u",
        ParticleSimulationPathLabel(health),
        ParticleRenderPathLabel(health),
        input.simulationTelemetry != nullptr && input.simulationTelemetry->valid ? "valid" : "waiting",
        input.simulationTelemetry != nullptr ? input.simulationTelemetry->dispatchGroupCount : 0u,
        input.simulationTelemetry != nullptr ? input.simulationTelemetry->maxParticles : 0u);
    if (input.simulationTelemetry != nullptr && input.simulationTelemetry->valid) {
        ImGui::Text("last simulation renderBuffer=%s uav=0x%llx",
            input.simulationTelemetry->renderBufferResource,
            static_cast<unsigned long long>(input.simulationTelemetry->renderBufferUav.ptr));
        ImGui::Text("last simulation state=%s uav=0x%llx",
            input.simulationTelemetry->stateBufferResource,
            static_cast<unsigned long long>(input.simulationTelemetry->stateBufferUav.ptr));
    }

    if (input.readbackTelemetry != nullptr) {
        ImGui::TextColored(
            health.readbackReady ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
            "readback=%s sampleReadback=%s drawArgsReadback=%s copied=%s renderSamples=%u stateSamples=%u",
            ParticleReadbackLabel(health),
            input.readbackTelemetry->valid ? "valid" : "waiting",
            input.readbackTelemetry->drawArgsValid ? "valid" : "waiting",
            input.readbackTelemetry->copiedThisFrame ? "yes" : "no",
            input.readbackTelemetry->sampledRenderCount,
            input.readbackTelemetry->sampledStateCount);
        ImGui::TextColored(
            health.validationOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
            "validation=%s sampleValidation=%s drawArgsValidation=%s actualDraw=%u expectedDraw=%u okRows=%u ngRows=%u fallbackReason=%s",
            ParticleValidationLabel(health),
            readbackSummary.AllRowsOk() ? "ok" : "attention",
            readbackSummary.drawArgsValidationOk ? "ok" : "attention",
            readbackSummary.actualDrawCount,
            readbackSummary.expectedDrawCount,
            readbackSummary.okRows,
            readbackSummary.ngRows,
            health.fallbackReason.c_str());
        if (input.readbackTelemetry->valid &&
            ImGui::BeginTable("ParticleDedicatedReadbackTelemetry", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28.0f);
            ImGui::TableSetupColumn("state pos");
            ImGui::TableSetupColumn("age/life", ImGuiTableColumnFlags_WidthFixed, 92.0f);
            ImGui::TableSetupColumn("velocity");
            ImGui::TableSetupColumn("render world");
            ImGui::TableSetupColumn("render color");
            ImGui::TableSetupColumn("pos delta", ImGuiTableColumnFlags_WidthFixed, 78.0f);
            ImGui::TableSetupColumn("checks", ImGuiTableColumnFlags_WidthFixed, 170.0f);
            ImGui::TableHeadersRow();
            const uint32_t sampleCount = (std::min)(
                input.readbackTelemetry->sampledRenderCount,
                input.readbackTelemetry->sampledStateCount);
            for (uint32_t i = 0; i < sampleCount; ++i) {
                const auto& render = input.readbackTelemetry->renderSamples[i];
                const auto& state = input.readbackTelemetry->stateSamples[i];
                const ParticleDedicatedReadbackRowValidation row =
                    ValidateParticleDedicatedReadbackRow(*input.readbackTelemetry, i);
                ImGui::TableNextRow();
                if (!row.rowOk) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(120, 68, 24, 165));
                }
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", i);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f %.2f %.2f", state.position.x, state.position.y, state.position.z);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.2f / %.2f", state.age, state.lifetime);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2f %.2f %.2f", state.velocity.x, state.velocity.y, state.velocity.z);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.2f %.2f %.2f", render.World.m[3][0], render.World.m[3][1], render.World.m[3][2]);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%.2f %.2f %.2f %.2f", render.color.x, render.color.y, render.color.z, render.color.w);
                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%.5f", row.positionDelta);
                ImGui::TableSetColumnIndex(7);
                ImGui::Text("%s", row.rowOk ? "OK" : "NG");
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::BeginTable("ParticleDedicatedResourceProbe", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Resource");
        ImGui::TableSetupColumn("Intent", ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableSetupColumn("Pass");
        ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableHeadersRow();

        const auto drawProbeRow = [](
            const char* resource,
            const char* intent,
            bool graphOk,
            bool handleOk,
            const char* pass,
            const char* access) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(resource != nullptr && resource[0] != '\0' ? resource : "-");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(intent);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(
                graphOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                "%s",
                graphOk ? "OK" : "NG");
            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(
                handleOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                "%s",
                handleOk ? "OK" : "NG");
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(pass != nullptr && pass[0] != '\0' ? pass : "-");
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(access);
        };

        drawProbeRow(
            resources.simulation.stateBuffer,
            "state",
            graphIntent.simulationStateRead && graphIntent.simulationStateWrite,
            status.simulationStateUavValid,
            resources.routing.simulationPass,
            "Read/WriteUAV");
        drawProbeRow(
            resources.simulation.renderBuffer,
            "simulation",
            graphIntent.simulationRenderBufferWrite,
            status.simulationRenderBufferUavValid,
            resources.routing.simulationPass,
            "WriteUAV");
        drawProbeRow(
            resources.renderer.renderBuffer,
            "renderer",
            graphIntent.drawRenderBufferRead,
            status.renderBufferSrvValid,
            resources.routing.drawPass,
            "ReadSRV");
        drawProbeRow(
            resources.renderer.indirectArgs,
            "draw args",
            graphIntent.drawIndirectArgsRead,
            status.indirectArgsValid,
            resources.routing.drawPass,
            "ReadIndirect");
        ImGui::EndTable();
    }
    ImGui::TreePop();
}

void DrawDistortionDedicatedRuntimeTelemetryPanel(
    const DistortionDedicatedRuntimeTelemetryPanelInput& input) {
    if (input.health == nullptr || input.graphIntent == nullptr ||
        input.status == nullptr || input.runtimeState == nullptr) {
        return;
    }

    const AppVfxRuntimeState& runtimeState = *input.runtimeState;
    const DistortionDedicatedOperationalHealthSummary& health = *input.health;
    const DistortionRenderGraphIntentTelemetry& graphIntent = *input.graphIntent;
    const vfx::DistortionRendererOperationalStatus& status = *input.status;

    DrawInputDescriptorLine(input.primaryInput);
    ImGui::TextColored(
        health.healthy ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "DistortionDedicatedOperational=%s graph=%s handles=%s readback=%s validation=%s drawPath=%s fallbackReason=%s",
        health.healthy ? "healthy" : "attention",
        health.graphReady ? "ready" : "attention",
        health.handlesReady ? "ready" : "attention",
        DistortionReadbackLabel(health),
        DistortionValidationLabel(health),
        health.drawDedicated ? "dedicated" : "legacy_or_custom",
        health.fallbackReason.c_str());
    ImGui::TextColored(
        health.validationOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "distortionDedicatedReadback=%s validation=%s renderSamples=%u drawArgs=%s actualDraw=%u expectedDraw=%u",
        DistortionReadbackLabel(health),
        DistortionValidationLabel(health),
        health.renderSamples,
        health.drawArgsReadbackValid ? "valid" : "waiting",
        health.actualDrawCount,
        health.expectedDrawCount);
    ImGui::TextColored(
        graphIntent.simulationPassFound &&
            graphIntent.simulationPassExecuted &&
            graphIntent.simulationRenderBufferWrite
            ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f)
            : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "distortionDedicatedSimulation=%s state=%s/%s renderWrite=%s resources=%s/%s",
        graphIntent.simulationPassFound
            ? (graphIntent.simulationPassExecuted ? "executed" : "culled")
            : "missing",
        graphIntent.simulationStateRead ? "read" : "no-read",
        graphIntent.simulationStateWrite ? "write" : "no-write",
        graphIntent.simulationRenderBufferWrite ? "yes" : "no",
        status.simulationStateResource,
        status.simulationRenderBufferResource);
    ImGui::TextColored(
        input.stableEnough ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "distortionDedicatedStableEnough=%s stableFrames=%u/%u",
        input.stableEnough ? "yes" : "no",
        input.stableFrames,
        input.stableRequiredFrames);
    ImGui::TextColored(
        input.activeStable ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "distortionDedicatedActiveStable=%s activeStableFrames=%u/%u",
        input.activeStable ? "yes" : "no",
        input.activeStableFrames,
        input.activeStableRequiredFrames);
    ImGui::TextColored(
        runtimeState.distortionDedicatedOperationalStable ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "distortionDedicatedOperationalStable=%s defaultOnHealth=%s",
        runtimeState.distortionDedicatedOperationalStable ? "yes" : "no",
        runtimeState.distortionDedicatedDefaultOnHealth.c_str());
    ImGui::TextColored(
        runtimeState.distortionDedicatedResourceFallbackActive
            ? ImVec4(1.0f, 0.55f, 0.35f, 1.0f)
            : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "distortionDedicatedFallback=%s target=%s auto=%s warmup=%s/%u resources=%s/%s",
        runtimeState.distortionDedicatedResourceFallbackActive ? "active" : "off",
        runtimeState.distortionDedicatedResourceFallbackActive ? "legacy" : "dedicated",
        runtimeState.enableDistortionDedicatedAutoFallback ? "on" : "off",
        input.warmup ? "yes" : "no",
        input.warmupFrames,
        status.renderBufferResource,
        status.indirectArgsResource);
    ImGui::Text(
        "distortionDedicatedTelemetry=%s frames=%u/%u",
        runtimeState.enableDistortionDedicatedTelemetry ? "on" : "off",
        input.telemetryFrames,
        input.telemetryFrameLimit);
}

void DrawBeamDedicatedRuntimeTelemetryPanel(
    const BeamDedicatedRuntimeTelemetryPanelInput& input) {
    if (input.health == nullptr || input.graphIntent == nullptr ||
        input.status == nullptr || input.runtimeState == nullptr) {
        return;
    }

    const AppVfxRuntimeState& runtimeState = *input.runtimeState;
    const BeamDedicatedOperationalHealthSummary& health = *input.health;
    const BeamRenderGraphIntentTelemetry& graphIntent = *input.graphIntent;
    const vfx::BeamRendererOperationalStatus& status = *input.status;

    DrawInputDescriptorLine(input.primaryInput);
    ImGui::TextColored(
        health.healthy ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "BeamDedicatedOperational=%s graph=%s handles=%s readback=%s validation=%s drawPath=%s fallbackReason=%s",
        health.healthy ? "healthy" : "attention",
        health.graphReady ? "ready" : "attention",
        health.handlesReady ? "ready" : "attention",
        health.readbackReady ? "valid" : "waiting",
        health.validationOk ? "ok" : "attention",
        health.drawDedicated ? "dedicated" : "legacy_or_custom",
        health.fallbackReason.c_str());
    ImGui::TextColored(
        health.validationOk ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "beamDedicatedReadback=%s validation=%s renderSamples=%u drawArgs=%s actualDraw=%u expectedDraw=%u",
        health.readbackReady ? "valid" : "waiting",
        health.validationOk ? "ok" : "attention",
        health.renderSamples,
        health.drawArgsReadbackValid ? "valid" : "waiting",
        health.actualDrawCount,
        health.expectedDrawCount);
    ImGui::TextColored(
        graphIntent.simulationPassFound &&
            graphIntent.simulationPassExecuted &&
            graphIntent.simulationRenderBufferWrite
            ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f)
            : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "beamDedicatedSimulation=%s state=%s/%s renderWrite=%s resources=%s/%s",
        graphIntent.simulationPassFound
            ? (graphIntent.simulationPassExecuted ? "executed" : "culled")
            : "missing",
        graphIntent.simulationStateRead ? "read" : "no-read",
        graphIntent.simulationStateWrite ? "write" : "no-write",
        graphIntent.simulationRenderBufferWrite ? "yes" : "no",
        status.simulationStateResource,
        status.simulationRenderBufferResource);
    ImGui::TextColored(
        graphIntent.drawPassFound &&
            graphIntent.drawPassExecuted &&
            graphIntent.drawRenderBufferRead &&
            graphIntent.drawIndirectArgsRead
            ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f)
            : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "beamDedicatedDraw=%s renderRead=%s indirectRead=%s resources=%s/%s",
        graphIntent.drawPassFound
            ? (graphIntent.drawPassExecuted ? "executed" : "culled")
            : "missing",
        graphIntent.drawRenderBufferRead ? "yes" : "no",
        graphIntent.drawIndirectArgsRead ? "yes" : "no",
        status.renderBufferResource,
        status.indirectArgsResource);
    ImGui::TextColored(
        input.stableEnough ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "beamDedicatedStableEnough=%s stableFrames=%u/%u",
        input.stableEnough ? "yes" : "no",
        input.stableFrames,
        input.stableRequiredFrames);
    ImGui::TextColored(
        input.activeStable ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "beamDedicatedActiveStable=%s activeStableFrames=%u/%u",
        input.activeStable ? "yes" : "no",
        input.activeStableFrames,
        input.activeStableRequiredFrames);
    ImGui::TextColored(
        runtimeState.beamDedicatedOperationalStable ? ImVec4(0.45f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        "beamDedicatedOperationalStable=%s defaultOnReady=%s defaultOnHealth=%s",
        runtimeState.beamDedicatedOperationalStable ? "yes" : "no",
        runtimeState.beamDedicatedDefaultOnReady ? "yes" : "no",
        runtimeState.beamDedicatedDefaultOnHealth.c_str());
    ImGui::TextColored(
        runtimeState.beamDedicatedResourceFallbackActive
            ? ImVec4(1.0f, 0.55f, 0.35f, 1.0f)
            : ImVec4(0.72f, 0.74f, 0.78f, 1.0f),
        "beamDedicatedFallback=%s target=%s auto=%s warmup=%s/%u resources=%s/%s",
        runtimeState.beamDedicatedResourceFallbackActive ? "active" : "off",
        runtimeState.beamDedicatedResourceFallbackActive ? "legacy" : "dedicated",
        runtimeState.enableBeamDedicatedAutoFallback ? "on" : "off",
        input.warmup ? "yes" : "no",
        input.warmupFrames,
        status.renderBufferResource,
        status.indirectArgsResource);
    ImGui::Text(
        "beamDedicatedTelemetry=%s frames=%u/%u",
        runtimeState.enableBeamDedicatedTelemetry ? "on" : "off",
        input.telemetryFrames,
        input.telemetryFrameLimit);
}
