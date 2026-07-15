#include "PostProcessStack.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr const char* kPostProcessOutputResource = "PostProcessOutput";
constexpr const char* kPostProcessSwapOutputResource = "PostProcessSwapOutput";
constexpr const char* kPostProcessPreviousInputResource = "PostProcessPreviousInput";
constexpr const char* kPostProcessPreviousSwapOutputResource = "PostProcessPreviousSwapOutput";

float SmoothStep01(float value) {
    const float saturated = (std::clamp)(value, 0.0f, 1.0f);
    return saturated * saturated * (3.0f - 2.0f * saturated);
}

std::string ResolvePostProcessInputResource(
    std::string_view resource,
    std::string_view currentOutput,
    std::string_view previousInput) {
    if (resource == kPostProcessOutputResource) {
        return std::string(currentOutput);
    }
    if (resource == kPostProcessPreviousInputResource) {
        return std::string(previousInput);
    }
    return std::string(resource);
}

std::string ResolvePostProcessOutputResource(
    std::string_view resource,
    std::string_view currentOutput,
    std::string_view previousInput) {
    std::string_view swapSource = currentOutput;
    if (resource == kPostProcessPreviousSwapOutputResource) {
        swapSource = previousInput;
    } else if (resource != kPostProcessSwapOutputResource) {
        return std::string(resource);
    }
    return swapSource == "PostColorA" ? "PostColorB" : "PostColorA";
}
} // namespace

