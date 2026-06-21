#include "AppPostProcessPipeline.h"

#include <string>
#include <utility>
#include <vector>

#include "AppFrameGraphBuilder.h"
#include "AppPipelines.h"
#include "AppVfxRenderTargets.h"
#include "PostProcessStack.h"
#include "graphics/RenderGraph.h"

namespace {
ID3D12PipelineState* ResolvePostProcessPso(const AppPipelines& pipelines, const std::string& name) {
    if (name == "BloomExtract") {
        return pipelines.GetBloomExtractPSO();
    }
    if (name == "BloomDownsample") {
        return pipelines.GetBloomDownsamplePSO();
    }
    if (name == "BloomUpsample") {
        return pipelines.GetBloomUpsamplePSO();
    }
    if (name == "BlurHorizontal") {
        return pipelines.GetBlurHorizontalPSO();
    }
    if (name == "BlurVertical") {
        return pipelines.GetBlurVerticalPSO();
    }
    if (name == "BoxBlurHorizontal") {
        return pipelines.GetBoxBlurHorizontalPSO();
    }
    if (name == "BoxBlurVertical") {
        return pipelines.GetBoxBlurVerticalPSO();
    }
    if (name == "GaussianBlurHorizontal") {
        return pipelines.GetGaussianBlurHorizontalPSO();
    }
    if (name == "GaussianBlurVertical") {
        return pipelines.GetGaussianBlurVerticalPSO();
    }
    if (name == "DistortionComposite") {
        return pipelines.GetDistortionCompositePSO();
    }
    if (name == "AccretionComposite") {
        return pipelines.GetAccretionCompositePSO();
    }
    if (name == "ToneMapping") {
        return pipelines.GetToneMappingPSO();
    }
    if (name == "GlowComposite") {
        return pipelines.GetGlowCompositePSO();
    }
    if (name == "PrewittOutline") {
        return pipelines.GetPrewittOutlinePSO();
    }
    if (name == "Grayscale") {
        return pipelines.GetGrayscalePSO();
    }
    if (name == "Vignette") {
        return pipelines.GetVignettePSO();
    }
    return nullptr;
}

constexpr uint32_t kPostProcessParamCount = 16;

void BuildPassParams(const PostProcessPass& postPass, float passParams[kPostProcessParamCount]) {
    for (uint32_t i = 0; i < kPostProcessParamCount; ++i) {
        passParams[i] = 0.0f;
    }
    passParams[0] = postPass.intensity;

    if (postPass.pipeline == "BloomExtract") {
        passParams[1] = postPass.parameters.bloomThresholdMin;
        passParams[2] = postPass.parameters.bloomThresholdMax;
        passParams[3] = postPass.parameters.bloomSoftKnee;
        return;
    }
    if (postPass.pipeline == "BloomDownsample") {
        return;
    }
    if (postPass.pipeline == "BloomUpsample") {
        passParams[1] = postPass.parameters.bloomUpsampleBlend;
        passParams[2] = postPass.parameters.bloomUpsampleSoftKnee;
        return;
    }
    if (postPass.pipeline == "BlurHorizontal" || postPass.pipeline == "BlurVertical") {
        passParams[1] = postPass.parameters.blurRadius;
        return;
    }
    if (postPass.pipeline == "BoxBlurHorizontal" || postPass.pipeline == "BoxBlurVertical") {
        passParams[1] = postPass.parameters.boxBlurKernelRadius;
        return;
    }
    if (postPass.pipeline == "GaussianBlurHorizontal" || postPass.pipeline == "GaussianBlurVertical") {
        passParams[1] = postPass.parameters.gaussianBlurKernelRadius;
        passParams[2] = postPass.parameters.gaussianBlurSigma;
        return;
    }
    if (postPass.pipeline == "DistortionComposite") {
        passParams[1] = postPass.parameters.distortionScale;
        return;
    }
    if (postPass.pipeline == "AccretionComposite") {
        passParams[1] = postPass.parameters.accretionRadius;
        passParams[4] = postPass.parameters.accretionDiskStretch;
        passParams[5] = postPass.parameters.accretionTurbulence;
        passParams[6] = postPass.parameters.accretionChromaticAberration;
        passParams[7] = postPass.parameters.accretionCoreSize;
        passParams[8] = postPass.parameters.accretionCenterX;
        passParams[9] = postPass.parameters.accretionCenterY;
        passParams[10] = postPass.parameters.accretionFlowSpeed;
        passParams[11] = postPass.parameters.accretionRoadDepthFade;
        passParams[12] = postPass.parameters.accretionCoreDarkness;
        passParams[13] = postPass.parameters.accretionGuideOpacity;
        passParams[14] = postPass.parameters.accretionLensStrength;
        passParams[15] = postPass.parameters.accretionGuideWidth;
        return;
    }
    if (postPass.pipeline == "ToneMapping") {
        passParams[1] = postPass.parameters.toneExposure;
        return;
    }
    if (postPass.pipeline == "GlowComposite") {
        passParams[1] = postPass.parameters.glowWeight;
        passParams[2] = postPass.parameters.glowTintR;
        passParams[3] = postPass.parameters.glowTintG;
        passParams[4] = postPass.parameters.glowTintB;
        return;
    }
    if (postPass.pipeline == "PrewittOutline") {
        passParams[1] = postPass.parameters.outlineThreshold;
        passParams[2] = postPass.parameters.outlineThickness;
        passParams[3] = postPass.parameters.outlineSoftness;
        passParams[4] = postPass.parameters.outlineColorR;
        passParams[5] = postPass.parameters.outlineColorG;
        passParams[6] = postPass.parameters.outlineColorB;
        passParams[7] = postPass.parameters.outlineDepthWeight;
        return;
    }
    if (postPass.pipeline == "Grayscale") {
        passParams[1] = postPass.parameters.grayscaleMode;
        return;
    }
    if (postPass.pipeline == "Vignette") {
        passParams[1] = postPass.parameters.vignetteRadius;
        passParams[2] = postPass.parameters.vignetteSoftness;
        passParams[3] = postPass.parameters.vignettePower;
        return;
    }
}

} // namespace

