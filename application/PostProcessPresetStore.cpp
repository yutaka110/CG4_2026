#include "PostProcessPresetStore.h"

#include "PostProcessStack.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

namespace {
std::string Trim(std::string value) {
    const char* whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return {};
    }
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

bool ParseFloat(const std::string& text, float& out) {
    try {
        size_t consumed = 0;
        out = std::stof(text, &consumed);
        return consumed > 0;
    } catch (...) {
        return false;
    }
}

bool ParseBool(const std::string& text, bool& out) {
    if (text == "1" || text == "true" || text == "True") {
        out = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "False") {
        out = false;
        return true;
    }
    return false;
}

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
}

PostProcessPass* FindPass(PostProcessStack& stack, const std::string& name) {
    for (PostProcessPass& pass : stack.MutablePasses()) {
        if (pass.name == name) {
            return &pass;
        }
    }
    return nullptr;
}

void WritePass(std::ostream& output, const PostProcessPass& pass) {
    const std::string prefix = "pass." + pass.name + ".";
    output << prefix << "enabled=" << (pass.enabled ? 1 : 0) << "\n";
    output << prefix << "intensity=" << pass.intensity << "\n";
    output << prefix << "resolutionScale=" << pass.resolutionScale << "\n";
    output << prefix << "bloomThresholdMin=" << pass.parameters.bloomThresholdMin << "\n";
    output << prefix << "bloomThresholdMax=" << pass.parameters.bloomThresholdMax << "\n";
    output << prefix << "bloomSoftKnee=" << pass.parameters.bloomSoftKnee << "\n";
    output << prefix << "bloomUpsampleBlend=" << pass.parameters.bloomUpsampleBlend << "\n";
    output << prefix << "bloomUpsampleSoftKnee=" << pass.parameters.bloomUpsampleSoftKnee << "\n";
    output << prefix << "blurRadius=" << pass.parameters.blurRadius << "\n";
    output << prefix << "distortionScale=" << pass.parameters.distortionScale << "\n";
    output << prefix << "toneExposure=" << pass.parameters.toneExposure << "\n";
    output << prefix << "glowWeight=" << pass.parameters.glowWeight << "\n";
    output << prefix << "glowTintR=" << pass.parameters.glowTintR << "\n";
    output << prefix << "glowTintG=" << pass.parameters.glowTintG << "\n";
    output << prefix << "glowTintB=" << pass.parameters.glowTintB << "\n";
    output << prefix << "grayscaleMode=" << pass.parameters.grayscaleMode << "\n";
    output << prefix << "vignetteRadius=" << pass.parameters.vignetteRadius << "\n";
    output << prefix << "vignetteSoftness=" << pass.parameters.vignetteSoftness << "\n";
    output << prefix << "vignettePower=" << pass.parameters.vignettePower << "\n";
    output << prefix << "boxBlurKernelRadius=" << pass.parameters.boxBlurKernelRadius << "\n";
    output << prefix << "gaussianBlurKernelRadius=" << pass.parameters.gaussianBlurKernelRadius << "\n";
    output << prefix << "gaussianBlurSigma=" << pass.parameters.gaussianBlurSigma << "\n";
    output << prefix << "outlineThreshold=" << pass.parameters.outlineThreshold << "\n";
    output << prefix << "outlineThickness=" << pass.parameters.outlineThickness << "\n";
    output << prefix << "outlineSoftness=" << pass.parameters.outlineSoftness << "\n";
    output << prefix << "outlineColorR=" << pass.parameters.outlineColorR << "\n";
    output << prefix << "outlineColorG=" << pass.parameters.outlineColorG << "\n";
    output << prefix << "outlineColorB=" << pass.parameters.outlineColorB << "\n";
    output << prefix << "outlineDepthWeight=" << pass.parameters.outlineDepthWeight << "\n";
    output << prefix << "accretionRadius=" << pass.parameters.accretionRadius << "\n";
    output << prefix << "accretionDiskStretch=" << pass.parameters.accretionDiskStretch << "\n";
    output << prefix << "accretionTurbulence=" << pass.parameters.accretionTurbulence << "\n";
    output << prefix << "accretionChromaticAberration=" << pass.parameters.accretionChromaticAberration << "\n";
    output << prefix << "accretionCoreSize=" << pass.parameters.accretionCoreSize << "\n";
    output << prefix << "accretionCenterX=" << pass.parameters.accretionCenterX << "\n";
    output << prefix << "accretionCenterY=" << pass.parameters.accretionCenterY << "\n";
    output << prefix << "accretionFlowSpeed=" << pass.parameters.accretionFlowSpeed << "\n";
    output << prefix << "accretionRoadDepthFade=" << pass.parameters.accretionRoadDepthFade << "\n";
    output << prefix << "accretionCoreDarkness=" << pass.parameters.accretionCoreDarkness << "\n";
    output << prefix << "accretionGuideOpacity=" << pass.parameters.accretionGuideOpacity << "\n";
    output << prefix << "accretionLensStrength=" << pass.parameters.accretionLensStrength << "\n";
    output << prefix << "accretionGuideWidth=" << pass.parameters.accretionGuideWidth << "\n";
    output << prefix << "fogStart=" << pass.parameters.fogStart << "\n";
    output << prefix << "fogEnd=" << pass.parameters.fogEnd << "\n";
    output << prefix << "fogDensity=" << pass.parameters.fogDensity << "\n";
    output << prefix << "fogColorR=" << pass.parameters.fogColorR << "\n";
    output << prefix << "fogColorG=" << pass.parameters.fogColorG << "\n";
    output << prefix << "fogColorB=" << pass.parameters.fogColorB << "\n";
    output << prefix << "fogNearPlane=" << pass.parameters.fogNearPlane << "\n";
    output << prefix << "fogFarPlane=" << pass.parameters.fogFarPlane << "\n";
    output << prefix << "contactAoRadiusPixels=" << pass.parameters.contactAoRadiusPixels << "\n";
    output << prefix << "contactAoBias=" << pass.parameters.contactAoBias << "\n";
    output << prefix << "contactAoFalloff=" << pass.parameters.contactAoFalloff << "\n";
    output << prefix << "contactAoNearPlane=" << pass.parameters.contactAoNearPlane << "\n";
    output << prefix << "contactAoFarPlane=" << pass.parameters.contactAoFarPlane << "\n";
}