void PostProcessStack::ResetToVfxDefaults() {
    passes_.clear();
    warpTunnelPhase_ = WarpTunnelPhase::Idle;
    warpTunnelTransition_ = 0.0f;
    warpTunnelFlash_ = 0.0f;
    warpTunnelPhaseElapsed_ = 0.0f;
    warpTunnelEnterDuration_ = 0.65f;
    warpTunnelExitDuration_ = 0.55f;
    dissolvePhase_ = DissolvePhase::Idle;
    dissolveThreshold_ = 0.0f;
    dissolvePhaseElapsed_ = 0.0f;
    dissolveOutDuration_ = 0.80f;
    dissolveSwitchDuration_ = 0.12f;
    dissolveInDuration_ = 0.70f;
    dissolveSwitchRequested_ = false;
    PostProcessPass bloomExtract{};
    bloomExtract.name = "BloomExtract";
    bloomExtract.inputResource = "VfxAccumulation";
    bloomExtract.outputResource = "BloomExtractHalf";
    bloomExtract.pipeline = "BloomExtract";
    bloomExtract.secondaryInputResource = "VfxAccumulation";
    bloomExtract.tertiaryInputResource = "VfxAccumulation";
    bloomExtract.enabled = true;
    bloomExtract.intensity = 1.0f;
    bloomExtract.resolutionScale = 0.5f;
    bloomExtract.parameters.bloomThresholdMin = 0.62f;
    bloomExtract.parameters.bloomThresholdMax = 1.35f;
    bloomExtract.parameters.bloomSoftKnee = 0.16f;
    passes_.push_back(bloomExtract);

    PostProcessPass bloomDownsampleQuarter{};
    bloomDownsampleQuarter.name = "BloomDownsampleQuarter";
    bloomDownsampleQuarter.inputResource = "BloomExtractHalf";
    bloomDownsampleQuarter.outputResource = "BloomQuarterA";
    bloomDownsampleQuarter.pipeline = "BloomDownsample";
    bloomDownsampleQuarter.secondaryInputResource = "BloomExtractHalf";
    bloomDownsampleQuarter.tertiaryInputResource = "BloomExtractHalf";
    bloomDownsampleQuarter.enabled = true;
    bloomDownsampleQuarter.intensity = 1.0f;
    bloomDownsampleQuarter.resolutionScale = 0.25f;
    passes_.push_back(bloomDownsampleQuarter);

    PostProcessPass bloomBlurHorizontalQuarter{};
    bloomBlurHorizontalQuarter.name = "BloomBlurHorizontalQuarter";
    bloomBlurHorizontalQuarter.inputResource = "BloomQuarterA";
    bloomBlurHorizontalQuarter.outputResource = "BloomQuarterB";
    bloomBlurHorizontalQuarter.pipeline = "BlurHorizontal";
    bloomBlurHorizontalQuarter.secondaryInputResource = "BloomQuarterA";
    bloomBlurHorizontalQuarter.tertiaryInputResource = "BloomQuarterA";
    bloomBlurHorizontalQuarter.enabled = true;
    bloomBlurHorizontalQuarter.intensity = 1.0f;
    bloomBlurHorizontalQuarter.resolutionScale = 0.25f;
    bloomBlurHorizontalQuarter.parameters.blurRadius = 6.0f;
    passes_.push_back(bloomBlurHorizontalQuarter);

    PostProcessPass bloomBlurVerticalQuarter{};
    bloomBlurVerticalQuarter.name = "BloomBlurVerticalQuarter";
    bloomBlurVerticalQuarter.inputResource = "BloomQuarterB";
    bloomBlurVerticalQuarter.outputResource = "BloomQuarterA";
    bloomBlurVerticalQuarter.pipeline = "BlurVertical";
    bloomBlurVerticalQuarter.secondaryInputResource = "BloomQuarterB";
    bloomBlurVerticalQuarter.tertiaryInputResource = "BloomQuarterB";
    bloomBlurVerticalQuarter.enabled = true;
    bloomBlurVerticalQuarter.intensity = 1.0f;
    bloomBlurVerticalQuarter.resolutionScale = 0.25f;
    bloomBlurVerticalQuarter.parameters.blurRadius = 6.0f;
    passes_.push_back(bloomBlurVerticalQuarter);

    PostProcessPass bloomDownsampleEighth{};
    bloomDownsampleEighth.name = "BloomDownsampleEighth";
    bloomDownsampleEighth.inputResource = "BloomQuarterA";
    bloomDownsampleEighth.outputResource = "BloomEighthA";
    bloomDownsampleEighth.pipeline = "BloomDownsample";
    bloomDownsampleEighth.secondaryInputResource = "BloomQuarterA";
    bloomDownsampleEighth.tertiaryInputResource = "BloomQuarterA";
    bloomDownsampleEighth.enabled = true;
    bloomDownsampleEighth.intensity = 1.0f;
    bloomDownsampleEighth.resolutionScale = 0.125f;
    passes_.push_back(bloomDownsampleEighth);

    PostProcessPass bloomBlurHorizontalEighth{};
    bloomBlurHorizontalEighth.name = "BloomBlurHorizontalEighth";
    bloomBlurHorizontalEighth.inputResource = "BloomEighthA";
    bloomBlurHorizontalEighth.outputResource = "BloomEighthB";
    bloomBlurHorizontalEighth.pipeline = "BlurHorizontal";
    bloomBlurHorizontalEighth.secondaryInputResource = "BloomEighthA";
    bloomBlurHorizontalEighth.tertiaryInputResource = "BloomEighthA";
    bloomBlurHorizontalEighth.enabled = true;
    bloomBlurHorizontalEighth.intensity = 1.0f;
    bloomBlurHorizontalEighth.resolutionScale = 0.125f;
    bloomBlurHorizontalEighth.parameters.blurRadius = 4.0f;
    passes_.push_back(bloomBlurHorizontalEighth);

    PostProcessPass bloomBlurVerticalEighth{};
    bloomBlurVerticalEighth.name = "BloomBlurVerticalEighth";
    bloomBlurVerticalEighth.inputResource = "BloomEighthB";
    bloomBlurVerticalEighth.outputResource = "BloomEighthA";
    bloomBlurVerticalEighth.pipeline = "BlurVertical";
    bloomBlurVerticalEighth.secondaryInputResource = "BloomEighthB";
    bloomBlurVerticalEighth.tertiaryInputResource = "BloomEighthB";
    bloomBlurVerticalEighth.enabled = true;
    bloomBlurVerticalEighth.intensity = 1.0f;
    bloomBlurVerticalEighth.resolutionScale = 0.125f;
    bloomBlurVerticalEighth.parameters.blurRadius = 4.0f;
    passes_.push_back(bloomBlurVerticalEighth);

    PostProcessPass bloomUpsampleQuarter{};
    bloomUpsampleQuarter.name = "BloomUpsampleQuarter";
    bloomUpsampleQuarter.inputResource = "BloomEighthA";
    bloomUpsampleQuarter.outputResource = "BloomQuarterComposite";
    bloomUpsampleQuarter.pipeline = "BloomUpsample";
    bloomUpsampleQuarter.secondaryInputResource = "BloomQuarterA";
    bloomUpsampleQuarter.tertiaryInputResource = "BloomQuarterA";
    bloomUpsampleQuarter.enabled = true;
    bloomUpsampleQuarter.intensity = 1.0f;
    bloomUpsampleQuarter.resolutionScale = 0.25f;
    bloomUpsampleQuarter.parameters.bloomUpsampleBlend = 0.68f;
    bloomUpsampleQuarter.parameters.bloomUpsampleSoftKnee = 0.35f;
    passes_.push_back(bloomUpsampleQuarter);

    PostProcessPass bloomUpsampleHalf{};
    bloomUpsampleHalf.name = "BloomUpsampleHalf";
    bloomUpsampleHalf.inputResource = "BloomQuarterComposite";
    bloomUpsampleHalf.outputResource = "BloomComposite";
    bloomUpsampleHalf.pipeline = "BloomUpsample";
    bloomUpsampleHalf.secondaryInputResource = "BloomExtractHalf";
    bloomUpsampleHalf.tertiaryInputResource = "BloomExtractHalf";
    bloomUpsampleHalf.enabled = true;
    bloomUpsampleHalf.intensity = 1.0f;
    bloomUpsampleHalf.resolutionScale = 0.5f;
    bloomUpsampleHalf.parameters.bloomUpsampleBlend = 0.55f;
    bloomUpsampleHalf.parameters.bloomUpsampleSoftKnee = 0.25f;
    passes_.push_back(bloomUpsampleHalf);

    PostProcessPass distortionComposite{};
    distortionComposite.name = "DistortionComposite";
    distortionComposite.inputResource = "VfxAccumulation";
    distortionComposite.outputResource = "PostColorA";
    distortionComposite.pipeline = "DistortionComposite";
    distortionComposite.secondaryInputResource = "SceneColor";
    distortionComposite.tertiaryInputResource = "VfxAccumulation";
    distortionComposite.enabled = true;
    distortionComposite.intensity = 1.0f;
    distortionComposite.resolutionScale = 1.0f;
    distortionComposite.parameters.distortionScale = 0.02f;
    passes_.push_back(distortionComposite);

    PostProcessPass contactAo{};
    contactAo.name = "ContactAO";
    contactAo.inputResource = kPostProcessOutputResource;
    contactAo.outputResource = kPostProcessSwapOutputResource;
    contactAo.pipeline = "ContactAO";
    contactAo.secondaryInputResource = kPostProcessOutputResource;
    contactAo.tertiaryInputResource = kPostProcessOutputResource;
    contactAo.enabled = true;
    contactAo.intensity = 0.42f;
    contactAo.resolutionScale = 1.0f;
    contactAo.parameters.contactAoRadiusPixels = 3.0f;
    contactAo.parameters.contactAoBias = 0.25f;
    contactAo.parameters.contactAoFalloff = 9.0f;
    contactAo.parameters.contactAoNearPlane = 0.1f;
    contactAo.parameters.contactAoFarPlane = 5000.0f;
    passes_.push_back(contactAo);

    PostProcessPass distanceFog{};
    distanceFog.name = "DistanceFog";
    distanceFog.inputResource = kPostProcessOutputResource;
    distanceFog.outputResource = kPostProcessSwapOutputResource;
    distanceFog.pipeline = "DistanceFog";
    distanceFog.secondaryInputResource = kPostProcessOutputResource;
    distanceFog.tertiaryInputResource = kPostProcessOutputResource;
    distanceFog.enabled = true;
    distanceFog.intensity = 0.48f;
    distanceFog.resolutionScale = 1.0f;
    distanceFog.parameters.fogStart = 135.0f;
    distanceFog.parameters.fogEnd = 1450.0f;
    distanceFog.parameters.fogDensity = 0.26f;
    distanceFog.parameters.fogColorR = 0.42f;
    distanceFog.parameters.fogColorG = 0.38f;
    distanceFog.parameters.fogColorB = 0.36f;
    distanceFog.parameters.fogNearPlane = 0.1f;
    distanceFog.parameters.fogFarPlane = 5000.0f;
    distanceFog.parameters.fogDepthBoost = 0.42f;
    distanceFog.parameters.fogDepthBoostStart = 0.50f;
    distanceFog.parameters.backlitFogLift = 0.34f;
    distanceFog.parameters.openingGlowStrength = 0.58f;
    distanceFog.parameters.foregroundSilhouetteStrength = 0.48f;
    distanceFog.parameters.lowFogLayerStrength = 0.30f;
    distanceFog.parameters.coolFloorHazeStrength = 0.24f;
    passes_.push_back(distanceFog);

    PostProcessPass accretionComposite{};
    accretionComposite.name = "AccretionComposite";
    accretionComposite.inputResource = kPostProcessOutputResource;
    accretionComposite.outputResource = kPostProcessSwapOutputResource;
    accretionComposite.pipeline = "AccretionComposite";
    accretionComposite.secondaryInputResource = "SceneColor";
    accretionComposite.tertiaryInputResource = "VfxAccumulation";
    accretionComposite.enabled = false;
    accretionComposite.intensity = 1.0f;
    accretionComposite.resolutionScale = 1.0f;
    accretionComposite.parameters.accretionRadius = 0.42f;
    accretionComposite.parameters.accretionDiskStretch = 2.35f;
    accretionComposite.parameters.accretionTurbulence = 0.85f;
    accretionComposite.parameters.accretionChromaticAberration = 0.95f;
    accretionComposite.parameters.accretionCoreSize = 0.18f;
    accretionComposite.parameters.accretionCenterX = 0.5f;
    accretionComposite.parameters.accretionCenterY = 0.5f;
    accretionComposite.parameters.accretionFlowSpeed = 1.0f;
    accretionComposite.parameters.accretionRoadDepthFade = 0.35f;
    accretionComposite.parameters.accretionCoreDarkness = 0.96f;
    accretionComposite.parameters.accretionGuideOpacity = 1.0f;
    accretionComposite.parameters.accretionLensStrength = 1.0f;
    accretionComposite.parameters.accretionGuideWidth = 0.18f;
    passes_.push_back(accretionComposite);

    PostProcessPass toneMapping{};
    toneMapping.name = "ToneMapping";
    toneMapping.inputResource = kPostProcessOutputResource;
    toneMapping.outputResource = kPostProcessSwapOutputResource;
    toneMapping.pipeline = "ToneMapping";
    toneMapping.secondaryInputResource = kPostProcessOutputResource;
    toneMapping.tertiaryInputResource = kPostProcessOutputResource;
    toneMapping.enabled = true;
    toneMapping.intensity = 1.0f;
    toneMapping.resolutionScale = 1.0f;
    toneMapping.parameters.toneExposure = 1.0f;
    passes_.push_back(toneMapping);

    PostProcessPass glowComposite{};
    glowComposite.name = "GlowComposite";
    glowComposite.inputResource = "BloomComposite";
    glowComposite.outputResource = kPostProcessSwapOutputResource;
    glowComposite.pipeline = "GlowComposite";
    glowComposite.secondaryInputResource = "VfxAccumulation";
    glowComposite.tertiaryInputResource = kPostProcessOutputResource;
    glowComposite.enabled = true;
    glowComposite.intensity = 0.88f;
    glowComposite.resolutionScale = 1.0f;
    glowComposite.parameters.glowWeight = 0.74f;
    glowComposite.parameters.glowTintR = 1.0f;
    glowComposite.parameters.glowTintG = 0.95f;
    glowComposite.parameters.glowTintB = 1.2f;
    passes_.push_back(glowComposite);

    // WarpTunnel is an atomic two-pass branch. Both passes stay disabled until
    // the effect director enables them together in the gameplay integration block.
    PostProcessPass warpTunnelGenerate{};
    warpTunnelGenerate.name = "WarpTunnelGenerate";
    warpTunnelGenerate.inputResource = kPostProcessOutputResource;
    warpTunnelGenerate.outputResource = "WarpTunnelHalf";
    warpTunnelGenerate.pipeline = "WarpTunnelGenerate";
    warpTunnelGenerate.secondaryInputResource = kPostProcessOutputResource;
    warpTunnelGenerate.tertiaryInputResource = kPostProcessOutputResource;
    warpTunnelGenerate.enabled = false;
    warpTunnelGenerate.intensity = 1.0f;
    warpTunnelGenerate.resolutionScale = 0.5f;
    passes_.push_back(warpTunnelGenerate);

    PostProcessPass warpTunnelComposite{};
    warpTunnelComposite.name = "WarpTunnelComposite";
    warpTunnelComposite.inputResource = "WarpTunnelHalf";
    warpTunnelComposite.outputResource = kPostProcessPreviousSwapOutputResource;
    warpTunnelComposite.pipeline = "WarpTunnelComposite";
    warpTunnelComposite.secondaryInputResource = kPostProcessPreviousInputResource;
    warpTunnelComposite.tertiaryInputResource = kPostProcessPreviousInputResource;
    warpTunnelComposite.enabled = false;
    warpTunnelComposite.intensity = 1.0f;
    warpTunnelComposite.resolutionScale = 1.0f;
    passes_.push_back(warpTunnelComposite);

    // Dissolve is also an atomic side branch: first generate the grayscale
    // assignment mask, then combine it with the preserved SceneColor.
    PostProcessPass dissolveMask{};
    dissolveMask.name = "DissolveMask";
    dissolveMask.inputResource = kPostProcessOutputResource;
    dissolveMask.outputResource = "DissolveMaskTexture";
    dissolveMask.pipeline = "DissolveMask";
    dissolveMask.secondaryInputResource = kPostProcessOutputResource;
    dissolveMask.tertiaryInputResource = kPostProcessOutputResource;
    dissolveMask.enabled = false;
    dissolveMask.intensity = 1.0f;
    dissolveMask.resolutionScale = 1.0f;
    passes_.push_back(dissolveMask);

    PostProcessPass dissolve{};
    dissolve.name = "Dissolve";
    dissolve.inputResource = kPostProcessPreviousInputResource;
    dissolve.outputResource = kPostProcessPreviousSwapOutputResource;
    dissolve.pipeline = "Dissolve";
    dissolve.secondaryInputResource = "DissolveMaskTexture";
    dissolve.tertiaryInputResource = kPostProcessPreviousInputResource;
    dissolve.enabled = false;
    dissolve.intensity = 1.0f;
    dissolve.resolutionScale = 1.0f;
    passes_.push_back(dissolve);

    PostProcessPass boxBlurHorizontal{};
    boxBlurHorizontal.name = "BoxBlurHorizontal";
    boxBlurHorizontal.inputResource = kPostProcessOutputResource;
    boxBlurHorizontal.outputResource = kPostProcessSwapOutputResource;
    boxBlurHorizontal.pipeline = "BoxBlurHorizontal";
    boxBlurHorizontal.secondaryInputResource = kPostProcessOutputResource;
    boxBlurHorizontal.tertiaryInputResource = kPostProcessOutputResource;
    boxBlurHorizontal.enabled = false;
    boxBlurHorizontal.intensity = 1.0f;
    boxBlurHorizontal.resolutionScale = 1.0f;
    boxBlurHorizontal.parameters.boxBlurKernelRadius = 1.0f;
    passes_.push_back(boxBlurHorizontal);

    PostProcessPass boxBlurVertical{};
    boxBlurVertical.name = "BoxBlurVertical";
    boxBlurVertical.inputResource = kPostProcessOutputResource;
    boxBlurVertical.outputResource = "PostBlurComposite";
    boxBlurVertical.pipeline = "BoxBlurVertical";
    boxBlurVertical.secondaryInputResource = kPostProcessPreviousInputResource;
    boxBlurVertical.tertiaryInputResource = kPostProcessPreviousInputResource;
    boxBlurVertical.enabled = false;
    boxBlurVertical.intensity = 1.0f;
    boxBlurVertical.resolutionScale = 1.0f;
    boxBlurVertical.parameters.boxBlurKernelRadius = 1.0f;
    passes_.push_back(boxBlurVertical);

    PostProcessPass gaussianBlurHorizontal{};
    gaussianBlurHorizontal.name = "GaussianBlurHorizontal";
    gaussianBlurHorizontal.inputResource = kPostProcessOutputResource;
    gaussianBlurHorizontal.outputResource = kPostProcessSwapOutputResource;
    gaussianBlurHorizontal.pipeline = "GaussianBlurHorizontal";
    gaussianBlurHorizontal.secondaryInputResource = kPostProcessOutputResource;
    gaussianBlurHorizontal.tertiaryInputResource = kPostProcessOutputResource;
    gaussianBlurHorizontal.enabled = false;
    gaussianBlurHorizontal.intensity = 1.0f;
    gaussianBlurHorizontal.resolutionScale = 1.0f;
    gaussianBlurHorizontal.parameters.gaussianBlurKernelRadius = 2.0f;
    gaussianBlurHorizontal.parameters.gaussianBlurSigma = 1.5f;
    passes_.push_back(gaussianBlurHorizontal);

    PostProcessPass gaussianBlurVertical{};
    gaussianBlurVertical.name = "GaussianBlurVertical";
    gaussianBlurVertical.inputResource = kPostProcessOutputResource;
    gaussianBlurVertical.outputResource = "PostGaussianComposite";
    gaussianBlurVertical.pipeline = "GaussianBlurVertical";
    gaussianBlurVertical.secondaryInputResource = kPostProcessPreviousInputResource;
    gaussianBlurVertical.tertiaryInputResource = kPostProcessPreviousInputResource;
    gaussianBlurVertical.enabled = false;
    gaussianBlurVertical.intensity = 1.0f;
    gaussianBlurVertical.resolutionScale = 1.0f;
    gaussianBlurVertical.parameters.gaussianBlurKernelRadius = 2.0f;
    gaussianBlurVertical.parameters.gaussianBlurSigma = 1.5f;
    passes_.push_back(gaussianBlurVertical);

    PostProcessPass prewittOutline{};
    prewittOutline.name = "PrewittOutline";
    prewittOutline.inputResource = kPostProcessOutputResource;
    prewittOutline.outputResource = kPostProcessSwapOutputResource;
    prewittOutline.pipeline = "PrewittOutline";
    prewittOutline.secondaryInputResource = kPostProcessOutputResource;
    prewittOutline.tertiaryInputResource = kPostProcessOutputResource;
    prewittOutline.enabled = false;
    prewittOutline.intensity = 1.0f;
    prewittOutline.resolutionScale = 1.0f;
    prewittOutline.parameters.outlineThreshold = 0.08f;
    prewittOutline.parameters.outlineThickness = 1.0f;
    prewittOutline.parameters.outlineSoftness = 0.04f;
    prewittOutline.parameters.outlineColorR = 0.0f;
    prewittOutline.parameters.outlineColorG = 0.0f;
    prewittOutline.parameters.outlineColorB = 0.0f;
    prewittOutline.parameters.outlineDepthWeight = 8.0f;
    passes_.push_back(prewittOutline);

    PostProcessPass grayscale{};
    grayscale.name = "Grayscale";
    grayscale.inputResource = kPostProcessOutputResource;
    grayscale.outputResource = kPostProcessSwapOutputResource;
    grayscale.pipeline = "Grayscale";
    grayscale.secondaryInputResource = kPostProcessOutputResource;
    grayscale.tertiaryInputResource = kPostProcessOutputResource;
    grayscale.enabled = false;
    grayscale.intensity = 1.0f;
    grayscale.resolutionScale = 1.0f;
    grayscale.parameters.grayscaleMode = 1.0f;
    passes_.push_back(grayscale);

    PostProcessPass vignette{};
    vignette.name = "Vignette";
    vignette.inputResource = kPostProcessOutputResource;
    vignette.outputResource = kPostProcessSwapOutputResource;
    vignette.pipeline = "Vignette";
    vignette.secondaryInputResource = kPostProcessOutputResource;
    vignette.tertiaryInputResource = kPostProcessOutputResource;
    vignette.enabled = false;
    vignette.intensity = 1.0f;
    vignette.resolutionScale = 1.0f;
    vignette.parameters.vignetteRadius = 0.75f;
    vignette.parameters.vignetteSoftness = 0.35f;
    vignette.parameters.vignettePower = 1.0f;
    passes_.push_back(vignette);

    PostProcessPass random{};
    random.name = "Random";
    random.inputResource = kPostProcessOutputResource;
    random.outputResource = kPostProcessSwapOutputResource;
    random.pipeline = "Random";
    random.secondaryInputResource = kPostProcessOutputResource;
    random.tertiaryInputResource = kPostProcessOutputResource;
    random.enabled = false;
    random.intensity = 1.0f;
    random.resolutionScale = 1.0f;
    passes_.push_back(random);
    SyncDissolvePasses_();
}

