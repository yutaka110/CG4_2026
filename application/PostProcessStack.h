#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct PostProcessPass {
    struct Parameters {
        float bloomThresholdMin = 0.45f;
        float bloomThresholdMax = 1.1f;
        float bloomSoftKnee = 0.2f;
        float bloomUpsampleBlend = 1.0f;
        float bloomUpsampleSoftKnee = 0.35f;
        float blurRadius = 4.0f;
        float distortionScale = 0.02f;
        float toneExposure = 1.0f;
        float glowWeight = 1.0f;
        float glowTintR = 1.0f;
        float glowTintG = 1.0f;
        float glowTintB = 1.0f;
        float grayscaleMode = 1.0f;
        float vignetteRadius = 0.75f;
        float vignetteSoftness = 0.35f;
        float vignettePower = 1.0f;
        float boxBlurKernelRadius = 1.0f;
        float gaussianBlurKernelRadius = 2.0f;
        float gaussianBlurSigma = 1.5f;
        float outlineThreshold = 0.08f;
        float outlineThickness = 1.0f;
        float outlineSoftness = 0.04f;
        float outlineColorR = 0.0f;
        float outlineColorG = 0.0f;
        float outlineColorB = 0.0f;
        float outlineDepthWeight = 8.0f;
        float accretionRadius = 0.42f;
        float accretionDiskStretch = 2.35f;
        float accretionTurbulence = 0.85f;
        float accretionChromaticAberration = 0.95f;
        float accretionCoreSize = 0.18f;
        float accretionCenterX = 0.5f;
        float accretionCenterY = 0.5f;
        float accretionFlowSpeed = 1.0f;
        float accretionRoadDepthFade = 0.35f;
        float accretionCoreDarkness = 0.96f;
        float accretionGuideOpacity = 1.0f;
        float accretionLensStrength = 1.0f;
        float accretionGuideWidth = 0.18f;
        float fogStart = 180.0f;
        float fogEnd = 1200.0f;
        float fogDensity = 0.32f;
        float fogColorR = 0.46f;
        float fogColorG = 0.40f;
        float fogColorB = 0.34f;
        float fogNearPlane = 0.1f;
        float fogFarPlane = 5000.0f;
        float fogDepthBoost = 0.35f;
        float fogDepthBoostStart = 0.58f;
        float backlitFogLift = 0.18f;
        float contactAoRadiusPixels = 3.0f;
        float contactAoBias = 0.25f;
        float contactAoFalloff = 9.0f;
        float contactAoNearPlane = 0.1f;
        float contactAoFarPlane = 5000.0f;
    };

    std::string name;
    std::string inputResource = "SceneColor";
    std::string outputResource = "PostColor";
    std::string pipeline = "FullscreenComposite";
    std::string secondaryInputResource = "VfxAccumulation";
    std::string tertiaryInputResource = "SceneColor";
    bool enabled = true;
    float intensity = 1.0f;
    float resolutionScale = 1.0f;
    Parameters parameters{};
};

struct PostProcessExecutionPass {
    PostProcessPass pass;
};

struct PostProcessExecutionPlan {
    std::vector<PostProcessExecutionPass> passes;
    std::string finalOutputResource = "SceneColor";
};

class PostProcessStack {
public:
    void ResetToVfxDefaults();
    void SetEnabled(const std::string& name, bool enabled);
    void SetIntensity(const std::string& name, float intensity);
    bool IsEnabled(const std::string& name) const;
    float Intensity(const std::string& name) const;
    std::string FinalOutputResource() const;
    PostProcessExecutionPlan BuildExecutionPlan() const;
    const std::vector<PostProcessPass>& Passes() const { return passes_; }
    std::vector<PostProcessPass>& MutablePasses() { return passes_; }

private:
    std::vector<PostProcessPass> passes_;
};
