#include "vfx/RingRenderer.h"

#include "../../AppFrameGraphBuilder.h"
#include "../../AppFrameState.h"
#include "../../AppPipelines.h"
#include "../../AppRuntimeState.h"
#include "../../AppSceneResources.h"
#include "graphics/RenderGraph.h"
#include "vfx/AppVfxRendererSet.h"
#include "vfx/VfxPassRegistration.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
struct RingDrawConstants {
    Matrix4x4 worldViewProjection{};
    Vector4 color{};
    Vector4 ringParams{};
};

Vector4 MultiplyColor(const Vector4& a, const Vector4& b) {
    return {
        a.x * b.x,
        a.y * b.y,
        a.z * b.z,
        a.w * b.w,
    };
}

float Clamp01(float value) {
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

float ResolveAlpha(float normalizedAge, float fadeOut) {
    const float fade = (std::max)(0.0f, fadeOut);
    if (fade <= 0.0f) {
        return 1.0f;
    }
    return std::pow(1.0f - Clamp01(normalizedAge), fade);
}
} // namespace

void RingRenderer::RegisterPasses(
    const AppFrameGraphBuildContext& ctx,
    const vfx::VfxTypedResourceSet& resources) const {
    (void)resources;
    ctx.renderGraph->AddPass({
        "VFX.Ring",
        ge3::graphics::RenderPassLayer::Vfx,
        {
            {"SceneDepth", ge3::graphics::RenderResourceAccessType::ReadDepth},
            {"VfxAccumulation", ge3::graphics::RenderResourceAccessType::WriteRtv},
        },
        "SceneDepth",
        [this, ctx](ge3::graphics::RenderPassContext& passContext) {
            if (ctx.effectRuntime == nullptr || ctx.effectRuntime->ringQueue.empty()) {
                return;
            }
            VfxRenderContext renderContext{};
            renderContext.appPipelines = ctx.appPipelines;
            renderContext.renderResources = ctx.renderResources;
            renderContext.scene = ctx.scene;
            renderContext.gpuParticleSystem = ctx.gpuParticleSystem;
            renderContext.beam = ctx.vfxRenderers != nullptr ? ctx.vfxRenderers->beam : nullptr;
            renderContext.frameState = ctx.frameState;
            renderContext.srvDescriptorHeap = ctx.srvDescriptorHeap;
            renderContext.vfxTextureHandle = ctx.vfxTextureHandle;
            renderContext.depthTextureHandle = ctx.depthTextureHandle;
            renderContext.beamTime = ctx.beamTime;
            Draw(
                passContext.commandList,
                renderContext,
                ctx.effectRuntime->ringQueue);
        }});
}

void RingRenderer::Draw(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const RingRenderQueue& queue) const {
    if (commandList == nullptr ||
        context.appPipelines == nullptr ||
        context.scene == nullptr ||
        context.frameState == nullptr ||
        context.srvDescriptorHeap == nullptr ||
        context.scene->ring.vbv.BufferLocation == 0 ||
        context.scene->gradationLineTextureSrvHandleGPU.ptr == 0 ||
        context.appPipelines->GetRingRootSignature() == nullptr ||
        context.appPipelines->GetRingPSO() == nullptr) {
        return;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { context.srvDescriptorHeap };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(context.appPipelines->GetRingRootSignature());
    commandList->SetPipelineState(context.appPipelines->GetRingPSO());
    commandList->IASetVertexBuffers(0, 1, &context.scene->ring.vbv);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(1, context.scene->gradationLineTextureSrvHandleGPU);

    for (const RingRenderItem& item : queue) {
        if (item.common.instance == nullptr ||
            item.common.componentCommon == nullptr ||
            item.settings == nullptr) {
            continue;
        }

        const EffectInstance& instance = *item.common.instance;
        const EffectComponentCommon& component = *item.common.componentCommon;
        const EffectRingSettings& settings = *item.settings;

        const Vector3 scale = {
            instance.transform.scale.x * component.size.x,
            instance.transform.scale.y * component.size.y,
            instance.transform.scale.z * component.size.z,
        };
        Matrix4x4 world = MakeAffineMatrix(
            scale,
            instance.transform.rotate,
            instance.transform.translate);

        const float expand = settings.expansion * Clamp01(item.common.normalizedAge);
        float innerRadius = (std::max)(0.0f, settings.innerRadius + expand);
        float outerRadius = (std::max)(innerRadius + 0.001f, settings.outerRadius + expand);
        const float alpha = ResolveAlpha(item.common.normalizedAge, settings.fadeOut);
        Vector4 color = MultiplyColor(instance.color, component.color);
        color.x *= settings.emissive;
        color.y *= settings.emissive;
        color.z *= settings.emissive;

        RingDrawConstants constants{};
        constants.worldViewProjection = Multiply(world, context.frameState->viewProjectionMatrix);
        constants.color = color;
        constants.ringParams = {
            outerRadius,
            innerRadius,
            context.beamTime * settings.uvScrollSpeed,
            alpha
        };

        commandList->SetGraphicsRoot32BitConstants(
            0,
            static_cast<UINT>(sizeof(RingDrawConstants) / sizeof(uint32_t)),
            &constants,
            0);
        commandList->DrawInstanced(context.scene->ring.vertexCount, 1, 0, 0);
    }
}