void PostProcessStack::SetEnabled(const std::string& name, bool enabled) {
    for (PostProcessPass& pass : passes_) {
        if (pass.name == name) {
            pass.enabled = enabled;
            return;
        }
    }
}

void PostProcessStack::SetIntensity(const std::string& name, float intensity) {
    for (PostProcessPass& pass : passes_) {
        if (pass.name == name) {
            pass.intensity = intensity;
            return;
        }
    }
}

bool PostProcessStack::IsEnabled(const std::string& name) const {
    for (const PostProcessPass& pass : passes_) {
        if (pass.name == name) {
            return pass.enabled;
        }
    }
    return false;
}

float PostProcessStack::Intensity(const std::string& name) const {
    for (const PostProcessPass& pass : passes_) {
        if (pass.name == name) {
            return pass.enabled ? pass.intensity : 0.0f;
        }
    }
    return 0.0f;
}

void PostProcessStack::StartWarpTunnel() {
    if (warpTunnelPhase_ == WarpTunnelPhase::Enter ||
        warpTunnelPhase_ == WarpTunnelPhase::Cruise) {
        SyncWarpTunnelPasses_();
        return;
    }
    warpTunnelPhase_ = WarpTunnelPhase::Enter;
    warpTunnelPhaseElapsed_ = 0.0f;
    SyncWarpTunnelPasses_();
}

