#include "EffectAssetLoader.h"
#include "AppRuntimeUtils.h"
#include "TechniqueRegistry.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>
#include <variant>

namespace {
using ComponentEditBuffer = std::variant<
    ParticleComponentAsset,
    TrailComponentAsset,
    BeamComponentAsset,
    DistortionComponentAsset>;

std::string Trim(std::string text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

float ToFloat(const std::string& value, float fallback) {
    try {
        return std::stof(value);
    } catch (...) {
        return fallback;
    }
}

Vector4 ToVector4(const std::string& value, const Vector4& fallback) {
    Vector4 result = fallback;
    std::stringstream stream(value);
    char comma = ',';
    if (!(stream >> result.x)) {
        return fallback;
    }
    if (!(stream >> comma >> result.y)) {
        result.y = result.x;
        result.z = result.x;
        result.w = fallback.w;
        return result;
    }
    if (!(stream >> comma >> result.z)) {
        result.z = result.y;
        result.w = fallback.w;
        return result;
    }
    if (!(stream >> comma >> result.w)) {
        result.w = fallback.w;
    }
    return result;
}

uint32_t ToUint(const std::string& value, uint32_t fallback) {
    try {
        return static_cast<uint32_t>((std::max)(0, std::stoi(value)));
    } catch (...) {
        return fallback;
    }
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

EffectTrailFollowMode ToTrailFollowMode(const std::string& value, EffectTrailFollowMode fallback) {
    const std::string normalized = ToLower(value);
    if (normalized == "forward" || normalized == "instanceforward" || normalized == "localx") {
        return EffectTrailFollowMode::InstanceForward;
    }
    if (normalized == "up" || normalized == "instanceup" || normalized == "localy") {
        return EffectTrailFollowMode::InstanceUp;
    }
    if (normalized == "worldx") {
        return EffectTrailFollowMode::WorldX;
    }
    if (normalized == "movement" || normalized == "movementhistory" || normalized == "velocity") {
        return EffectTrailFollowMode::MovementHistory;
    }
    return static_cast<EffectTrailFollowMode>(ToUint(value, static_cast<uint32_t>(fallback)));
}

bool SplitTypedKey(const std::string& key, std::string& scope, std::string& field) {
    const size_t dot = key.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= key.size()) {
        return false;
    }

    scope = key.substr(0, dot);
    field = key.substr(dot + 1);
    return true;
}

bool ApplyParticleSettingsKey(
    EffectParticleSettings& settings,
    const std::string& key,
    const std::string& value) {
    if (key == "emissive") {
        settings.emissive = ToFloat(value, settings.emissive);
    } else if (key == "distortion" || key == "distortionStrength") {
        settings.distortionStrength = ToFloat(value, settings.distortionStrength);
    } else if (key == "noise" || key == "noiseStrength") {
        settings.noiseStrength = ToFloat(value, settings.noiseStrength);
    } else if (key == "uvScroll" || key == "uvScrollSpeed") {
        settings.uvScrollSpeed = ToFloat(value, settings.uvScrollSpeed);
    } else if (key == "pulseSpeed") {
        settings.pulseSpeed = ToFloat(value, settings.pulseSpeed);
    } else if (key == "spawnRadius") {
        settings.spawnRadius = ToFloat(value, settings.spawnRadius);
    } else if (key == "depthFadeSoftness" || key == "softness") {
        settings.depthFadeSoftness = ToFloat(value, settings.depthFadeSoftness);
    } else if (key == "particleEdgeSoftness" || key == "edgeSoftness") {
        settings.edgeSoftness = ToFloat(value, settings.edgeSoftness);
    } else {
        return false;
    }
    return true;
}

bool ApplyTrailSettingsKey(
    EffectTrailSettings& settings,
    const std::string& key,
    const std::string& value) {
    if (key == "depthFadeSoftness" || key == "softness") {
        settings.depthFadeSoftness = ToFloat(value, settings.depthFadeSoftness);
    } else if (key == "trailTailFade" || key == "tailFade") {
        settings.trailTailFade = ToFloat(value, settings.trailTailFade);
    } else if (key == "length" || key == "trailLength") {
        settings.length = ToFloat(value, settings.length);
    } else if (key == "width" || key == "trailWidth") {
        settings.width = ToFloat(value, settings.width);
    } else if (key == "sampleDistance" || key == "trailSampleDistance" || key == "historySampleDistance") {
        settings.sampleDistance = ToFloat(value, settings.sampleDistance);
    } else if (key == "smoothing" || key == "trailSmoothing" || key == "historySmoothing") {
        settings.smoothing = ToFloat(value, settings.smoothing);
    } else if (key == "widthHead" || key == "headWidth") {
        settings.widthHead = ToFloat(value, settings.widthHead);
    } else if (key == "widthTail" || key == "tailWidth") {
        settings.widthTail = ToFloat(value, settings.widthTail);
    } else if (key == "alphaTail" || key == "tailAlpha") {
        settings.alphaTail = ToFloat(value, settings.alphaTail);
    } else if (key == "miterLimit" || key == "joinMiterLimit") {
        settings.miterLimit = ToFloat(value, settings.miterLimit);
    } else if (key == "colorTail" || key == "tailColor") {
        settings.colorTail = ToVector4(value, settings.colorTail);
    } else if (key == "followMode" || key == "trailFollowMode") {
        settings.followMode = ToTrailFollowMode(value, settings.followMode);
    } else if (key == "segmentBudget" || key == "segmentCount" || key == "meshSegmentBudget") {
        settings.segmentBudget = ToUint(value, settings.segmentBudget);
    } else {
        return false;
    }
    return true;
}

bool ApplyBeamSettingsKey(
    EffectBeamSettings& settings,
    const std::string& key,
    const std::string& value) {
    if (key == "emissive") {
        settings.emissive = ToFloat(value, settings.emissive);
    } else {
        return false;
    }
    return true;
}

bool ApplyDistortionSettingsKey(
    EffectDistortionSettings& settings,
    const std::string& key,
    const std::string& value) {
    if (key == "distortion" || key == "strength") {
        settings.strength = ToFloat(value, settings.strength);
    } else if (key == "noise" || key == "noiseStrength") {
        settings.noiseStrength = ToFloat(value, settings.noiseStrength);
    } else if (key == "uvScroll" || key == "uvScrollSpeed") {
        settings.uvScrollSpeed = ToFloat(value, settings.uvScrollSpeed);
    } else if (key == "depthFadeSoftness" || key == "softness") {
        settings.depthFadeSoftness = ToFloat(value, settings.depthFadeSoftness);
    } else if (key == "distortionDepthAttenuation" || key == "distortionAttenuation" || key == "depthAttenuation") {
        settings.depthAttenuation = ToFloat(value, settings.depthAttenuation);
    } else {
        return false;
    }
    return true;
}

bool ApplyTypedDefaultKey(
    EffectAsset& asset,
    const std::string& scope,
    const std::string& field,
    const std::string& value) {
    if (scope == "particle") {
        return ApplyParticleSettingsKey(asset.defaultParticle, field, value);
    }
    if (scope == "trail") {
        return ApplyTrailSettingsKey(asset.defaultTrail, field, value);
    }
    if (scope == "beam") {
        return ApplyBeamSettingsKey(asset.defaultBeam, field, value);
    }
    if (scope == "distortion") {
        return ApplyDistortionSettingsKey(asset.defaultDistortion, field, value);
    }
    return false;
}

bool ApplyTypedComponentKey(
    ComponentEditBuffer& component,
    const std::string& scope,
    const std::string& field,
    const std::string& value) {
    if (scope == "particle") {
        if (auto* particle = std::get_if<ParticleComponentAsset>(&component)) {
            return ApplyParticleSettingsKey(particle->settings, field, value);
        }
        return false;
    }
    if (scope == "trail") {
        if (auto* trail = std::get_if<TrailComponentAsset>(&component)) {
            return ApplyTrailSettingsKey(trail->settings, field, value);
        }
        return false;
    }
    if (scope == "beam") {
        if (auto* beam = std::get_if<BeamComponentAsset>(&component)) {
            return ApplyBeamSettingsKey(beam->settings, field, value);
        }
        return false;
    }
    if (scope == "distortion") {
        if (auto* distortion = std::get_if<DistortionComponentAsset>(&component)) {
            return ApplyDistortionSettingsKey(distortion->settings, field, value);
        }
        return false;
    }
    return false;
}

bool ApplyLegacyAssetSettingsKey(
    EffectAsset& asset,
    const std::string& key,
    const std::string& value) {
    if (key == "emissive") {
        asset.defaultParticle.emissive = ToFloat(value, asset.defaultParticle.emissive);
        asset.defaultBeam.emissive = asset.defaultParticle.emissive;
    } else if (key == "distortion") {
        asset.defaultParticle.distortionStrength = ToFloat(value, asset.defaultParticle.distortionStrength);
        asset.defaultDistortion.strength = asset.defaultParticle.distortionStrength;
    } else if (key == "noise") {
        asset.defaultParticle.noiseStrength = ToFloat(value, asset.defaultParticle.noiseStrength);
        asset.defaultDistortion.noiseStrength = asset.defaultParticle.noiseStrength;
    } else if (key == "uvScroll") {
        asset.defaultParticle.uvScrollSpeed = ToFloat(value, asset.defaultParticle.uvScrollSpeed);
        asset.defaultDistortion.uvScrollSpeed = asset.defaultParticle.uvScrollSpeed;
    } else if (key == "pulseSpeed") {
        asset.defaultParticle.pulseSpeed = ToFloat(value, asset.defaultParticle.pulseSpeed);
    } else if (key == "spawnRadius") {
        asset.defaultParticle.spawnRadius = ToFloat(value, asset.defaultParticle.spawnRadius);
    } else if (key == "depthFadeSoftness" || key == "softness") {
        asset.defaultParticle.depthFadeSoftness = ToFloat(value, asset.defaultParticle.depthFadeSoftness);
        asset.defaultTrail.depthFadeSoftness = asset.defaultParticle.depthFadeSoftness;
        asset.defaultDistortion.depthFadeSoftness = asset.defaultParticle.depthFadeSoftness;
    } else if (key == "distortionDepthAttenuation" || key == "distortionAttenuation") {
        asset.defaultDistortion.depthAttenuation = ToFloat(value, asset.defaultDistortion.depthAttenuation);
    } else if (key == "particleEdgeSoftness" || key == "edgeSoftness") {
        asset.defaultParticle.edgeSoftness = ToFloat(value, asset.defaultParticle.edgeSoftness);
    } else if (key == "trailTailFade" || key == "tailFade") {
        asset.defaultTrail.trailTailFade = ToFloat(value, asset.defaultTrail.trailTailFade);
    } else if (key == "trailLength") {
        asset.defaultTrail.length = ToFloat(value, asset.defaultTrail.length);
    } else if (key == "trailWidth") {
        asset.defaultTrail.width = ToFloat(value, asset.defaultTrail.width);
    } else if (key == "trailSampleDistance" || key == "trailHistorySampleDistance") {
        asset.defaultTrail.sampleDistance = ToFloat(value, asset.defaultTrail.sampleDistance);
    } else if (key == "trailSmoothing" || key == "trailHistorySmoothing") {
        asset.defaultTrail.smoothing = ToFloat(value, asset.defaultTrail.smoothing);
    } else if (key == "trailWidthHead" || key == "trailHeadWidth") {
        asset.defaultTrail.widthHead = ToFloat(value, asset.defaultTrail.widthHead);
    } else if (key == "trailWidthTail" || key == "trailTailWidth") {
        asset.defaultTrail.widthTail = ToFloat(value, asset.defaultTrail.widthTail);
    } else if (key == "trailAlphaTail" || key == "trailTailAlpha") {
        asset.defaultTrail.alphaTail = ToFloat(value, asset.defaultTrail.alphaTail);
    } else if (key == "trailMiterLimit" || key == "trailJoinMiterLimit") {
        asset.defaultTrail.miterLimit = ToFloat(value, asset.defaultTrail.miterLimit);
    } else if (key == "trailColorTail" || key == "trailTailColor") {
        asset.defaultTrail.colorTail = ToVector4(value, asset.defaultTrail.colorTail);
    } else if (key == "trailFollowMode") {
        asset.defaultTrail.followMode = ToTrailFollowMode(value, asset.defaultTrail.followMode);
    } else if (key == "trailSegmentBudget" || key == "trailSegmentCount" || key == "trailMeshSegmentBudget") {
        asset.defaultTrail.segmentBudget = ToUint(value, asset.defaultTrail.segmentBudget);
    } else {
        return false;
    }
    return true;
}

bool ApplyLegacyTypedSettingsKey(
    ParticleComponentAsset& component,
    const std::string& key,
    const std::string& value) {
    return ApplyParticleSettingsKey(component.settings, key, value);
}

bool ApplyLegacyTypedSettingsKey(
    TrailComponentAsset& component,
    const std::string& key,
    const std::string& value) {
    return ApplyTrailSettingsKey(component.settings, key, value);
}

bool ApplyLegacyTypedSettingsKey(
    BeamComponentAsset& component,
    const std::string& key,
    const std::string& value) {
    return ApplyBeamSettingsKey(component.settings, key, value);
}

bool ApplyLegacyTypedSettingsKey(
    DistortionComponentAsset& component,
    const std::string& key,
    const std::string& value) {
    return ApplyDistortionSettingsKey(component.settings, key, value);
}

bool ApplyLegacyComponentSettingsKey(
    ComponentEditBuffer& component,
    const std::string& key,
    const std::string& value) {
    return std::visit(
        [&key, &value](auto& typedComponent) {
            return ApplyLegacyTypedSettingsKey(typedComponent, key, value);
        },
        component);
}

ge3::graphics::BlendMode ParseBlend(const std::string& value) {
    if (value == "alpha") {
        return ge3::graphics::BlendMode::Alpha;
    }
    if (value == "additive") {
        return ge3::graphics::BlendMode::Additive;
    }
    if (value == "distortion") {
        return ge3::graphics::BlendMode::Distortion;
    }
    return ge3::graphics::BlendMode::Opaque;
}

EffectLayer ParseLayer(const std::string& value) {
    if (value == "opaque") {
        return EffectLayer::OpaqueFx;
    }
    if (value == "distortion") {
        return EffectLayer::DistortionFx;
    }
    if (value == "trail") {
        return EffectLayer::TrailFx;
    }
    if (value == "volumetric") {
        return EffectLayer::VolumetricFx;
    }
    return EffectLayer::AdditiveFx;
}

EffectComponentType ParseComponentType(const std::string& value) {
    if (value == "trail") {
        return EffectComponentType::Trail;
    }
    if (value == "beam") {
        return EffectComponentType::Beam;
    }
    if (value == "distortion") {
        return EffectComponentType::Distortion;
    }
    return EffectComponentType::Particle;
}

const char* DiagnosticSeverityLabel(EffectAssetDiagnosticSeverity severity) {
    switch (severity) {
    case EffectAssetDiagnosticSeverity::Info:
        return "info";
    case EffectAssetDiagnosticSeverity::Warning:
        return "warning";
    case EffectAssetDiagnosticSeverity::Error:
        return "error";
    }
    return "warning";
}

void AddDiagnostic(
    std::vector<EffectAssetDiagnostic>& diagnostics,
    EffectAssetDiagnostic diagnostic) {
    if (diagnostic.message.empty()) {
        diagnostic.message = diagnostic.code;
    }
    const std::string logMessage =
        std::string(DiagnosticSeverityLabel(diagnostic.severity)) + ": " + diagnostic.message;
    diagnostics.push_back(std::move(diagnostic));
    Log("[EffectAssetLoader] " + logMessage + "\n");
}

void AddDiagnostic(
    std::vector<EffectAssetDiagnostic>& diagnostics,
    std::string warning) {
    EffectAssetDiagnostic diagnostic{};
    diagnostic.severity = EffectAssetDiagnosticSeverity::Warning;
    diagnostic.code = "loaderWarning";
    diagnostic.source = "loader";
    diagnostic.message = std::move(warning);
    AddDiagnostic(diagnostics, std::move(diagnostic));
}

void AddMetadataOverrideDiagnostic(
    std::vector<EffectAssetDiagnostic>& diagnostics,
    const std::filesystem::path& path,
    uint32_t lineNumber,
    const char* scope,
    const std::string& ownerName,
    const std::string& field,
    const std::string& value) {
    EffectAssetDiagnostic diagnostic{};
    diagnostic.severity = EffectAssetDiagnosticSeverity::Info;
    diagnostic.code = "metadataOverride";
    diagnostic.scope = scope;
    diagnostic.field = field;
    diagnostic.source = std::string(scope) + "Override";
    diagnostic.lineNumber = lineNumber;
    diagnostic.message =
        "metadataOverride scope=" + std::string(scope) +
        " owner=\"" + ownerName + "\"" +
        " field=" + field +
        " value=\"" + value + "\" in " +
        path.string() + ":" + std::to_string(lineNumber);
    AddDiagnostic(diagnostics, std::move(diagnostic));
}

void AddTechniqueRegistryDiagnostic(
    std::vector<EffectAssetDiagnostic>& diagnostics,
    const std::filesystem::path& path,
    uint32_t lineNumber,
    std::string code,
    std::string source,
    std::string field,
    const std::string& techniqueId,
    const std::string& message) {
    EffectAssetDiagnostic diagnostic{};
    diagnostic.severity = EffectAssetDiagnosticSeverity::Warning;
    diagnostic.code = std::move(code);
    diagnostic.scope = "technique";
    diagnostic.field = std::move(field);
    diagnostic.source = std::move(source);
    diagnostic.lineNumber = lineNumber;
    diagnostic.message = message + " in " + path.string() + ":" + std::to_string(lineNumber);
    if (!techniqueId.empty()) {
        diagnostic.message += " technique=\"" + techniqueId + "\"";
    }
    AddDiagnostic(diagnostics, std::move(diagnostic));
}

std::string LegacyRendererFallbackId(
    EffectRendererType rendererType,
    const EffectAuthoringRegistry& authoringRegistry) {
    if (const EffectRendererDescriptor* renderer =
            authoringRegistry.Renderers().FindByLegacyRendererType(rendererType)) {
        return renderer->id;
    }
    return "compatMirror:" + std::to_string(static_cast<int>(rendererType));
}

std::string LegacySimulationFallbackId(
    EffectSimulationType simulationType,
    const EffectAuthoringRegistry& authoringRegistry) {
    if (const EffectSimulationDescriptor* simulation =
            authoringRegistry.Simulations().FindByLegacySimulationType(simulationType)) {
        return simulation->id;
    }
    return "compatMirror:" + std::to_string(static_cast<int>(simulationType));
}

void ValidateTechniqueRenderer(
    const std::filesystem::path& path,
    uint32_t lineNumber,
    const std::string& techniqueId,
    const EffectTechniqueDescriptor& descriptor,
    std::vector<EffectAssetDiagnostic>& diagnostics,
    const EffectAuthoringRegistry& authoringRegistry) {
    if (descriptor.rendererId.empty()) {
        AddTechniqueRegistryDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "emptyRendererId",
            "rendererRegistry",
            "rendererId",
            techniqueId,
            "empty rendererId; fallbackRendererId=" +
                LegacyRendererFallbackId(descriptor.rendererType, authoringRegistry));
        return;
    }

    if (authoringRegistry.Renderers().Find(descriptor.rendererId) == nullptr) {
        AddTechniqueRegistryDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "unknownRendererId",
            "rendererRegistry",
            "rendererId",
            techniqueId,
            "unknown rendererId=\"" + descriptor.rendererId +
                "\"; fallbackRendererId=" +
                LegacyRendererFallbackId(descriptor.rendererType, authoringRegistry));
    }
}