void AppPostProcessPipeline::RegisterPasses(const AppFrameGraphBuildContext& ctx) const {
    const PostProcessExecutionPlan executionPlan = ctx.postProcessStack->BuildExecutionPlan();
    const std::string finalOutputResource =
        executionPlan.finalOutputResource.empty() ? "SceneColor" : executionPlan.finalOutputResource;
    for (const PostProcessExecutionPass& executionPass : executionPlan.passes) {
        const PostProcessPass& postPass = executionPass.pass;

        ctx.renderGraph->DeclareTransientRenderTarget(
            postPass.outputResource,
            postPass.resolutionScale,
            DXGI_FORMAT_R8G8B8A8_UNORM);
        std::vector<ge3::graphics::RenderPassResourceAccess> accesses = {
            {postPass.inputResource, ge3::graphics::RenderResourceAccessType::ReadSrv},
            {postPass.outputResource, ge3::graphics::RenderResourceAccessType::WriteRtv},
        };
        if (postPass.pipeline == "PrewittOutline") {
            accesses.push_back({"SceneDepth", ge3::graphics::RenderResourceAccessType::ReadDepth});
            accesses.push_back({postPass.tertiaryInputResource, ge3::graphics::RenderResourceAccessType::ReadSrv});
        } else {
            accesses.push_back({postPass.secondaryInputResource, ge3::graphics::RenderResourceAccessType::ReadSrv});
            accesses.push_back({postPass.tertiaryInputResource, ge3::graphics::RenderResourceAccessType::ReadSrv});
        }

        ctx.renderGraph->AddPass({
            std::string("PostProcess.") + postPass.name,
            ge3::graphics::RenderPassLayer::PostProcess,
            std::move(accesses),
            "",
            [ctx, postPass](ge3::graphics::RenderPassContext& passContext) {
                ID3D12DescriptorHeap* descriptorHeaps[] = { ctx.srvDescriptorHeap };
                passContext.commandList->SetDescriptorHeaps(1, descriptorHeaps);
                ID3D12PipelineState* pipelineState = ResolvePostProcessPso(*ctx.appPipelines, postPass.pipeline);
                if (pipelineState == nullptr) {
                    return;
                }
                float passParams[kPostProcessParamCount] = {};
                BuildPassParams(postPass, passParams);
                if (postPass.pipeline == "AccretionComposite") {
                    passParams[2] = ctx.beamTime;
                    uint32_t targetWidth = 0;
                    uint32_t targetHeight = 0;
                    if (ctx.vfxRenderTargets->GetTargetSize(postPass.outputResource, targetWidth, targetHeight)) {
                        passParams[3] = static_cast<float>(targetWidth) / static_cast<float>(targetHeight);
                    } else {
                        passParams[3] = 16.0f / 9.0f;
                    }
                }
                if (postPass.pipeline == "PrewittOutline") {
                    ctx.vfxRenderTargets->ExecuteDebugPreviewPass(
                        passContext.commandList,
                        postPass.outputResource,
                        ctx.appPipelines->GetCompositeRootSignature(),
                        pipelineState,
                        ctx.vfxRenderTargets->GetSrvHandle(postPass.inputResource),
                        ctx.depthTextureHandle,
                        ctx.vfxRenderTargets->GetSrvHandle(postPass.tertiaryInputResource),
                        passParams);
                } else {
                    ctx.vfxRenderTargets->ExecutePostProcessPass(
                        passContext.commandList,
                        postPass.outputResource,
                        ctx.appPipelines->GetCompositeRootSignature(),
                        pipelineState,
                        postPass.inputResource,
                        postPass.secondaryInputResource,
                        postPass.tertiaryInputResource,
                        passParams);
                }
        }});
    }

    ctx.renderGraph->AddPass({
        "PostProcess.CompositeToBackBuffer",
        ge3::graphics::RenderPassLayer::PostProcess,
        {
            {"SceneColor", ge3::graphics::RenderResourceAccessType::ReadSrv},
            {"VfxAccumulation", ge3::graphics::RenderResourceAccessType::ReadSrv},
            {finalOutputResource, ge3::graphics::RenderResourceAccessType::ReadSrv},
            {"BackBuffer", ge3::graphics::RenderResourceAccessType::WriteRtv},
        },
        "",
        [ctx, finalOutputResource](ge3::graphics::RenderPassContext& passContext) {
            ID3D12DescriptorHeap* descriptorHeaps[] = { ctx.srvDescriptorHeap };
            passContext.commandList->SetDescriptorHeaps(1, descriptorHeaps);
            const float compositeParams[kPostProcessParamCount] = {
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f
            };
            ctx.vfxRenderTargets->CompositeToBackBuffer(
                passContext.commandList,
                ctx.backBuffer,
                ctx.rtv,
                ctx.appPipelines->GetCompositeRootSignature(),
                ctx.appPipelines->GetCompositePSO(),
                finalOutputResource,
                compositeParams);
        },
        true});
}