void PostProcessStack::StopWarpTunnel() {
    if (warpTunnelPhase_ == WarpTunnelPhase::Idle ||
        warpTunnelPhase_ == WarpTunnelPhase::Exit) {
        SyncWarpTunnelPasses_();
        return;
    }
    warpTunnelPhase_ = WarpTunnelPhase::Exit;
    warpTunnelPhaseElapsed_ = 0.0f;
    SyncWarpTunnelPasses_();
}

void PostProcessStack::UpdateWarpTunnel(float deltaTime) {
    constexpr float kPi = 3.14159265359f;
    const float safeDeltaTime = (std::max)(0.0f, deltaTime);
    warpTunnelPhaseElapsed_ += safeDeltaTime;

    switch (warpTunnelPhase_) {
    case WarpTunnelPhase::Idle:
        warpTunnelTransition_ = 0.0f;
        warpTunnelFlash_ = 0.0f;
        break;
    case WarpTunnelPhase::Enter: {
        warpTunnelTransition_ = (std::min)(
            1.0f,
            warpTunnelTransition_ + safeDeltaTime / (std::max)(0.05f, warpTunnelEnterDuration_));
        const float phaseProgress = (std::clamp)(
            warpTunnelPhaseElapsed_ / (std::max)(0.05f, warpTunnelEnterDuration_),
            0.0f,
            1.0f);
        warpTunnelFlash_ = std::sin(phaseProgress * kPi) * 1.15f;
        if (warpTunnelTransition_ >= 1.0f) {
            warpTunnelPhase_ = WarpTunnelPhase::Cruise;
            warpTunnelPhaseElapsed_ = 0.0f;
            warpTunnelFlash_ = 0.0f;
        }
        break;
    }
    case WarpTunnelPhase::Cruise:
        warpTunnelTransition_ = 1.0f;
        warpTunnelFlash_ = 0.0f;
        break;
    case WarpTunnelPhase::Exit: {
        warpTunnelTransition_ = (std::max)(
            0.0f,
            warpTunnelTransition_ - safeDeltaTime / (std::max)(0.05f, warpTunnelExitDuration_));
        const float phaseProgress = (std::clamp)(
            warpTunnelPhaseElapsed_ / (std::max)(0.05f, warpTunnelExitDuration_),
            0.0f,
            1.0f);
        warpTunnelFlash_ = std::sin(phaseProgress * kPi) * 0.85f;
        if (warpTunnelTransition_ <= 0.0f) {
            warpTunnelPhase_ = WarpTunnelPhase::Idle;
            warpTunnelPhaseElapsed_ = 0.0f;
            warpTunnelFlash_ = 0.0f;
        }
        break;
    }
    }

    SyncWarpTunnelPasses_();
}