void ValidateTechniqueSimulation(
    const std::filesystem::path& path,
    uint32_t lineNumber,
    const std::string& techniqueId,
    const EffectTechniqueDescriptor& descriptor,
    std::vector<EffectAssetDiagnostic>& diagnostics,
    const EffectAuthoringRegistry& authoringRegistry) {
    if (descriptor.simulationId.empty()) {
        AddTechniqueRegistryDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "emptySimulationId",
            "simulationRegistry",
            "simulationId",
            techniqueId,
            "empty simulationId; fallbackSimulationId=" +
                LegacySimulationFallbackId(descriptor.simulationType, authoringRegistry));
        return;
    }

    if (authoringRegistry.Simulations().Find(descriptor.simulationId) == nullptr) {
        AddTechniqueRegistryDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "unknownSimulationId",
            "simulationRegistry",
            "simulationId",
            techniqueId,
            "unknown simulationId=\"" + descriptor.simulationId +
                "\"; fallbackSimulationId=" +
                LegacySimulationFallbackId(descriptor.simulationType, authoringRegistry));
    }
}

const EffectTechniqueDescriptor* ResolveTechniqueDescriptor(
    const std::filesystem::path& path,
    uint32_t lineNumber,
    const std::string& value,
    std::vector<EffectAssetDiagnostic>& diagnostics,
    const EffectAuthoringRegistry& authoringRegistry) {
    const TechniqueRegistry& registry = authoringRegistry.Techniques();
    if (const EffectTechniqueDescriptor* descriptor = registry.Find(value)) {
        ValidateTechniqueRenderer(
            path,
            lineNumber,
            value,
            *descriptor,
            diagnostics,
            authoringRegistry);
        ValidateTechniqueSimulation(
            path,
            lineNumber,
            value,
            *descriptor,
            diagnostics,
            authoringRegistry);
        return descriptor;
    }

    AddTechniqueRegistryDiagnostic(
        diagnostics,
        path,
        lineNumber,
        "unknownTechnique",
        "techniqueRegistry",
        "technique",
        value,
        "unknown technique=\"" + value + "\"; fallbackTechniqueId=ParticleAdditive");
    const EffectTechniqueDescriptor* fallback = registry.Find(std::string_view{"ParticleAdditive"});
    if (fallback == nullptr) {
        fallback = registry.FindByLegacyTechnique(EffectTechnique::ParticleAdditive);
    }
    if (fallback != nullptr) {
        ValidateTechniqueRenderer(
            path,
            lineNumber,
            fallback->id,
            *fallback,
            diagnostics,
            authoringRegistry);
        ValidateTechniqueSimulation(
            path,
            lineNumber,
            fallback->id,
            *fallback,
            diagnostics,
            authoringRegistry);
    }
    return fallback;
}