void ApplyFloatField(PostProcessPass& pass, const std::string& field, float value) {
    if (field == "intensity") pass.intensity = (std::max)(0.0f, value);
    else if (field == "resolutionScale") pass.resolutionScale = (std::clamp)(value, 0.0625f, 1.0f);
    else if (field == "bloomThresholdMin") pass.parameters.bloomThresholdMin = value;
    else if (field == "bloomThresholdMax") pass.parameters.bloomThresholdMax = value;
    else if (field == "bloomSoftKnee") pass.parameters.bloomSoftKnee = value;
    else if (field == "bloomUpsampleBlend") pass.parameters.bloomUpsampleBlend = value;
    else if (field == "bloomUpsampleSoftKnee") pass.parameters.bloomUpsampleSoftKnee = value;
    else if (field == "blurRadius") pass.parameters.blurRadius = value;
    else if (field == "distortionScale") pass.parameters.distortionScale = value;
    else if (field == "toneExposure") pass.parameters.toneExposure = value;
    else if (field == "glowWeight") pass.parameters.glowWeight = value;
    else if (field == "glowTintR") pass.parameters.glowTintR = value;
    else if (field == "glowTintG") pass.parameters.glowTintG = value;
    else if (field == "glowTintB") pass.parameters.glowTintB = value;
    else if (field == "grayscaleMode") pass.parameters.grayscaleMode = value;
    else if (field == "vignetteRadius") pass.parameters.vignetteRadius = value;
    else if (field == "vignetteSoftness") pass.parameters.vignetteSoftness = value;
    else if (field == "vignettePower") pass.parameters.vignettePower = value;
    else if (field == "boxBlurKernelRadius") pass.parameters.boxBlurKernelRadius = value;
    else if (field == "gaussianBlurKernelRadius") pass.parameters.gaussianBlurKernelRadius = value;
    else if (field == "gaussianBlurSigma") pass.parameters.gaussianBlurSigma = value;
    else if (field == "outlineThreshold") pass.parameters.outlineThreshold = value;
    else if (field == "outlineThickness") pass.parameters.outlineThickness = value;
    else if (field == "outlineSoftness") pass.parameters.outlineSoftness = value;
    else if (field == "outlineColorR") pass.parameters.outlineColorR = value;
    else if (field == "outlineColorG") pass.parameters.outlineColorG = value;
    else if (field == "outlineColorB") pass.parameters.outlineColorB = value;
    else if (field == "outlineDepthWeight") pass.parameters.outlineDepthWeight = value;
    else if (field == "accretionRadius") pass.parameters.accretionRadius = value;
    else if (field == "accretionDiskStretch") pass.parameters.accretionDiskStretch = value;
    else if (field == "accretionTurbulence") pass.parameters.accretionTurbulence = value;
    else if (field == "accretionChromaticAberration") pass.parameters.accretionChromaticAberration = value;
    else if (field == "accretionCoreSize") pass.parameters.accretionCoreSize = value;
    else if (field == "accretionCenterX") pass.parameters.accretionCenterX = value;
    else if (field == "accretionCenterY") pass.parameters.accretionCenterY = value;
    else if (field == "accretionFlowSpeed") pass.parameters.accretionFlowSpeed = value;
    else if (field == "accretionRoadDepthFade") pass.parameters.accretionRoadDepthFade = value;
    else if (field == "accretionCoreDarkness") pass.parameters.accretionCoreDarkness = value;
    else if (field == "accretionGuideOpacity") pass.parameters.accretionGuideOpacity = value;
    else if (field == "accretionLensStrength") pass.parameters.accretionLensStrength = value;
    else if (field == "accretionGuideWidth") pass.parameters.accretionGuideWidth = value;
    else if (field == "fogStart") pass.parameters.fogStart = value;
    else if (field == "fogEnd") pass.parameters.fogEnd = value;
    else if (field == "fogDensity") pass.parameters.fogDensity = value;
    else if (field == "fogColorR") pass.parameters.fogColorR = value;
    else if (field == "fogColorG") pass.parameters.fogColorG = value;
    else if (field == "fogColorB") pass.parameters.fogColorB = value;
    else if (field == "fogNearPlane") pass.parameters.fogNearPlane = value;
    else if (field == "fogFarPlane") pass.parameters.fogFarPlane = value;
    else if (field == "contactAoRadiusPixels") pass.parameters.contactAoRadiusPixels = value;
    else if (field == "contactAoBias") pass.parameters.contactAoBias = value;
    else if (field == "contactAoFalloff") pass.parameters.contactAoFalloff = value;
    else if (field == "contactAoNearPlane") pass.parameters.contactAoNearPlane = value;
    else if (field == "contactAoFarPlane") pass.parameters.contactAoFarPlane = value;
}
} // namespace