void PostProcessStack::SetWarpTunnelDurations(float enterDuration, float exitDuration) {
    warpTunnelEnterDuration_ = (std::clamp)(enterDuration, 0.05f, 10.0f);
    warpTunnelExitDuration_ = (std::clamp)(exitDuration, 0.05f, 10.0f);
}

void PostProcessStack::SyncWarpTunnelPasses_() {
    const bool enabled = warpTunnelPhase_ != WarpTunnelPhase::Idle;
    for (PostProcessPass& pass : passes_) {
        if (pass.pipeline != "WarpTunnelGenerate" && pass.pipeline != "WarpTunnelComposite") {
            continue;
        }
        pass.enabled = enabled;
        pass.parameters.warpTransition = warpTunnelTransition_;
        pass.parameters.warpFlash = warpTunnelFlash_;
    }
}

void PostProcessStack::StartDissolveTransition() {
    if (dissolvePhase_ != DissolvePhase::Idle) {
        return;
    }
    dissolvePhase_ = DissolvePhase::DissolveOut;
    dissolveThreshold_ = 0.0f;
    dissolvePhaseElapsed_ = 0.0f;
    dissolveSwitchRequested_ = false;
    SyncDissolvePasses_();
}

void PostProcessStack::CancelDissolveTransition() {
    dissolvePhase_ = DissolvePhase::Idle;
    dissolveThreshold_ = 0.0f;
    dissolvePhaseElapsed_ = 0.0f;
    dissolveSwitchRequested_ = false;
    SyncDissolvePasses_();
}