void ApplyTechniqueDescriptor(
    EffectComponentCommon& common,
    const EffectTechniqueDescriptor& descriptor,
    std::string techniqueId,
    const EffectAuthoringRegistry& authoringRegistry) {
    const std::string displayNameOverride = common.techniqueDisplayName;
    const std::string categoryOverride = common.techniqueCategory;
    const std::string descriptionOverride = common.techniqueDescription;
    const EffectTechniqueMetadataSource displayNameSource = common.techniqueDisplayNameSource;
    const EffectTechniqueMetadataSource categorySource = common.techniqueCategorySource;
    const EffectTechniqueMetadataSource descriptionSource = common.techniqueDescriptionSource;

    common.techniqueId = std::move(techniqueId);
    authoringRegistry.Techniques().ApplyDescriptor(common, descriptor);
    if (displayNameSource != EffectTechniqueMetadataSource::Registry) {
        common.techniqueDisplayName = displayNameOverride;
        common.techniqueDisplayNameSource = displayNameSource;
    }
    if (categorySource != EffectTechniqueMetadataSource::Registry) {
        common.techniqueCategory = categoryOverride;
        common.techniqueCategorySource = categorySource;
    }
    if (descriptionSource != EffectTechniqueMetadataSource::Registry) {
        common.techniqueDescription = descriptionOverride;
        common.techniqueDescriptionSource = descriptionSource;
    }
    if (common.techniqueDisplayNameSource == EffectTechniqueMetadataSource::Component ||
        common.techniqueCategorySource == EffectTechniqueMetadataSource::Component ||
        common.techniqueDescriptionSource == EffectTechniqueMetadataSource::Component) {
        common.techniqueMetadataSource = EffectTechniqueMetadataSource::Component;
    } else if (
        common.techniqueDisplayNameSource == EffectTechniqueMetadataSource::Asset ||
        common.techniqueCategorySource == EffectTechniqueMetadataSource::Asset ||
        common.techniqueDescriptionSource == EffectTechniqueMetadataSource::Asset) {
        common.techniqueMetadataSource = EffectTechniqueMetadataSource::Asset;
    }
}

