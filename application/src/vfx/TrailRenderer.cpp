#include "vfx/TrailRenderer.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "../../AppFrameGraphBuilder.h"
#include "../../AppFrameState.h"
#include "../../AppGpuParticleSystem.h"
#include "../../AppPipelines.h"
#include "../../AppRuntimeState.h"
#include "VfxComponentDraw.h"
#include "graphics/RenderGraph.h"
#include "vfx/VfxPassRegistration.h"
#include "vfx/VfxResources.h"

namespace {
constexpr uint32_t kTrailVerticesPerControlPoint = 2;
constexpr uint32_t kTrailIndicesPerSegment = 6;
constexpr uint32_t kMinTrailSegmentBudget = 1;
constexpr uint32_t kTrailComputeThreadGroupSize = 256;
constexpr float kTrailMovementEpsilonSq = 0.0001f;

struct TrailMeshStreamConstants {
    uint32_t maxSegments = 0;
    float time = 0.0f;
    float normalizedAge = 0.0f;
    uint32_t hasPrimary = 0;
    uint32_t segmentBudget = 0;
    float width = 1.0f;
    float length = 1.0f;
    uint32_t historyCount = 0;
    Vector4 origin = {0.0f, 0.0f, 0.0f, 1.0f};
    Vector4 trailVector = {1.0f, 0.0f, 0.0f, 0.0f};
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 widthAlpha = {1.0f, 0.35f, 0.0f, 0.0f};
    Vector4 colorTail = {0.65f, 0.65f, 0.65f, 1.0f};
};

struct TrailMeshBuildConstants {
    uint32_t maxSegments = 0;
    uint32_t segmentBudget = 0;
    uint32_t hasPrimary = 0;
    uint32_t padding0 = 0;
    Vector4 cameraPosition = {0.0f, 0.0f, -1.0f, 1.0f};
    Vector4 joinParams = {2.0f, 0.0f, 0.0f, 0.0f};
};

float LengthSq(const Vector3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

float DistanceSq(const Vector3& a, const Vector3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

float Distance(const Vector3& a, const Vector3& b) {
    return std::sqrt(DistanceSq(a, b));
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

Vector3 Average(const Vector3& a, const Vector3& b) {
    return {
        (a.x + b.x) * 0.5f,
        (a.y + b.y) * 0.5f,
        (a.z + b.z) * 0.5f,
    };
}

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
    const float lengthSq = LengthSq(value);
    if (lengthSq <= kTrailMovementEpsilonSq) {
        return fallback;
    }
    const float invLength = 1.0f / std::sqrt(lengthSq);
    return {value.x * invLength, value.y * invLength, value.z * invLength};
}

Vector3 AlignSide(const Vector3& side, const Vector3& referenceSide) {
    return Dot(side, referenceSide) < 0.0f ? Scale(side, -1.0f) : side;
}

bool ShouldBuildTrailMeshStreamProbe(const AppRuntimeState* runtimeState) {
    return runtimeState != nullptr && runtimeState->vfx.enableTrailMeshStream;
}

std::vector<Vector4> BuildTrailHistoryUpload(
    const EffectInstance& instance,
    uint32_t controlPointCount,
    float smoothing) {
    std::vector<Vector4> points;
    points.reserve(controlPointCount);
    if (controlPointCount == 0) {
        return points;
    }

    const uint32_t available = (std::min)(instance.trailHistoryCount, kEffectTrailHistoryCapacity);
    if (available == 0) {
        for (uint32_t i = 0; i < controlPointCount; ++i) {
            const Vector3& point = instance.transform.translate;
            points.push_back({point.x, point.y, point.z, 1.0f});
        }
        return points;
    }

    std::vector<Vector3> history;
    history.reserve(available);
    for (uint32_t i = 0; i < available; ++i) {
        const uint32_t index = (instance.trailHistoryHead + i) % kEffectTrailHistoryCapacity;
        history.push_back(instance.trailHistory[index]);
    }

    smoothing = (std::clamp)(smoothing, 0.0f, 1.0f);
    if (smoothing > 0.0f && history.size() > 2) {
        std::vector<Vector3> smoothed = history;
        for (uint32_t i = 1; i + 1 < static_cast<uint32_t>(history.size()); ++i) {
            smoothed[i] = Lerp(history[i], Average(history[i - 1], history[i + 1]), smoothing);
        }
        history = std::move(smoothed);
    }

    std::vector<float> segmentLengths;
    segmentLengths.reserve(available > 0 ? available - 1 : 0);
    float totalLength = 0.0f;
    for (uint32_t i = 0; i + 1 < available; ++i) {
        const float length = Distance(history[i], history[i + 1]);
        segmentLengths.push_back(length);
        totalLength += length;
    }

    if (available == 1 || totalLength <= 0.0001f || controlPointCount == 1) {
        const Vector3& point = history.front();
        for (uint32_t i = 0; i < controlPointCount; ++i) {
            points.push_back({point.x, point.y, point.z, 1.0f});
        }
        return points;
    }

    uint32_t segmentIndex = 0;
    float segmentStartDistance = 0.0f;
    for (uint32_t i = 0; i < controlPointCount; ++i) {
        const float targetDistance =
            totalLength * static_cast<float>(i) / static_cast<float>(controlPointCount - 1);
        while (segmentIndex + 1 < segmentLengths.size() &&
               segmentStartDistance + segmentLengths[segmentIndex] < targetDistance) {
            segmentStartDistance += segmentLengths[segmentIndex];
            ++segmentIndex;
        }
        const float segmentLength = segmentLengths[segmentIndex];
        const float localT = segmentLength > 0.0001f
            ? (targetDistance - segmentStartDistance) / segmentLength
            : 0.0f;
        const Vector3 point = Lerp(
            history[segmentIndex],
            history[(std::min)(segmentIndex + 1, static_cast<uint32_t>(history.size() - 1))],
            (std::clamp)(localT, 0.0f, 1.0f));
        points.push_back({point.x, point.y, point.z, 1.0f});
    }
    return points;
}

Vector3 ToVector3(const Vector4& value) {
    return {value.x, value.y, value.z};
}

Vector3 ComputeControlPointTangent(
    const std::vector<Vector3>& positions,
    uint32_t index,
    const Vector3& fallback) {
    if (positions.empty()) {
        return fallback;
    }
    const uint32_t last = static_cast<uint32_t>(positions.size() - 1);
    const uint32_t current = (std::min)(index, last);
    const uint32_t prev = current > 0 ? current - 1 : current;
    const uint32_t next = (std::min)(current + 1, last);
    return NormalizeOr(Subtract(positions[next], positions[prev]), fallback);
}

Vector3 ComputeControlPointSide(
    const Vector3& position,
    const Vector3& tangent,
    const Vector3& cameraWorldPosition,
    const Vector3& fallbackSide) {
    const Vector3 viewDirection = NormalizeOr(Subtract(cameraWorldPosition, position), {0.0f, 0.0f, -1.0f});
    return NormalizeOr(Cross(viewDirection, tangent), fallbackSide);
}

Vector3 ComputeControlPointMiterOffset(
    const std::vector<Vector3>& positions,
    uint32_t index,
    const Vector3& fallbackTangent,
    const Vector3& currentSegmentSide,
    const Vector3& cameraWorldPosition,
    float miterLimit) {
    if (positions.empty()) {
        return currentSegmentSide;
    }

    const uint32_t last = static_cast<uint32_t>(positions.size() - 1);
    const uint32_t current = (std::min)(index, last);
    const uint32_t prev = current > 0 ? current - 1 : current;
    const uint32_t next = (std::min)(current + 1, last);
    const Vector3& currentPosition = positions[current];
    const Vector3 prevTangent = NormalizeOr(Subtract(currentPosition, positions[prev]), fallbackTangent);
    const Vector3 nextTangent = NormalizeOr(Subtract(positions[next], currentPosition), fallbackTangent);
    const Vector3 viewDirection = NormalizeOr(Subtract(cameraWorldPosition, currentPosition), {0.0f, 0.0f, -1.0f});
    const Vector3 prevSide = AlignSide(NormalizeOr(Cross(viewDirection, prevTangent), currentSegmentSide), currentSegmentSide);
    const Vector3 nextSide = AlignSide(NormalizeOr(Cross(viewDirection, nextTangent), currentSegmentSide), currentSegmentSide);

    const bool isEndpoint = current == 0 || current == last;
    Vector3 miterSide = isEndpoint ? currentSegmentSide : NormalizeOr(Add(prevSide, nextSide), currentSegmentSide);
    miterSide = AlignSide(miterSide, currentSegmentSide);
    const float projection = std::abs(Dot(miterSide, currentSegmentSide));
    const float miterScale = isEndpoint ? 1.0f : (std::min)((std::max)(1.0f, miterLimit), 1.0f / (std::max)(0.35f, projection));
    return Scale(miterSide, miterScale);
}
} // namespace

void TrailRenderer::RegisterPasses(
    const AppFrameGraphBuildContext& ctx,
    const vfx::VfxTypedResourceSet& resources) const {
    const vfx::VfxTypedResourceSet vfxResources = resources;
    ctx.renderGraph->AddPass({
        vfxResources.trail.routing.simulationPass,
        ge3::graphics::RenderPassLayer::Vfx,
        vfx::TrailMeshStreamSimulationAccesses(vfxResources.trail.renderer.meshStream),
        "",
        [this, ctx, vfxResources](ge3::graphics::RenderPassContext& passContext) {
            const TrailRenderQueue& queue = ctx.effectRuntime->trailQueue;
            if (queue.empty() || !ShouldBuildTrailMeshStreamProbe(ctx.runtimeState)) {
                return;
            }
            Simulate(
                passContext.commandList,
                vfx::BuildPassRenderContext(ctx, vfxResources),
                ctx.effectRuntime->TrailInput());
        }});

    ctx.renderGraph->AddPass({
        "VFX.TrailMeshBuild",
        ge3::graphics::RenderPassLayer::Vfx,
        vfx::TrailMeshBuildAccesses(vfxResources.trail.renderer.meshStream),
        "",
        [this, ctx, vfxResources](ge3::graphics::RenderPassContext& passContext) {
            const TrailRenderQueue& queue = ctx.effectRuntime->trailQueue;
            if (queue.empty() || !ShouldBuildTrailMeshStreamProbe(ctx.runtimeState)) {
                return;
            }
            BuildMeshStream(
                passContext.commandList,
                vfx::BuildPassRenderContext(ctx, vfxResources),
                ctx.effectRuntime->TrailInput());
        }});

    ctx.renderGraph->AddPass({
        vfxResources.trail.routing.drawPass,
        ge3::graphics::RenderPassLayer::Vfx,
        vfx::DrawAccesses(vfxResources.trail.renderer),
        vfxResources.trail.routing.depthTarget,
        [this, ctx, vfxResources](ge3::graphics::RenderPassContext& passContext) {
            const TrailRenderQueue& queue = ctx.effectRuntime->trailQueue;
            if (queue.empty()) {
                return;
            }
            Draw(
                passContext.commandList,
                vfx::BuildPassRenderContext(ctx, vfxResources),
                ctx.effectRuntime->TrailInput());
        }});
}

void TrailRenderer::Simulate(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const TrailRenderInput& input) const {
    if (commandList == nullptr ||
        context.srvDescriptorHeap == nullptr ||
        context.appPipelines == nullptr ||
        context.gpuParticleSystem == nullptr ||
        context.frameState == nullptr) {
        return;
    }

    const vfx::TrailMeshStreamResourceSet* meshStreamResources =
        context.typedResources != nullptr ? &context.typedResources->trail.renderer.meshStream : nullptr;
    const vfx::TrailMeshStreamPlan plan = BuildMeshStreamPlan(input, meshStreamResources);
    if (plan.resources == nullptr ||
        plan.resources->controlPointBuffer[0] == '\0' ||
        plan.resources->segmentBuffer[0] == '\0') {
        return;
    }

    ID3D12RootSignature* rootSignature = context.appPipelines->GetTrailMeshStreamComputeRootSignature();
    ID3D12PipelineState* pipelineState = context.appPipelines->GetTrailMeshStreamComputePSO();
    if (rootSignature == nullptr || pipelineState == nullptr) {
        return;
    }

    const D3D12_GPU_DESCRIPTOR_HANDLE controlPointUav =
        context.gpuParticleSystem->UavHandleForResource(plan.resources->controlPointBuffer);
    const D3D12_GPU_DESCRIPTOR_HANDLE segmentUav =
        context.gpuParticleSystem->UavHandleForResource(plan.resources->segmentBuffer);
    if (controlPointUav.ptr == 0 || segmentUav.ptr == 0) {
        return;
    }

    const uint32_t maxSegments = (std::max)(context.gpuParticleSystem->MaxParticles(), kMinTrailSegmentBudget);
    TrailMeshStreamConstants constants{};
    constants.maxSegments = maxSegments;
    constants.time = context.beamTime;
    constants.normalizedAge = plan.normalizedAge;
    constants.hasPrimary = plan.hasPrimaryItem ? 1u : 0u;
    constants.segmentBudget = (std::min)(
        (std::max)(plan.segmentCountEstimate, kMinTrailSegmentBudget),
        maxSegments);
    D3D12_GPU_DESCRIPTOR_HANDLE historySrv{};
    if (input.primary.instance != nullptr && input.primary.componentCommon != nullptr && input.settings != nullptr) {
        const EffectInstance& instance = *input.primary.instance;
        const EffectComponentCommon& component = *input.primary.componentCommon;
        const EffectTrailSettings& settings = *input.settings;
        const float width = (std::max)(0.001f, settings.width * instance.transform.scale.x);
        const float length = (std::max)(0.001f, settings.length * instance.transform.scale.y);
        const float yaw = instance.transform.rotate.z;
        Vector3 direction{std::cos(yaw), std::sin(yaw), 0.0f};
        if (settings.followMode == EffectTrailFollowMode::InstanceUp) {
            direction = {-std::sin(yaw), std::cos(yaw), 0.0f};
        } else if (settings.followMode == EffectTrailFollowMode::WorldX) {
            direction = {1.0f, 0.0f, 0.0f};
        } else if (settings.followMode == EffectTrailFollowMode::MovementHistory) {
            direction = NormalizeOr(instance.velocity, direction);
        }
        constants.width = width;
        constants.length = length;
        constants.origin = {
            instance.transform.translate.x,
            instance.transform.translate.y,
            instance.transform.translate.z,
            1.0f,
        };
        constants.trailVector = {
            direction.x * length,
            direction.y * length,
            direction.z * length,
            0.0f,
        };
        constants.color = {
            instance.color.x * component.color.x,
            instance.color.y * component.color.y,
            instance.color.z * component.color.z,
            instance.color.w * component.color.w,
        };
        constants.widthAlpha = {
            (std::max)(0.0f, settings.widthHead),
            (std::max)(0.0f, settings.widthTail),
            (std::clamp)(settings.alphaTail, 0.0f, 1.0f),
            0.0f,
        };
        constants.colorTail = {
            settings.colorTail.x,
            settings.colorTail.y,
            settings.colorTail.z,
            settings.colorTail.w,
        };
        const uint32_t requestedControlPoints = constants.segmentBudget + 1u;
        const std::vector<Vector4> history =
            BuildTrailHistoryUpload(instance, requestedControlPoints, settings.smoothing);
        constants.historyCount = static_cast<uint32_t>(history.size());
        historySrv = context.gpuParticleSystem->UpdateTrailHistory(
            history.empty() ? nullptr : history.data(),
            static_cast<uint32_t>(history.size()));
    }
    if (historySrv.ptr == 0) {
        return;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { context.srvDescriptorHeap };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetComputeRootSignature(rootSignature);
    commandList->SetPipelineState(pipelineState);
    commandList->SetComputeRoot32BitConstants(
        0,
        static_cast<UINT>(sizeof(TrailMeshStreamConstants) / sizeof(uint32_t)),
        &constants,
        0);
    commandList->SetComputeRootDescriptorTable(1, historySrv);
    commandList->SetComputeRootDescriptorTable(2, controlPointUav);
    commandList->SetComputeRootDescriptorTable(3, segmentUav);

    const uint32_t groupCount = (maxSegments + kTrailComputeThreadGroupSize - 1) / kTrailComputeThreadGroupSize;
    commandList->Dispatch(groupCount, 1, 1);
}

void TrailRenderer::BuildMeshStream(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const TrailRenderInput& input) const {
    if (commandList == nullptr ||
        context.srvDescriptorHeap == nullptr ||
        context.appPipelines == nullptr ||
        context.gpuParticleSystem == nullptr) {
        return;
    }

    const vfx::TrailMeshStreamResourceSet* meshStreamResources =
        context.typedResources != nullptr ? &context.typedResources->trail.renderer.meshStream : nullptr;
    const vfx::TrailMeshStreamPlan plan = BuildMeshStreamPlan(input, meshStreamResources);
    if (plan.resources == nullptr ||
        plan.resources->controlPointBuffer[0] == '\0' ||
        plan.resources->segmentBuffer[0] == '\0' ||
        plan.resources->vertexBuffer[0] == '\0' ||
        plan.resources->indexBuffer[0] == '\0' ||
        plan.resources->drawArgs[0] == '\0') {
        return;
    }

    ID3D12RootSignature* rootSignature = context.appPipelines->GetTrailMeshBuildComputeRootSignature();
    ID3D12PipelineState* pipelineState = context.appPipelines->GetTrailMeshBuildComputePSO();
    if (rootSignature == nullptr || pipelineState == nullptr) {
        return;
    }

    const D3D12_GPU_DESCRIPTOR_HANDLE controlPointSrv =
        context.gpuParticleSystem->SrvHandleForResource(plan.resources->controlPointBuffer);
    const D3D12_GPU_DESCRIPTOR_HANDLE segmentSrv =
        context.gpuParticleSystem->SrvHandleForResource(plan.resources->segmentBuffer);
    const D3D12_GPU_DESCRIPTOR_HANDLE vertexUav =
        context.gpuParticleSystem->UavHandleForResource(plan.resources->vertexBuffer);
    const D3D12_GPU_DESCRIPTOR_HANDLE indexUav =
        context.gpuParticleSystem->UavHandleForResource(plan.resources->indexBuffer);
    const D3D12_GPU_DESCRIPTOR_HANDLE drawArgsUav =
        context.gpuParticleSystem->UavHandleForResource(plan.resources->drawArgs);
    if (controlPointSrv.ptr == 0 ||
        segmentSrv.ptr == 0 ||
        vertexUav.ptr == 0 ||
        indexUav.ptr == 0 ||
        drawArgsUav.ptr == 0) {
        return;
    }

    const uint32_t maxSegments = (std::max)(context.gpuParticleSystem->MaxParticles(), kMinTrailSegmentBudget);
    TrailMeshBuildConstants constants{};
    constants.maxSegments = maxSegments;
    constants.hasPrimary = plan.hasPrimaryItem ? 1u : 0u;
    constants.segmentBudget = (std::min)(
        (std::max)(plan.segmentCountEstimate, kMinTrailSegmentBudget),
        maxSegments);
    constants.cameraPosition = {
        context.frameState->cameraWorldPosition.x,
        context.frameState->cameraWorldPosition.y,
        context.frameState->cameraWorldPosition.z,
        1.0f,
    };
    if (input.settings != nullptr) {
        constants.joinParams.x = (std::clamp)(input.settings->miterLimit, 1.0f, 8.0f);
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { context.srvDescriptorHeap };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetComputeRootSignature(rootSignature);
    commandList->SetPipelineState(pipelineState);
    commandList->SetComputeRoot32BitConstants(
        0,
        static_cast<UINT>(sizeof(TrailMeshBuildConstants) / sizeof(uint32_t)),
        &constants,
        0);
    commandList->SetComputeRootDescriptorTable(1, controlPointSrv);
    commandList->SetComputeRootDescriptorTable(2, segmentSrv);
    commandList->SetComputeRootDescriptorTable(3, vertexUav);
    commandList->SetComputeRootDescriptorTable(4, indexUav);
    commandList->SetComputeRootDescriptorTable(5, drawArgsUav);

    const uint32_t groupCount = (maxSegments + kTrailComputeThreadGroupSize - 1) / kTrailComputeThreadGroupSize;
    commandList->Dispatch(groupCount, 1, 1);
}

void TrailRenderer::Draw(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const TrailRenderInput& input) const {
    const vfx::TrailRendererResources* trailResources =
        context.typedResources != nullptr ? &context.typedResources->trail.renderer : nullptr;
    const vfx::TrailMeshStreamResourceSet* meshStreamResources =
        trailResources != nullptr ? &trailResources->meshStream : nullptr;
    const vfx::TrailMeshStreamPlan meshStreamPlan =
        BuildMeshStreamPlan(input, meshStreamResources);
    if (DrawMeshStream(commandList, context, meshStreamPlan)) {
        return;
    }

    const vfx::ComponentDrawParams drawParams =
        vfx::ResolveTrailDrawParams(
            input.settings,
            {0.02f, 1.0f, 0.5f, 1.35f});
    const vfx::VfxRendererResourceSet* rendererResources =
        trailResources != nullptr ? &trailResources->indirectSprite : nullptr;
    vfx::DrawIndirectSpriteComponents(
        commandList,
        context,
        context.appPipelines != nullptr ? context.appPipelines->GetTrailMeshPSO() : nullptr,
        drawParams,
        rendererResources);
}

vfx::TrailMeshStreamPlan vfx::BuildTrailMeshStreamPlan(
    const TrailRenderInput& input,
    const TrailMeshStreamResourceSet* resources) {
    TrailMeshStreamPlan plan{};
    plan.resources = resources;
    plan.input = &input;
    plan.enabled = resources != nullptr && resources->usesDedicatedMeshStream;
    plan.hasPrimaryItem = input.primary.primaryItem != nullptr;
    plan.normalizedAge = input.primary.normalizedAge;
    plan.renderQueue = input.primary.renderQueue;
    if (plan.hasPrimaryItem) {
        plan.segmentCountEstimate =
            input.settings != nullptr ? (std::max)(input.settings->segmentBudget, kMinTrailSegmentBudget) : kMinTrailSegmentBudget;
        plan.controlPointCountEstimate = plan.segmentCountEstimate + 1;
        plan.vertexCountEstimate = plan.controlPointCountEstimate * kTrailVerticesPerControlPoint;
        plan.indexCountEstimate = plan.segmentCountEstimate * kTrailIndicesPerSegment;
    }
    return plan;
}

vfx::TrailMeshStreamDrawStatus vfx::EvaluateTrailMeshStreamDrawStatus(
    const TrailMeshStreamPlan& plan,
    const VfxRenderContext& context) {
    TrailMeshStreamDrawStatus status{};
    if (!plan.enabled) {
        status.fallbackReason = "toggle off";
        return status;
    }
    if (!plan.hasPrimaryItem) {
        status.fallbackReason = "no primary trail item";
        return status;
    }
    if (plan.resources == nullptr) {
        status.fallbackReason = "missing mesh stream resource set";
        return status;
    }
    if (plan.input == nullptr) {
        status.fallbackReason = "missing TrailRenderInput";
        return status;
    }
    if (context.appPipelines == nullptr) {
        status.fallbackReason = "missing AppPipelines";
        return status;
    }
    if (context.gpuParticleSystem == nullptr) {
        status.fallbackReason = "missing AppGpuParticleSystem";
        return status;
    }
    if (context.frameState == nullptr) {
        status.fallbackReason = "missing FrameLoopState";
        return status;
    }
    if (context.srvDescriptorHeap == nullptr) {
        status.fallbackReason = "missing SRV descriptor heap";
        return status;
    }
    if (context.vfxTextureHandle.ptr == 0) {
        status.fallbackReason = "missing VFX texture SRV";
        return status;
    }
    if (context.gpuParticleSystem->CommandSignature() == nullptr) {
        status.fallbackReason = "missing command signature";
        return status;
    }
    if (context.appPipelines->GetParticleRootSignature() == nullptr) {
        status.fallbackReason = "missing particle root signature";
        return status;
    }
    if (context.appPipelines->GetTrailMeshStreamPSO() == nullptr) {
        status.fallbackReason = "missing Trail mesh stream PSO";
        return status;
    }
    if (context.gpuParticleSystem->IndirectArgsForResource(plan.resources->drawArgs) == nullptr) {
        status.fallbackReason = "missing Trail draw args buffer";
        return status;
    }
    if (context.gpuParticleSystem->TrailVertexBufferView().BufferLocation == 0) {
        status.fallbackReason = "missing Trail vertex buffer";
        return status;
    }
    if (context.gpuParticleSystem->TrailIndexBufferView().BufferLocation == 0) {
        status.fallbackReason = "missing Trail index buffer";
        return status;
    }
    if (!context.gpuParticleSystem->HasTrailViewProjectionBuffer()) {
        status.fallbackReason = "missing Trail view-projection buffer";
        return status;
    }

    status.ready = true;
    status.fallbackReason = "ready";
    return status;
}

vfx::TrailMeshStreamDebugPreview vfx::BuildTrailMeshStreamDebugPreview(
    const TrailRenderInput& input,
    const Vector3& cameraWorldPosition,
    uint32_t maxPreviewPoints) {
    TrailMeshStreamDebugPreview preview{};
    preview.hasPrimaryItem =
        input.primary.primaryItem != nullptr &&
        input.primary.instance != nullptr &&
        input.primary.componentCommon != nullptr &&
        input.settings != nullptr;
    if (!preview.hasPrimaryItem) {
        return preview;
    }

    const EffectInstance& instance = *input.primary.instance;
    const EffectTrailSettings& settings = *input.settings;
    const uint32_t segmentBudget = (std::max)(settings.segmentBudget, kMinTrailSegmentBudget);
    const uint32_t requestedControlPoints = segmentBudget + 1u;
    const uint32_t previewControlPoints = (std::min)(
        (std::max)(maxPreviewPoints, 2u),
        requestedControlPoints);
    const std::vector<Vector4> historyUpload =
        BuildTrailHistoryUpload(instance, previewControlPoints, settings.smoothing);
    if (historyUpload.empty()) {
        return preview;
    }

    preview.historyCount = instance.trailHistoryCount;
    preview.sampleDistance = settings.sampleDistance;
    preview.smoothing = settings.smoothing;
    preview.miterLimit = settings.miterLimit;

    std::vector<Vector3> positions;
    positions.reserve(historyUpload.size());
    for (const Vector4& point : historyUpload) {
        positions.push_back(ToVector3(point));
    }

    preview.points.reserve(positions.size());
    Vector3 previousSide{1.0f, 0.0f, 0.0f};
    const float baseWidth = (std::max)(0.001f, settings.width * instance.transform.scale.x);
    for (uint32_t i = 0; i < static_cast<uint32_t>(positions.size()); ++i) {
        const float t = positions.size() > 1
            ? static_cast<float>(i) / static_cast<float>(positions.size() - 1)
            : 0.0f;
        const uint32_t nextIndex = (std::min)(i + 1, static_cast<uint32_t>(positions.size() - 1));
        const Vector3 segmentTangent = NormalizeOr(Subtract(positions[nextIndex], positions[i]), {0.0f, 1.0f, 0.0f});
        const Vector3 tangent = ComputeControlPointTangent(positions, i, segmentTangent);
        Vector3 side = ComputeControlPointSide(positions[i], tangent, cameraWorldPosition, previousSide);
        side = AlignSide(side, previousSide);
        Vector3 miterOffset = ComputeControlPointMiterOffset(
            positions,
            i,
            tangent,
            side,
            cameraWorldPosition,
            settings.miterLimit);
        miterOffset = AlignSide(miterOffset, side);

        TrailMeshStreamDebugPoint point{};
        point.position = positions[i];
        point.tangent = tangent;
        point.side = side;
        point.miterOffset = miterOffset;
        point.width = baseWidth * (settings.widthHead + (settings.widthTail - settings.widthHead) * t);
        point.alpha = 1.0f + (settings.alphaTail - 1.0f) * t;
        point.normalized = t;
        preview.points.push_back(point);
        previousSide = side;
    }
    return preview;
}

vfx::TrailMeshStreamPlan TrailRenderer::BuildMeshStreamPlan(
    const TrailRenderInput& input,
    const vfx::TrailMeshStreamResourceSet* resources) const {
    return vfx::BuildTrailMeshStreamPlan(input, resources);
}

bool TrailRenderer::DrawMeshStream(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const vfx::TrailMeshStreamPlan& plan) const {
    if (commandList == nullptr) {
        return false;
    }

    const vfx::TrailMeshStreamDrawStatus status = vfx::EvaluateTrailMeshStreamDrawStatus(plan, context);
    if (!status.ready) {
        return false;
    }

    ID3D12PipelineState* pipelineState = context.appPipelines->GetTrailMeshStreamPSO();
    ID3D12Resource* drawArgs = context.gpuParticleSystem->IndirectArgsForResource(plan.resources->drawArgs);
    const D3D12_VERTEX_BUFFER_VIEW vertexBufferView = context.gpuParticleSystem->TrailVertexBufferView();
    const D3D12_INDEX_BUFFER_VIEW indexBufferView = context.gpuParticleSystem->TrailIndexBufferView();
    const D3D12_GPU_VIRTUAL_ADDRESS viewProjectionCbv =
        context.gpuParticleSystem->UpdateTrailViewProjection(context.frameState->viewProjectionMatrix);
    if (viewProjectionCbv == 0) {
        return false;
    }

    const vfx::ComponentDrawParams drawParams =
        vfx::ResolveTrailDrawParams(
            plan.input->settings,
            {0.02f, 1.0f, 0.5f, 1.35f});

    ID3D12DescriptorHeap* descriptorHeaps[] = { context.srvDescriptorHeap };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(context.appPipelines->GetParticleRootSignature());
    commandList->SetPipelineState(pipelineState);
    commandList->SetGraphicsRootConstantBufferView(0, viewProjectionCbv);
    commandList->SetGraphicsRootDescriptorTable(
        1,
        context.gpuParticleSystem->SrvHandleForResource(plan.resources->controlPointBuffer));
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(2, context.vfxTextureHandle);
    if (context.depthTextureHandle.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(3, context.depthTextureHandle);
    }

    const float depthFadeParams[4] = {
        drawParams.depthFadeSoftness,
        drawParams.distortionDepthAttenuation,
        drawParams.particleEdgeSoftness,
        drawParams.trailTailFade
    };
    commandList->SetGraphicsRoot32BitConstants(4, 4, depthFadeParams, 0);
    commandList->ExecuteIndirect(
        context.gpuParticleSystem->CommandSignature(),
        1,
        drawArgs,
        0,
        nullptr,
        0);
    return true;
}