void PostProcessStack::UpdateDissolve(float deltaTime) {
    const float safeDeltaTime = (std::max)(0.0f, deltaTime);
    dissolvePhaseElapsed_ += safeDeltaTime;

    switch (dissolvePhase_) {
    case DissolvePhase::Idle:
        dissolveThreshold_ = 0.0f;
        break;
    case DissolvePhase::DissolveOut: {
        const float progress = dissolvePhaseElapsed_ / (std::max)(0.05f, dissolveOutDuration_);
        dissolveThreshold_ = SmoothStep01(progress);
        if (progress >= 1.0f) {
            dissolvePhase_ = DissolvePhase::Switch;
            dissolveThreshold_ = 1.0f;
            dissolvePhaseElapsed_ = 0.0f;
            // This one-shot request is the safe point for a stage, camera,
            // gameplay phase, or post-effect change while the screen is hidden.
            dissolveSwitchRequested_ = true;
        }
        break;
    }
    case DissolvePhase::Switch:
        dissolveThreshold_ = 1.0f;
        if (dissolvePhaseElapsed_ >= (std::max)(0.0f, dissolveSwitchDuration_)) {
            dissolvePhase_ = DissolvePhase::DissolveIn;
            dissolvePhaseElapsed_ = 0.0f;
        }
        break;
    case DissolvePhase::DissolveIn: {
        const float progress = dissolvePhaseElapsed_ / (std::max)(0.05f, dissolveInDuration_);
        dissolveThreshold_ = 1.0f - SmoothStep01(progress);
        if (progress >= 1.0f) {
            dissolvePhase_ = DissolvePhase::Idle;
            dissolveThreshold_ = 0.0f;
            dissolvePhaseElapsed_ = 0.0f;
        }
        break;
    }
    }

    SyncDissolvePasses_();
}