void ApplyRoutingFromTechnique(
    EffectComponentCommon& common,
    const EffectAuthoringRegistry& authoringRegistry) {
    authoringRegistry.Techniques().ApplyRouting(common);
}

uint32_t TechniqueRenderQueueOffset(
    const EffectComponentCommon& common,
    const EffectAuthoringRegistry& authoringRegistry) {
    if (const EffectTechniqueDescriptor* descriptor =
            authoringRegistry.Techniques().Find(common.techniqueId)) {
        return descriptor->renderQueueOffset;
    }
    if (const EffectTechniqueDescriptor* descriptor =
            authoringRegistry.Techniques().FindByLegacyTechnique(common.technique)) {
        return descriptor->renderQueueOffset;
    }
    return 0;
}

void ApplyAssetTechniqueMetadata(
    const EffectAsset& asset,
    EffectComponentCommon& common) {
    if (!asset.techniqueMetadataOverridden) {
        return;
    }
    if (asset.techniqueDisplayNameOverridden) {
        common.techniqueDisplayName = asset.techniqueDisplayName;
        common.techniqueDisplayNameSource = EffectTechniqueMetadataSource::Asset;
    }
    if (asset.techniqueCategoryOverridden) {
        common.techniqueCategory = asset.techniqueCategory;
        common.techniqueCategorySource = EffectTechniqueMetadataSource::Asset;
    }
    if (asset.techniqueDescriptionOverridden) {
        common.techniqueDescription = asset.techniqueDescription;
        common.techniqueDescriptionSource = EffectTechniqueMetadataSource::Asset;
    }
    common.techniqueMetadataSource = EffectTechniqueMetadataSource::Asset;
}

