#include "EditorPropertyRegistry.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

EditorPropertyDescriptor MakeDescriptor(
    EditorDomainId domain,
    std::string name,
    std::string displayName,
    EditorPropertyKind kind,
    std::string category,
    std::string valueType,
    float minValue = 0.0f,
    float maxValue = 0.0f,
    bool hasRange = false) {
    EditorPropertyDescriptor descriptor{};
    descriptor.domain = domain;
    descriptor.name = std::move(name);
    descriptor.displayName = std::move(displayName);
    descriptor.kind = kind;
    descriptor.category = std::move(category);
    descriptor.valueType = std::move(valueType);
    descriptor.minValue = minValue;
    descriptor.maxValue = maxValue;
    descriptor.hasRange = hasRange;
    return descriptor;
}

EditorPropertyDescriptor MakeEnumDescriptor(
    EditorDomainId domain,
    std::string name,
    std::string displayName,
    std::string category,
    std::vector<std::string> options) {
    EditorPropertyDescriptor descriptor = MakeDescriptor(
        domain,
        std::move(name),
        std::move(displayName),
        EditorPropertyKind::Enum,
        std::move(category),
        "enum");
    descriptor.enumOptions = std::move(options);
    return descriptor;
}

EditorPropertyDescriptor MakeAssetDescriptor(
    EditorDomainId domain,
    std::string name,
    std::string displayName,
    std::string category,
    EditorAssetKind assetKind) {
    EditorPropertyDescriptor descriptor = MakeDescriptor(
        domain,
        std::move(name),
        std::move(displayName),
        EditorPropertyKind::AssetRef,
        std::move(category),
        ToString(assetKind));
    descriptor.assetKind = assetKind;
    return descriptor;
}

EditorPropertyDescriptor WithDefault(EditorPropertyDescriptor descriptor, std::string defaultValue) {
    descriptor.defaultValue = std::move(defaultValue);
    descriptor.resettable = true;
    return descriptor;
}

EditorPropertyDescriptor WithValidationHint(EditorPropertyDescriptor descriptor, std::string hint) {
    descriptor.validationHint = std::move(hint);
    return descriptor;
}

EditorPropertyDescriptor WithReadOnlyReason(EditorPropertyDescriptor descriptor, std::string reason) {
    descriptor.readOnly = true;
    descriptor.readOnlyReason = std::move(reason);
    return descriptor;
}

EditorPropertyDescriptor RuntimeReadOnly(EditorPropertyDescriptor descriptor) {
    descriptor.runtimeOnly = true;
    return WithReadOnlyReason(std::move(descriptor), "Runtime inspection only; authoring data is not mutated.");
}

} // namespace

void EditorPropertyRegistry::Clear() {
    if (descriptors_.empty()) {
        return;
    }
    descriptors_.clear();
    Touch();
}

bool EditorPropertyRegistry::Register(EditorPropertyDescriptor descriptor) {
    if (descriptor.name.empty()) {
        return false;
    }

    auto it = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&](const EditorPropertyDescriptor& existing) {
            return existing.domain == descriptor.domain && existing.name == descriptor.name;
        });
    if (it != descriptors_.end()) {
        *it = std::move(descriptor);
        Touch();
        return true;
    }

    descriptors_.push_back(std::move(descriptor));
    Touch();
    return true;
}

const EditorPropertyDescriptor* EditorPropertyRegistry::Find(
    EditorDomainId domain,
    std::string_view name) const {
    const auto it = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&](const EditorPropertyDescriptor& descriptor) {
            return descriptor.domain == domain && descriptor.name == name;
        });
    return it != descriptors_.end() ? &*it : nullptr;
}

std::vector<const EditorPropertyDescriptor*> EditorPropertyRegistry::FindByDomain(EditorDomainId domain) const {
    std::vector<const EditorPropertyDescriptor*> results;
    for (const EditorPropertyDescriptor& descriptor : descriptors_) {
        if (descriptor.domain == domain) {
            results.push_back(&descriptor);
        }
    }
    return results;
}

void EditorPropertyRegistry::Touch() {
    ++revision_;
}

void RegisterBuiltInCourseObjectProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.id", "Id", Kind::String, "Identity", "string"));
    registry.Register(MakeAssetDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.meshId", "Mesh", "Identity", EditorAssetKind::Mesh));
    registry.Register(MakeEnumDescriptor(
        EditorDomainId::CourseTerrainPlacement,
        "CourseTerrainPlacement.layer",
        "Layer",
        "Identity",
        {"gameplay_collision", "hero_landmark", "vista_background"}));
    registry.Register(MakeEnumDescriptor(
        EditorDomainId::CourseTerrainPlacement,
        "CourseTerrainPlacement.collisionMode",
        "Collision",
        "Collision",
        {"none", "proxy", "solid"}));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.distance", "Distance", Kind::Float, "Transform", "float", 0.0f, 0.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.lateralOffset", "Lateral", Kind::Float, "Transform", "float", -500.0f, 500.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.verticalOffset", "Vertical", Kind::Float, "Transform", "float", -500.0f, 500.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.forwardOffset", "Forward", Kind::Float, "Transform", "float", -500.0f, 500.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.scale", "Scale", Kind::Vec3, "Transform", "vec3", 0.01f, 200.0f, true), "1.000, 1.000, 1.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.rotation", "Rotation Deg", Kind::Vec3, "Transform", "vec3", -360.0f, 360.0f, true), "0.000, 0.000, 0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.renderPriority", "Render Priority", Kind::Int, "Rendering", "int", -100.0f, 100.0f, true), "0"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.cullBehindDistance", "Cull Behind", Kind::Float, "Rendering", "float", -1.0f, 2000.0f, true), "-1.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.cullAheadDistance", "Cull Ahead", Kind::Float, "Rendering", "float", -1.0f, 3000.0f, true), "-1.000"));

    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.id", "Id", Kind::String, "Identity", "string"));
    registry.Register(MakeAssetDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.meshId", "Mesh", "Identity", EditorAssetKind::Mesh));
    registry.Register(MakeEnumDescriptor(
        EditorDomainId::CourseRockCluster,
        "CourseRockCluster.anchor",
        "Anchor",
        "Placement",
        {"left_wall", "right_wall", "floor", "ceiling_break", "vista_wall"}));
    registry.Register(MakeEnumDescriptor(
        EditorDomainId::CourseRockCluster,
        "CourseRockCluster.type",
        "Type",
        "Placement",
        {"attached_debris", "hero_fracture", "falling_debris", "vista_silhouette"}));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.rotation", "Rotation Deg", Kind::Vec3, "Transform", "vec3", -360.0f, 360.0f, true), "0.000, 0.000, 0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.distance", "Distance", Kind::Float, "Placement", "float", 0.0f, 0.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.count", "Count", Kind::UInt, "Instances", "uint", 0.0f, 32.0f, true), "1"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.minScale", "Min Scale", Kind::Float, "Instances", "float", 0.01f, 20.0f, true), "1.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.maxScale", "Max Scale", Kind::Float, "Instances", "float", 0.01f, 20.0f, true), "1.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.spread", "Spread", Kind::Vec3, "Instances", "vec3", 0.0f, 500.0f, true), "0.000, 0.000, 0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.clearLaneRadius", "Clear Lane", Kind::Float, "Gameplay", "float", 0.0f, 200.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.cullBehindDistance", "Cull Behind", Kind::Float, "Rendering", "float", 0.0f, 2000.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.cullAheadDistance", "Cull Ahead", Kind::Float, "Rendering", "float", 0.0f, 3000.0f, true), "0.000"));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.instanceOverrides.index", "Instance Index", Kind::UInt, "Instance Overrides", "uint", 0.0f, 31.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.instanceOverrides.localOffset", "Instance Offset", Kind::Vec3, "Instance Overrides", "vec3", -500.0f, 500.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.instanceOverrides.scale", "Instance Scale", Kind::Vec3, "Instance Overrides", "vec3", 0.01f, 20.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.instanceOverrides.rotation", "Instance Rotation", Kind::Vec3, "Instance Overrides", "vec3", -360.0f, 360.0f, true));
}

void RegisterBuiltInVfxProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(WithValidationHint(MakeDescriptor(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.name", "Name", Kind::String, "Identity", "string"), "Must be stable enough for asset references and diagnostics."));
    registry.Register(WithReadOnlyReason(MakeEnumDescriptor(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.technique", "Technique", "Renderer", {"particle", "trail", "beam", "ring", "cylinder", "distortion", "mixed"}), "Technique is derived from typed VFX components; use typed component replacement to change it."));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.spawnRate", "Spawn Rate", Kind::Float, "Particle", "float", 0.0f, 2000.0f, true), "64.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.lifetime", "Lifetime", Kind::Float, "Particle", "float", 0.01f, 30.0f, true), "1.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.initialSpeed", "Initial Speed", Kind::Float, "Particle", "float", 0.0f, 250.0f, true), "8.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.color", "Color", Kind::Color, "Shading", "color", 0.0f, 1.0f, true), "1.000, 1.000, 1.000"));
    registry.Register(MakeAssetDescriptor(EditorDomainId::VfxEffectAsset, "VfxEffectAsset.texture", "Texture", "Shading", EditorAssetKind::Texture));
    registry.Register(RuntimeReadOnly(MakeDescriptor(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.active", "Active", Kind::Bool, "Runtime", "bool")));
    registry.Register(RuntimeReadOnly(MakeDescriptor(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.age", "Age", Kind::Float, "Runtime", "float", 0.0f, 0.0f, false)));
    registry.Register(RuntimeReadOnly(MakeDescriptor(EditorDomainId::VfxEffectInstance, "VfxEffectInstance.particleCount", "Particle Count", Kind::UInt, "Runtime", "uint")));
}

void RegisterBuiltInTerrainProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::TerrainGeneration, "TerrainGeneration.seed", "Seed", Kind::UInt, "Generation", "uint"), "0"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::TerrainGeneration, "TerrainGeneration.chunkLength", "Chunk Length", Kind::Float, "Generation", "float", 16.0f, 2048.0f, true), "192.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::TerrainGeneration, "TerrainGeneration.chunkRadius", "Chunk Radius", Kind::UInt, "Streaming", "uint", 1.0f, 16.0f, true), "4"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::TerrainGeneration, "TerrainGeneration.noiseScale", "Noise Scale", Kind::Float, "Shape", "float", 0.001f, 1.0f, true), "0.085"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::TerrainGeneration, "TerrainGeneration.wallHeight", "Wall Height", Kind::Float, "Shape", "float", 0.0f, 200.0f, true), "24.000"));
    registry.Register(MakeAssetDescriptor(EditorDomainId::CourseTerrainMaterialPreset, "CourseTerrainMaterialPreset.albedo", "Albedo", "Material", EditorAssetKind::Texture));
    registry.Register(MakeAssetDescriptor(EditorDomainId::CourseTerrainMaterialPreset, "CourseTerrainMaterialPreset.normal", "Normal", "Material", EditorAssetKind::Texture));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseTerrainMaterialPreset, "CourseTerrainMaterialPreset.roughness", "Roughness", Kind::Float, "Material", "float", 0.04f, 1.0f, true), "0.650"));
}

void RegisterBuiltInPostProcessProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(MakeDescriptor(EditorDomainId::PostProcessPass, "PostProcessPass.enabled", "Enabled", Kind::Bool, "Pass", "bool"));
    registry.Register(WithReadOnlyReason(MakeEnumDescriptor(EditorDomainId::PostProcessPass, "PostProcessPass.stage", "Stage", "Pass", {"pre_tonemap", "tonemap", "post_tonemap", "debug"}), "Stage is derived from the production post-process pipeline."));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::PostProcessPass, "PostProcessPass.intensity", "Intensity", Kind::Float, "Tuning", "float", 0.0f, 10.0f, true), "1.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::PostProcessPass, "PostProcessPass.threshold", "Threshold", Kind::Float, "Tuning", "float", 0.0f, 8.0f, true), "1.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::PostProcessPass, "PostProcessPass.radius", "Radius", Kind::Float, "Tuning", "float", 0.0f, 64.0f, true), "8.000"));
}

void RegisterBuiltInRenderProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(WithReadOnlyReason(MakeDescriptor(EditorDomainId::RenderPreset, "RenderPreset.name", "Name", Kind::String, "Identity", "string"), "Render preset authoring store is not bound yet."));
    registry.Register(WithReadOnlyReason(MakeEnumDescriptor(EditorDomainId::RenderPreset, "RenderPreset.quality", "Quality", "Frame", {"low", "medium", "high", "cinematic"}), "Render preset authoring store is not bound yet."));
    registry.Register(WithReadOnlyReason(WithDefault(MakeDescriptor(EditorDomainId::RenderPreset, "RenderPreset.renderScale", "Render Scale", Kind::Float, "Frame", "float", 0.25f, 2.0f, true), "1.000"), "Render preset authoring store is not bound yet."));
    registry.Register(WithReadOnlyReason(WithDefault(MakeDescriptor(EditorDomainId::RenderPreset, "RenderPreset.vsync", "VSync", Kind::Bool, "Frame", "bool"), "true"), "Render preset authoring store is not bound yet."));
    registry.Register(RuntimeReadOnly(MakeDescriptor(EditorDomainId::RenderGraphPass, "RenderGraphPass.gpuMs", "GPU ms", Kind::Float, "Runtime", "float")));
    registry.Register(RuntimeReadOnly(MakeDescriptor(EditorDomainId::RenderGraphPass, "RenderGraphPass.enabled", "Enabled", Kind::Bool, "Runtime", "bool")));
}

void RegisterBuiltInCameraProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.distance", "Distance", Kind::Float, "Timeline", "float", 0.0f, 0.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.backDistance", "Back Distance", Kind::Float, "Rig", "float", 0.0f, 200.0f, true), "18.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.verticalOffset", "Vertical Offset", Kind::Float, "Rig", "float", -100.0f, 100.0f, true), "6.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.lateralOffset", "Lateral Offset", Kind::Float, "Rig", "float", -100.0f, 100.0f, true), "0.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.lookAheadDistance", "Look Ahead", Kind::Float, "Rig", "float", 0.0f, 500.0f, true), "54.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.lookUpOffset", "Look Up", Kind::Float, "Rig", "float", -100.0f, 100.0f, true), "2.200"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.lookForwardOffset", "Look Forward", Kind::Float, "Rig", "float", -100.0f, 100.0f, true), "8.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.fov", "FOV", Kind::Float, "Lens", "float", 10.0f, 140.0f, true), "54.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseCameraKey, "CourseCameraKey.roll", "Roll", Kind::Float, "Lens", "float", -180.0f, 180.0f, true), "0.000"));
    registry.Register(MakeEnumDescriptor(EditorDomainId::CameraRig, "CameraRig.mode", "Mode", "Rig", {"authoring", "gameplay", "cinematic", "debug"}));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CameraRig, "CameraRig.nearClip", "Near Clip", Kind::Float, "Lens", "float", 0.001f, 10.0f, true), "0.100"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CameraRig, "CameraRig.farClip", "Far Clip", Kind::Float, "Lens", "float", 10.0f, 100000.0f, true), "10000.000"));
}

void RegisterBuiltInCourseEventProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::CourseEventMarker, "CourseEventMarker.distance", "Distance", Kind::Float, "Timeline", "float", 0.0f, 0.0f, true), "0.000"));
    registry.Register(WithValidationHint(MakeDescriptor(EditorDomainId::CourseEventMarker, "CourseEventMarker.type", "Type", Kind::String, "Dispatch", "string"), "Must match a production course event dispatcher route."));
    registry.Register(WithValidationHint(MakeDescriptor(EditorDomainId::CourseEventMarker, "CourseEventMarker.id", "Id", Kind::String, "Dispatch", "string"), "Must resolve to a production event asset or scripted event id."));
    registry.Register(MakeDescriptor(EditorDomainId::CourseEventMarker, "CourseEventMarker.payload", "Payload", Kind::String, "Dispatch", "string"));
}

void RegisterBuiltInGameplayProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::GameplayTuning, "GameplayTuning.playerSpeed", "Player Speed", Kind::Float, "Player", "float", 0.0f, 500.0f, true), "60.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::GameplayTuning, "GameplayTuning.lockOnRange", "Lock-on Range", Kind::Float, "Combat", "float", 0.0f, 5000.0f, true), "900.000"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::GameplayTuning, "GameplayTuning.reticleAssist", "Reticle Assist", Kind::Float, "Combat", "float", 0.0f, 1.0f, true), "0.350"));
    registry.Register(WithDefault(MakeDescriptor(EditorDomainId::GameplayTuning, "GameplayTuning.enemySpawnBudget", "Enemy Spawn Budget", Kind::UInt, "Encounter", "uint", 0.0f, 256.0f, true), "32"));
}

void RegisterBuiltInEditorProperties(EditorPropertyRegistry& registry) {
    RegisterBuiltInCourseObjectProperties(registry);
    RegisterBuiltInVfxProperties(registry);
    RegisterBuiltInTerrainProperties(registry);
    RegisterBuiltInPostProcessProperties(registry);
    RegisterBuiltInRenderProperties(registry);
    RegisterBuiltInCameraProperties(registry);
    RegisterBuiltInCourseEventProperties(registry);
    RegisterBuiltInGameplayProperties(registry);
}

const char* ToString(EditorPropertyKind kind) {
    switch (kind) {
    case EditorPropertyKind::Bool:
        return "Bool";
    case EditorPropertyKind::Int:
        return "Int";
    case EditorPropertyKind::UInt:
        return "UInt";
    case EditorPropertyKind::Float:
        return "Float";
    case EditorPropertyKind::String:
        return "String";
    case EditorPropertyKind::Vec2:
        return "Vec2";
    case EditorPropertyKind::Vec3:
        return "Vec3";
    case EditorPropertyKind::Vec4:
        return "Vec4";
    case EditorPropertyKind::Color:
        return "Color";
    case EditorPropertyKind::Enum:
        return "Enum";
    case EditorPropertyKind::AssetRef:
        return "AssetRef";
    case EditorPropertyKind::ObjectRef:
        return "ObjectRef";
    }
    return "Unknown";
}

} // namespace editor