void PostProcessStack::SetDissolveDurations(
    float outDuration,
    float switchDuration,
    float inDuration) {
    dissolveOutDuration_ = (std::clamp)(outDuration, 0.05f, 10.0f);
    dissolveSwitchDuration_ = (std::clamp)(switchDuration, 0.0f, 10.0f);
    dissolveInDuration_ = (std::clamp)(inDuration, 0.05f, 10.0f);
}

bool PostProcessStack::ConsumeDissolveSwitchRequest() {
    const bool requested = dissolveSwitchRequested_;
    dissolveSwitchRequested_ = false;
    return requested;
}

void PostProcessStack::SyncDissolvePasses_() {
    const bool enabled = dissolvePhase_ != DissolvePhase::Idle;
    for (PostProcessPass& pass : passes_) {
        if (pass.pipeline != "DissolveMask" && pass.pipeline != "Dissolve") {
            continue;
        }
        pass.enabled = enabled;
        pass.parameters.dissolveThreshold = dissolveThreshold_;
    }
}

std::string PostProcessStack::FinalOutputResource() const {
    return BuildExecutionPlan().finalOutputResource;
}

PostProcessExecutionPlan PostProcessStack::BuildExecutionPlan() const {
    PostProcessExecutionPlan plan{};
    std::unordered_set<std::string> availableResources = {
        "SceneColor",
        "VfxAccumulation",
    };
    std::string previousInputResource = plan.finalOutputResource;

    for (const PostProcessPass& pass : passes_) {
        if (!pass.enabled) {
            continue;
        }

        PostProcessPass resolvedPass = pass;
        resolvedPass.inputResource =
            ResolvePostProcessInputResource(pass.inputResource, plan.finalOutputResource, previousInputResource);
        resolvedPass.outputResource = ResolvePostProcessOutputResource(
            pass.outputResource,
            plan.finalOutputResource,
            previousInputResource);
        resolvedPass.secondaryInputResource =
            ResolvePostProcessInputResource(
                pass.secondaryInputResource,
                plan.finalOutputResource,
                previousInputResource);
        resolvedPass.tertiaryInputResource =
            ResolvePostProcessInputResource(
                pass.tertiaryInputResource,
                plan.finalOutputResource,
                previousInputResource);

        const bool hasPrimaryInput = availableResources.contains(resolvedPass.inputResource);
        const bool hasSecondaryInput = availableResources.contains(resolvedPass.secondaryInputResource);
        const bool hasTertiaryInput = availableResources.contains(resolvedPass.tertiaryInputResource);
        if (!hasPrimaryInput || !hasSecondaryInput || !hasTertiaryInput) {
            continue;
        }

        PostProcessExecutionPass executionPass{};
        executionPass.pass = std::move(resolvedPass);
        previousInputResource = executionPass.pass.inputResource;
        plan.finalOutputResource = executionPass.pass.outputResource;
        availableResources.insert(executionPass.pass.outputResource);
        plan.passes.push_back(std::move(executionPass));
    }

    return plan;
}