EffectComponentCommon MakeComponentCommonFromAsset(
    const EffectAsset& asset,
    EffectComponentType type,
    uint32_t id,
    const EffectAuthoringRegistry& authoringRegistry) {
    EffectComponentCommon common =
        EffectComponentAssetBuilder::MakeCommon(
            asset,
            type,
            id,
            authoringRegistry.Techniques());
    ApplyRoutingFromTechnique(common, authoringRegistry);
    ApplyAssetTechniqueMetadata(asset, common);
    common.passState.renderQueue += TechniqueRenderQueueOffset(common, authoringRegistry);
    return common;
}

ComponentEditBuffer MakeComponentBufferFromAsset(
    const EffectAsset& asset,
    EffectComponentType type,
    uint32_t id,
    const EffectAuthoringRegistry& authoringRegistry) {
    EffectComponentCommon common = MakeComponentCommonFromAsset(
        asset,
        type,
        id,
        authoringRegistry);
    switch (type) {
    case EffectComponentType::Trail:
        return EffectComponentAssetBuilder::MakeTrail(asset, common);
    case EffectComponentType::Beam:
        return EffectComponentAssetBuilder::MakeBeam(asset, common);
    case EffectComponentType::Distortion:
        return EffectComponentAssetBuilder::MakeDistortion(asset, common);
    case EffectComponentType::Particle:
    default:
        return EffectComponentAssetBuilder::MakeParticle(asset, common);
    }
}

