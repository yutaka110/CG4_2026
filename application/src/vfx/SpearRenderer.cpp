#include "vfx/SpearRenderer.h"

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

namespace {
struct SpearDrawConstants {
    Matrix4x4 worldViewProjection{};
    Vector4 color{};
    Vector4 spearParams{};
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

bool IsSpearComponent(const EffectComponentCommon& component) {
    return component.rendererId == "SpearRenderer" || component.techniqueId == "SpearMesh";
}
} // namespace

void SpearRenderer::RegisterPasses(
    const AppFrameGraphBuildContext& ctx,
    const vfx::VfxTypedResourceSet& resources) const {
    (void)resources;
    ctx.renderGraph->AddPass({
        "VFX.Spear",
        ge3::graphics::RenderPassLayer::Vfx,
        {
            {"SceneDepth", ge3::graphics::RenderResourceAccessType::ReadDepth},
            {"VfxAccumulation", ge3::graphics::RenderResourceAccessType::WriteRtv},
        },
        "SceneDepth",
        [this, ctx](ge3::graphics::RenderPassContext& passContext) {
            if (ctx.effectRuntime == nullptr || ctx.effectRuntime->cylinderQueue.empty()) {
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
                ctx.effectRuntime->cylinderQueue);
        }});
}

void SpearRenderer::Draw(
    ID3D12GraphicsCommandList* commandList,
    const VfxRenderContext& context,
    const CylinderRenderQueue& queue) const {
    if (commandList == nullptr ||
        context.appPipelines == nullptr ||
        context.scene == nullptr ||
        context.frameState == nullptr ||
        context.srvDescriptorHeap == nullptr ||
        context.scene->spear.vbv.BufferLocation == 0 ||
        context.appPipelines->GetRingRootSignature() == nullptr ||
        context.appPipelines->GetSpearPSO() == nullptr) {
        return;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { context.srvDescriptorHeap };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(context.appPipelines->GetRingRootSignature());
    commandList->SetPipelineState(context.appPipelines->GetSpearPSO());
    commandList->IASetVertexBuffers(0, 1, &context.scene->spear.vbv);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (context.scene->gradationLineTextureSrvHandleGPU.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(1, context.scene->gradationLineTextureSrvHandleGPU);
    }

    for (const CylinderRenderItem& item : queue) {
        if (item.common.instance == nullptr ||
            item.common.componentCommon == nullptr ||
            item.settings == nullptr ||
            !IsSpearComponent(*item.common.componentCommon)) {
            continue;
        }

        const EffectInstance& instance = *item.common.instance;
        const EffectComponentCommon& component = *item.common.componentCommon;
        const EffectCylinderSettings& settings = *item.settings;

        const Vector3 scale = {
            instance.transform.scale.x * component.size.x,
            instance.transform.scale.y * component.size.y,
            instance.transform.scale.z * component.size.z,
        };
        const Matrix4x4 world = MakeAffineMatrix(
            scale,
            instance.transform.rotate,
            instance.transform.translate);

        const float alpha = ResolveAlpha(item.common.normalizedAge, settings.fadeOut);
        Vector4 color = MultiplyColor(instance.color, component.color);
        color.x *= settings.emissive;
        color.y *= settings.emissive;
        color.z *= settings.emissive;

        SpearDrawConstants constants{};
        constants.worldViewProjection = Multiply(world, context.frameState->viewProjectionMatrix);
        constants.color = color;
        constants.spearParams = {
            alpha,
            context.beamTime,
            settings.topRadius,
            0.0f,
        };

        commandList->SetGraphicsRoot32BitConstants(
            0,
            static_cast<UINT>(sizeof(SpearDrawConstants) / sizeof(uint32_t)),
            &constants,
            0);
        commandList->DrawInstanced(context.scene->spear.vertexCount, 1, 0, 0);
    }
}