PostProcessPresetStore::PostProcessPresetStore(std::filesystem::path path)
    : path_(std::move(path)) {
}

bool PostProcessPresetStore::Load(PostProcessStack& stack, std::string* error) {
    std::ifstream input(path_);
    if (!input) {
        SetError(error, "postprocess preset not found: " + path_.string());
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        const size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        constexpr std::string_view prefix = "pass.";
        if (!key.starts_with(prefix) || value.empty()) {
            continue;
        }

        const size_t passNameBegin = prefix.size();
        const size_t fieldSeparator = key.find('.', passNameBegin);
        if (fieldSeparator == std::string::npos) {
            continue;
        }

        const std::string passName = key.substr(passNameBegin, fieldSeparator - passNameBegin);
        const std::string field = key.substr(fieldSeparator + 1);
        PostProcessPass* pass = FindPass(stack, passName);
        if (pass == nullptr) {
            continue;
        }

        if (field == "enabled") {
            bool enabled = false;
            if (ParseBool(value, enabled)) {
                pass->enabled = enabled;
            }
            continue;
        }

        float numericValue = 0.0f;
        if (ParseFloat(value, numericValue)) {
            ApplyFloatField(*pass, field, numericValue);
        }
    }

    return true;
}

bool PostProcessPresetStore::Save(const PostProcessStack& stack, std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (ec) {
        SetError(error, "failed to create postprocess preset directory: " + ec.message());
        return false;
    }

    std::ofstream output(path_, std::ios::out | std::ios::trunc);
    if (!output) {
        SetError(error, "failed to write postprocess preset: " + path_.string());
        return false;
    }

    output << "# Rail shooter postprocess look preset\n";
    for (const PostProcessPass& pass : stack.Passes()) {
        WritePass(output, pass);
    }
    return true;
}

std::filesystem::path PostProcessPresetStore::DefaultPath() {
    return std::filesystem::path{"Resources"} / "postprocess" / "default.postpreset";
}