EffectComponentCommon& ComponentCommon(ComponentEditBuffer& component) {
    return std::visit(
        [](auto& typedComponent) -> EffectComponentCommon& {
            return typedComponent.common;
        },
        component);
}

void ResetComponentPayloadForCommon(const EffectAsset& asset, ComponentEditBuffer& component) {
    EffectComponentCommon common = ComponentCommon(component);
    switch (common.type) {
    case EffectComponentType::Trail:
        component = EffectComponentAssetBuilder::MakeTrail(asset, common);
        break;
    case EffectComponentType::Beam:
        component = EffectComponentAssetBuilder::MakeBeam(asset, common);
        break;
    case EffectComponentType::Distortion:
        component = EffectComponentAssetBuilder::MakeDistortion(asset, common);
        break;
    case EffectComponentType::Particle:
    default:
        component = EffectComponentAssetBuilder::MakeParticle(asset, common);
        break;
    }
}

void AddComponentBuffer(EffectAsset& asset, const ComponentEditBuffer& component) {
    std::visit(
        [&asset](const auto& typedComponent) {
            asset.MutableComponents().Add(typedComponent);
        },
        component);
}

bool ApplyComponentKeyValue(
    const EffectAsset& asset,
    ComponentEditBuffer& component,
    const std::string& key,
    const std::string& value,
    const std::filesystem::path& path,
    uint32_t lineNumber,
    std::vector<EffectAssetDiagnostic>& diagnostics,
    const EffectAuthoringRegistry& authoringRegistry) {
    // Authoring order: common component keys, typed settings, then legacy flat keys.
    std::string scope;
    std::string field;
    if (SplitTypedKey(key, scope, field) &&
        ApplyTypedComponentKey(component, scope, field, value)) {
        return true;
    }

    EffectComponentCommon& common = ComponentCommon(component);
    if (key == "name") {
        common.name = value;
    } else if (key == "technique") {
        if (const EffectTechniqueDescriptor* descriptor =
                ResolveTechniqueDescriptor(path, lineNumber, value, diagnostics, authoringRegistry)) {
            ApplyTechniqueDescriptor(common, *descriptor, value, authoringRegistry);
            ResetComponentPayloadForCommon(asset, component);
        }
    } else if (key == "techniqueDisplayName") {
        common.techniqueDisplayName = value;
        common.techniqueMetadataSource = EffectTechniqueMetadataSource::Component;
        common.techniqueDisplayNameSource = EffectTechniqueMetadataSource::Component;
        AddMetadataOverrideDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "component",
            common.name,
            key,
            value);
    } else if (key == "techniqueCategory") {
        common.techniqueCategory = value;
        common.techniqueMetadataSource = EffectTechniqueMetadataSource::Component;
        common.techniqueCategorySource = EffectTechniqueMetadataSource::Component;
        AddMetadataOverrideDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "component",
            common.name,
            key,
            value);
    } else if (key == "techniqueDescription") {
        common.techniqueDescription = value;
        common.techniqueMetadataSource = EffectTechniqueMetadataSource::Component;
        common.techniqueDescriptionSource = EffectTechniqueMetadataSource::Component;
        AddMetadataOverrideDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "component",
            common.name,
            key,
            value);
    } else if (key == "shader") {
        common.shader = value;
    } else if (key == "texture") {
        common.texture = value;
    } else if (key == "blend") {
        common.passState.blend = ParseBlend(value);
    } else if (key == "layer") {
        common.layer = ParseLayer(value);
    } else if (key == "start" || key == "startTime") {
        common.startTime = ToFloat(value, common.startTime);
    } else if (key == "duration" || key == "lifetime") {
        common.duration = ToFloat(value, common.duration);
    } else if (ApplyLegacyComponentSettingsKey(component, key, value)) {
    } else if (key == "renderQueue") {
        common.passState.renderQueue = static_cast<uint32_t>(
            ToFloat(value, static_cast<float>(common.passState.renderQueue)));
    } else if (key == "size") {
        std::stringstream stream(value);
        char comma = ',';
        float x = common.size.x;
        float y = common.size.y;
        float z = common.size.z;
        if (stream >> x) {
            if (stream >> comma >> y >> comma >> z) {
                common.size = {x, y, z};
            } else {
                common.size = {x, x, x};
            }
        }
    } else if (key == "color") {
        std::stringstream stream(value);
        char comma = ',';
        stream >> common.color.x >> comma >> common.color.y >> comma >> common.color.z;
        if (stream >> comma >> common.color.w) {
        }
    } else {
        return false;
    }
    return true;
}
} // namespace

std::vector<LoadedEffectAsset> EffectAssetLoader::LoadDirectory(
    const std::filesystem::path& directory) const {
    return LoadDirectory(directory, EffectAuthoringRegistry::Default());
}

std::vector<LoadedEffectAsset> EffectAssetLoader::LoadDirectory(
    const std::filesystem::path& directory,
    const EffectAuthoringRegistry& authoringRegistry) const {
    std::vector<LoadedEffectAsset> assets;
    if (!std::filesystem::exists(directory)) {
        return assets;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".effect") {
            continue;
        }

        LoadedEffectAsset loaded{};
        if (LoadFile(entry.path(), loaded, authoringRegistry)) {
            assets.push_back(std::move(loaded));
        }
    }
    return assets;
}

bool EffectAssetLoader::LoadFile(const std::filesystem::path& path, LoadedEffectAsset& outAsset) const {
    return LoadFile(path, outAsset, EffectAuthoringRegistry::Default());
}

bool EffectAssetLoader::LoadFile(
    const std::filesystem::path& path,
    LoadedEffectAsset& outAsset,
    const EffectAuthoringRegistry& authoringRegistry) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    EffectAsset asset{};
    asset.name = path.stem().string();
    asset.passState.blend = ge3::graphics::BlendMode::Additive;
    asset.passState.depth = ge3::graphics::DepthMode::ReadOnly;
    ComponentEditBuffer currentComponent{};
    bool hasCurrentComponent = false;
    uint32_t nextComponentId = 1;
    uint32_t lineNumber = 0;
    std::vector<EffectAssetDiagnostic> diagnostics;

    auto flushCurrentComponent = [&]() {
        if (!hasCurrentComponent) {
            return;
        }
        AddComponentBuffer(asset, currentComponent);
        hasCurrentComponent = false;
    };

    std::string line;
    while (std::getline(file, line)) {
        ++lineNumber;
        const size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }

        const size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, separator));
        const std::string value = Trim(line.substr(separator + 1));
        if (key == "component") {
            flushCurrentComponent();
            const EffectComponentType type = ParseComponentType(value);
            currentComponent = MakeComponentBufferFromAsset(
                asset,
                type,
                nextComponentId++,
                authoringRegistry);
            hasCurrentComponent = true;
            continue;
        }

        if (key == "technique" && !hasCurrentComponent) {
            const EffectTechniqueDescriptor* descriptor =
                ResolveTechniqueDescriptor(path, lineNumber, value, diagnostics, authoringRegistry);
            const EffectComponentType componentType =
                descriptor != nullptr ? descriptor->componentType : EffectComponentType::Particle;
            currentComponent = MakeComponentBufferFromAsset(
                asset,
                componentType,
                nextComponentId++,
                authoringRegistry);
            hasCurrentComponent = true;
            if (descriptor != nullptr) {
                EffectComponentCommon& common = ComponentCommon(currentComponent);
                ApplyTechniqueDescriptor(common, *descriptor, value, authoringRegistry);
                ResetComponentPayloadForCommon(asset, currentComponent);
            }
            continue;
        }

        if (hasCurrentComponent) {
            ApplyComponentKeyValue(
                asset,
                currentComponent,
                key,
                value,
                path,
                lineNumber,
                diagnostics,
                authoringRegistry);
        } else {
            ApplyKeyValue(asset, key, value, path, lineNumber, diagnostics);
        }
    }

    flushCurrentComponent();
    asset.MutableComponents().SyncTypedStorageFromPackedForNormalization();
    outAsset.asset = std::move(asset);
    outAsset.path = path;
    outAsset.diagnostics = std::move(diagnostics);
    outAsset.lastWriteTime = std::filesystem::last_write_time(path);
    return true;
}

bool EffectAssetLoader::ApplyKeyValue(
    EffectAsset& asset,
    const std::string& key,
    const std::string& value,
    const std::filesystem::path& path,
    uint32_t lineNumber,
    std::vector<EffectAssetDiagnostic>& diagnostics) {
    // Authoring order: common asset keys, typed defaults, then legacy flat keys.
    std::string scope;
    std::string field;
    if (SplitTypedKey(key, scope, field) &&
        ApplyTypedDefaultKey(asset, scope, field, value)) {
        return true;
    }

    if (key == "name") {
        asset.name = value;
    } else if (key == "shader") {
        asset.shader = value;
    } else if (key == "texture") {
        asset.texture = value;
    } else if (key == "techniqueDisplayName") {
        asset.techniqueDisplayName = value;
        asset.techniqueMetadataOverridden = true;
        asset.techniqueDisplayNameOverridden = true;
        AddMetadataOverrideDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "asset",
            asset.name,
            key,
            value);
    } else if (key == "techniqueCategory") {
        asset.techniqueCategory = value;
        asset.techniqueMetadataOverridden = true;
        asset.techniqueCategoryOverridden = true;
        AddMetadataOverrideDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "asset",
            asset.name,
            key,
            value);
    } else if (key == "techniqueDescription") {
        asset.techniqueDescription = value;
        asset.techniqueMetadataOverridden = true;
        asset.techniqueDescriptionOverridden = true;
        AddMetadataOverrideDiagnostic(
            diagnostics,
            path,
            lineNumber,
            "asset",
            asset.name,
            key,
            value);
    } else if (key == "blend") {
        asset.passState.blend = ParseBlend(value);
    } else if (key == "layer") {
        asset.layer = ParseLayer(value);
    } else if (key == "lifetime") {
        asset.lifetime = ToFloat(value, asset.lifetime);
    } else if (ApplyLegacyAssetSettingsKey(asset, key, value)) {
    } else if (key == "renderQueue") {
        asset.passState.renderQueue = static_cast<uint32_t>(ToFloat(value, static_cast<float>(asset.passState.renderQueue)));
    } else if (key == "size") {
        std::stringstream stream(value);
        char comma = ',';
        float x = asset.size.x;
        float y = asset.size.y;
        float z = asset.size.z;
        if (stream >> x) {
            if (stream >> comma >> y >> comma >> z) {
                asset.size = {x, y, z};
            } else {
                asset.size = {x, x, x};
            }
        }
    } else if (key == "color") {
        std::stringstream stream(value);
        char comma = ',';
        stream >> asset.color.x >> comma >> asset.color.y >> comma >> asset.color.z;
        if (stream >> comma >> asset.color.w) {
        }
    } else {
        return false;
    }
    return true;
}

